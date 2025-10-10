/*******************************************************************************
  Linear Interpolation Engine

  File Name:
    interpolation_engine.h

  Summary:
    Professional linear and circular interpolation for CNC motion control

  Description:
    This module provides high-precision trajectory generation for G0 (rapid)
    and G1 (linear) moves with smooth acceleration profiles. Integrates with
    Timer1 for 1kHz motion control and OCR modules for precise step timing.
*******************************************************************************/

#ifndef INTERPOLATION_ENGINE_H
#define INTERPOLATION_ENGINE_H

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "definitions.h"

// *****************************************************************************
// *****************************************************************************
// Section: Constants
// *****************************************************************************
// *****************************************************************************

#define INTERP_MAX_AXES                 4           // X, Y, Z, A
#define INTERP_TIMER_FREQUENCY          1000        // 1kHz Timer1
#define INTERP_STEPS_PER_MM             200.0f      // Configurable steps/mm
#define INTERP_MAX_VELOCITY             5000.0f     // mm/min
#define INTERP_MAX_ACCELERATION         1000.0f     // mm/min²
#define INTERP_JERK_LIMIT               10000.0f    // mm/min³
#define INTERP_POSITION_TOLERANCE       0.001f      // 1 micron

/* Motion State Machine */
typedef enum {
    MOTION_STATE_IDLE = 0,
    MOTION_STATE_ACCELERATING,
    MOTION_STATE_CONSTANT_VELOCITY,
    MOTION_STATE_DECELERATING,
    MOTION_STATE_COMPLETE,
    MOTION_STATE_ERROR
} motion_state_t;

/* Motion Profile Types */
typedef enum {
    MOTION_PROFILE_TRAPEZOIDAL = 0,
    MOTION_PROFILE_S_CURVE,
    MOTION_PROFILE_LINEAR
} motion_profile_type_t;

/* Axis Configuration */
typedef enum {
    AXIS_X = 0,
    AXIS_Y = 1,
    AXIS_Z = 2,
    AXIS_A = 3
} axis_id_t;

// *****************************************************************************
// *****************************************************************************
// Section: Data Types
// *****************************************************************************
// *****************************************************************************

/* 3D Position Vector */
typedef struct {
    float x;
    float y;
    float z;
    float a;    // 4th axis (rotational)
} position_t;

/* Velocity Vector */
typedef struct {
    float x;
    float y;
    float z;
    float magnitude;
} velocity_t;

/* Motion Parameters */
typedef struct {
    position_t start_position;
    position_t end_position;
    position_t current_position;
    
    velocity_t current_velocity;
    float target_velocity;          // Feed rate in mm/min
    float max_velocity;             // Machine limit
    float acceleration;             // Acceleration in mm/min²
    float deceleration;             // Deceleration in mm/min²
    
    float total_distance;
    float distance_traveled;
    float time_elapsed;
    float estimated_time;
    
    motion_state_t state;
    motion_profile_type_t profile_type;
    
    bool emergency_stop;
    bool feed_hold;
} motion_parameters_t;

/* Step Generation Data */
typedef struct {
    int32_t step_count[INTERP_MAX_AXES];        // Absolute step position
    int32_t target_steps[INTERP_MAX_AXES];      // Target step position
    int32_t delta_steps[INTERP_MAX_AXES];       // Steps remaining
    
    uint32_t step_period_us[INTERP_MAX_AXES];   // Microseconds between steps
    uint32_t next_step_time[INTERP_MAX_AXES];   // Next step timestamp
    
    bool direction[INTERP_MAX_AXES];            // Step direction
    bool step_active[INTERP_MAX_AXES];          // Step signal state
} step_generation_t;

/* Interpolation Context */
typedef struct {
    motion_parameters_t motion;
    step_generation_t steps;
    
    /* Configuration */
    float steps_per_mm[INTERP_MAX_AXES];
    float max_velocity_per_axis[INTERP_MAX_AXES];
    float acceleration_per_axis[INTERP_MAX_AXES];
    
    /* Statistics */
    uint32_t moves_completed;
    uint32_t total_steps_generated;
    float average_velocity;
    uint32_t motion_time_ms;
    
    /* Callbacks */
    void (*step_callback)(axis_id_t axis, bool direction);
    void (*motion_complete_callback)(void);
    void (*error_callback)(const char *error_message);
    
    bool initialized;
    bool enabled;
} interpolation_context_t;

// *****************************************************************************
// *****************************************************************************
// Section: Interface Functions
// *****************************************************************************
// *****************************************************************************

/* Initialization and Configuration */
bool INTERP_Initialize(void);
bool INTERP_Configure(float steps_per_mm[], float max_vel[], float accel[]);
void INTERP_Enable(bool enable);
void INTERP_Reset(void);
void INTERP_Shutdown(void);

/* Motion Planning */
bool INTERP_PlanLinearMove(position_t start, position_t end, float feed_rate);
bool INTERP_PlanRapidMove(position_t start, position_t end);
bool INTERP_ExecuteMove(void);
bool INTERP_IsMotionComplete(void);
void INTERP_StopMotion(void);

