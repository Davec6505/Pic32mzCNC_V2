/*******************************************************************************
  Motion Planner Implementation File

  Company:
    Microchip Technology Inc.

  File Name:
    motion_planner.c

  Summary:
    This file contains the implementation of motion planning with look-ahead
    optimization.

  Description:
    This file implements a sophisticated motion planner that provides:
    - 16-buffer look-ahead processing
    - Velocity optimization and junction analysis
    - Smooth acceleration/deceleration profiles
    - Trajectory calculation and execution coordination
*******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include "motion_planner.h"
#include "motion_gcode_parser.h"
#include "interpolation_engine.h"
#include "peripheral/ocmp/plib_ocmp1.h"
#include "peripheral/ocmp/plib_ocmp4.h"
#include "peripheral/ocmp/plib_ocmp5.h"
#include "peripheral/tmr/plib_tmr2.h" // For OCMP1
#include "peripheral/tmr/plib_tmr3.h" // For OCMP4
#include "peripheral/tmr/plib_tmr4.h" // For OCMP5
#include "peripheral/coretimer/plib_coretimer.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// Motion planner state
static motion_execution_state_t execution_state = PLANNER_STATE_IDLE;
static float default_acceleration = ACCELERATION_DEFAULT;

// Simple position tracking (replacing faulty interpolation engine)
static position_t current_position = {0.0f, 0.0f, 0.0f, 0.0f};
static position_t target_position = {0.0f, 0.0f, 0.0f, 0.0f};

// Simple position interpolation function
static void UpdatePosition(float progress)
{
    // Linear interpolation from current to target position
    current_position.x = current_position.x + (target_position.x - current_position.x) * progress;
    current_position.y = current_position.y + (target_position.y - current_position.y) * progress;
    current_position.z = current_position.z + (target_position.z - current_position.z) * progress;
}

// External interface for status reporting
position_t MotionPlanner_GetCurrentPosition(void)
{
    return current_position;
}
static motion_planner_stats_t statistics;

// Current motion execution variables
static motion_block_t *current_motion_block = NULL;
static uint32_t motion_execution_timer = 0;

// OCR calculation constants
#define CORE_TIMER_FREQ 100000000UL  // 100MHz core timer frequency
#define STEPS_PER_MM 400.0f          // Steps per mm (configurable)
#define MIN_STEP_FREQ 1.0f           // Minimum step frequency (Hz)
#define MAX_STEP_FREQ 50000.0f       // Maximum step frequency (Hz)
#define OCR_STOPPED_VALUE 0xFFFFFFFF // Very large value to effectively stop

// Current axis velocities (mm/min)
static float current_axis_velocities[MAX_AXES] = {0.0f, 0.0f, 0.0f};

// Position feedback from OCR interrupts
static volatile int32_t axis_positions[MAX_AXES] = {0, 0, 0};

// *****************************************************************************
// *****************************************************************************
// Section: Local Helper Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    uint32_t MotionPlanner_CalculateOCRPeriod(float velocity_mm_min)

  Summary:
    Converts velocity in mm/min to OCR period value.

  Description:
    Calculates the OCR compare period needed to achieve the specified velocity.

  Parameters:
    velocity_mm_min - Desired velocity in mm/min

  Returns:
    OCR period value (0xFFFFFFFF if velocity is too low)
*******************************************************************************/
uint32_t MotionPlanner_CalculateOCRPeriod(float velocity_mm_min)
{
    if (velocity_mm_min <= 0.0f)
    {
        return OCR_STOPPED_VALUE; // Stop the axis
    }

    // Convert mm/min to steps/second
    float steps_per_second = (velocity_mm_min * STEPS_PER_MM) / 60.0f;

    // Limit step frequency
    if (steps_per_second < MIN_STEP_FREQ)
    {
        return OCR_STOPPED_VALUE;
    }
    if (steps_per_second > MAX_STEP_FREQ)
    {
        steps_per_second = MAX_STEP_FREQ;
    }

    // Calculate OCR period: Period = CORE_TIMER_FREQ / desired_frequency
    uint32_t ocr_period = (uint32_t)(CORE_TIMER_FREQ / steps_per_second);

    return ocr_period;
}

