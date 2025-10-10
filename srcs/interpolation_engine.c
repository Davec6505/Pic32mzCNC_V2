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
    for (int i = 0; i < INTERP_MAX_AXES; i++) {
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
    
    // Configure hardware peripherals
    if (!INTERP_ConfigureTimer1()) {
        return false;
    }
    
    if (!INTERP_ConfigureOCRModules()) {
        return false;
    }
    
    if (!INTERP_ConfigureStepperGPIO()) {
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
    if (!interp_context.initialized || !steps_per_mm || !max_vel || !accel) {
        return false;
    }
    
    for (int i = 0; i < INTERP_MAX_AXES; i++) {
        if (steps_per_mm[i] <= 0 || max_vel[i] <= 0 || accel[i] <= 0) {
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
    
    if (enable) {
        printf("Interpolation Engine Enabled\n");
    } else {
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
    if (!interp_context.initialized || !interp_context.enabled) {
        return false;
    }
    
    if (interp_context.motion.state != MOTION_STATE_IDLE) {
        return false; // Motion already in progress
    }
    
    // Validate positions
    if (!INTERP_IsPositionValid(start) || !INTERP_IsPositionValid(end)) {
        return false;
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
    for (int i = 0; i < INTERP_MAX_AXES; i++) {
        if (interp_context.max_velocity_per_axis[i] < max_feed_rate) {
            max_feed_rate = interp_context.max_velocity_per_axis[i];
        }
    }
    
    if (interp_context.motion.target_velocity > max_feed_rate) {
        interp_context.motion.target_velocity = max_feed_rate;
    }
    
    // Calculate estimated time
    interp_context.motion.estimated_time = INTERP_CalculateMoveTime(start, end, feed_rate);
    
    // Validate motion parameters
    if (!validate_motion_parameters()) {
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
    if (!interp_context.initialized || !interp_context.enabled) {
        return false;
    }
    
    // Check if we're already executing or if buffer is empty
    if (interp_context.motion.state != MOTION_STATE_IDLE) {
        return false; // Already executing
    }
    
    // Get next block from planner buffer
    planner_block_t *current_block = INTERP_PlannerGetCurrentBlock();
    if (!current_block) {
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
    switch (current_block->profile_type) {
        case MOTION_PROFILE_S_CURVE: {
            scurve_profile_t scurve_profile;
            if (INTERP_CalculateSCurveProfile(
                    current_block->distance,
                    current_block->nominal_speed,
                    current_block->acceleration,
                    INTERP_JERK_LIMIT,
                    &scurve_profile)) {
                
                interp_context.motion.estimated_time = 
                    2.0f * scurve_profile.jerk_time_accel +
                    2.0f * scurve_profile.accel_time +
                    scurve_profile.constant_velocity_time +
                    2.0f * scurve_profile.jerk_time_decel;
            }
            break;
        }
        
        case MOTION_PROFILE_TRAPEZOIDAL: {
            trapezoidal_profile_t trap_profile;
            if (INTERP_CalculateTrapezoidalProfile(
                    current_block->distance,
                    current_block->nominal_speed,
                    current_block->acceleration,
                    &trap_profile)) {
                
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
    if (!validate_motion_parameters()) {
        return false;
    }
    
    // Start motion execution
    interp_context.motion.state = MOTION_STATE_ACCELERATING;
    interp_context.motion.time_elapsed = 0.0f;
    interp_context.motion.distance_traveled = 0.0f;
    motion_start_time = TMR1_CounterGet();
    
    printf("Executing planner block %d: %.2f mm at %.1f mm/min\n", 
           current_block->block_id, current_block->distance, current_block->nominal_speed);
    
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
    
    // Reset step generation
    for (int i = 0; i < INTERP_MAX_AXES; i++) {
        interp_context.steps.step_active[i] = false;
    }
    
    printf("Motion stopped\n");
}

// *****************************************************************************
// Real-time Control Functions
// *****************************************************************************

void INTERP_EmergencyStop(void)
{
    interp_context.motion.emergency_stop = true;
    interp_context.motion.state = MOTION_STATE_IDLE;
    
    // Immediately stop all step generation
    for (int i = 0; i < INTERP_MAX_AXES; i++) {
        interp_context.steps.step_active[i] = false;
        INTERP_SetStepPin((axis_id_t)i, false);
    }
    
    if (interp_context.error_callback) {
        interp_context.error_callback("Emergency stop activated");
    }
    
    printf("EMERGENCY STOP ACTIVATED\n");
}

void INTERP_FeedHold(bool hold)
{
    interp_context.motion.feed_hold = hold;
    
    if (hold) {
        printf("Feed hold activated\n");
    } else {
        printf("Feed hold released\n");
    }
}

void INTERP_OverrideFeedRate(float percentage)
{
    if (percentage < 10.0f) percentage = 10.0f;   // Minimum 10%
    if (percentage > 200.0f) percentage = 200.0f; // Maximum 200%
    
    float original_feed_rate = interp_context.motion.target_velocity;
    interp_context.motion.target_velocity = original_feed_rate * (percentage / 100.0f);
    
    printf("Feed rate override: %.1f%% (%.1f mm/min)\n", 
           percentage, interp_context.motion.target_velocity);
}

void INTERP_Tasks(void)
{
    if (!interp_context.initialized || !interp_context.enabled) {
        return;
    }
    
    // Check for emergency stop
    if (interp_context.motion.emergency_stop) {
        return;
    }
    
    // Check for feed hold
    if (interp_context.motion.feed_hold) {
        return;
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
    if (interp_context.motion.total_distance <= 0.0f) {
        return 0.0f;
    }
    
    float progress = interp_context.motion.distance_traveled / interp_context.motion.total_distance;
    
    if (progress > 1.0f) progress = 1.0f;
    if (progress < 0.0f) progress = 0.0f;
    
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
    
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

float INTERP_CalculateMoveTime(position_t start, position_t end, float feed_rate)
{
    float distance = INTERP_CalculateDistance(start, end);
    
    if (feed_rate <= 0.0f) {
        return 0.0f;
    }
    
    // Convert feed_rate from mm/min to mm/sec, then calculate time in seconds
    return (distance / feed_rate) * 60.0f;
}

bool INTERP_IsPositionValid(position_t position)
{
    // Check for NaN or infinite values
    if (!isfinite(position.x) || !isfinite(position.y) || 
        !isfinite(position.z) || !isfinite(position.a)) {
        return false;
    }
    
    // Add machine-specific position limits here
    // For now, accept any finite position
    
    return true;
}

void INTERP_LimitVelocity(velocity_t *velocity, float max_velocity)
{
    if (!velocity) return;
    
    if (velocity->magnitude > max_velocity) {
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
    // This should be called from Timer1 ISR at 1kHz
    (void)status;   // Suppress unused parameter warning
    (void)context;  // Suppress unused parameter warning
    INTERP_Tasks();
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
    // OCR modules should already be configured by Harmony
    // They will be used for precise step pulse generation
    
    return true;
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
    
    switch (axis) {
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
    if (interp_context.step_callback) {
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
    switch(axis) {
        case AXIS_X:
            return LIMIT_X_Get() ? true : false;
        case AXIS_Y:
            return LIMIT_Y_Get() ? true : false;
        case AXIS_Z:
            return LIMIT_Z_Get() ? true : false;
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
        .a = interp_context.motion.end_position.a - interp_context.motion.start_position.a
    };
    
    // Convert distances to step counts
    interp_context.steps.target_steps[AXIS_X] = (int32_t)(delta.x * interp_context.steps_per_mm[AXIS_X]);
    interp_context.steps.target_steps[AXIS_Y] = (int32_t)(delta.y * interp_context.steps_per_mm[AXIS_Y]);
    interp_context.steps.target_steps[AXIS_Z] = (int32_t)(delta.z * interp_context.steps_per_mm[AXIS_Z]);
    interp_context.steps.target_steps[AXIS_A] = (int32_t)(delta.a * interp_context.steps_per_mm[AXIS_A]);
    
    // Set step directions
    for (int i = 0; i < INTERP_MAX_AXES; i++) {
        interp_context.steps.direction[i] = (interp_context.steps.target_steps[i] >= 0);
        interp_context.steps.delta_steps[i] = abs(interp_context.steps.target_steps[i]);
        interp_context.steps.step_count[i] = 0;
        interp_context.steps.step_active[i] = false;
    }
}

static void update_motion_state(void)
{
    if (interp_context.motion.state == MOTION_STATE_IDLE || 
        interp_context.motion.state == MOTION_STATE_COMPLETE) {
        return;
    }
    
    // Update time elapsed
    uint32_t current_time = TMR1_CounterGet();
    interp_context.motion.time_elapsed = (current_time - motion_start_time) / 1000.0f; // Convert to seconds
    
    float current_velocity = 0.0f;
    float distance_progress = 0.0f;
    
    // Execute motion profile based on type
    switch (interp_context.motion.profile_type) {
        case MOTION_PROFILE_S_CURVE: {
            // Use S-curve profile for smooth acceleration
            scurve_profile_t scurve_profile;
            if (INTERP_CalculateSCurveProfile(
                    interp_context.motion.total_distance,
                    interp_context.motion.target_velocity,
                    interp_context.motion.acceleration,
                    INTERP_JERK_LIMIT,
                    &scurve_profile)) {
                
                current_velocity = INTERP_GetSCurveVelocity(interp_context.motion.time_elapsed, &scurve_profile);
                distance_progress = INTERP_GetSCurvePosition(interp_context.motion.time_elapsed, &scurve_profile);
            }
            break;
        }
        
        case MOTION_PROFILE_TRAPEZOIDAL: {
            // Use trapezoidal profile for standard acceleration
            trapezoidal_profile_t trap_profile;
            if (INTERP_CalculateTrapezoidalProfile(
                    interp_context.motion.total_distance,
                    interp_context.motion.target_velocity,
                    interp_context.motion.acceleration,
                    &trap_profile)) {
                
                current_velocity = INTERP_GetProfileVelocity(interp_context.motion.time_elapsed, &trap_profile);
                distance_progress = INTERP_GetProfilePosition(interp_context.motion.time_elapsed, &trap_profile);
            }
            break;
        }
        
        case MOTION_PROFILE_LINEAR:
        default: {
            // Simple linear profile for basic moves
            float progress = interp_context.motion.time_elapsed / interp_context.motion.estimated_time;
            if (progress > 1.0f) progress = 1.0f;
            
            current_velocity = interp_context.motion.target_velocity;
            distance_progress = progress * interp_context.motion.total_distance;
            break;
        }
    }
    
    // Update motion state based on progress
    float motion_progress = distance_progress / interp_context.motion.total_distance;
    
    if (motion_progress >= 1.0f) {
        // Motion complete - advance to next block in planner buffer
        interp_context.motion.state = MOTION_STATE_COMPLETE;
        interp_context.motion.current_position = interp_context.motion.end_position;
        interp_context.motion.distance_traveled = interp_context.motion.total_distance;
        interp_context.moves_completed++;
        
        // Remove completed block from planner buffer
        INTERP_PlannerAdvanceBlock();
        
        // Check if there are more blocks to execute
        if (!INTERP_PlannerIsBufferEmpty()) {
            // Automatically start next block for continuous motion
            interp_context.motion.state = MOTION_STATE_IDLE;
            INTERP_ExecuteMove(); // Start next block immediately
        } else {
            // All blocks completed
            if (interp_context.motion_complete_callback) {
                interp_context.motion_complete_callback();
            }
            
            printf("All planner blocks completed in %.2f seconds (%.1f mm/min avg)\n", 
                   interp_context.motion.time_elapsed,
                   (interp_context.motion.total_distance / interp_context.motion.time_elapsed) * 60.0f);
        }
    } else {
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
        if (current_velocity < interp_context.motion.target_velocity * 0.1f) {
            if (motion_progress < 0.1f) {
                interp_context.motion.state = MOTION_STATE_ACCELERATING;
            } else {
                interp_context.motion.state = MOTION_STATE_DECELERATING;
            }
        } else if (current_velocity >= interp_context.motion.target_velocity * 0.9f) {
            interp_context.motion.state = MOTION_STATE_CONSTANT_VELOCITY;
        } else {
            if (motion_progress < 0.5f) {
                interp_context.motion.state = MOTION_STATE_ACCELERATING;
            } else {
                interp_context.motion.state = MOTION_STATE_DECELERATING;
            }
        }
    }
}

static void generate_step_signals(void)
{
    // Simple step generation based on current position
    // TODO: Implement proper step timing and pulse generation
    
    for (int i = 0; i < INTERP_MAX_AXES; i++) {
        if (interp_context.steps.delta_steps[i] > 0) {
            // Generate step pulse
            INTERP_GenerateStepPulse((axis_id_t)i);
            interp_context.steps.step_count[i]++;
            interp_context.steps.delta_steps[i]--;
            interp_context.total_steps_generated++;
        }
    }
}

static bool validate_motion_parameters(void)
{
    // Validate that motion parameters are reasonable
    if (interp_context.motion.total_distance <= 0.0f) {
        return false;
    }
    
    if (interp_context.motion.target_velocity <= 0.0f) {
        return false;
    }
    
    if (interp_context.motion.estimated_time <= 0.0f) {
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
    if (!profile || distance <= 0.0f || target_vel <= 0.0f || max_accel <= 0.0f) {
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
    if (profile->acceleration_distance + profile->deceleration_distance > distance) {
        // Triangle profile - cannot reach target velocity
        profile->peak_velocity = sqrtf(distance * max_accel);
        profile->acceleration_time = profile->peak_velocity / max_accel;
        profile->deceleration_time = profile->acceleration_time;
        profile->acceleration_distance = 0.5f * max_accel * profile->acceleration_time * profile->acceleration_time;
        profile->deceleration_distance = profile->acceleration_distance;
        profile->constant_velocity_time = 0.0f;
        profile->constant_velocity_distance = 0.0f;
    } else {
        // Trapezoidal profile - can reach target velocity
        profile->peak_velocity = target_vel;
        profile->constant_velocity_distance = distance - profile->acceleration_distance - profile->deceleration_distance;
        profile->constant_velocity_time = profile->constant_velocity_distance / target_vel;
    }
    
    return true;
}

float INTERP_GetProfileVelocity(float time, const trapezoidal_profile_t *profile)
{
    if (!profile) return 0.0f;
    
    if (time <= profile->acceleration_time) {
        // Acceleration phase
        return (profile->peak_velocity / profile->acceleration_time) * time;
    } else if (time <= profile->acceleration_time + profile->constant_velocity_time) {
        // Constant velocity phase
        return profile->peak_velocity;
    } else if (time <= profile->acceleration_time + profile->constant_velocity_time + profile->deceleration_time) {
        // Deceleration phase
        float decel_time = time - profile->acceleration_time - profile->constant_velocity_time;
        return profile->peak_velocity - (profile->peak_velocity / profile->deceleration_time) * decel_time;
    } else {
        // Motion complete
        return 0.0f;
    }
}

float INTERP_GetProfilePosition(float time, const trapezoidal_profile_t *profile)
{
    if (!profile) return 0.0f;
    
    if (time <= profile->acceleration_time) {
        // Acceleration phase
        return 0.5f * (profile->peak_velocity / profile->acceleration_time) * time * time;
    } else if (time <= profile->acceleration_time + profile->constant_velocity_time) {
        // Constant velocity phase
        float const_time = time - profile->acceleration_time;
        return profile->acceleration_distance + profile->peak_velocity * const_time;
    } else if (time <= profile->acceleration_time + profile->constant_velocity_time + profile->deceleration_time) {
        // Deceleration phase
        float decel_time = time - profile->acceleration_time - profile->constant_velocity_time;
        return profile->acceleration_distance + profile->constant_velocity_distance + 
               profile->peak_velocity * decel_time - 
               0.5f * (profile->peak_velocity / profile->deceleration_time) * decel_time * decel_time;
    } else {
        // Motion complete
        return profile->acceleration_distance + profile->constant_velocity_distance + profile->deceleration_distance;
    }
}

bool INTERP_CalculateSCurveProfile(float distance, float target_vel, 
                                  float max_accel, float jerk_limit, scurve_profile_t *profile)
{
    if (!profile || distance <= 0.0f || target_vel <= 0.0f || max_accel <= 0.0f || jerk_limit <= 0.0f) {
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
    
    if (accel_distance_needed * 2.0f > distance) {
        // Cannot reach target velocity - reduce peak velocity
        profile->peak_velocity = sqrtf(distance * max_accel);
    } else {
        profile->peak_velocity = target_vel;
    }
    
    // Calculate acceleration phase timing
    if (profile->peak_velocity > vel_at_max_accel) {
        // Full S-curve with constant acceleration phase
        profile->jerk_time_accel = t_jerk_accel;
        profile->peak_acceleration = max_accel;
        
        // Time at constant acceleration
        float remaining_vel = profile->peak_velocity - vel_at_max_accel;
        profile->accel_time = remaining_vel / max_accel;
    } else {
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
    
    if (profile->constant_velocity_distance < 0.0f) {
        profile->constant_velocity_distance = 0.0f;
        profile->constant_velocity_time = 0.0f;
    } else {
        profile->constant_velocity_time = profile->constant_velocity_distance / profile->peak_velocity;
    }
    
    return true;
}

bool INTERP_PlanSCurveMove(position_t start, position_t end, float feed_rate)
{
    if (!interp_context.initialized || !interp_context.enabled) {
        return false;
    }
    
    if (interp_context.motion.state != MOTION_STATE_IDLE) {
        return false; // Motion already in progress
    }
    
    // Validate positions
    if (!INTERP_IsPositionValid(start) || !INTERP_IsPositionValid(end)) {
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
    
    if (interp_context.motion.total_distance < INTERP_POSITION_TOLERANCE) {
        return false; // Move too small
    }
    
    // Calculate S-curve profile
    scurve_profile_t scurve_profile;
    if (!INTERP_CalculateSCurveProfile(
            interp_context.motion.total_distance,
            feed_rate,
            interp_context.motion.acceleration,
            INTERP_JERK_LIMIT,
            &scurve_profile)) {
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
    if (!validate_motion_parameters()) {
        return false;
    }
    
    return true;
}

float INTERP_GetSCurveVelocity(float time, const scurve_profile_t *profile)
{
    if (!profile) return 0.0f;
    
    float t = 0.0f;
    
    // Phase 1: Jerk-limited acceleration up
    if (time <= profile->jerk_time_accel) {
        return 0.5f * (profile->peak_acceleration / profile->jerk_time_accel) * time * time;
    }
    t += profile->jerk_time_accel;
    
    // Phase 2: Constant acceleration
    if (time <= t + profile->accel_time) {
        float t_phase = time - t;
        float vel_phase1 = 0.5f * profile->peak_acceleration * profile->jerk_time_accel;
        return vel_phase1 + profile->peak_acceleration * t_phase;
    }
    t += profile->accel_time;
    
    // Phase 3: Jerk-limited acceleration down
    if (time <= t + profile->jerk_time_decel) {
        float t_phase = time - t;
        float vel_prev = profile->peak_velocity - 0.5f * profile->peak_acceleration * profile->jerk_time_decel;
        return vel_prev + profile->peak_acceleration * t_phase - 
               0.5f * (profile->peak_acceleration / profile->jerk_time_decel) * t_phase * t_phase;
    }
    t += profile->jerk_time_decel;
    
    // Phase 4: Constant velocity
    if (time <= t + profile->constant_velocity_time) {
        return profile->peak_velocity;
    }
    t += profile->constant_velocity_time;
    
    // Phase 5: Jerk-limited deceleration down (mirror of phase 3)
    if (time <= t + profile->jerk_time_decel) {
        float t_phase = time - t;
        return profile->peak_velocity - 0.5f * (profile->peak_acceleration / profile->jerk_time_decel) * t_phase * t_phase;
    }
    t += profile->jerk_time_decel;
    
    // Phase 6: Constant deceleration (mirror of phase 2)
    if (time <= t + profile->decel_time) {
        float t_phase = time - t;
        float vel_phase5 = profile->peak_velocity - 0.5f * profile->peak_acceleration * profile->jerk_time_decel;
        return vel_phase5 - profile->peak_acceleration * t_phase;
    }
    t += profile->decel_time;
    
    // Phase 7: Jerk-limited deceleration up (mirror of phase 1)
    if (time <= t + profile->jerk_time_final) {
        float t_phase = time - t;
        float vel_phase6 = 0.5f * profile->peak_acceleration * profile->jerk_time_final;
        return vel_phase6 - 0.5f * (profile->peak_acceleration / profile->jerk_time_final) * t_phase * t_phase;
    }
    
    // Motion complete
    return 0.0f;
}

float INTERP_GetSCurvePosition(float time, const scurve_profile_t *profile)
{
    if (!profile) return 0.0f;
    
    float position = 0.0f;
    float t = 0.0f;
    
    // Phase 1: Jerk-limited acceleration up
    if (time <= profile->jerk_time_accel) {
        return (1.0f/6.0f) * (profile->peak_acceleration / profile->jerk_time_accel) * time * time * time;
    }
    position += (1.0f/6.0f) * profile->peak_acceleration * profile->jerk_time_accel * profile->jerk_time_accel;
    t += profile->jerk_time_accel;
    
    // Phase 2: Constant acceleration
    if (time <= t + profile->accel_time) {
        float t_phase = time - t;
        float vel_start = 0.5f * profile->peak_acceleration * profile->jerk_time_accel;
        return position + vel_start * t_phase + 0.5f * profile->peak_acceleration * t_phase * t_phase;
    }
    float vel_phase1 = 0.5f * profile->peak_acceleration * profile->jerk_time_accel;
    position += vel_phase1 * profile->accel_time + 0.5f * profile->peak_acceleration * profile->accel_time * profile->accel_time;
    t += profile->accel_time;
    
    // Phase 3: Jerk-limited acceleration down  
    if (time <= t + profile->jerk_time_decel) {
        float t_phase = time - t;
        float vel_start = profile->peak_velocity - 0.5f * profile->peak_acceleration * profile->jerk_time_decel;
        return position + vel_start * t_phase + 0.5f * profile->peak_acceleration * t_phase * t_phase -
               (1.0f/6.0f) * (profile->peak_acceleration / profile->jerk_time_decel) * t_phase * t_phase * t_phase;
    }
    position += (profile->peak_velocity - 0.5f * profile->peak_acceleration * profile->jerk_time_decel) * profile->jerk_time_decel +
                0.5f * profile->peak_acceleration * profile->jerk_time_decel * profile->jerk_time_decel -
                (1.0f/6.0f) * profile->peak_acceleration * profile->jerk_time_decel * profile->jerk_time_decel;
    t += profile->jerk_time_decel;
    
    // Phase 4: Constant velocity
    if (time <= t + profile->constant_velocity_time) {
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
    if (!interp_context.initialized) {
        return false;
    }
    
    // Check if buffer is full
    if (INTERP_PlannerIsBufferFull()) {
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
    
    block->distance = sqrtf(dx*dx + dy*dy + dz*dz + da*da);
    
    if (block->distance < INTERP_POSITION_TOLERANCE) {
        return false; // Block too small
    }
    
    // Calculate unit vector
    block->unit_vector[AXIS_X] = dx / block->distance;
    block->unit_vector[AXIS_Y] = dy / block->distance;
    block->unit_vector[AXIS_Z] = dz / block->distance;
    block->unit_vector[AXIS_A] = da / block->distance;
    
    // Set acceleration limit (use minimum of all axes)
    block->acceleration = interp_context.acceleration_per_axis[AXIS_X]; // Start with X
    for (int i = 1; i < INTERP_MAX_AXES; i++) {
        if (interp_context.acceleration_per_axis[i] < block->acceleration) {
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

planner_block_t* INTERP_PlannerGetCurrentBlock(void)
{
    if (INTERP_PlannerIsBufferEmpty()) {
        return NULL;
    }
    
    return &interp_context.planner.blocks[interp_context.planner.tail];
}

void INTERP_PlannerAdvanceBlock(void)
{
    if (!INTERP_PlannerIsBufferEmpty()) {
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
    if (deviation < 0.001f || deviation > 10.0f) {
        return false; // Invalid range
    }
    
    interp_context.planner.junction_deviation = deviation;
    interp_context.planner.recalculate_needed = true;
    return true;
}

void INTERP_PlannerRecalculate(void)
{
    if (!interp_context.planner.recalculate_needed || INTERP_PlannerIsBufferEmpty()) {
        return;
    }
    
    // Implement GRBL-style junction speed calculation
    uint8_t block_index = interp_context.planner.tail;
    planner_block_t *block = &interp_context.planner.blocks[block_index];
    planner_block_t *next_block = NULL;
    
    // Forward pass: Calculate maximum entry speeds
    for (uint8_t i = 0; i < interp_context.planner.count - 1; i++) {
        uint8_t next_index = (block_index + 1) % INTERP_PLANNER_BUFFER_SIZE;
        next_block = &interp_context.planner.blocks[next_index];
        
        if (block->recalculate_flag) {
            // Calculate junction speed using dot product and junction deviation
            float cos_theta = 0.0f;
            for (int axis = 0; axis < INTERP_MAX_AXES; axis++) {
                cos_theta -= block->unit_vector[axis] * next_block->unit_vector[axis];
            }
            
            if (cos_theta < 0.95f) { // Only calculate for significant direction changes
                // Junction speed based on centripetal acceleration
                float sin_theta_d2 = sqrtf(0.5f * (1.0f - cos_theta));
                float junction_speed = sqrtf(block->acceleration * interp_context.planner.junction_deviation * sin_theta_d2 / (1.0f - sin_theta_d2));
                
                // Limit to minimum of block speeds
                if (junction_speed > block->nominal_speed) junction_speed = block->nominal_speed;
                if (junction_speed > next_block->nominal_speed) junction_speed = next_block->nominal_speed;
                
                block->exit_speed = junction_speed;
                next_block->entry_speed = junction_speed;
            } else {
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
    for (uint8_t i = 0; i < interp_context.planner.count - 1; i++) {
        block = &interp_context.planner.blocks[block_index];
        uint8_t prev_index = (block_index - 1 + INTERP_PLANNER_BUFFER_SIZE) % INTERP_PLANNER_BUFFER_SIZE;
        planner_block_t *prev_block = &interp_context.planner.blocks[prev_index];
        
        // Check if exit speed is achievable given entry speed and acceleration
        float max_exit_speed = sqrtf(block->entry_speed * block->entry_speed + 2.0f * block->acceleration * block->distance);
        
        if (block->exit_speed > max_exit_speed) {
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
    if (!waypoints || point_count < 2) {
        return false;
    }
    
    // Add all segments to planner buffer
    for (uint8_t i = 0; i < point_count - 1; i++) {
        if (!INTERP_PlannerAddBlock(waypoints[i], waypoints[i + 1], feed_rate, MOTION_PROFILE_S_CURVE)) {
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
    for (int i = 0; i < INTERP_MAX_AXES; i++) {
        interp_context.steps.step_count[i] = 0;
        interp_context.steps.delta_steps[i] = 0;
        interp_context.steps.step_active[i] = false;
    }
}

/*******************************************************************************
 End of File
 */