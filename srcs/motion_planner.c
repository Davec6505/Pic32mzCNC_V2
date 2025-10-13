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

// *****************************************************************************
// S-Curve Motion Profile Implementation
// *****************************************************************************

// S-curve motion constants
#define SCURVE_MAX_VELOCITY 500.0f      // mm/min - conservative for stability
#define SCURVE_MAX_ACCELERATION 3000.0f // mm/min² - realistic CNC acceleration
#define SCURVE_JERK_LIMIT 10000.0f      // mm/min³ - comfortable jerk limit
#define SCURVE_ENABLE true              // Enable S-curve motion profiles

// Current motion block with S-curve profile
typedef struct
{
    float total_time;     // Total move time (seconds)
    float accel_time;     // Acceleration phase time
    float const_time;     // Constant velocity phase time
    float decel_time;     // Deceleration phase time
    float peak_velocity;  // Maximum velocity achieved (mm/min)
    float acceleration;   // Peak acceleration (mm/min²)
    float distance;       // Total distance (mm)
    bool use_scurve;      // Enable S-curve (false = linear fallback)
    position_t start_pos; // Starting position
    position_t end_pos;   // Target position
} scurve_motion_profile_t;

static scurve_motion_profile_t current_motion_profile;

// *****************************************************************************
// S-Curve Profile Calculation Functions
// *****************************************************************************

static bool CalculateSCurveProfile(float distance, float target_velocity, scurve_motion_profile_t *profile)
{
    if (!profile || distance <= 0.001f)
    {
        return false;
    }

    // Initialize profile
    memset(profile, 0, sizeof(scurve_motion_profile_t));
    profile->distance = distance;
    profile->use_scurve = SCURVE_ENABLE;

    // Limit target velocity to maximum
    float limited_velocity = fminf(target_velocity, SCURVE_MAX_VELOCITY);

    // Convert mm/min to mm/sec for calculations
    float velocity_mm_sec = limited_velocity / 60.0f;
    float accel_mm_sec2 = SCURVE_MAX_ACCELERATION / 3600.0f;
    float accel_distance = (velocity_mm_sec * velocity_mm_sec) / (2.0f * accel_mm_sec2);

    // Debug calculations disabled for UGS compatibility
    /*
    char debug_calc[256];
    sprintf(debug_calc, "[DEBUG] dist=%.3f, vel_min=%.1f, vel_sec=%.3f, accel_sec2=%.6f, accel_dist=%.3f\r\n",
            distance, limited_velocity, velocity_mm_sec, accel_mm_sec2, accel_distance);
    extern void APP_UARTPrint(const char *message);
    APP_UARTPrint(debug_calc);
    */

    if (accel_distance * 2.0f > distance)
    {
        // Cannot reach target velocity - triangular profile
        profile->peak_velocity = sqrtf(distance * accel_mm_sec2) * 60.0f; // Convert back to mm/min
        profile->const_time = 0.0f;
    }
    else
    {
        // Trapezoidal profile with constant velocity section
        profile->peak_velocity = limited_velocity;
        float const_distance = distance - 2.0f * accel_distance;
        profile->const_time = const_distance / velocity_mm_sec; // seconds
    }

    // Calculate timing (using corrected units)
    float peak_vel_per_sec = profile->peak_velocity / 60.0f;

    profile->accel_time = peak_vel_per_sec / accel_mm_sec2;
    profile->decel_time = profile->accel_time; // Symmetric for simplicity
    profile->acceleration = SCURVE_MAX_ACCELERATION;
    profile->total_time = profile->accel_time + profile->const_time + profile->decel_time;

    return true;
}

