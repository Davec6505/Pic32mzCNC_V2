/*******************************************************************************
  Linear Interpolation Engine Implementation

  File Name:
    interpolation_engine.c

  Summary:
    Professional trajectory generation and motion control implementation

  Description:
    Provides smooth, precise linear interpolation with trapezoidal and S-curve
    velocity profiles for professional CNC motion control.
*******************************************************************************/

#include "interpolation_engine.h"
#include "peripheral/tmr1/plib_tmr1.h"
#include "peripheral/ocmp/plib_ocmp1.h"
#include "peripheral/ocmp/plib_ocmp4.h"
#include "peripheral/ocmp/plib_ocmp5.h"
#include "peripheral/gpio/plib_gpio.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// *****************************************************************************
// Local Variables
// *****************************************************************************

static interpolation_context_t interp_context;
static uint32_t motion_start_time = 0;

// *****************************************************************************
// Local Function Prototypes
// *****************************************************************************

static void calculate_step_parameters(void);
static void update_motion_state(void);
static void generate_step_signals(void);
static bool validate_motion_parameters(void);
static void reset_motion_state(void);

// *****************************************************************************
// Initialization and Configuration Functions
// *****************************************************************************

bool INTERP_Initialize(void)
{
    // Clear the interpolation context
    memset(&interp_context, 0, sizeof(interpolation_context_t));

    // Set default configuration
    for (int i = 0; i < INTERP_MAX_AXES; i++)
    {
        interp_context.steps_per_mm[i] = INTERP_STEPS_PER_MM;
        interp_context.max_velocity_per_axis[i] = INTERP_MAX_VELOCITY;
        interp_context.acceleration_per_axis[i] = INTERP_MAX_ACCELERATION;
    }

    // Initialize motion parameters
    interp_context.motion.state = MOTION_STATE_IDLE;
    interp_context.motion.profile_type = MOTION_PROFILE_TRAPEZOIDAL;
    interp_context.motion.max_velocity = INTERP_MAX_VELOCITY;
    interp_context.motion.acceleration = INTERP_MAX_ACCELERATION;
    interp_context.motion.deceleration = INTERP_MAX_ACCELERATION;

    // Initialize look-ahead planner
    INTERP_PlannerClearBuffer();
    interp_context.planner.junction_deviation = INTERP_JUNCTION_DEVIATION;
    interp_context.planner.minimum_planner_speed = INTERP_MIN_PLANNER_SPEED;

    // Initialize homing system
    interp_context.homing.state = HOMING_STATE_IDLE;
    interp_context.homing.axis_mask = 0;
    interp_context.homing.current_axis = AXIS_X;
    interp_context.homing.seek_rate = INTERP_HOMING_SEEK_RATE;
    interp_context.homing.locate_rate = INTERP_HOMING_FEED_RATE;
    interp_context.homing.pulloff_distance = INTERP_HOMING_PULLOFF_DISTANCE;

    // Set default home positions (typically 0,0,0)
    for (int i = 0; i < INTERP_MAX_AXES; i++)
    {
        INTERP_SetHomingPosition((axis_id_t)i, 0.0f);
    }

    // Initialize safety controls
    interp_context.active_limit_mask = LIMIT_MASK_NONE; // No limits masked by default

    // Configure hardware peripherals
    if (!INTERP_ConfigureTimer1())
    {
        return false;
    }

    if (!INTERP_ConfigureOCRModules())
    {
        return false;
    }

    if (!INTERP_ConfigureStepperGPIO())
    {
        return false;
    }

    interp_context.initialized = true;
    interp_context.enabled = false;

    // Remove problematic printf that causes corruption
    // printf("Interpolation Engine Initialized\n");

    return true;
}

bool INTERP_Configure(float steps_per_mm[], float max_vel[], float accel[])
{
    if (!interp_context.initialized || !steps_per_mm || !max_vel || !accel)
    {
        return false;
    }

    for (int i = 0; i < INTERP_MAX_AXES; i++)
    {
        if (steps_per_mm[i] <= 0 || max_vel[i] <= 0 || accel[i] <= 0)
        {
            return false;
        }

        interp_context.steps_per_mm[i] = steps_per_mm[i];
        interp_context.max_velocity_per_axis[i] = max_vel[i];
        interp_context.acceleration_per_axis[i] = accel[i];
    }

    printf("Interpolation Engine Configured:\n");
    printf("  X: %.1f steps/mm, %.1f mm/min max, %.1f accel\n",
           steps_per_mm[AXIS_X], max_vel[AXIS_X], accel[AXIS_X]);
    printf("  Y: %.1f steps/mm, %.1f mm/min max, %.1f accel\n",
           steps_per_mm[AXIS_Y], max_vel[AXIS_Y], accel[AXIS_Y]);
    printf("  Z: %.1f steps/mm, %.1f mm/min max, %.1f accel\n",
           steps_per_mm[AXIS_Z], max_vel[AXIS_Z], accel[AXIS_Z]);

    return true;
}

void INTERP_Enable(bool enable)
{
    interp_context.enabled = enable;

    if (enable)
    {
        printf("Interpolation Engine Enabled\n");
    }
    else
    {
        printf("Interpolation Engine Disabled\n");
        INTERP_StopMotion();
    }
}

void INTERP_Reset(void)
{
    reset_motion_state();

    // Reset statistics
    interp_context.moves_completed = 0;
    interp_context.total_steps_generated = 0;
    interp_context.average_velocity = 0.0f;
    interp_context.motion_time_ms = 0;

    printf("Interpolation Engine Reset\n");
}

void INTERP_Shutdown(void)
{
    INTERP_StopMotion();
    interp_context.enabled = false;
    interp_context.initialized = false;

    printf("Interpolation Engine Shutdown\n");
}

// *****************************************************************************
// Motion Planning Functions
// *****************************************************************************

bool INTERP_PlanLinearMove(position_t start, position_t end, float feed_rate)
{
    if (!interp_context.initialized || !interp_context.enabled)
    {
        return false;
    }

    if (interp_context.motion.state != MOTION_STATE_IDLE)
    {
        return false; // Motion already in progress
    }

    // Validate positions
    if (!INTERP_IsPositionValid(start) || !INTERP_IsPositionValid(end))
    {
        return false;
    }

    // CHECK SOFT LIMITS BEFORE MOTION PLANNING
    if (!INTERP_CheckSoftLimits(end))
    {
        printf("Linear move rejected - soft limit violation\n");
        return false; // Move violates soft limits
    }

    // Calculate motion parameters
    interp_context.motion.start_position = start;
    interp_context.motion.end_position = end;
    interp_context.motion.current_position = start;
    interp_context.motion.target_velocity = feed_rate;
    interp_context.motion.total_distance = INTERP_CalculateDistance(start, end);
    interp_context.motion.distance_traveled = 0.0f;
    interp_context.motion.time_elapsed = 0.0f;

    // Limit feed rate to machine capabilities
    float max_feed_rate = INTERP_MAX_VELOCITY;
    for (int i = 0; i < INTERP_MAX_AXES; i++)
    {
        if (interp_context.max_velocity_per_axis[i] < max_feed_rate)
        {
            max_feed_rate = interp_context.max_velocity_per_axis[i];
        }
    }

    if (interp_context.motion.target_velocity > max_feed_rate)
    {
        interp_context.motion.target_velocity = max_feed_rate;
    }

    // Calculate estimated time
    interp_context.motion.estimated_time = INTERP_CalculateMoveTime(start, end, feed_rate);

    // Validate motion parameters
    if (!validate_motion_parameters())
    {
        return false;
    }

    // Calculate step parameters
    calculate_step_parameters();

    printf("Linear move planned: %.2f mm in %.2f seconds at %.1f mm/min\n",
           interp_context.motion.total_distance,
           interp_context.motion.estimated_time / 60.0f,
           interp_context.motion.target_velocity);

    return true;
}

bool INTERP_PlanRapidMove(position_t start, position_t end)
{
    // Rapid moves use maximum velocity
    float rapid_feed_rate = INTERP_MAX_VELOCITY;

    printf("Planning rapid move at %.1f mm/min\n", rapid_feed_rate);

    return INTERP_PlanLinearMove(start, end, rapid_feed_rate);
}

bool INTERP_ExecuteMove(void)
{
    if (!interp_context.initialized || !interp_context.enabled)
    {
        return false;
    }

    // Check if we're already executing or if buffer is empty
    if (interp_context.motion.state != MOTION_STATE_IDLE)
    {
        return false; // Already executing
    }

    // Get next block from planner buffer
    planner_block_t *current_block = INTERP_PlannerGetCurrentBlock();
    if (!current_block)
    {
        return false; // No blocks to execute
    }

    // Optimize buffer before execution
    INTERP_PlannerOptimizeBuffer();

    // Set up motion parameters from planner block
    interp_context.motion.start_position = current_block->start_position;
    interp_context.motion.end_position = current_block->end_position;
    interp_context.motion.current_position = current_block->start_position;
    interp_context.motion.target_velocity = current_block->nominal_speed;
    interp_context.motion.profile_type = current_block->profile_type;
    interp_context.motion.total_distance = current_block->distance;

    // Calculate motion timing based on profile
    switch (current_block->profile_type)
    {
    case MOTION_PROFILE_S_CURVE:
    {
        scurve_profile_t scurve_profile;
        if (INTERP_CalculateSCurveProfile(
                current_block->distance,
                current_block->nominal_speed,
                current_block->acceleration,
                INTERP_JERK_LIMIT,
                &scurve_profile))
        {

            interp_context.motion.estimated_time =
                2.0f * scurve_profile.jerk_time_accel +
                2.0f * scurve_profile.accel_time +
                scurve_profile.constant_velocity_time +
                2.0f * scurve_profile.jerk_time_decel;
        }
        break;
    }

    case MOTION_PROFILE_TRAPEZOIDAL:
    {
        trapezoidal_profile_t trap_profile;
        if (INTERP_CalculateTrapezoidalProfile(
                current_block->distance,
                current_block->nominal_speed,
                current_block->acceleration,
                &trap_profile))
        {

            interp_context.motion.estimated_time =
                trap_profile.acceleration_time +
                trap_profile.constant_velocity_time +
                trap_profile.deceleration_time;
        }
        break;
    }

    default:
        interp_context.motion.estimated_time = current_block->distance / current_block->nominal_speed;
        break;
    }

    // Calculate step parameters
    calculate_step_parameters();

    // Validate motion parameters
    if (!validate_motion_parameters())
    {
        return false;
    }

    // Start motion execution
    interp_context.motion.state = MOTION_STATE_ACCELERATING;
    interp_context.motion.time_elapsed = 0.0f;
    interp_context.motion.distance_traveled = 0.0f;
    motion_start_time = TMR1_CounterGet();

    // START STEP PULSE GENERATION - this enables the OCR hardware
    INTERP_StartStepGeneration();

    printf("Executing planner block %d: %.2f mm at %.1f mm/min\n",
           current_block->block_id, current_block->distance, current_block->nominal_speed);
    printf("Step pulse generation STARTED\n");

    return true;
}

bool INTERP_IsMotionComplete(void)
{
    return (interp_context.motion.state == MOTION_STATE_COMPLETE ||
            interp_context.motion.state == MOTION_STATE_IDLE);
}

