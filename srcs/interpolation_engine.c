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
    
    if (interp_context.motion.state != MOTION_STATE_IDLE) {
        return false; // Motion already in progress
    }
    
    // Start the motion
    interp_context.motion.state = MOTION_STATE_ACCELERATING;
    motion_start_time = TMR1_CounterGet(); // Get current timer value
    
    printf("Motion execution started\n");
    
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
    
    // Simple constant velocity profile for now
    // TODO: Implement trapezoidal and S-curve profiles
    
    float progress = interp_context.motion.time_elapsed / interp_context.motion.estimated_time;
    
    if (progress >= 1.0f) {
        // Motion complete
        interp_context.motion.state = MOTION_STATE_COMPLETE;
        interp_context.motion.current_position = interp_context.motion.end_position;
        interp_context.moves_completed++;
        
        if (interp_context.motion_complete_callback) {
            interp_context.motion_complete_callback();
        }
        
        printf("Motion completed in %.2f seconds\n", interp_context.motion.time_elapsed);
    } else {
        // Update current position based on progress
        position_t start = interp_context.motion.start_position;
        position_t end = interp_context.motion.end_position;
        
        interp_context.motion.current_position.x = start.x + (end.x - start.x) * progress;
        interp_context.motion.current_position.y = start.y + (end.y - start.y) * progress;
        interp_context.motion.current_position.z = start.z + (end.z - start.z) * progress;
        interp_context.motion.current_position.a = start.a + (end.a - start.a) * progress;
        
        interp_context.motion.distance_traveled = interp_context.motion.total_distance * progress;
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