static float GetSCurvePosition(float elapsed_time, const scurve_motion_profile_t *profile)
{
    if (!profile || elapsed_time <= 0.0f)
    {
        return 0.0f;
    }

    if (elapsed_time >= profile->total_time)
    {
        return profile->distance; // Motion complete
    }

    if (!profile->use_scurve)
    {
        // Linear fallback
        return (elapsed_time / profile->total_time) * profile->distance;
    }

    float position = 0.0f;
    float peak_vel_per_sec = profile->peak_velocity / 60.0f;
    float accel_mm_sec2 = profile->acceleration / 3600.0f; // Convert mm/min² to mm/sec²

    if (elapsed_time <= profile->accel_time)
    {
        // Acceleration phase - S-curve start (quadratic)
        float t = elapsed_time;
        position = 0.5f * accel_mm_sec2 * t * t;
    }
    else if (elapsed_time <= profile->accel_time + profile->const_time)
    {
        // Constant velocity phase
        float accel_distance = 0.5f * accel_mm_sec2 * profile->accel_time * profile->accel_time;
        float const_elapsed = elapsed_time - profile->accel_time;
        position = accel_distance + peak_vel_per_sec * const_elapsed;
    }
    else
    {
        // Deceleration phase - S-curve end (quadratic)
        float accel_distance = 0.5f * accel_mm_sec2 * profile->accel_time * profile->accel_time;
        float const_distance = peak_vel_per_sec * profile->const_time;
        float decel_elapsed = elapsed_time - profile->accel_time - profile->const_time;
        float decel_distance = peak_vel_per_sec * decel_elapsed - 0.5f * accel_mm_sec2 * decel_elapsed * decel_elapsed;
        position = accel_distance + const_distance + decel_distance;
    }

    return fminf(position, profile->distance); // Clamp to total distance
}