void INTERP_StopMotion(void)
{
    interp_context.motion.state = MOTION_STATE_IDLE;
    interp_context.motion.emergency_stop = false;
    interp_context.motion.feed_hold = false;

    // STOP STEP PULSE GENERATION - this disables the OCR hardware
    INTERP_StopStepGeneration();

    // Reset step generation
    for (int i = 0; i < INTERP_MAX_AXES; i++)
    {
        interp_context.steps.step_active[i] = false;
    }

    printf("Motion stopped - step pulse generation STOPPED\n");
}

// *****************************************************************************
// Limit Switch and Safety Functions
// *****************************************************************************

void INTERP_StopSingleAxis(axis_id_t axis, const char *reason)
{
    if (axis >= INTERP_MAX_AXES)
        return;

    printf("STOPPING AXIS %c: %s\n", (axis == AXIS_X) ? 'X' : (axis == AXIS_Y) ? 'Y'
                                                          : (axis == AXIS_Z)   ? 'Z'
                                                                               : 'A',
           reason);

    // Immediately stop step generation for this specific axis
    INTERP_SetAxisStepRate(axis, 0.0f);

    // Disable OCR module for this axis specifically
    switch (axis)
    {
    case AXIS_X:
        OCMP1_Disable(); // Stop X-axis pulse generation immediately
        printf("X-axis OCR1 DISABLED - No more pulses\n");
        break;
    case AXIS_Y:
        OCMP4_Disable(); // Stop Y-axis pulse generation immediately
        printf("Y-axis OCR4 DISABLED - No more pulses\n");
        break;
    case AXIS_Z:
        OCMP5_Disable(); // Stop Z-axis pulse generation immediately
        printf("Z-axis OCR5 DISABLED - No more pulses\n");
        break;
    case AXIS_A:
        // A-axis would use OCMP2 or OCMP3 when implemented
        printf("A-axis stopped (OCR not yet configured)\n");
        break;
    }

    // Mark axis as stopped
    interp_context.steps.step_active[axis] = false;
    interp_context.steps.step_frequency[axis] = 0.0f;

    // Trigger error callback if registered
    if (interp_context.error_callback)
    {
        char error_msg[100];
        snprintf(error_msg, sizeof(error_msg), "Axis %c limit hit: %s",
                 (axis == AXIS_X) ? 'X' : (axis == AXIS_Y) ? 'Y'
                                      : (axis == AXIS_Z)   ? 'Z'
                                                           : 'A',
                 reason);
        interp_context.error_callback(error_msg);
    }
}

void INTERP_HandleHardLimit(axis_id_t axis, bool min_limit, bool max_limit)
{
    // Hard limit switch has been triggered - IMMEDIATE STOP required

    if (min_limit)
    {
        printf("HARD LIMIT HIT: Axis %c MINIMUM limit switch triggered!\n",
               (axis == AXIS_X) ? 'X' : (axis == AXIS_Y) ? 'Y'
                                    : (axis == AXIS_Z)   ? 'Z'
                                                         : 'A');
        INTERP_StopSingleAxis(axis, "Hard limit - MIN switch");

        // Set alarm state - motion must be reset before continuing
        interp_context.motion.state = MOTION_STATE_ALARM;
    }

    if (max_limit)
    {
        printf("HARD LIMIT HIT: Axis %c MAXIMUM limit switch triggered!\n",
               (axis == AXIS_X) ? 'X' : (axis == AXIS_Y) ? 'Y'
                                    : (axis == AXIS_Z)   ? 'Z'
                                                         : 'A');
        INTERP_StopSingleAxis(axis, "Hard limit - MAX switch");

        // Set alarm state - motion must be reset before continuing
        interp_context.motion.state = MOTION_STATE_ALARM;
    }

    // Optional: Stop all motion if any hard limit is hit (GRBL behavior)
    if (min_limit || max_limit)
    {
        // printf("Hard limit detected - stopping ALL motion for safety\n");  // Commented out - UGS treats as error
        INTERP_EmergencyStop();
    }
}

bool INTERP_CheckSoftLimits(position_t target_position)
{
    // Software limits - check before motion starts
    // These should be set based on your machine's working envelope

    // Default soft limits (adjust for your machine)
    const float soft_limits_min[INTERP_MAX_AXES] = {-200.0f, -200.0f, -100.0f, -360.0f}; // X,Y,Z,A mins
    const float soft_limits_max[INTERP_MAX_AXES] = {200.0f, 200.0f, 0.0f, 360.0f};       // X,Y,Z,A maxs

    bool limit_violated = false;

    // Check X-axis soft limits
    if (target_position.x < soft_limits_min[AXIS_X])
    {
        printf("SOFT LIMIT VIOLATION: X-axis target %.2f < minimum %.2f\n",
               target_position.x, soft_limits_min[AXIS_X]);
        limit_violated = true;
    }
    if (target_position.x > soft_limits_max[AXIS_X])
    {
        printf("SOFT LIMIT VIOLATION: X-axis target %.2f > maximum %.2f\n",
               target_position.x, soft_limits_max[AXIS_X]);
        limit_violated = true;
    }

    // Check Y-axis soft limits
    if (target_position.y < soft_limits_min[AXIS_Y])
    {
        printf("SOFT LIMIT VIOLATION: Y-axis target %.2f < minimum %.2f\n",
               target_position.y, soft_limits_min[AXIS_Y]);
        limit_violated = true;
    }
    if (target_position.y > soft_limits_max[AXIS_Y])
    {
        printf("SOFT LIMIT VIOLATION: Y-axis target %.2f > maximum %.2f\n",
               target_position.y, soft_limits_max[AXIS_Y]);
        limit_violated = true;
    }

    // Check Z-axis soft limits
    if (target_position.z < soft_limits_min[AXIS_Z])
    {
        printf("SOFT LIMIT VIOLATION: Z-axis target %.2f < minimum %.2f\n",
               target_position.z, soft_limits_min[AXIS_Z]);
        limit_violated = true;
    }
    if (target_position.z > soft_limits_max[AXIS_Z])
    {
        printf("SOFT LIMIT VIOLATION: Z-axis target %.2f > maximum %.2f\n",
               target_position.z, soft_limits_max[AXIS_Z]);
        limit_violated = true;
    }

    if (limit_violated)
    {
        if (interp_context.error_callback)
        {
            interp_context.error_callback("Soft limit violation - move rejected");
        }
        return false; // Reject the move
    }

    return true; // Move is within soft limits
}

void INTERP_LimitSwitchISR(axis_id_t axis, bool min_switch_state, bool max_switch_state)
{
    // This function should be called from GPIO interrupt service routine
    // when limit switch pins change state

    // Debounce check - only act on switch closure (low state for normally-open switches)
    static bool last_min_state[INTERP_MAX_AXES] = {true, true, true, true};
    static bool last_max_state[INTERP_MAX_AXES] = {true, true, true, true};

    // Check for min limit switch activation (transition from high to low)
    if (last_min_state[axis] && !min_switch_state)
    {
        // Min limit switch just closed - STOP IMMEDIATELY
        INTERP_HandleHardLimit(axis, true, false);
    }

    // Check for max limit switch activation (transition from high to low)
    if (last_max_state[axis] && !max_switch_state)
    {
        // Max limit switch just closed - STOP IMMEDIATELY
        INTERP_HandleHardLimit(axis, false, true);
    }

    // Update last states for debouncing
    last_min_state[axis] = min_switch_state;
    last_max_state[axis] = max_switch_state;
}

// *****************************************************************************
// Real-time Control Functions
// *****************************************************************************

void INTERP_EmergencyStop(void)
{
    interp_context.motion.emergency_stop = true;
    interp_context.motion.state = MOTION_STATE_IDLE;

    // IMMEDIATELY STOP ALL STEP GENERATION - hardware emergency stop
    INTERP_StopStepGeneration();

    // Immediately stop all step generation
    for (int i = 0; i < INTERP_MAX_AXES; i++)
    {
        interp_context.steps.step_active[i] = false;
        INTERP_SetStepPin((axis_id_t)i, false);
    }

    if (interp_context.error_callback)
    {
        interp_context.error_callback("Emergency stop activated");
    }

    printf("EMERGENCY STOP ACTIVATED - ALL PULSE GENERATION STOPPED\n");
}

void INTERP_ClearAlarmState(void)
{
    /* Clear alarm state after emergency stop or limit switch trigger */
    interp_context.motion.emergency_stop = false;
    interp_context.motion.state = MOTION_STATE_IDLE;

    /* Clear planner buffer */
    interp_context.planner.head = 0;
    interp_context.planner.tail = 0;
    interp_context.planner.count = 0;

    /* Reset motion state and clear any step generation */
    INTERP_StopStepGeneration();

    if (interp_context.error_callback)
    {
        interp_context.error_callback("Alarm state cleared");
    }

    printf("ALARM STATE CLEARED - SYSTEM READY\n");
}

void INTERP_FeedHold(bool hold)
{
    interp_context.motion.feed_hold = hold;

    if (hold)
    {
        printf("Feed hold activated\n");
    }
    else
    {
        printf("Feed hold released\n");
    }
}

void INTERP_OverrideFeedRate(float percentage)
{
    if (percentage < 10.0f)
        percentage = 10.0f; // Minimum 10%
    if (percentage > 200.0f)
        percentage = 200.0f; // Maximum 200%

    float original_feed_rate = interp_context.motion.target_velocity;
    interp_context.motion.target_velocity = original_feed_rate * (percentage / 100.0f);

    printf("Feed rate override: %.1f%% (%.1f mm/min)\n",
           percentage, interp_context.motion.target_velocity);
}

void INTERP_Tasks(void)
{
    if (!interp_context.initialized || !interp_context.enabled)
    {
        return;
    }

    // Check for emergency stop
    if (interp_context.motion.emergency_stop)
    {
        return;
    }

    // Check for feed hold
    if (interp_context.motion.feed_hold)
    {
        return;
    }

    // Process homing cycle if active
    if (INTERP_IsHomingActive())
    {
        INTERP_ProcessHomingCycle();
        return; // Homing takes priority over normal motion
    }

    // Update motion state machine
    update_motion_state();

    // Generate step signals
    generate_step_signals();
}

// *****************************************************************************
// Position and Status Functions
// *****************************************************************************

position_t INTERP_GetCurrentPosition(void)
{
    return interp_context.motion.current_position;
}

velocity_t INTERP_GetCurrentVelocity(void)
{
    return interp_context.motion.current_velocity;
}

motion_state_t INTERP_GetMotionState(void)
{
    return interp_context.motion.state;
}

float INTERP_GetMotionProgress(void)
{
    if (interp_context.motion.total_distance <= 0.0f)
    {
        return 0.0f;
    }

    float progress = interp_context.motion.distance_traveled / interp_context.motion.total_distance;

    if (progress > 1.0f)
        progress = 1.0f;
    if (progress < 0.0f)
        progress = 0.0f;

    return progress;
}

// *****************************************************************************
// Callback Registration Functions
// *****************************************************************************

void INTERP_RegisterStepCallback(void (*callback)(axis_id_t axis, bool direction))
{
    interp_context.step_callback = callback;
}