/*******************************************************************************
  Function:
    static void UpdateAxisOCRPeriods(void)

  Summary:
    Updates OCR periods for all axes based on current velocities.
*******************************************************************************/
static void __attribute__((unused)) UpdateAxisOCRPeriods(void)
{
    uint32_t y_period = MotionPlanner_CalculateOCRPeriod(current_axis_velocities[1]); // Y-axis = OCMP1
    uint32_t x_period = MotionPlanner_CalculateOCRPeriod(current_axis_velocities[0]); // X-axis = OCMP4
    uint32_t z_period = MotionPlanner_CalculateOCRPeriod(current_axis_velocities[2]); // Z-axis = OCMP5

    // CRITICAL FIX: Update cnc_axes flags to connect motion planner with step counting system
    extern cnc_axis_t cnc_axes[MAX_AXES]; // Declared in app.c

    // Update axis active flags and motion states based on current velocities
    for (int i = 0; i < MAX_AXES; i++)
    {
        if (current_axis_velocities[i] > 0.0f)
        {
            cnc_axes[i].is_active = true;
            cnc_axes[i].motion_state = AXIS_CONSTANT; // Non-idle state
        }
        else
        {
            cnc_axes[i].is_active = false;
            cnc_axes[i].motion_state = AXIS_IDLE;
        }
    } // Based on working stepper code: Set pulse width and period, but don't start/stop timers
    // Timers are started once during initialization and stay running

    // Y-axis (OCMP1 + TMR2)
    if (current_axis_velocities[1] > 0.0f)
    {
        TMR2_PeriodSet(y_period);
        OCMP1_CompareValueSet(100);               // Short pulse width for step signals
        OCMP1_CompareSecondaryValueSet(y_period); // Period
    }

    // X-axis (OCMP4 + TMR3)
    if (current_axis_velocities[0] > 0.0f)
    {
        TMR3_PeriodSet(x_period);
        OCMP4_CompareValueSet(100);               // Short pulse width for step signals
        OCMP4_CompareSecondaryValueSet(x_period); // Period

        // Enable output compare for X-axis
        OCMP4_Enable();
    }

    // Z-axis (OCMP5 + TMR4)
    if (current_axis_velocities[2] > 0.0f)
    {
        TMR4_PeriodSet(z_period);
        OCMP5_CompareValueSet(100);               // Short pulse width for step signals
        OCMP5_CompareSecondaryValueSet(z_period); // Period
    }
}

// *****************************************************************************
// *****************************************************************************
// Section: Motion Planner Functions
// *****************************************************************************
// *****************************************************************************

void MotionPlanner_Initialize(void)
{
    execution_state = PLANNER_STATE_IDLE;
    default_acceleration = ACCELERATION_DEFAULT;
    current_motion_block = NULL;
    motion_execution_timer = 0;

    // Clear statistics
    memset(&statistics, 0, sizeof(motion_planner_stats_t));

    // Initialize interpolation engine for position tracking
    extern bool INTERP_Initialize(void);
    extern void INTERP_Enable(bool enable);
    INTERP_Initialize();
    INTERP_Enable(true);
}

void MotionPlanner_ProcessBuffer(void)
{
    // This function is called when new blocks are added to trigger planning
    // But it should NOT consume blocks - that's done by MotionPlanner_UpdateTrajectory()

    // Just check if we have blocks to process and set state
    if (!MotionBuffer_IsEmpty())
    {
        execution_state = PLANNER_STATE_PLANNING;
        // The actual block consumption and execution happens in MotionPlanner_UpdateTrajectory()
    }
    else
    {
        execution_state = PLANNER_STATE_IDLE;
    }
}
void MotionPlanner_CalculateDistance(motion_block_t *block)
{
    if (block == NULL)
        return;

    // Get current position from parser state
    motion_parser_state_t parser_state = MotionGCodeParser_GetState();

    float dx = block->target_pos[0] - parser_state.current_position[0];
    float dy = block->target_pos[1] - parser_state.current_position[1];
    float dz = block->target_pos[2] - parser_state.current_position[2];

    block->distance = sqrtf(dx * dx + dy * dy + dz * dz);

    if (block->distance > 0.0f && block->feedrate > 0.0f)
    {
        block->duration = block->distance / block->feedrate;
    }
    else
    {
        block->duration = 0.0f;
    }
}

void MotionPlanner_OptimizeVelocityProfile(motion_block_t *block)
{
    if (block == NULL)
        return;

    // Initialize entry velocity (conservative start)
    block->entry_velocity = 0.0f;

    // Look ahead to optimize exit velocity
    motion_block_t *next_block = MotionBuffer_Peek(1);

    if (next_block && next_block->is_valid)
    {
        // Calculate junction velocity between current and next block
        float junction_velocity = MotionPlanner_CalculateJunctionVelocity(block, next_block);

        // Set exit velocity to safe junction velocity
        block->exit_velocity = fminf(junction_velocity, block->max_velocity);

        statistics.blocks_optimized++;
    }
    else
    {
        // Last block - come to complete stop
        block->exit_velocity = 0.0f;
    }

    // Update average velocity calculation
    float block_avg_velocity = (block->entry_velocity + block->exit_velocity) / 2.0f;
    statistics.average_velocity = (statistics.average_velocity * statistics.blocks_processed +
                                   block_avg_velocity) /
                                  (statistics.blocks_processed + 1);
}