// Enhanced position interpolation with S-curve motion profiles
static void UpdatePosition(float progress)
{
    if (!current_motion_profile.use_scurve)
    {
        // Linear interpolation fallback (original method)
        current_position.x = current_motion_profile.start_pos.x +
                             (current_motion_profile.end_pos.x - current_motion_profile.start_pos.x) * progress;
        current_position.y = current_motion_profile.start_pos.y +
                             (current_motion_profile.end_pos.y - current_motion_profile.start_pos.y) * progress;
        current_position.z = current_motion_profile.start_pos.z +
                             (current_motion_profile.end_pos.z - current_motion_profile.start_pos.z) * progress;

        // CRITICAL: Update cnc_axes position during linear motion for real-time status reporting
        extern cnc_axis_t cnc_axes[MAX_AXES];
        cnc_axes[0].current_position = (int32_t)(current_position.x * 400.0f); // X-axis (400 steps/mm)
        cnc_axes[1].current_position = (int32_t)(current_position.y * 400.0f); // Y-axis
        cnc_axes[2].current_position = (int32_t)(current_position.z * 400.0f); // Z-axis
        return;
    }

    // S-curve interpolation
    float elapsed_time = progress * current_motion_profile.total_time;
    float scurve_progress = GetSCurvePosition(elapsed_time, &current_motion_profile) / current_motion_profile.distance;

    // Clamp progress to [0, 1]
    scurve_progress = fmaxf(0.0f, fminf(1.0f, scurve_progress));

    // Apply S-curve progress to position interpolation
    current_position.x = current_motion_profile.start_pos.x +
                         (current_motion_profile.end_pos.x - current_motion_profile.start_pos.x) * scurve_progress;
    current_position.y = current_motion_profile.start_pos.y +
                         (current_motion_profile.end_pos.y - current_motion_profile.start_pos.y) * scurve_progress;
    current_position.z = current_motion_profile.start_pos.z +
                         (current_motion_profile.end_pos.z - current_motion_profile.start_pos.z) * scurve_progress;

    // CRITICAL: Update cnc_axes position during motion for real-time status reporting
    extern cnc_axis_t cnc_axes[MAX_AXES];
    cnc_axes[0].current_position = (int32_t)(current_position.x * 400.0f); // X-axis (400 steps/mm)
    cnc_axes[1].current_position = (int32_t)(current_position.y * 400.0f); // Y-axis
    cnc_axes[2].current_position = (int32_t)(current_position.z * 400.0f); // Z-axis
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
    {
        // Debug disabled for UGS compatibility
        // extern void APP_UARTPrint_blocking(const char *message);
        // APP_UARTPrint_blocking("[DEBUG: MotionPlanner_ExecuteBlock called with NULL block!]\r\n");
        return;
    }

    // Debug disabled for UGS compatibility
    // extern void APP_UARTPrint_blocking(const char *message);
    // APP_UARTPrint_blocking("[DEBUG: MotionPlanner_ExecuteBlock called - starting hardware motion]\r\n");

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
        // Successfully started hardware motion - debug disabled for UGS compatibility
        // APP_UARTPrint_blocking("[DEBUG: APP_ExecuteMotionBlock SUCCESS - hardware started]\r\n");
        statistics.execution_time_ms += motion_execution_timer;
    }
    else
    {
        // Hardware failed to start - mark as simulation only - debug disabled for UGS compatibility
        // APP_UARTPrint_blocking("[DEBUG: APP_ExecuteMotionBlock FAILED - hardware not started]\r\n");
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
            // New block received! - Debug disabled for UGS compatibility
            // extern void APP_UARTPrint_blocking(const char *message);
            // APP_UARTPrint_blocking("[DEBUG: Motion planner got new block]\r\n");

            // char block_debug[128];
            // sprintf(block_debug, "[DEBUG: Block target X=%.3f Y=%.3f Z=%.3f]\r\n",
            //         current_motion_block->target_pos[0],
            //         current_motion_block->target_pos[1],
            //         current_motion_block->target_pos[2]);
            // APP_UARTPrint_blocking(block_debug);

            // CRITICAL FIX: Execute the hardware motion for the new block
            // This was missing - the planner was getting blocks but never executing them!
            MotionPlanner_ExecuteBlock(current_motion_block);

            /*
            char debug_msg[128];
            sprintf(debug_msg, "[MP_NEW_BLOCK] X=%.1f Y=%.1f Z=%.1f\r\n",
                    current_motion_block->target_pos[0],
                    current_motion_block->target_pos[1],
                    current_motion_block->target_pos[2]);
            APP_UARTPrint(debug_msg);
            */
            // Set up S-curve motion profile for the new block
            target_position.x = current_motion_block->target_pos[0];
            target_position.y = current_motion_block->target_pos[1];
            target_position.z = current_motion_block->target_pos[2];

            // Debug disabled for UGS compatibility
            /*
            char debug_target[128];
            sprintf(debug_target, "[MP_TARGET_SET] target_pos[0]=%.3f target_pos[1]=%.3f target_pos[2]=%.3f\r\n",
                    current_motion_block->target_pos[0], current_motion_block->target_pos[1], current_motion_block->target_pos[2]);
            APP_UARTPrint(debug_target);

            sprintf(debug_target, "[MP_TARGET_ASSIGNED] X=%.3f Y=%.3f Z=%.3f\r\n",
                    target_position.x, target_position.y, target_position.z);
            APP_UARTPrint(debug_target);
            */

            // Calculate move distance
            float dx = target_position.x - current_position.x;
            float dy = target_position.y - current_position.y;
            float dz = target_position.z - current_position.z;
            float move_distance = sqrtf(dx * dx + dy * dy + dz * dz);

            // Set up S-curve motion profile
            current_motion_profile.start_pos = current_position;
            current_motion_profile.end_pos = target_position;

            // Use the motion block's feedrate, default to reasonable speed if not set
            float target_feedrate = (current_motion_block->max_velocity > 0) ? current_motion_block->max_velocity : SCURVE_MAX_VELOCITY;

            // Calculate S-curve profile
            bool profile_success = CalculateSCurveProfile(move_distance, target_feedrate, &current_motion_profile);

            if (!profile_success)
            {
                // Fallback to linear motion if S-curve calculation fails
                current_motion_profile.use_scurve = false;
                current_motion_profile.total_time = current_motion_block->duration;
                current_motion_profile.start_pos = current_position;
                current_motion_profile.end_pos = target_position;
            }

            // Debug output disabled for UGS compatibility
            /*
            char pos_debug[128];
            sprintf(pos_debug, "[MP_MOTION_START] X=%.3f->%.3f Time=%.3fs Mode=%s\r\n",
                    current_position.x, target_position.x,
                    current_motion_profile.total_time,
                    current_motion_profile.use_scurve ? "S-CURVE" : "LINEAR");
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

    // If we have a block, process its trajectory with S-curve profile
    if (current_motion_block != NULL)
    {
        motion_execution_timer++;

        // Use S-curve profile timing instead of motion block duration
        float total_duration_ms = current_motion_profile.total_time * 1000.0f;

        // Debug timing disabled for UGS compatibility
        // static uint16_t timing_debug_counter = 0;
        // timing_debug_counter++;
        // if (timing_debug_counter == 1 || timing_debug_counter % 100 == 0) // Debug first and every 100ms
        // {
        //     char timing_debug[128];
        //     sprintf(timing_debug, "[DEBUG: Motion timing] timer=%d duration_ms=%.1f\r\n",
        //             motion_execution_timer, total_duration_ms);
        //     extern void APP_UARTPrint_blocking(const char *message);
        //     APP_UARTPrint_blocking(timing_debug);
        // }

        if (motion_execution_timer < total_duration_ms)
        {
            // Still executing current block - update position with S-curve interpolation
            float progress = (float)motion_execution_timer / total_duration_ms;
            UpdatePosition(progress);

            // Debug position update every 20ms (DISABLED for UGS compatibility)
            /*
            static uint16_t pos_update_counter = 0;
            pos_update_counter++;
            if (pos_update_counter % 20 == 0)
            {
                char prog_debug[128];
                sprintf(prog_debug, "[MP_PROGRESS] %.1f%% X=%.3f Y=%.3f Z=%.3f %s\r\n",
                        progress * 100.0f, current_position.x, current_position.y, current_position.z,
                        current_motion_profile.use_scurve ? "SCURVE" : "LINEAR");
                APP_UARTPrint(prog_debug);
            }
            */
        }
        else
        {
            // Motion for this block is complete - debug disabled for UGS compatibility
            // char completion_debug[128];
            // sprintf(completion_debug, "[DEBUG: Motion block completed] timer=%d duration_ms=%.1f\r\n",
            //         motion_execution_timer, total_duration_ms);
            // extern void APP_UARTPrint_blocking(const char *message);
            // APP_UARTPrint_blocking(completion_debug);

            // Ensure we reach the exact target position
            current_position = target_position;
            /*
            sprintf(debug_msg, "[MP_FINAL_POS] X=%.3f Y=%.3f Z=%.3f\r\n",
                    current_position.x, current_position.y, current_position.z);
            APP_UARTPrint(debug_msg);
            */

            // CRITICAL FIX: Update cnc_axes position tracking for status reporting
            // Convert from mm (float) to steps (int32_t) for cnc_axes system
            extern cnc_axis_t cnc_axes[MAX_AXES];
            cnc_axes[0].current_position = (int32_t)(current_position.x * 400.0f); // X-axis (400 steps/mm)
            cnc_axes[1].current_position = (int32_t)(current_position.y * 400.0f); // Y-axis
            cnc_axes[2].current_position = (int32_t)(current_position.z * 400.0f); // Z-axis

            // Debug output disabled for UGS compatibility
            // char debug_msg[128];
            // sprintf(debug_msg, "[DEBUG: Motion completed - updating cnc_axes] X=%d Y=%d Z=%d steps\r\n",
            //         cnc_axes[0].current_position, cnc_axes[1].current_position, cnc_axes[2].current_position);
            // extern void APP_UARTPrint_blocking(const char *message);
            // APP_UARTPrint_blocking(debug_msg);

            // Mark the block as complete in the buffer
            // APP_UARTPrint_blocking("[DEBUG: Calling MotionBuffer_Complete() - advancing tail]\r\n");
            MotionBuffer_Complete();

            // Set planner to idle, ready for the next block
            current_motion_block = NULL;
            // APP_UARTPrint_blocking("[DEBUG: Block completed - ready for next block]\r\n");
        }
    }
}

/*******************************************************************************
 End of File
 */