void INTERP_RegisterMotionCompleteCallback(void (*callback)(void))
{
    interp_context.motion_complete_callback = callback;
}

void INTERP_RegisterErrorCallback(void (*callback)(const char *error_message))
{
    interp_context.error_callback = callback;
}

// *****************************************************************************
// Utility Functions
// *****************************************************************************

float INTERP_CalculateDistance(position_t start, position_t end)
{
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float dz = end.z - start.z;

    return sqrtf(dx * dx + dy * dy + dz * dz);
}

float INTERP_CalculateMoveTime(position_t start, position_t end, float feed_rate)
{
    float distance = INTERP_CalculateDistance(start, end);

    if (feed_rate <= 0.0f)
    {
        return 0.0f;
    }

    // Convert feed_rate from mm/min to mm/sec, then calculate time in seconds
    return (distance / feed_rate) * 60.0f;
}

bool INTERP_IsPositionValid(position_t position)
{
    // Check for NaN or infinite values
    if (!isfinite(position.x) || !isfinite(position.y) ||
        !isfinite(position.z) || !isfinite(position.a))
    {
        return false;
    }

    // Add machine-specific position limits here
    // For now, accept any finite position

    return true;
}

void INTERP_LimitVelocity(velocity_t *velocity, float max_velocity)
{
    if (!velocity)
        return;

    if (velocity->magnitude > max_velocity)
    {
        float scale = max_velocity / velocity->magnitude;
        velocity->x *= scale;
        velocity->y *= scale;
        velocity->z *= scale;
        velocity->magnitude = max_velocity;
    }
}

// *****************************************************************************
// Hardware Integration Functions
// *****************************************************************************

void INTERP_Timer1Callback(uint32_t status, uintptr_t context)
{
    // This should be called from Timer1 ISR at 1kHz (now that prescaler is fixed)
    (void)status;  // Suppress unused parameter warning
    (void)context; // Suppress unused parameter warning

    // Handle interpolation engine tasks
    INTERP_Tasks();

    // ALSO handle motion planner trajectory updates
    // This replaces the Core Timer approach now that Timer1 prescaler is fixed
    extern void MotionPlanner_UpdateTrajectory(void);
    MotionPlanner_UpdateTrajectory();

    // Handle LED heartbeat (moved from Core Timer callback)
    static uint16_t led_counter = 0;
    led_counter++;
    if ((led_counter % 100) == 0) // Every 100ms at 1kHz
    {
        LED1_Toggle();
    }
}

bool INTERP_ConfigureTimer1(void)
{
    // Timer1 should already be configured by Harmony
    // Just register our callback
    TMR1_CallbackRegister(INTERP_Timer1Callback, 0);

    return true;
}

bool INTERP_ConfigureOCRModules(void)
{
    // Configure OCR modules for continuous pulse mode step generation
    // Each OCR module generates precise step pulses for one axis

    // CRITICAL: OCR modules must be configured in Harmony MCC as:
    // - Mode: "Dual Compare Continuous Pulse" or "PWM Mode"
    // - Timer Source: Timer2 (OCMP1), Timer3 (OCMP4), Timer4 (OCMP5)
    // - Output Pin: Enable external pin output

    printf("Configuring OCR modules for step pulse generation...\n");

    // OCR Module Mapping:
    // X-axis → OCMP1 → PulseX (RD4) - Timer 2 based
    // Y-axis → OCMP4 → PulseY (RD5) - Timer 3 based
    // Z-axis → OCMP5 → PulseZ (RF0) - Timer 4 based

    // Initialize OCR modules - they should be configured by Harmony but verify
    OCMP1_Initialize();
    OCMP4_Initialize();
    OCMP5_Initialize();

    // Set initial values for stopped state (maximum period)
    // In PWM mode: CompareValue = pulse width, SecondaryValue = period
    OCMP1_CompareValueSet(1);              // Minimum pulse width (1 timer tick)
    OCMP1_CompareSecondaryValueSet(65535); // Maximum period (stopped)

    OCMP4_CompareValueSet(1);
    OCMP4_CompareSecondaryValueSet(65535);

    OCMP5_CompareValueSet(1);
    OCMP5_CompareSecondaryValueSet(65535);

    // Enable OCR modules - this STARTS pulse generation
    // Note: Pulses will be very slow initially (65535 period = ~65ms pulses)
    OCMP1_Enable(); // Enable X-axis pulse generation
    OCMP4_Enable(); // Enable Y-axis pulse generation
    OCMP5_Enable(); // Enable Z-axis pulse generation

    // Initialize step timing parameters
    for (int i = 0; i < INTERP_MAX_AXES; i++)
    {
        interp_context.steps.step_period[i] = 65535; // Maximum period (stopped)
        interp_context.steps.step_frequency[i] = 0.0f;
        interp_context.steps.next_step_time[i] = 0;
        interp_context.steps.bresenham_error[i] = 0;
        interp_context.steps.bresenham_delta[i] = 0;
    }

    printf("OCR modules enabled - continuous pulse generation active\n");
    printf("X-axis: OCMP1 on pin RD4\n");
    printf("Y-axis: OCMP4 on pin RD5\n");
    printf("Z-axis: OCMP5 on pin RF0\n");
    printf("Initial pulse rate: ~15 Hz (very slow for safety)\n");

    return true;
}

static void update_step_rates(void)
{
    // Calculate required step rates for each axis based on current velocity
    // This is called continuously during motion execution

    if (interp_context.motion.state == MOTION_STATE_IDLE ||
        interp_context.motion.state == MOTION_STATE_COMPLETE)
    {
        // Stop all axes
        for (int i = 0; i < INTERP_MAX_AXES; i++)
        {
            INTERP_SetAxisStepRate((axis_id_t)i, 0.0f);
        }
        return;
    }

    // Get current velocity vector
    velocity_t current_velocity = interp_context.motion.current_velocity;

    // Convert velocity to steps per second for each axis (use default steps/mm if not configured)
    float steps_per_mm_x = 200.0f; // Default steps/mm for X axis
    float steps_per_mm_y = 200.0f; // Default steps/mm for Y axis
    float steps_per_mm_z = 200.0f; // Default steps/mm for Z axis
    float steps_per_mm_a = 200.0f; // Default steps/mm for A axis

    float x_steps_per_sec = fabsf(current_velocity.x) * steps_per_mm_x;
    float y_steps_per_sec = fabsf(current_velocity.y) * steps_per_mm_y;
    float z_steps_per_sec = fabsf(current_velocity.z) * steps_per_mm_z;
    float a_steps_per_sec = fabsf(current_velocity.magnitude) * steps_per_mm_a; // Use magnitude for A-axis

    // Update step rates for each axis
    INTERP_SetAxisStepRate(AXIS_X, x_steps_per_sec);
    INTERP_SetAxisStepRate(AXIS_Y, y_steps_per_sec);
    INTERP_SetAxisStepRate(AXIS_Z, z_steps_per_sec);
    INTERP_SetAxisStepRate(AXIS_A, a_steps_per_sec);

    // Update step directions
    INTERP_SetStepDirection(AXIS_X, current_velocity.x >= 0.0f);
    INTERP_SetStepDirection(AXIS_Y, current_velocity.y >= 0.0f);
    INTERP_SetStepDirection(AXIS_Z, current_velocity.z >= 0.0f);
    INTERP_SetStepDirection(AXIS_A, current_velocity.magnitude >= 0.0f);
}

void INTERP_SetAxisStepRate(axis_id_t axis, float steps_per_second)
{
    if (axis >= INTERP_MAX_AXES)
        return;

    interp_context.steps.step_frequency[axis] = steps_per_second;

    if (steps_per_second < 1.0f)
    {
        // Stop the axis by setting maximum period
        interp_context.steps.step_period[axis] = 65535;

        switch (axis)
        {
        case AXIS_X:
            // Stop X-axis pulses by setting very long period
            OCMP1_CompareSecondaryValueSet(65535); // 65.535ms period = ~15Hz
            printf("X-axis stopped\n");
            break;
        case AXIS_Y:
            // Stop Y-axis pulses
            OCMP4_CompareSecondaryValueSet(65535);
            printf("Y-axis stopped\n");
            break;
        case AXIS_Z:
            // Stop Z-axis pulses
            OCMP5_CompareSecondaryValueSet(65535);
            printf("Z-axis stopped\n");
            break;
        default:
            break;
        }
    }
    else
    {
        // Calculate OCR period for desired step frequency
        // Timer frequency = 1MHz (1μs resolution)
        // Period = Timer_Freq / Step_Freq
        uint32_t period = (uint32_t)(1000000.0f / steps_per_second);

        // Clamp period to valid range
        if (period < 100)
            period = 100; // Maximum 10kHz step rate
        if (period > 65535)
            period = 65535; // Minimum ~15Hz step rate

        interp_context.steps.step_period[axis] = period;

        // Configure OCR module for continuous pulse generation
        switch (axis)
        {
        case AXIS_X:
            // Configure OCMP1 period for X-axis step rate
            OCMP1_CompareValueSet(period / 2);      // 50% duty cycle
            OCMP1_CompareSecondaryValueSet(period); // Period = 1/frequency
            // Note: OCMP1 is already enabled, just changing the timing
            printf("X-axis: %.1f steps/sec (period=%u)\n", steps_per_second, (unsigned int)period);
            break;

        case AXIS_Y:
            // Configure OCMP4 period for Y-axis step rate
            OCMP4_CompareValueSet(period / 2);
            OCMP4_CompareSecondaryValueSet(period);
            printf("Y-axis: %.1f steps/sec (period=%u)\n", steps_per_second, (unsigned int)period);
            break;

        case AXIS_Z:
            // Configure OCMP5 period for Z-axis step rate
            OCMP5_CompareValueSet(period / 2);
            OCMP5_CompareSecondaryValueSet(period);
            printf("Z-axis: %.1f steps/sec (period=%u)\n", steps_per_second, (unsigned int)period);
            break;

        default:
            break;
        }
    }
}

void INTERP_StartStepGeneration(void)
{
    // This function starts ALL step generation
    // Called when motion begins

    printf("Starting step pulse generation on all axes\n");

    // Enable all OCR modules - this starts the hardware pulse generation
    OCMP1_Enable(); // X-axis pulse generation ON
    OCMP4_Enable(); // Y-axis pulse generation ON
    OCMP5_Enable(); // Z-axis pulse generation ON

    printf("Hardware step pulse generation ACTIVE\n");
}

void INTERP_StopStepGeneration(void)
{
    // This function stops ALL step generation
    // Called for emergency stop or end of motion

    printf("Stopping step pulse generation on all axes\n");

    // Method 1: Disable OCR modules completely
    OCMP1_Disable(); // X-axis pulse generation OFF
    OCMP4_Disable(); // Y-axis pulse generation OFF
    OCMP5_Disable(); // Z-axis pulse generation OFF

    // Method 2: Alternative - set very long periods to effectively stop
    // OCMP1_CompareSecondaryValueSet(65535);
    // OCMP4_CompareSecondaryValueSet(65535);
    // OCMP5_CompareSecondaryValueSet(65535);

    // Reset step frequencies
    for (int i = 0; i < INTERP_MAX_AXES; i++)
    {
        interp_context.steps.step_frequency[i] = 0.0f;
        interp_context.steps.step_period[i] = 65535;
    }

    // printf("Hardware step pulse generation STOPPED\n");  // Commented out - UGS treats as error
}