float MotionPlanner_CalculateJunctionVelocity(motion_block_t *block1, motion_block_t *block2)
{
    if (block1 == NULL || block2 == NULL)
    {
        return 0.0f;
    }

    // Calculate direction vectors
    float dx1 = block1->target_pos[0] - MotionGCodeParser_GetState().current_position[0];
    float dy1 = block1->target_pos[1] - MotionGCodeParser_GetState().current_position[1];
    float dz1 = block1->target_pos[2] - MotionGCodeParser_GetState().current_position[2];

    float dx2 = block2->target_pos[0] - block1->target_pos[0];
    float dy2 = block2->target_pos[1] - block1->target_pos[1];
    float dz2 = block2->target_pos[2] - block1->target_pos[2];

    // Normalize direction vectors
    float len1 = sqrtf(dx1 * dx1 + dy1 * dy1 + dz1 * dz1);
    float len2 = sqrtf(dx2 * dx2 + dy2 * dy2 + dz2 * dz2);

    if (len1 < 0.001f || len2 < 0.001f)
    {
        return MIN_JUNCTION_SPEED; // Very short move
    }

    dx1 /= len1;
    dy1 /= len1;
    dz1 /= len1;
    dx2 /= len2;
    dy2 /= len2;
    dz2 /= len2;

    // Calculate dot product (cosine of angle)
    float cos_angle = dx1 * dx2 + dy1 * dy2 + dz1 * dz2;

    // Limit angle to valid range
    if (cos_angle > 1.0f)
        cos_angle = 1.0f;
    if (cos_angle < -1.0f)
        cos_angle = -1.0f;

    // Calculate junction velocity based on angle
    // Sharper angles = lower velocity
    float angle_factor = (cos_angle + 1.0f) / 2.0f; // 0 to 1 scale

    float junction_velocity = MIN_JUNCTION_SPEED +
                              (MAX_JUNCTION_SPEED - MIN_JUNCTION_SPEED) * angle_factor;

    // Limit to block velocities
    junction_velocity = fminf(junction_velocity, block1->max_velocity);
    junction_velocity = fminf(junction_velocity, block2->max_velocity);

    return junction_velocity;
}

void MotionPlanner_ExecuteBlock(motion_block_t *block)
{
    if (block == NULL)
        return;

    execution_state = PLANNER_STATE_EXECUTING;

    // Update parser position state
    MotionGCodeParser_SetPosition(block->target_pos[0],
                                  block->target_pos[1],
                                  block->target_pos[2]);

    // Set current motion block for trajectory calculations
    current_motion_block = block;
    motion_execution_timer = (uint32_t)(block->duration * 1000.0f); // Convert to ms

    // Call hardware interface to start actual motion
    // This will activate OCR modules and start step pulse generation
    if (APP_ExecuteMotionBlock(block))
    {
        // Successfully started hardware motion
        statistics.execution_time_ms += motion_execution_timer;
    }
    else
    {
        // Hardware failed to start - mark as simulation only
        execution_state = PLANNER_STATE_IDLE;
    }
}

bool MotionPlanner_IsMotionComplete(void)
{
    // Check if buffer is empty and no current motion
    if (!MotionBuffer_IsEmpty())
    {
        return false;
    }

    if (current_motion_block != NULL)
    {
        return false;
    }

    return true;
}

motion_execution_state_t MotionPlanner_GetState(void)
{
    return execution_state;
}

motion_planner_stats_t MotionPlanner_GetStatistics(void)
{
    return statistics;
}

void MotionPlanner_EmergencyStop(void)
{
    // Clear motion buffer
    MotionBuffer_Clear();

    // Stop current motion
    current_motion_block = NULL;
    motion_execution_timer = 0;

    // Reset state
    execution_state = PLANNER_STATE_IDLE;

    // Note: In real implementation, this would also disable step generation
    // and apply emergency braking to all axes
}

void MotionPlanner_SetAcceleration(float acceleration)
{
    if (acceleration > 0.0f)
    {
        default_acceleration = acceleration;
    }
}

float MotionPlanner_GetAcceleration(void)
{
    return default_acceleration;
}

void MotionPlanner_UpdateAxisPosition(uint8_t axis, int32_t position)
{
    if (axis < MAX_AXES)
    {
        axis_positions[axis] = position;
    }
}

