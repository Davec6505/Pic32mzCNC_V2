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
#include "peripheral/ocmp/plib_ocmp1.h"
#include "peripheral/ocmp/plib_ocmp4.h"
#include "peripheral/ocmp/plib_ocmp5.h"
#include "peripheral/coretimer/plib_coretimer.h"
#include <math.h>
#include <string.h>

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// Motion planner state
static motion_execution_state_t execution_state = PLANNER_STATE_IDLE;
static float default_acceleration = ACCELERATION_DEFAULT;
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
static void UpdateAxisOCRPeriods(void)
{
    uint32_t y_period = MotionPlanner_CalculateOCRPeriod(current_axis_velocities[1]); // Y-axis = OCMP1
    uint32_t x_period = MotionPlanner_CalculateOCRPeriod(current_axis_velocities[0]); // X-axis = OCMP4
    uint32_t z_period = MotionPlanner_CalculateOCRPeriod(current_axis_velocities[2]); // Z-axis = OCMP5

    // Update OCR compare values
    // Note: This sets the period for continuous pulse generation
    OCMP1_CompareSecondaryValueSet(y_period);
    OCMP4_CompareSecondaryValueSet(x_period);
    OCMP5_CompareSecondaryValueSet(z_period);
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
}

void MotionPlanner_ProcessBuffer(void)
{
    motion_block_t *current_block = MotionBuffer_GetNext();
    if (current_block == NULL)
    {
        execution_state = PLANNER_STATE_IDLE;
        return; // No blocks to process
    }

    execution_state = PLANNER_STATE_PLANNING;

    // Calculate motion parameters
    MotionPlanner_CalculateDistance(current_block);
    MotionPlanner_OptimizeVelocityProfile(current_block);

    // Start motion execution
    MotionPlanner_ExecuteBlock(current_block);

    // Update statistics
    statistics.blocks_processed++;
    if (current_block->max_velocity > statistics.peak_velocity)
    {
        statistics.peak_velocity = current_block->max_velocity;
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

void MotionPlanner_UpdateTrajectory(void)
{
    // Real-time trajectory calculations called at 1kHz from Core Timer

    // Get the current motion block if we don't have one
    if (current_motion_block == NULL)
    {
        current_motion_block = MotionBuffer_GetNext();
        if (current_motion_block == NULL)
        {
            // No motion blocks available - stop all axes
            for (int i = 0; i < MAX_AXES; i++)
            {
                current_axis_velocities[i] = 0.0f;
            }
            UpdateAxisOCRPeriods();
            return;
        }
    }

    // Calculate current velocities for each axis based on acceleration profile
    motion_execution_timer++;

    if (motion_execution_timer < current_motion_block->duration * 1000.0f)
    {
        // Still executing current block
        float progress = motion_execution_timer / (current_motion_block->duration * 1000.0f);

        // Calculate velocity profile with basic acceleration/deceleration
        float current_velocity;
        float accel_time = 0.5f; // 50% of time for acceleration/deceleration

        if (progress < accel_time)
        {
            // Acceleration phase
            current_velocity = current_motion_block->max_velocity * (progress / accel_time);
        }
        else if (progress > (1.0f - accel_time))
        {
            // Deceleration phase
            float decel_progress = (progress - (1.0f - accel_time)) / accel_time;
            current_velocity = current_motion_block->max_velocity * (1.0f - decel_progress);
        }
        else
        {
            // Constant velocity phase
            current_velocity = current_motion_block->max_velocity;
        }

        // Calculate individual axis velocities based on motion direction
        // This is a simple implementation - actual implementation would consider
        // the vector components and calculate each axis velocity properly
        for (int i = 0; i < MAX_AXES; i++)
        {
            if (current_motion_block->target_pos[i] != 0.0f) // Axis is moving
            {
                current_axis_velocities[i] = current_velocity;
            }
            else
            {
                current_axis_velocities[i] = 0.0f; // Axis not moving
            }
        }

        // Update OCR periods for all axes
        UpdateAxisOCRPeriods();

        // Update statistics
        statistics.average_velocity = current_velocity;
    }
    else
    {
        // Block completed, move to next
        MotionBuffer_Complete();
        current_motion_block = NULL;
        motion_execution_timer = 0;
        statistics.blocks_processed++;

        // Stop all axes momentarily between blocks
        for (int i = 0; i < MAX_AXES; i++)
        {
            current_axis_velocities[i] = 0.0f;
        }
        UpdateAxisOCRPeriods();
    }
}

void MotionPlanner_UpdateAxisPosition(uint8_t axis, int32_t position)
{
    // Position feedback from OCR interrupts
    if (axis < MAX_AXES)
    {
        axis_positions[axis] = position;

        // TODO: Add position error checking here
        // Could compare with expected position from trajectory
        // and trigger alarms if position error exceeds threshold
    }
}

// *****************************************************************************
// *****************************************************************************
// Section: Motion System Getter/Setter Function Implementations
// *****************************************************************************
// *****************************************************************************

float MotionPlanner_GetCurrentVelocity(uint8_t axis)
{
    if (axis < MAX_AXES)
    {
        return current_axis_velocities[axis];
    }
    return 0.0f;
}

void MotionPlanner_SetCurrentVelocity(uint8_t axis, float velocity)
{
    if (axis < MAX_AXES)
    {
        current_axis_velocities[axis] = velocity;

        // Update OCR periods when velocity changes
        UpdateAxisOCRPeriods();
    }
}

int32_t MotionPlanner_GetAxisPosition(uint8_t axis)
{
    if (axis < MAX_AXES)
    {
        return axis_positions[axis];
    }
    return 0;
}

void MotionPlanner_SetAxisPosition(uint8_t axis, int32_t position)
{
    if (axis < MAX_AXES)
    {
        axis_positions[axis] = position;
    }
}

bool MotionPlanner_IsAxisActive(uint8_t axis)
{
    // This function would need access to hardware layer axis state
    // For now, check if there's current velocity
    if (axis < MAX_AXES)
    {
        return (current_axis_velocities[axis] > 0.0f);
    }
    return false;
}

void MotionPlanner_SetAxisActive(uint8_t axis, bool active)
{
    if (axis < MAX_AXES)
    {
        if (!active)
        {
            // Stop the axis by setting velocity to zero
            current_axis_velocities[axis] = 0.0f;
            UpdateAxisOCRPeriods();
        }
        // Note: Setting active=true would require starting motion
        // which should be done through normal motion planning
    }
}

uint32_t MotionPlanner_GetAxisStepCount(uint8_t axis)
{
    // This would need to interface with hardware layer
    // For now, return a calculated value based on position
    if (axis < MAX_AXES)
    {
        return (uint32_t)abs(axis_positions[axis]);
    }
    return 0;
}

void MotionPlanner_ResetAxisStepCount(uint8_t axis)
{
    // This would interface with hardware layer to reset step counters
    // For now, we'll reset position (though this should be done carefully)
    if (axis < MAX_AXES)
    {
        // Note: Resetting position should be done with caution
        // Usually only during homing or coordinate system reset
        // axis_positions[axis] = 0;  // Commented for safety
    }
}

/*******************************************************************************
 End of File
 */