bool INTERP_ConfigureStepperGPIO(void)
{
    // GPIO pins should already be configured by Harmony
    // This function would set up step and direction pins

    return true;
}

void INTERP_GenerateStepPulse(axis_id_t axis)
{
    // Generate a step pulse using OCR modules
    // OCR Module Mapping:
    // X-axis → OCMP1 → PulseX (RD4)
    // Y-axis → OCMP4 → PulseY (RD5)
    // Z-axis → OCMP5 → PulseZ (RF0)
    // A-axis → OCMP2/3 (not configured yet)

    switch (axis)
    {
    case AXIS_X:
        // Use OCMP1 for X-axis step generation
        OCMP1_CompareSecondaryValueSet(1000); // Pulse width
        break;

    case AXIS_Y:
        // Use OCMP4 for Y-axis step generation
        OCMP4_CompareSecondaryValueSet(1000);
        break;

    case AXIS_Z:
        // Use OCMP5 for Z-axis step generation
        OCMP5_CompareSecondaryValueSet(1000);
        break;

    case AXIS_A:
        // A-axis OCR module not configured yet
        // Would use OCMP2 or OCMP3 when implemented
        break;

    default:
        break;
    }

    // Call user callback if registered
    if (interp_context.step_callback)
    {
        interp_context.step_callback(axis, interp_context.steps.direction[axis]);
    }
}

void INTERP_SetStepDirection(axis_id_t axis, bool direction)
{
    interp_context.steps.direction[axis] = direction;
    INTERP_SetDirectionPin(axis, direction);
}

void INTERP_SetStepPin(axis_id_t axis, bool state)
{
    // Set step pin state using GPIO
    // This would be implemented based on your GPIO pin assignments
    (void)axis;
    (void)state;
}

void INTERP_SetDirectionPin(axis_id_t axis, bool direction)
{
    // Set direction pin state using GPIO
    // This would be implemented based on your GPIO pin assignments
    (void)axis;
    (void)direction;
}

bool INTERP_ReadLimitSwitch(axis_id_t axis)
{
    // Read limit switch state based on axis
    switch (axis)
    {
    case AXIS_X:
        // return LIMIT_X_Get() ? true : false;
        return false;
    case AXIS_Y:
        // return LIMIT_Y_Get() ? true : false;
        return false;
    case AXIS_Z:
        // return LIMIT_Z_Get() ? true : false;
        return false;
    default:
        return false;
    }
}

bool INTERP_ReadHomeSwitch(axis_id_t axis)
{
    // Read home switch state
    // This would be implemented based on your GPIO pin assignments
    (void)axis;
    return false; // No home switches for now
}

// *****************************************************************************
// Local Helper Functions
// *****************************************************************************

static void calculate_step_parameters(void)
{
    // Calculate step counts for each axis
    position_t delta = {
        .x = interp_context.motion.end_position.x - interp_context.motion.start_position.x,
        .y = interp_context.motion.end_position.y - interp_context.motion.start_position.y,
        .z = interp_context.motion.end_position.z - interp_context.motion.start_position.z,
        .a = interp_context.motion.end_position.a - interp_context.motion.start_position.a};

    // Convert distances to step counts
    interp_context.steps.target_steps[AXIS_X] = (int32_t)(delta.x * interp_context.steps_per_mm[AXIS_X]);
    interp_context.steps.target_steps[AXIS_Y] = (int32_t)(delta.y * interp_context.steps_per_mm[AXIS_Y]);
    interp_context.steps.target_steps[AXIS_Z] = (int32_t)(delta.z * interp_context.steps_per_mm[AXIS_Z]);
    interp_context.steps.target_steps[AXIS_A] = (int32_t)(delta.a * interp_context.steps_per_mm[AXIS_A]);

    // Set step directions
    for (int i = 0; i < INTERP_MAX_AXES; i++)
    {
        interp_context.steps.direction[i] = (interp_context.steps.target_steps[i] >= 0);
        interp_context.steps.delta_steps[i] = abs(interp_context.steps.target_steps[i]);
        interp_context.steps.step_count[i] = 0;
        interp_context.steps.step_active[i] = false;
    }
}

static void update_motion_state(void)
{
    if (interp_context.motion.state == MOTION_STATE_IDLE ||
        interp_context.motion.state == MOTION_STATE_COMPLETE)
    {
        return;
    }

    // Update time elapsed
    uint32_t current_time = TMR1_CounterGet();
    interp_context.motion.time_elapsed = (current_time - motion_start_time) / 1000.0f; // Convert to seconds

    float current_velocity = 0.0f;
    float distance_progress = 0.0f;

    // Execute motion profile based on type
    switch (interp_context.motion.profile_type)
    {
    case MOTION_PROFILE_S_CURVE:
    {
        // Use S-curve profile for smooth acceleration
        scurve_profile_t scurve_profile;
        if (INTERP_CalculateSCurveProfile(
                interp_context.motion.total_distance,
                interp_context.motion.target_velocity,
                interp_context.motion.acceleration,
                INTERP_JERK_LIMIT,
                &scurve_profile))
        {

            current_velocity = INTERP_GetSCurveVelocity(interp_context.motion.time_elapsed, &scurve_profile);
            distance_progress = INTERP_GetSCurvePosition(interp_context.motion.time_elapsed, &scurve_profile);
        }
        break;
    }

    case MOTION_PROFILE_TRAPEZOIDAL:
    {
        // Use trapezoidal profile for standard acceleration
        trapezoidal_profile_t trap_profile;
        if (INTERP_CalculateTrapezoidalProfile(
                interp_context.motion.total_distance,
                interp_context.motion.target_velocity,
                interp_context.motion.acceleration,
                &trap_profile))
        {

            current_velocity = INTERP_GetProfileVelocity(interp_context.motion.time_elapsed, &trap_profile);
            distance_progress = INTERP_GetProfilePosition(interp_context.motion.time_elapsed, &trap_profile);
        }
        break;
    }

    case MOTION_PROFILE_LINEAR:
    default:
    {
        // Simple linear profile for basic moves
        float progress = interp_context.motion.time_elapsed / interp_context.motion.estimated_time;
        if (progress > 1.0f)
            progress = 1.0f;

        current_velocity = interp_context.motion.target_velocity;
        distance_progress = progress * interp_context.motion.total_distance;
        break;
    }
    }

    // Update motion state based on progress
    float motion_progress = distance_progress / interp_context.motion.total_distance;

    if (motion_progress >= 1.0f)
    {
        // Motion complete - advance to next block in planner buffer
        interp_context.motion.state = MOTION_STATE_COMPLETE;
        interp_context.motion.current_position = interp_context.motion.end_position;
        interp_context.motion.distance_traveled = interp_context.motion.total_distance;
        interp_context.moves_completed++;

        // Remove completed block from planner buffer
        INTERP_PlannerAdvanceBlock();

        // Check if there are more blocks to execute
        if (!INTERP_PlannerIsBufferEmpty())
        {
            // Automatically start next block for continuous motion
            interp_context.motion.state = MOTION_STATE_IDLE;
            INTERP_ExecuteMove(); // Start next block immediately
        }
        else
        {
            // All blocks completed
            if (interp_context.motion_complete_callback)
            {
                interp_context.motion_complete_callback();
            }

            printf("All planner blocks completed in %.2f seconds (%.1f mm/min avg)\n",
                   interp_context.motion.time_elapsed,
                   (interp_context.motion.total_distance / interp_context.motion.time_elapsed) * 60.0f);
        }
    }
    else
    {
        // Update current position based on distance progress
        position_t start = interp_context.motion.start_position;
        position_t end = interp_context.motion.end_position;

        interp_context.motion.current_position.x = start.x + (end.x - start.x) * motion_progress;
        interp_context.motion.current_position.y = start.y + (end.y - start.y) * motion_progress;
        interp_context.motion.current_position.z = start.z + (end.z - start.z) * motion_progress;
        interp_context.motion.current_position.a = start.a + (end.a - start.a) * motion_progress;

        // Update velocity vector
        interp_context.motion.current_velocity.magnitude = current_velocity;
        interp_context.motion.distance_traveled = distance_progress;

        // Update motion state based on velocity profile
        if (current_velocity < interp_context.motion.target_velocity * 0.1f)
        {
            if (motion_progress < 0.1f)
            {
                interp_context.motion.state = MOTION_STATE_ACCELERATING;
            }
            else
            {
                interp_context.motion.state = MOTION_STATE_DECELERATING;
            }
        }
        else if (current_velocity >= interp_context.motion.target_velocity * 0.9f)
        {
            interp_context.motion.state = MOTION_STATE_CONSTANT_VELOCITY;
        }
        else
        {
            if (motion_progress < 0.5f)
            {
                interp_context.motion.state = MOTION_STATE_ACCELERATING;
            }
            else
            {
                interp_context.motion.state = MOTION_STATE_DECELERATING;
            }
        }
    }
}

static void generate_step_signals(void)
{
    // Real-time step signal generation using OCRx continuous pulse mode
    // This function runs at 1kHz from Timer1 ISR

    // Update step rates based on current motion profile
    update_step_rates();

    // For linear interpolation: OCRx modules handle step generation automatically
    // For arc interpolation: OCRx modules handle individual axis rates

    // The OCRx modules are now configured for continuous pulse generation:
    // - OCMP1: X-axis step pulses at calculated frequency
    // - OCMP4: Y-axis step pulses at calculated frequency
    // - OCMP5: Z-axis step pulses at calculated frequency

    // Multi-axis coordination is achieved by:
    // 1. Calculate velocity vector from motion profile (S-curve/trapezoidal)
    // 2. Convert velocity components to steps/sec for each axis
    // 3. Update OCRx periods to generate pulses at required rates
    // 4. Direction pins set based on velocity vector direction

    // Step counting for position feedback
    for (int i = 0; i < INTERP_MAX_AXES; i++)
    {
        if (interp_context.steps.step_frequency[i] > 1.0f)
        {
            // Estimate steps generated since last update (1ms)
            float steps_this_period = interp_context.steps.step_frequency[i] / 1000.0f;
            interp_context.steps.step_count[i] += (uint32_t)(steps_this_period + 0.5f);
            interp_context.total_steps_generated++;
        }
    }
}

static bool validate_motion_parameters(void)
{
    // Validate that motion parameters are reasonable
    if (interp_context.motion.total_distance <= 0.0f)
    {
        return false;
    }

    if (interp_context.motion.target_velocity <= 0.0f)
    {
        return false;
    }

    if (interp_context.motion.estimated_time <= 0.0f)
    {
        return false;
    }

    return true;
}

// *****************************************************************************
// S-Curve Motion Profile Implementation
// *****************************************************************************