float MotionPlanner_GetCurrentVelocity(uint8_t axis)
{
    if (axis < MAX_AXES)
    {
        return current_axis_velocities[axis];
    }
    return 0.0f;
}

void MotionPlanner_UpdateTrajectory(void)
{
    // Call interpolation engine tasks - this processes motion and updates positions
    extern void INTERP_Tasks(void);
    INTERP_Tasks();

    // Debug: Check position after INTERP_Tasks (reduced frequency for UGS compatibility)
    static uint16_t position_check_counter = 0;
    position_check_counter++;
    // Position check disabled for UGS clean protocol
    /*
    if (position_check_counter % 1000 == 0) // Check every 1000ms (1 second) instead of 100ms
    {
        position_t current_pos = GetCurrentPosition(); // Use our simple position tracking
        char pos_debug[128];
        sprintf(pos_debug, "[MP_POS_CHECK] X=%.3f Y=%.3f Z=%.3f\r\n",
                current_pos.x, current_pos.y, current_pos.z);
        APP_UARTPrint(pos_debug);
    }
    */

    // Try to get a new motion block if idle
    if (current_motion_block == NULL)
    {
        // Add counter to reduce debug spam
        static uint16_t debug_counter = 0;
        debug_counter++;

        current_motion_block = MotionBuffer_GetNext();

        if (current_motion_block != NULL)
        {
            // New block received! (debug disabled for UGS)
            /*
            char debug_msg[128];
            sprintf(debug_msg, "[MP_NEW_BLOCK] X=%.1f Y=%.1f Z=%.1f dur=%.3fs\r\n",
                    current_motion_block->target_pos[0],
                    current_motion_block->target_pos[1],
                    current_motion_block->target_pos[2],
                    current_motion_block->duration);
            APP_UARTPrint(debug_msg);
            */

            // Set up simple position tracking (replacing faulty interpolation engine)
            target_position.x = current_motion_block->target_pos[0];
            target_position.y = current_motion_block->target_pos[1];
            target_position.z = current_motion_block->target_pos[2];

            // Debug output disabled for UGS compatibility
            /*
            position_t start_pos = GetCurrentPosition();
            char pos_debug[128];
            sprintf(pos_debug, "[MP_SIMPLE] Start: X=%.3f Y=%.3f Z=%.3f -> Target: X=%.3f Y=%.3f Z=%.3f\r\n",
                    start_pos.x, start_pos.y, start_pos.z,
                    target_position.x, target_position.y, target_position.z);
            APP_UARTPrint(pos_debug);
            */

            motion_execution_timer = 0; // Reset timer for the new block
        }
        else
        {
            // DEBUG: Disabled for UGS compatibility - too much output
            // APP_UARTPrint("[MP_DEBUG] No blocks available\r\n");
        }
    }

    // If we have a block, process its trajectory
    if (current_motion_block != NULL)
    {
        motion_execution_timer++;
        float total_duration_ms = current_motion_block->duration * 1000.0f;

        if (motion_execution_timer < total_duration_ms)
        {
            // Still executing current block - update position
            float progress = (float)motion_execution_timer / total_duration_ms;
            UpdatePosition(progress);

            // Debug position update every 20ms (DISABLED for UGS compatibility)
            /*
            static uint16_t pos_update_counter = 0;
            pos_update_counter++;
            if (pos_update_counter % 20 == 0)
            {
                char prog_debug[128];
                sprintf(prog_debug, "[MP_PROGRESS] %.1f%% X=%.3f Y=%.3f Z=%.3f\r\n",
                        progress * 100.0f, current_position.x, current_position.y, current_position.z);
                APP_UARTPrint(prog_debug);
            }
            */
        }
        else
        {
            // Motion for this block is complete (debug disabled for UGS)
            /*
            char debug_msg[128];
            sprintf(debug_msg, "[MP_BLOCK_DONE] timer=%d duration_ms=%.1f\r\n",
                    motion_execution_timer, total_duration_ms);
            APP_UARTPrint(debug_msg);
            */

            // Ensure we reach the exact target position
            current_position = target_position;
            /*
            sprintf(debug_msg, "[MP_FINAL_POS] X=%.3f Y=%.3f Z=%.3f\r\n",
                    current_position.x, current_position.y, current_position.z);
            APP_UARTPrint(debug_msg);
            */

            // Mark the block as complete in the buffer
            MotionBuffer_Complete();

            // Set planner to idle, ready for the next block
            current_motion_block = NULL;
        }
    }
}

/*******************************************************************************
 End of File
 */