/* Real-time Control */
void INTERP_EmergencyStop(void);
void INTERP_FeedHold(bool hold);
void INTERP_OverrideFeedRate(float percentage);
void INTERP_Tasks(void);                        // Call from Timer1 ISR or main loop

/* Position and Status */
position_t INTERP_GetCurrentPosition(void);
velocity_t INTERP_GetCurrentVelocity(void);
motion_state_t INTERP_GetMotionState(void);
float INTERP_GetMotionProgress(void);           // 0.0 to 1.0

/* Callback Registration */
void INTERP_RegisterStepCallback(void (*callback)(axis_id_t axis, bool direction));
void INTERP_RegisterMotionCompleteCallback(void (*callback)(void));
void INTERP_RegisterErrorCallback(void (*callback)(const char *error_message));

/* Advanced Motion Control */
bool INTERP_PlanSCurveMove(position_t start, position_t end, float feed_rate);
bool INTERP_BlendMoves(position_t waypoints[], uint8_t point_count, float feed_rate);
bool INTERP_SetLookAheadDistance(float distance);

/* Utility Functions */
float INTERP_CalculateDistance(position_t start, position_t end);
float INTERP_CalculateMoveTime(position_t start, position_t end, float feed_rate);
bool INTERP_IsPositionValid(position_t position);
void INTERP_LimitVelocity(velocity_t *velocity, float max_velocity);

/* Diagnostics and Testing */
void INTERP_PrintMotionParameters(void);
void INTERP_PrintStepStatistics(void);
bool INTERP_SelfTest(void);
float INTERP_GetCPUUtilization(void);

// *****************************************************************************
// *****************************************************************************
// Section: Motion Profile Mathematics
// *****************************************************************************
// *****************************************************************************

/* Trapezoidal Motion Profile */
typedef struct {
    float acceleration_time;
    float constant_velocity_time;
    float deceleration_time;
    float acceleration_distance;
    float constant_velocity_distance;
    float deceleration_distance;
    float peak_velocity;
} trapezoidal_profile_t;

/* S-Curve Motion Profile */
typedef struct {
    float jerk_time_accel;
    float accel_time;
    float jerk_time_decel;
    float constant_velocity_time;
    float decel_time;
    float jerk_time_final;
    float peak_velocity;
    float peak_acceleration;
} scurve_profile_t;

/* Profile Calculation Functions */
bool INTERP_CalculateTrapezoidalProfile(float distance, float target_vel, 
                                       float max_accel, trapezoidal_profile_t *profile);
bool INTERP_CalculateSCurveProfile(float distance, float target_vel, 
                                  float max_accel, float jerk_limit, scurve_profile_t *profile);
float INTERP_GetProfileVelocity(float time, const trapezoidal_profile_t *profile);
float INTERP_GetProfilePosition(float time, const trapezoidal_profile_t *profile);

// *****************************************************************************
// *****************************************************************************
// Section: Hardware Integration
// *****************************************************************************
// *****************************************************************************

/* Timer1 Integration */
void INTERP_Timer1Callback(uint32_t status, uintptr_t context); // Call from Timer1 ISR
bool INTERP_ConfigureTimer1(void);

/* OCR Module Integration */
bool INTERP_ConfigureOCRModules(void);
void INTERP_GenerateStepPulse(axis_id_t axis);
void INTERP_SetStepDirection(axis_id_t axis, bool direction);

/* GPIO Integration */
bool INTERP_ConfigureStepperGPIO(void);
void INTERP_SetStepPin(axis_id_t axis, bool state);
void INTERP_SetDirectionPin(axis_id_t axis, bool direction);
bool INTERP_ReadLimitSwitch(axis_id_t axis);
bool INTERP_ReadHomeSwitch(axis_id_t axis);

#endif /* INTERPOLATION_ENGINE_H */

/*******************************************************************************
 Integration Example:
 
 // Initialize interpolation engine
 INTERP_Initialize();
 
 // Configure machine parameters
 float steps_per_mm[] = {200.0f, 200.0f, 800.0f, 360.0f}; // X, Y, Z, A
 float max_velocity[] = {5000.0f, 5000.0f, 1000.0f, 3600.0f};
 float acceleration[] = {1000.0f, 1000.0f, 500.0f, 1800.0f};
 INTERP_Configure(steps_per_mm, max_velocity, acceleration);
 
 // Register callbacks
 INTERP_RegisterStepCallback(my_step_handler);
 INTERP_RegisterMotionCompleteCallback(my_motion_complete_handler);
 
 // Execute a linear move
 position_t start = {0.0f, 0.0f, 0.0f, 0.0f};
 position_t end = {100.0f, 50.0f, 10.0f, 90.0f};
 INTERP_PlanLinearMove(start, end, 1000.0f); // 1000 mm/min feed rate
 INTERP_ExecuteMove();
 
 // In Timer1 ISR (1kHz):
 void Timer1_ISR(void) {
     INTERP_Tasks();
 }
 
 *******************************************************************************/