bool INTERP_CalculateTrapezoidalProfile(float distance, float target_vel,
                                        float max_accel, trapezoidal_profile_t *profile)
{
    if (!profile || distance <= 0.0f || target_vel <= 0.0f || max_accel <= 0.0f)
    {
        return false;
    }

    // Clear the profile
    memset(profile, 0, sizeof(trapezoidal_profile_t));

    // Calculate time and distance for acceleration phase
    profile->acceleration_time = target_vel / max_accel;
    profile->acceleration_distance = 0.5f * max_accel * profile->acceleration_time * profile->acceleration_time;

    // Deceleration phase (symmetric)
    profile->deceleration_time = profile->acceleration_time;
    profile->deceleration_distance = profile->acceleration_distance;

    // Check if we can reach target velocity
    if (profile->acceleration_distance + profile->deceleration_distance > distance)
    {
        // Triangle profile - cannot reach target velocity
        profile->peak_velocity = sqrtf(distance * max_accel);
        profile->acceleration_time = profile->peak_velocity / max_accel;
        profile->deceleration_time = profile->acceleration_time;
        profile->acceleration_distance = 0.5f * max_accel * profile->acceleration_time * profile->acceleration_time;
        profile->deceleration_distance = profile->acceleration_distance;
        profile->constant_velocity_time = 0.0f;
        profile->constant_velocity_distance = 0.0f;
    }
    else
    {
        // Trapezoidal profile - can reach target velocity
        profile->peak_velocity = target_vel;
        profile->constant_velocity_distance = distance - profile->acceleration_distance - profile->deceleration_distance;
        profile->constant_velocity_time = profile->constant_velocity_distance / target_vel;
    }

    return true;
}

float INTERP_GetProfileVelocity(float time, const trapezoidal_profile_t *profile)
{
    if (!profile)
        return 0.0f;

    if (time <= profile->acceleration_time)
    {
        // Acceleration phase
        return (profile->peak_velocity / profile->acceleration_time) * time;
    }
    else if (time <= profile->acceleration_time + profile->constant_velocity_time)
    {
        // Constant velocity phase
        return profile->peak_velocity;
    }
    else if (time <= profile->acceleration_time + profile->constant_velocity_time + profile->deceleration_time)
    {
        // Deceleration phase
        float decel_time = time - profile->acceleration_time - profile->constant_velocity_time;
        return profile->peak_velocity - (profile->peak_velocity / profile->deceleration_time) * decel_time;
    }
    else
    {
        // Motion complete
        return 0.0f;
    }
}

float INTERP_GetProfilePosition(float time, const trapezoidal_profile_t *profile)
{
    if (!profile)
        return 0.0f;

    if (time <= profile->acceleration_time)
    {
        // Acceleration phase
        return 0.5f * (profile->peak_velocity / profile->acceleration_time) * time * time;
    }
    else if (time <= profile->acceleration_time + profile->constant_velocity_time)
    {
        // Constant velocity phase
        float const_time = time - profile->acceleration_time;
        return profile->acceleration_distance + profile->peak_velocity * const_time;
    }
    else if (time <= profile->acceleration_time + profile->constant_velocity_time + profile->deceleration_time)
    {
        // Deceleration phase
        float decel_time = time - profile->acceleration_time - profile->constant_velocity_time;
        return profile->acceleration_distance + profile->constant_velocity_distance +
               profile->peak_velocity * decel_time -
               0.5f * (profile->peak_velocity / profile->deceleration_time) * decel_time * decel_time;
    }
    else
    {
        // Motion complete
        return profile->acceleration_distance + profile->constant_velocity_distance + profile->deceleration_distance;
    }
}

bool INTERP_CalculateSCurveProfile(float distance, float target_vel,
                                   float max_accel, float jerk_limit, scurve_profile_t *profile)
{
    if (!profile || distance <= 0.0f || target_vel <= 0.0f || max_accel <= 0.0f || jerk_limit <= 0.0f)
    {
        return false;
    }

    // Clear the profile
    memset(profile, 0, sizeof(scurve_profile_t));

    // Calculate jerk-limited acceleration profile
    // This implements the classic 7-segment S-curve profile used in professional CNC

    // Phase 1: Calculate time to reach maximum acceleration (jerk limited)
    float t_jerk_accel = max_accel / jerk_limit;

    // Phase 2: Calculate peak velocity achievable with current constraints
    float vel_at_max_accel = 0.5f * jerk_limit * t_jerk_accel * t_jerk_accel;

    // Check if we can reach target velocity with available distance
    float accel_distance_needed = target_vel * target_vel / (2.0f * max_accel);

    if (accel_distance_needed * 2.0f > distance)
    {
        // Cannot reach target velocity - reduce peak velocity
        profile->peak_velocity = sqrtf(distance * max_accel);
    }
    else
    {
        profile->peak_velocity = target_vel;
    }

    // Calculate acceleration phase timing
    if (profile->peak_velocity > vel_at_max_accel)
    {
        // Full S-curve with constant acceleration phase
        profile->jerk_time_accel = t_jerk_accel;
        profile->peak_acceleration = max_accel;

        // Time at constant acceleration
        float remaining_vel = profile->peak_velocity - vel_at_max_accel;
        profile->accel_time = remaining_vel / max_accel;
    }
    else
    {
        // Short move - only jerk phases
        profile->jerk_time_accel = sqrtf(profile->peak_velocity / jerk_limit);
        profile->peak_acceleration = jerk_limit * profile->jerk_time_accel;
        profile->accel_time = 0.0f;
    }

    // Deceleration phase (mirror of acceleration)
    profile->jerk_time_decel = profile->jerk_time_accel;
    profile->decel_time = profile->accel_time;
    profile->jerk_time_final = profile->jerk_time_accel;

    // Calculate distances for each phase
    float accel_distance = (profile->jerk_time_accel * profile->peak_acceleration * profile->jerk_time_accel / 3.0f) +
                           (profile->accel_time * profile->peak_acceleration *
                            (profile->jerk_time_accel + profile->accel_time / 2.0f));

    float decel_distance = accel_distance; // Symmetric

    // Constant velocity phase
    profile->constant_velocity_distance = distance - accel_distance - decel_distance;

    if (profile->constant_velocity_distance < 0.0f)
    {
        profile->constant_velocity_distance = 0.0f;
        profile->constant_velocity_time = 0.0f;
    }
    else
    {
        profile->constant_velocity_time = profile->constant_velocity_distance / profile->peak_velocity;
    }

    return true;
}

bool INTERP_PlanSCurveMove(position_t start, position_t end, float feed_rate)
{
    if (!interp_context.initialized || !interp_context.enabled)
    {
        return false;
    }

    if (interp_context.motion.state != MOTION_STATE_IDLE)
    {
        return false; // Motion already in progress
    }

    // Validate positions
    if (!INTERP_IsPositionValid(start) || !INTERP_IsPositionValid(end))
    {
        return false;
    }

    // Calculate motion parameters
    interp_context.motion.start_position = start;
    interp_context.motion.end_position = end;
    interp_context.motion.current_position = start;
    interp_context.motion.target_velocity = feed_rate;
    interp_context.motion.profile_type = MOTION_PROFILE_S_CURVE;

    // Calculate total distance
    interp_context.motion.total_distance = INTERP_CalculateDistance(start, end);

    if (interp_context.motion.total_distance < INTERP_POSITION_TOLERANCE)
    {
        return false; // Move too small
    }

    // Calculate S-curve profile
    scurve_profile_t scurve_profile;
    if (!INTERP_CalculateSCurveProfile(
            interp_context.motion.total_distance,
            feed_rate,
            interp_context.motion.acceleration,
            INTERP_JERK_LIMIT,
            &scurve_profile))
    {
        return false;
    }

    // Store profile data for execution
    interp_context.motion.estimated_time =
        2.0f * scurve_profile.jerk_time_accel +
        2.0f * scurve_profile.accel_time +
        scurve_profile.constant_velocity_time +
        2.0f * scurve_profile.jerk_time_decel;

    // Calculate step parameters
    calculate_step_parameters();

    // Validate motion parameters
    if (!validate_motion_parameters())
    {
        return false;
    }

    return true;
}

float INTERP_GetSCurveVelocity(float time, const scurve_profile_t *profile)
{
    if (!profile)
        return 0.0f;

    float t = 0.0f;

    // Phase 1: Jerk-limited acceleration up
    if (time <= profile->jerk_time_accel)
    {
        return 0.5f * (profile->peak_acceleration / profile->jerk_time_accel) * time * time;
    }
    t += profile->jerk_time_accel;

    // Phase 2: Constant acceleration
    if (time <= t + profile->accel_time)
    {
        float t_phase = time - t;
        float vel_phase1 = 0.5f * profile->peak_acceleration * profile->jerk_time_accel;
        return vel_phase1 + profile->peak_acceleration * t_phase;
    }
    t += profile->accel_time;

    // Phase 3: Jerk-limited acceleration down
    if (time <= t + profile->jerk_time_decel)
    {
        float t_phase = time - t;
        float vel_prev = profile->peak_velocity - 0.5f * profile->peak_acceleration * profile->jerk_time_decel;
        return vel_prev + profile->peak_acceleration * t_phase -
               0.5f * (profile->peak_acceleration / profile->jerk_time_decel) * t_phase * t_phase;
    }
    t += profile->jerk_time_decel;

    // Phase 4: Constant velocity
    if (time <= t + profile->constant_velocity_time)
    {
        return profile->peak_velocity;
    }
    t += profile->constant_velocity_time;

    // Phase 5: Jerk-limited deceleration down (mirror of phase 3)
    if (time <= t + profile->jerk_time_decel)
    {
        float t_phase = time - t;
        return profile->peak_velocity - 0.5f * (profile->peak_acceleration / profile->jerk_time_decel) * t_phase * t_phase;
    }
    t += profile->jerk_time_decel;

    // Phase 6: Constant deceleration (mirror of phase 2)
    if (time <= t + profile->decel_time)
    {
        float t_phase = time - t;
        float vel_phase5 = profile->peak_velocity - 0.5f * profile->peak_acceleration * profile->jerk_time_decel;
        return vel_phase5 - profile->peak_acceleration * t_phase;
    }
    t += profile->decel_time;

    // Phase 7: Jerk-limited deceleration up (mirror of phase 1)
    if (time <= t + profile->jerk_time_final)
    {
        float t_phase = time - t;
        float vel_phase6 = 0.5f * profile->peak_acceleration * profile->jerk_time_final;
        return vel_phase6 - 0.5f * (profile->peak_acceleration / profile->jerk_time_final) * t_phase * t_phase;
    }

    // Motion complete
    return 0.0f;
}

float INTERP_GetSCurvePosition(float time, const scurve_profile_t *profile)
{
    if (!profile)
        return 0.0f;

    float position = 0.0f;
    float t = 0.0f;

    // Phase 1: Jerk-limited acceleration up
    if (time <= profile->jerk_time_accel)
    {
        return (1.0f / 6.0f) * (profile->peak_acceleration / profile->jerk_time_accel) * time * time * time;
    }
    position += (1.0f / 6.0f) * profile->peak_acceleration * profile->jerk_time_accel * profile->jerk_time_accel;
    t += profile->jerk_time_accel;

    // Phase 2: Constant acceleration
    if (time <= t + profile->accel_time)
    {
        float t_phase = time - t;
        float vel_start = 0.5f * profile->peak_acceleration * profile->jerk_time_accel;
        return position + vel_start * t_phase + 0.5f * profile->peak_acceleration * t_phase * t_phase;
    }
    float vel_phase1 = 0.5f * profile->peak_acceleration * profile->jerk_time_accel;
    position += vel_phase1 * profile->accel_time + 0.5f * profile->peak_acceleration * profile->accel_time * profile->accel_time;
    t += profile->accel_time;

    // Phase 3: Jerk-limited acceleration down
    if (time <= t + profile->jerk_time_decel)
    {
        float t_phase = time - t;
        float vel_start = profile->peak_velocity - 0.5f * profile->peak_acceleration * profile->jerk_time_decel;
        return position + vel_start * t_phase + 0.5f * profile->peak_acceleration * t_phase * t_phase -
               (1.0f / 6.0f) * (profile->peak_acceleration / profile->jerk_time_decel) * t_phase * t_phase * t_phase;
    }
    position += (profile->peak_velocity - 0.5f * profile->peak_acceleration * profile->jerk_time_decel) * profile->jerk_time_decel +
                0.5f * profile->peak_acceleration * profile->jerk_time_decel * profile->jerk_time_decel -
                (1.0f / 6.0f) * profile->peak_acceleration * profile->jerk_time_decel * profile->jerk_time_decel;
    t += profile->jerk_time_decel;

    // Phase 4: Constant velocity
    if (time <= t + profile->constant_velocity_time)
    {
        float t_phase = time - t;
        return position + profile->peak_velocity * t_phase;
    }
    position += profile->peak_velocity * profile->constant_velocity_time;
    t += profile->constant_velocity_time;

    // Deceleration phases (mirror of acceleration)
    // Implementation continues with symmetric deceleration profile...

    return position;
}

// *****************************************************************************
// Look-ahead Planner Implementation
// *****************************************************************************

bool INTERP_PlannerAddBlock(position_t start, position_t end, float feed_rate, motion_profile_type_t profile)
{
    if (!interp_context.initialized)
    {
        return false;
    }

    // Check if buffer is full
    if (INTERP_PlannerIsBufferFull())
    {
        return false;
    }

    // Get pointer to new block
    planner_block_t *block = &interp_context.planner.blocks[interp_context.planner.head];

    // Clear the block
    memset(block, 0, sizeof(planner_block_t));

    // Set basic block data
    block->start_position = start;
    block->end_position = end;
    block->nominal_speed = feed_rate;
    block->profile_type = profile;
    block->block_id = interp_context.planner.head;

    // Calculate distance and unit vector
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float dz = end.z - start.z;
    float da = end.a - start.a;

    block->distance = sqrtf(dx * dx + dy * dy + dz * dz + da * da);

    if (block->distance < INTERP_POSITION_TOLERANCE)
    {
        return false; // Block too small
    }

    // Calculate unit vector
    block->unit_vector[AXIS_X] = dx / block->distance;
    block->unit_vector[AXIS_Y] = dy / block->distance;
    block->unit_vector[AXIS_Z] = dz / block->distance;
    block->unit_vector[AXIS_A] = da / block->distance;

    // Set acceleration limit (use minimum of all axes)
    block->acceleration = interp_context.acceleration_per_axis[AXIS_X]; // Start with X
    for (int i = 1; i < INTERP_MAX_AXES; i++)
    {
        if (interp_context.acceleration_per_axis[i] < block->acceleration)
        {
            block->acceleration = interp_context.acceleration_per_axis[i];
        }
    }

    // Initialize speeds
    block->entry_speed = 0.0f;
    block->exit_speed = 0.0f;
    block->max_entry_speed = feed_rate;

    // Set flags
    block->recalculate_flag = true;
    block->nominal_length_flag = false;
    block->entry_speed_max = false;

    // Add block to buffer
    interp_context.planner.head = (interp_context.planner.head + 1) % INTERP_PLANNER_BUFFER_SIZE;
    interp_context.planner.count++;

    // Mark buffer for recalculation
    interp_context.planner.recalculate_needed = true;

    return true;
}

bool INTERP_PlannerIsBufferFull(void)
{
    return (interp_context.planner.count >= INTERP_PLANNER_BUFFER_SIZE);
}

bool INTERP_PlannerIsBufferEmpty(void)
{
    return (interp_context.planner.count == 0);
}

uint8_t INTERP_PlannerGetBlockCount(void)
{
    return interp_context.planner.count;
}

planner_block_t *INTERP_PlannerGetCurrentBlock(void)
{
    if (INTERP_PlannerIsBufferEmpty())
    {
        return NULL;
    }

    return &interp_context.planner.blocks[interp_context.planner.tail];
}

void INTERP_PlannerAdvanceBlock(void)
{
    if (!INTERP_PlannerIsBufferEmpty())
    {
        interp_context.planner.tail = (interp_context.planner.tail + 1) % INTERP_PLANNER_BUFFER_SIZE;
        interp_context.planner.count--;
    }
}

void INTERP_PlannerClearBuffer(void)
{
    interp_context.planner.head = 0;
    interp_context.planner.tail = 0;
    interp_context.planner.count = 0;
    interp_context.planner.recalculate_needed = false;
}

bool INTERP_SetJunctionDeviation(float deviation)
{
    if (deviation < 0.001f || deviation > 10.0f)
    {
        return false; // Invalid range
    }

    interp_context.planner.junction_deviation = deviation;
    interp_context.planner.recalculate_needed = true;
    return true;
}

void INTERP_PlannerRecalculate(void)
{
    if (!interp_context.planner.recalculate_needed || INTERP_PlannerIsBufferEmpty())
    {
        return;
    }

    // Implement GRBL-style junction speed calculation
    uint8_t block_index = interp_context.planner.tail;
    planner_block_t *block = &interp_context.planner.blocks[block_index];
    planner_block_t *next_block = NULL;

    // Forward pass: Calculate maximum entry speeds
    for (uint8_t i = 0; i < interp_context.planner.count - 1; i++)
    {
        uint8_t next_index = (block_index + 1) % INTERP_PLANNER_BUFFER_SIZE;
        next_block = &interp_context.planner.blocks[next_index];

        if (block->recalculate_flag)
        {
            // Calculate junction speed using dot product and junction deviation
            float cos_theta = 0.0f;
            for (int axis = 0; axis < INTERP_MAX_AXES; axis++)
            {
                cos_theta -= block->unit_vector[axis] * next_block->unit_vector[axis];
            }

            if (cos_theta < 0.95f)
            { // Only calculate for significant direction changes
                // Junction speed based on centripetal acceleration
                float sin_theta_d2 = sqrtf(0.5f * (1.0f - cos_theta));
                float junction_speed = sqrtf(block->acceleration * interp_context.planner.junction_deviation * sin_theta_d2 / (1.0f - sin_theta_d2));

                // Limit to minimum of block speeds
                if (junction_speed > block->nominal_speed)
                    junction_speed = block->nominal_speed;
                if (junction_speed > next_block->nominal_speed)
                    junction_speed = next_block->nominal_speed;

                block->exit_speed = junction_speed;
                next_block->entry_speed = junction_speed;
            }
            else
            {
                // Straight line - use nominal speed
                block->exit_speed = fminf(block->nominal_speed, next_block->nominal_speed);
                next_block->entry_speed = block->exit_speed;
            }

            block->recalculate_flag = false;
        }

        block_index = next_index;
        block = next_block;
    }

    // Backward pass: Ensure achievable speeds
    block_index = (interp_context.planner.head - 1 + INTERP_PLANNER_BUFFER_SIZE) % INTERP_PLANNER_BUFFER_SIZE;
    for (uint8_t i = 0; i < interp_context.planner.count - 1; i++)
    {
        block = &interp_context.planner.blocks[block_index];
        uint8_t prev_index = (block_index - 1 + INTERP_PLANNER_BUFFER_SIZE) % INTERP_PLANNER_BUFFER_SIZE;
        planner_block_t *prev_block = &interp_context.planner.blocks[prev_index];

        // Check if exit speed is achievable given entry speed and acceleration
        float max_exit_speed = sqrtf(block->entry_speed * block->entry_speed + 2.0f * block->acceleration * block->distance);

        if (block->exit_speed > max_exit_speed)
        {
            block->exit_speed = max_exit_speed;
            prev_block->exit_speed = block->entry_speed;
            prev_block->recalculate_flag = true;
        }

        block_index = prev_index;
    }

    interp_context.planner.recalculate_needed = false;
}

void INTERP_PlannerOptimizeBuffer(void)
{
    // Perform look-ahead optimization
    INTERP_PlannerRecalculate();

    // Additional optimization passes could be added here
    // - Cornering speed optimization
    // - Acceleration limiting
    // - Feed rate override application
}

bool INTERP_BlendMoves(position_t waypoints[], uint8_t point_count, float feed_rate)
{
    if (!waypoints || point_count < 2)
    {
        return false;
    }

    // Add all segments to planner buffer
    for (uint8_t i = 0; i < point_count - 1; i++)
    {
        if (!INTERP_PlannerAddBlock(waypoints[i], waypoints[i + 1], feed_rate, MOTION_PROFILE_S_CURVE))
        {
            return false; // Buffer full or other error
        }
    }

    // Optimize the entire sequence
    INTERP_PlannerOptimizeBuffer();

    return true;
}

static void reset_motion_state(void)
{
    interp_context.motion.state = MOTION_STATE_IDLE;
    interp_context.motion.distance_traveled = 0.0f;
    interp_context.motion.time_elapsed = 0.0f;
    interp_context.motion.emergency_stop = false;
    interp_context.motion.feed_hold = false;

    // Reset step counters
    for (int i = 0; i < INTERP_MAX_AXES; i++)
    {
        interp_context.steps.step_count[i] = 0;
        interp_context.steps.delta_steps[i] = 0;
        interp_context.steps.step_active[i] = false;
    }
}

// *****************************************************************************
// Arc Interpolation Implementation
// *****************************************************************************

bool INTERP_CalculateArcParameters(arc_parameters_t *arc)
{
    if (!arc)
        return false;

    // Calculate arc center based on format
    if (arc->format == ARC_FORMAT_IJK)
    {
        // I,J,K offset format - center relative to start point
        arc->center.x = arc->start.x + arc->i_offset;
        arc->center.y = arc->start.y + arc->j_offset;
        arc->center.z = arc->start.z + arc->k_offset;
        arc->center.a = arc->start.a; // A-axis doesn't participate in arc center

        // Calculate radius from start to center
        float dx = arc->start.x - arc->center.x;
        float dy = arc->start.y - arc->center.y;
        float dz = arc->start.z - arc->center.z;
        arc->radius = sqrtf(dx * dx + dy * dy + dz * dz);
    }
    else if (arc->format == ARC_FORMAT_RADIUS)
    {
        // R radius format - calculate center position
        arc->radius = fabsf(arc->r_radius);

        // Calculate center using chord midpoint and perpendicular distance
        float dx = arc->end.x - arc->start.x;
        float dy = arc->end.y - arc->start.y;
        float chord_length = sqrtf(dx * dx + dy * dy);

        if (chord_length > 2.0f * arc->radius)
        {
            return false; // Impossible arc - chord longer than diameter
        }

        // Distance from chord midpoint to arc center
        float h = sqrtf(arc->radius * arc->radius - (chord_length / 2.0f) * (chord_length / 2.0f));

        // Chord midpoint
        float mid_x = (arc->start.x + arc->end.x) / 2.0f;
        float mid_y = (arc->start.y + arc->end.y) / 2.0f;

        // Unit vector perpendicular to chord
        float perp_x = -dy / chord_length;
        float perp_y = dx / chord_length;

        // Choose arc center based on direction and radius sign
        bool large_arc = (arc->r_radius < 0.0f);
        if ((arc->direction == ARC_DIRECTION_CW && !large_arc) ||
            (arc->direction == ARC_DIRECTION_CCW && large_arc))
        {
            h = -h; // Flip to other side
        }

        arc->center.x = mid_x + h * perp_x;
        arc->center.y = mid_y + h * perp_y;
        arc->center.z = arc->start.z; // Assume planar arc for R format
        arc->center.a = arc->start.a;
    }
    else
    {
        return false; // Invalid format
    }

    // Calculate start and end angles
    arc->start_angle = atan2f(arc->start.y - arc->center.y, arc->start.x - arc->center.x);
    arc->end_angle = atan2f(arc->end.y - arc->center.y, arc->end.x - arc->center.x);

    // Calculate total angle considering direction
    if (arc->direction == ARC_DIRECTION_CCW)
    {
        // Counter-clockwise
        if (arc->end_angle <= arc->start_angle)
        {
            arc->total_angle = arc->end_angle - arc->start_angle + 2.0f * M_PI;
        }
        else
        {
            arc->total_angle = arc->end_angle - arc->start_angle;
        }
    }
    else
    {
        // Clockwise
        if (arc->end_angle >= arc->start_angle)
        {
            arc->total_angle = arc->end_angle - arc->start_angle - 2.0f * M_PI;
        }
        else
        {
            arc->total_angle = arc->end_angle - arc->start_angle;
        }
        arc->total_angle = -arc->total_angle; // Make positive for calculations
    }

    // Calculate arc length
    arc->arc_length = arc->radius * fabsf(arc->total_angle);

    // Add Z-axis (helical) component if present
    float dz = arc->end.z - arc->start.z;
    if (fabsf(dz) > INTERP_POSITION_TOLERANCE)
    {
        arc->arc_length = sqrtf(arc->arc_length * arc->arc_length + dz * dz);
    }

    return true;
}

bool INTERP_ValidateArcGeometry(arc_parameters_t *arc)
{
    if (!arc)
        return false;

    // Check minimum radius
    if (arc->radius < 0.001f)
    {
        return false; // Radius too small
    }

    // Check maximum radius (prevent numerical issues)
    if (arc->radius > 1000.0f)
    {
        return false; // Radius too large
    }

    // Verify start and end points are approximately on the circle
    float start_radius = sqrtf(
        (arc->start.x - arc->center.x) * (arc->start.x - arc->center.x) +
        (arc->start.y - arc->center.y) * (arc->start.y - arc->center.y));

    float end_radius = sqrtf(
        (arc->end.x - arc->center.x) * (arc->end.x - arc->center.x) +
        (arc->end.y - arc->center.y) * (arc->end.y - arc->center.y));

    float radius_tolerance = fmaxf(0.001f, arc->tolerance);

    if (fabsf(start_radius - arc->radius) > radius_tolerance ||
        fabsf(end_radius - arc->radius) > radius_tolerance)
    {
        return false; // Points not on circle
    }

    // Check for reasonable arc angle
    if (fabsf(arc->total_angle) > 4.0f * M_PI)
    {
        return false; // More than 2 full circles
    }

    return true;
}

position_t INTERP_CalculateArcPoint(arc_parameters_t *arc, float angle)
{
    position_t point;

    // Calculate X,Y position on arc
    point.x = arc->center.x + arc->radius * cosf(angle);
    point.y = arc->center.y + arc->radius * sinf(angle);

    // Linear interpolation for Z (helical motion)
    float angle_progress = (angle - arc->start_angle) / arc->total_angle;
    if (arc->direction == ARC_DIRECTION_CW)
    {
        angle_progress = (arc->start_angle - angle) / arc->total_angle;
    }

    point.z = arc->start.z + (arc->end.z - arc->start.z) * angle_progress;
    point.a = arc->start.a + (arc->end.a - arc->start.a) * angle_progress;

    return point;
}

bool INTERP_SegmentArc(arc_parameters_t *arc, position_t segments[], uint16_t max_segments)
{
    if (!arc || !segments || max_segments == 0)
    {
        return false;
    }

    // Calculate number of segments based on arc tolerance
    // Use GRBL-style segmentation: segments = arc_length / (2 * sqrt(2 * tolerance * radius))
    float segment_length = 2.0f * sqrtf(2.0f * arc->tolerance * arc->radius);
    arc->num_segments = (uint16_t)(arc->arc_length / segment_length) + 1;

    // Limit to maximum segments
    if (arc->num_segments > max_segments)
    {
        arc->num_segments = max_segments;
    }

    // Minimum segments for any arc
    if (arc->num_segments < 3)
    {
        arc->num_segments = 3;
    }

    // Calculate angle increment
    float angle_increment = arc->total_angle / (float)(arc->num_segments - 1);
    if (arc->direction == ARC_DIRECTION_CW)
    {
        angle_increment = -angle_increment;
    }

    // Generate segment points
    for (uint16_t i = 0; i < arc->num_segments; i++)
    {
        float angle = arc->start_angle + (float)i * angle_increment;
        segments[i] = INTERP_CalculateArcPoint(arc, angle);
    }

    // Ensure last point exactly matches end position
    segments[arc->num_segments - 1] = arc->end;

    return true;
}

bool INTERP_PlanArcMove(position_t start, position_t end, float i, float j, float k,
                        arc_direction_t direction, float feed_rate)
{
    if (!interp_context.initialized || !interp_context.enabled)
    {
        return false;
    }

    // Validate positions
    if (!INTERP_IsPositionValid(start) || !INTERP_IsPositionValid(end))
    {
        return false;
    }

    // Set up arc parameters
    arc_parameters_t arc;
    memset(&arc, 0, sizeof(arc_parameters_t));

    arc.start = start;
    arc.end = end;
    arc.i_offset = i;
    arc.j_offset = j;
    arc.k_offset = k;
    arc.direction = direction;
    arc.format = ARC_FORMAT_IJK;
    arc.tolerance = 0.002f; // Default arc tolerance (GRBL $12 setting)

    // Calculate arc parameters
    if (!INTERP_CalculateArcParameters(&arc))
    {
        return false;
    }

    // Validate arc geometry
    if (!INTERP_ValidateArcGeometry(&arc))
    {
        return false;
    }

    // Segment the arc into linear moves
    position_t segments[64]; // Maximum 64 segments per arc
    if (!INTERP_SegmentArc(&arc, segments, 64))
    {
        return false;
    }

    // Add arc segments to planner buffer
    for (uint16_t i = 0; i < arc.num_segments - 1; i++)
    {
        if (!INTERP_PlannerAddBlock(segments[i], segments[i + 1], feed_rate, MOTION_PROFILE_S_CURVE))
        {
            return false; // Buffer full
        }
    }

    printf("Arc move planned: %.2f mm radius, %.1f degrees, %d segments\n",
           arc.radius, fabsf(arc.total_angle) * 180.0f / M_PI, arc.num_segments);

    return true;
}

bool INTERP_PlanArcMoveRadius(position_t start, position_t end, float radius,
                              arc_direction_t direction, float feed_rate)
{
    if (!interp_context.initialized || !interp_context.enabled)
    {
        return false;
    }

    // Validate positions
    if (!INTERP_IsPositionValid(start) || !INTERP_IsPositionValid(end))
    {
        return false;
    }

    // Set up arc parameters
    arc_parameters_t arc;
    memset(&arc, 0, sizeof(arc_parameters_t));

    arc.start = start;
    arc.end = end;
    arc.r_radius = radius;
    arc.direction = direction;
    arc.format = ARC_FORMAT_RADIUS;
    arc.tolerance = 0.002f; // Default arc tolerance

    // Calculate arc parameters
    if (!INTERP_CalculateArcParameters(&arc))
    {
        return false;
    }

    // Validate arc geometry
    if (!INTERP_ValidateArcGeometry(&arc))
    {
        return false;
    }

    // Segment the arc into linear moves
    position_t segments[64];
    if (!INTERP_SegmentArc(&arc, segments, 64))
    {
        return false;
    }

    // Add arc segments to planner buffer
    for (uint16_t i = 0; i < arc.num_segments - 1; i++)
    {
        if (!INTERP_PlannerAddBlock(segments[i], segments[i + 1], feed_rate, MOTION_PROFILE_S_CURVE))
        {
            return false; // Buffer full
        }
    }

    printf("Arc move (R format) planned: %.2f mm radius, %.1f degrees, %d segments\n",
           arc.radius, fabsf(arc.total_angle) * 180.0f / M_PI, arc.num_segments);

    return true;
}

// *****************************************************************************
// Step Rate Control Functions - OCRx Integration
// *****************************************************************************

float INTERP_GetAxisStepRate(axis_id_t axis)
{
    if (axis >= INTERP_MAX_AXES)
        return 0.0f;

    return interp_context.steps.step_frequency[axis];
}

void INTERP_UpdateStepRates(void)
{
    // Public function to update step rates - calls internal update_step_rates()
    update_step_rates();
}

// *****************************************************************************
// Homing Cycle Implementation
// *****************************************************************************

bool INTERP_StartHomingCycle(uint8_t axis_mask)
{
    if (!interp_context.initialized)
    {
        printf("ERROR: Interpolation engine not initialized\n");
        return false;
    }

    if (interp_context.homing.state != HOMING_STATE_IDLE)
    {
        printf("ERROR: Homing cycle already in progress\n");
        return false;
    }

    if (axis_mask == 0)
    {
        printf("ERROR: No axes specified for homing\n");
        return false;
    }

    // Initialize homing control structure
    interp_context.homing.state = HOMING_STATE_SEEK;
    interp_context.homing.axis_mask = axis_mask;
    interp_context.homing.current_axis = AXIS_X;         // Start with X axis
    interp_context.homing.start_time = _CP0_GET_COUNT(); // Use core timer
    interp_context.homing.debounce_time = 0;
    interp_context.homing.switch_triggered = false;

    // Set homing parameters - can be configured via GRBL settings
    interp_context.homing.seek_rate = INTERP_HOMING_SEEK_RATE;
    interp_context.homing.locate_rate = INTERP_HOMING_FEED_RATE;
    interp_context.homing.pulloff_distance = INTERP_HOMING_PULLOFF_DISTANCE;

    // Determine homing direction for current axis (typically towards limit switch)
    interp_context.homing.direction_positive = false; // Home towards negative limit

    // Clear any existing motion
    interp_context.planner.head = 0;
    interp_context.planner.tail = 0;
    interp_context.planner.count = 0;

    printf("Starting homing cycle for axes: 0x%02X\n", axis_mask);

    return true;
}

void INTERP_AbortHomingCycle(void)
{
    if (interp_context.homing.state != HOMING_STATE_IDLE)
    {
        // Stop motion immediately
        INTERP_StopStepGeneration();

        // Reset homing state
        interp_context.homing.state = HOMING_STATE_IDLE;
        interp_context.homing.axis_mask = 0;

        printf("Homing cycle aborted\n");
    }
}

homing_state_t INTERP_GetHomingState(void)
{
    return interp_context.homing.state;
}

bool INTERP_IsHomingActive(void)
{
    return (interp_context.homing.state != HOMING_STATE_IDLE &&
            interp_context.homing.state != HOMING_STATE_COMPLETE &&
            interp_context.homing.state != HOMING_STATE_ERROR);
}

void INTERP_SetHomingPosition(axis_id_t axis, float position)
{
    if (axis < INTERP_MAX_AXES)
    {
        interp_context.homing.home_position[axis].x = (axis == AXIS_X) ? position : 0.0f;
        interp_context.homing.home_position[axis].y = (axis == AXIS_Y) ? position : 0.0f;
        interp_context.homing.home_position[axis].z = (axis == AXIS_Z) ? position : 0.0f;
        interp_context.homing.home_position[axis].a = (axis == AXIS_A) ? position : 0.0f;
    }
}

/* ============================================================================
 * Limit Switch Masking Functions for Pick-and-Place Operations
 * ============================================================================ */

void INTERP_SetLimitMask(limit_mask_t mask)
{
    interp_context.active_limit_mask = mask;
    printf("LIMIT MASK: Set to 0x%02X\n", mask);

    // Safety warning for dangerous mask combinations
    if (mask == LIMIT_MASK_ALL)
    {
        printf("WARNING: ALL LIMITS MASKED - USE WITH EXTREME CAUTION!\n");
    }
}

limit_mask_t INTERP_GetLimitMask(void)
{
    return interp_context.active_limit_mask;
}

void INTERP_EnableLimitMask(limit_mask_t mask)
{
    interp_context.active_limit_mask |= mask;
    printf("LIMIT MASK: Enabled 0x%02X (total: 0x%02X)\n", mask, interp_context.active_limit_mask);
}

void INTERP_DisableLimitMask(limit_mask_t mask)
{
    interp_context.active_limit_mask &= ~mask;
    printf("LIMIT MASK: Disabled 0x%02X (total: 0x%02X)\n", mask, interp_context.active_limit_mask);
}

bool INTERP_IsLimitMasked(axis_id_t axis, bool is_max_limit)
{
    limit_mask_t check_mask = LIMIT_MASK_NONE;

    switch (axis)
    {
    case AXIS_X:
        check_mask = is_max_limit ? LIMIT_MASK_X_MAX : LIMIT_MASK_X_MIN;
        break;
    case AXIS_Y:
        check_mask = is_max_limit ? LIMIT_MASK_Y_MAX : LIMIT_MASK_Y_MIN;
        break;
    case AXIS_Z:
        check_mask = is_max_limit ? LIMIT_MASK_Z_MAX : LIMIT_MASK_Z_MIN;
        break;
    case AXIS_A:
        check_mask = is_max_limit ? LIMIT_MASK_A_MAX : LIMIT_MASK_A_MIN;
        break;
    default:
        return false;
    }

    return (interp_context.active_limit_mask & check_mask) != 0;
}

static bool is_limit_switch_triggered(axis_id_t axis, bool check_positive)
{
    /* Check limit switch state for homing and safety
     * Min switches: Your actual MCC-configured pins (used for homing and min limits)
     * Max switches: Dummy for now - configure these pins later in MCC:
     *   - LIMIT_X_MAX_PIN, LIMIT_Y_MAX_PIN, LIMIT_Z_MAX_PIN
     *
     * IMPORTANT: Respects limit masking for pick-and-place operations
     */

    // First check if this specific limit is masked
    if (INTERP_IsLimitMasked(axis, check_positive))
    {
        return false; // Masked limit always reports inactive
    }

    switch (axis)
    {
    case AXIS_X:
        if (check_positive)
        {
            // TODO: Replace with actual max switch when configured in MCC
            // return !GPIO_PinRead(LIMIT_X_MAX_PIN);
            return false; // Dummy max switch (always inactive)
        }
        else
        {
            // return !GPIO_PinRead(LIMIT_X_PIN); // Min switch (homing + min limit)
            return false;
        }

    case AXIS_Y:
        if (check_positive)
        {
            // TODO: Replace with actual max switch when configured in MCC
            // return !GPIO_PinRead(LIMIT_Y_MAX_PIN);
            return false; // Dummy max switch (always inactive)
        }
        else
        {
            // return !GPIO_PinRead(LIMIT_Y_PIN); // Min switch (homing + min limit)
            return false;
        }

    case AXIS_Z:
        if (check_positive)
        {
            // TODO: Replace with actual max switch when configured in MCC
            // return !GPIO_PinRead(LIMIT_Z_MAX_PIN);
            return false; // Dummy max switch (always inactive)
        }
        else
        {
            // return !GPIO_PinRead(LIMIT_Z_PIN); // Min switch (homing + min limit)
            return false;
        }

    case AXIS_A:
        // 4th axis not implemented yet - no limit switches configured
        return false;

    default:
        return false;
    }
}

void INTERP_ProcessHomingCycle(void)
{
    if (!INTERP_IsHomingActive())
    {
        return;
    }

    uint32_t current_time = _CP0_GET_COUNT(); // Use core timer

    // Check for timeout (convert core timer ticks to milliseconds)
    // Core timer runs at CPU_FREQ/2, so ticks per ms = CPU_FREQ/2000
    uint32_t elapsed_ticks = current_time - interp_context.homing.start_time;
    uint32_t elapsed_ms = elapsed_ticks / (200000000UL / 2000); // Assuming 200MHz CPU

    if (elapsed_ms > INTERP_HOMING_TIMEOUT_MS)
    {
        interp_context.homing.state = HOMING_STATE_ERROR;
        printf("ERROR: Homing cycle timeout\n");
        return;
    }

    switch (interp_context.homing.state)
    {

    case HOMING_STATE_SEEK:
    {
        // Fast move towards limit switch
        bool limit_hit = is_limit_switch_triggered(interp_context.homing.current_axis,
                                                   interp_context.homing.direction_positive);

        if (limit_hit)
        {
            if (interp_context.homing.debounce_time == 0)
            {
                interp_context.homing.debounce_time = current_time;
            }
            else
            {
                uint32_t debounce_elapsed_ticks = current_time - interp_context.homing.debounce_time;
                uint32_t debounce_elapsed_ms = debounce_elapsed_ticks / (200000000UL / 2000);

                if (debounce_elapsed_ms > INTERP_HOMING_DEBOUNCE_MS)
                {
                    // Limit switch confirmed - stop fast motion
                    INTERP_StopSingleAxis(interp_context.homing.current_axis, "Limit reached in seek phase");

                    // Move to locate phase
                    interp_context.homing.state = HOMING_STATE_LOCATE;
                    interp_context.homing.direction_positive = !interp_context.homing.direction_positive; // Reverse direction

                    printf("Homing seek complete for axis %d\n", interp_context.homing.current_axis);
                }
            }
        }
        else
        {
            interp_context.homing.debounce_time = 0; // Reset debounce

            // Continue fast move if not already moving
            // Generate move command towards limit switch
            position_t current_pos = INTERP_GetCurrentPosition();
            position_t target_pos = current_pos;

            // Move a small distance towards the limit
            float move_distance = 1.0f; // 1mm incremental moves
            if (interp_context.homing.direction_positive)
            {
                move_distance = -move_distance; // Move towards negative limit
            }

            switch (interp_context.homing.current_axis)
            {
            case AXIS_X:
                target_pos.x += move_distance;
                break;
            case AXIS_Y:
                target_pos.y += move_distance;
                break;
            case AXIS_Z:
                target_pos.z += move_distance;
                break;
            case AXIS_A:
                target_pos.a += move_distance;
                break;
            }

            INTERP_PlanLinearMove(current_pos, target_pos, interp_context.homing.seek_rate);
        }
        break;
    }

    case HOMING_STATE_LOCATE:
    {
        // Slow precise move away from limit switch
        bool limit_hit = is_limit_switch_triggered(interp_context.homing.current_axis,
                                                   !interp_context.homing.direction_positive);

        if (!limit_hit)
        {
            // Limit switch released - found precise position
            INTERP_StopSingleAxis(interp_context.homing.current_axis, "Precise home position found");

            // Set current position as home (typically 0,0,0)
            INTERP_SetHomingPosition(interp_context.homing.current_axis, 0.0f);

            // Move to pulloff phase
            interp_context.homing.state = HOMING_STATE_PULLOFF;

            printf("Homing locate complete for axis %d\n", interp_context.homing.current_axis);
        }
        else
        {
            // Continue slow move away from limit
            position_t current_pos = INTERP_GetCurrentPosition();
            position_t target_pos = current_pos;

            float move_distance = 0.1f; // 0.1mm incremental moves for precision
            if (!interp_context.homing.direction_positive)
            {
                move_distance = -move_distance;
            }

            switch (interp_context.homing.current_axis)
            {
            case AXIS_X:
                target_pos.x += move_distance;
                break;
            case AXIS_Y:
                target_pos.y += move_distance;
                break;
            case AXIS_Z:
                target_pos.z += move_distance;
                break;
            case AXIS_A:
                target_pos.a += move_distance;
                break;
            }

            INTERP_PlanLinearMove(current_pos, target_pos, interp_context.homing.locate_rate);
        }
        break;
    }

    case HOMING_STATE_PULLOFF:
    {
        // Move pulloff distance away from limit switch
        position_t current_pos = INTERP_GetCurrentPosition();
        position_t target_pos = current_pos;

        float pulloff = interp_context.homing.pulloff_distance;
        if (!interp_context.homing.direction_positive)
        {
            pulloff = -pulloff;
        }

        switch (interp_context.homing.current_axis)
        {
        case AXIS_X:
            target_pos.x += pulloff;
            break;
        case AXIS_Y:
            target_pos.y += pulloff;
            break;
        case AXIS_Z:
            target_pos.z += pulloff;
            break;
        case AXIS_A:
            target_pos.a += pulloff;
            break;
        }

        if (INTERP_PlanLinearMove(current_pos, target_pos, interp_context.homing.locate_rate))
        {
            // Check if we need to home more axes
            uint8_t remaining_axes = interp_context.homing.axis_mask;
            remaining_axes &= ~(1 << interp_context.homing.current_axis); // Clear current axis

            if (remaining_axes != 0)
            {
                // Find next axis to home
                for (int i = interp_context.homing.current_axis + 1; i < INTERP_MAX_AXES; i++)
                {
                    if (remaining_axes & (1 << i))
                    {
                        interp_context.homing.current_axis = (axis_id_t)i;
                        interp_context.homing.state = HOMING_STATE_SEEK;
                        interp_context.homing.direction_positive = false; // Reset direction
                        printf("Starting homing for axis %d\n", i);
                        break;
                    }
                }
            }
            else
            {
                // All axes homed successfully
                interp_context.homing.state = HOMING_STATE_COMPLETE;
                printf("Homing cycle complete - all axes homed\n");
            }
        }
        break;
    }

    default:
        // Invalid state
        interp_context.homing.state = HOMING_STATE_ERROR;
        printf("ERROR: Invalid homing state\n");
        break;
    }
}

/*******************************************************************************
 End of File
 */