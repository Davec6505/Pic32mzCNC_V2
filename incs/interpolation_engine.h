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

#define INTERP_MAX_AXES 4                // X, Y, Z, A
#define INTERP_TIMER_FREQUENCY 1000      // 1kHz Timer1
#define INTERP_STEPS_PER_MM 200.0f       // Configurable steps/mm
#define INTERP_MAX_VELOCITY 5000.0f      // mm/min
#define INTERP_MAX_ACCELERATION 1000.0f  // mm/min²
#define INTERP_JERK_LIMIT 10000.0f       // mm/min³
#define INTERP_POSITION_TOLERANCE 0.001f // 1 micron

/* Look-ahead Planner Constants */
#define INTERP_PLANNER_BUFFER_SIZE 16   // GRBL-style 16-block buffer
#define INTERP_JUNCTION_DEVIATION 0.02f // Default junction deviation (mm)
#define INTERP_MIN_PLANNER_SPEED 10.0f  // Minimum planning speed (mm/min)

/* Homing Cycle Constants */
#define INTERP_HOMING_SEEK_RATE 800.0f      // Fast homing rate (mm/min)
#define INTERP_HOMING_FEED_RATE 25.0f       // Slow locate rate (mm/min)
#define INTERP_HOMING_PULLOFF_DISTANCE 1.0f // Pulloff distance (mm)
#define INTERP_HOMING_TIMEOUT_MS 30000      // 30 second timeout
#define INTERP_HOMING_DEBOUNCE_MS 10        // Switch debounce time

/* Motion State Machine */
typedef enum
{
    MOTION_STATE_IDLE = 0,
    MOTION_STATE_ACCELERATING,
    MOTION_STATE_CONSTANT_VELOCITY,
    MOTION_STATE_DECELERATING,
    MOTION_STATE_COMPLETE,
    MOTION_STATE_ERROR,
    MOTION_STATE_ALARM
} motion_state_t;

/* Motion Profile Types */
typedef enum
{
    MOTION_PROFILE_TRAPEZOIDAL = 0,
    MOTION_PROFILE_S_CURVE,
    MOTION_PROFILE_LINEAR
} motion_profile_type_t;

/* Arc Interpolation Types */
typedef enum
{
    ARC_DIRECTION_CW = 0, // G2 - Clockwise
    ARC_DIRECTION_CCW = 1 // G3 - Counter-clockwise
} arc_direction_t;

/* Arc Definition Methods */
typedef enum
{
    ARC_FORMAT_IJK = 0,   // I,J,K offset format
    ARC_FORMAT_RADIUS = 1 // R radius format
} arc_format_t;

/* Axis Configuration */
typedef enum
{
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
typedef struct
{
    float x;
    float y;
    float z;
    float a; // 4th axis (rotational)
} position_t;

/* Arc Parameters */
typedef struct
{
    position_t start;  // Arc start position
    position_t end;    // Arc end position
    position_t center; // Arc center position (calculated)

    float radius;      // Arc radius
    float start_angle; // Start angle in radians
    float end_angle;   // End angle in radians
    float total_angle; // Total arc angle (signed)
    float arc_length;  // Total arc length

    arc_direction_t direction; // Clockwise or counter-clockwise
    arc_format_t format;       // IJK offset or R radius format

    /* Input parameters */
    float i_offset; // I offset (X center offset)
    float j_offset; // J offset (Y center offset)
    float k_offset; // K offset (Z center offset)
    float r_radius; // R radius (alternative format)

    /* Segmentation parameters */
    float tolerance;       // Arc tolerance for segmentation
    uint16_t num_segments; // Number of linear segments
    float segment_length;  // Length of each segment
} arc_parameters_t;

/* Velocity Vector */
typedef struct
{
    float x;
    float y;
    float z;
    float magnitude;
} velocity_t;

/* Motion Parameters */
typedef struct
{
    position_t start_position;
    position_t end_position;
    position_t current_position;

    velocity_t current_velocity;
    float target_velocity; // Feed rate in mm/min
    float max_velocity;    // Machine limit
    float acceleration;    // Acceleration in mm/min²
    float deceleration;    // Deceleration in mm/min²

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
typedef struct
{
    int32_t step_count[INTERP_MAX_AXES];   // Absolute step position
    int32_t target_steps[INTERP_MAX_AXES]; // Target step position
    int32_t delta_steps[INTERP_MAX_AXES];  // Steps remaining

    uint32_t step_period_us[INTERP_MAX_AXES]; // Microseconds between steps
    uint32_t next_step_time[INTERP_MAX_AXES]; // Next step timestamp

    /* OCRx Continuous Pulse Mode Control */
    uint32_t step_period[INTERP_MAX_AXES]; // OCRx period value for step frequency
    float step_frequency[INTERP_MAX_AXES]; // Current step frequency (steps/sec)

    /* Bresenham Line Algorithm for Multi-Axis Coordination */
    int32_t bresenham_error[INTERP_MAX_AXES]; // Bresenham error accumulator
    int32_t bresenham_delta[INTERP_MAX_AXES]; // Bresenham delta values

    bool direction[INTERP_MAX_AXES];   // Step direction
    bool step_active[INTERP_MAX_AXES]; // Step signal state
} step_generation_t;

/* Planner Block - Individual motion segment in look-ahead buffer */
typedef struct
{
    position_t start_position; // Start position for this block
    position_t end_position;   // End position for this block

    float distance;                     // Total distance of this block
    float unit_vector[INTERP_MAX_AXES]; // Unit direction vector
    float nominal_speed;                // Programmed feed rate
    float entry_speed;                  // Entry speed (look-ahead optimized)
    float exit_speed;                   // Exit speed (look-ahead optimized)
    float max_entry_speed;              // Maximum allowable entry speed

    float acceleration;                 // Block acceleration limit
    motion_profile_type_t profile_type; // Motion profile to use

    bool recalculate_flag;    // Needs recalculation
    bool nominal_length_flag; // Block is at nominal length
    bool entry_speed_max;     // Entry speed is at maximum

    uint8_t block_id; // Block identifier
} planner_block_t;

/* Look-ahead Planner Buffer */
typedef struct
{
    planner_block_t blocks[INTERP_PLANNER_BUFFER_SIZE];
    uint8_t head;  // Next block to add
    uint8_t tail;  // Next block to execute
    uint8_t count; // Number of blocks in buffer

    float junction_deviation;    // Junction deviation setting
    float minimum_planner_speed; // Minimum speed for planning

    bool recalculate_needed;   // Buffer needs recalculation
    uint8_t recalculate_index; // Index to start recalculation
} planner_buffer_t;

/* Homing Cycle State Enum */
typedef enum
{
    HOMING_STATE_IDLE,
    HOMING_STATE_SEEK,     // Fast move to limit switch
    HOMING_STATE_LOCATE,   // Slow move for precise location
    HOMING_STATE_PULLOFF,  // Move away from limit switch
    HOMING_STATE_COMPLETE, // Homing completed successfully
    HOMING_STATE_ERROR     // Homing failed
} homing_state_t;

/* Homing Cycle Control */
typedef struct
{
    homing_state_t state;                      // Current homing state
    uint8_t axis_mask;                         // Axes to home (bitmask)
    axis_id_t current_axis;                    // Currently homing axis
    bool direction_positive;                   // Homing direction
    uint32_t start_time;                       // Homing start timestamp
    uint32_t debounce_time;                    // Switch debounce timer
    bool switch_triggered;                     // Limit switch state
    float seek_rate;                           // Fast homing speed
    float locate_rate;                         // Slow locate speed
    float pulloff_distance;                    // Distance to pull off limit
    position_t home_position[INTERP_MAX_AXES]; // Home position for each axis
} homing_control_t;

/* Limit Switch Masking for Pick-and-Place Operations */
typedef enum
{
    LIMIT_MASK_NONE = 0x00,  // No limits masked
    LIMIT_MASK_X_MIN = 0x01, // Mask X minimum limit
    LIMIT_MASK_X_MAX = 0x02, // Mask X maximum limit
    LIMIT_MASK_Y_MIN = 0x04, // Mask Y minimum limit
    LIMIT_MASK_Y_MAX = 0x08, // Mask Y maximum limit
    LIMIT_MASK_Z_MIN = 0x10, // Mask Z minimum limit (for spring-loaded nozzle)
    LIMIT_MASK_Z_MAX = 0x20, // Mask Z maximum limit
    LIMIT_MASK_A_MIN = 0x40, // Mask A minimum limit
    LIMIT_MASK_A_MAX = 0x80, // Mask A maximum limit
    LIMIT_MASK_ALL = 0xFF    // Mask all limits (DANGEROUS - use with caution!)
} limit_mask_t;

/* Interpolation Context */
typedef struct
{
    motion_parameters_t motion;
    step_generation_t steps;
    planner_buffer_t planner; // Look-ahead planner buffer
    homing_control_t homing;  // Homing cycle control

    /* Configuration */
    float steps_per_mm[INTERP_MAX_AXES];
    float max_velocity_per_axis[INTERP_MAX_AXES];
    float acceleration_per_axis[INTERP_MAX_AXES];

    /* Safety Control */
    limit_mask_t active_limit_mask; // Currently masked limit switches

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
bool INTERP_PlanArcMove(position_t start, position_t end, float i, float j, float k,
                        arc_direction_t direction, float feed_rate);
bool INTERP_PlanArcMoveRadius(position_t start, position_t end, float radius,
                              arc_direction_t direction, float feed_rate);
bool INTERP_ExecuteMove(void);
bool INTERP_IsMotionComplete(void);
void INTERP_StopMotion(void);

/* Real-time Control */
void INTERP_EmergencyStop(void);
void INTERP_ClearAlarmState(void);
void INTERP_FeedHold(bool hold);
void INTERP_OverrideFeedRate(float percentage);
void INTERP_Tasks(void); // Call from Timer1 ISR or main loop

/* Position and Status */
position_t INTERP_GetCurrentPosition(void);
velocity_t INTERP_GetCurrentVelocity(void);
motion_state_t INTERP_GetMotionState(void);
float INTERP_GetMotionProgress(void); // 0.0 to 1.0

/* Step Rate Control - OCRx Integration */
void INTERP_SetAxisStepRate(axis_id_t axis, float steps_per_second);
float INTERP_GetAxisStepRate(axis_id_t axis);
void INTERP_UpdateStepRates(void);
void INTERP_StartStepGeneration(void);
void INTERP_StopStepGeneration(void);

/* Limit Switch and Safety Control */
void INTERP_StopSingleAxis(axis_id_t axis, const char *reason);
void INTERP_HandleHardLimit(axis_id_t axis, bool min_limit, bool max_limit);
bool INTERP_CheckSoftLimits(position_t target_position);
void INTERP_LimitSwitchISR(axis_id_t axis, bool min_switch_state, bool max_switch_state);

/* Limit Switch Masking for Pick-and-Place Operations */
void INTERP_SetLimitMask(limit_mask_t mask);                  // Set which limits to ignore
limit_mask_t INTERP_GetLimitMask(void);                       // Get current limit mask
void INTERP_EnableLimitMask(limit_mask_t mask);               // Enable specific limit masks
void INTERP_DisableLimitMask(limit_mask_t mask);              // Disable specific limit masks
bool INTERP_IsLimitMasked(axis_id_t axis, bool is_max_limit); // Check if specific limit is masked

/* Homing Cycle Control */
bool INTERP_StartHomingCycle(uint8_t axis_mask);               // Start homing for specified axes
void INTERP_AbortHomingCycle(void);                            // Abort current homing cycle
homing_state_t INTERP_GetHomingState(void);                    // Get current homing state
bool INTERP_IsHomingActive(void);                              // Check if homing is in progress
void INTERP_ProcessHomingCycle(void);                          // Process homing state machine
void INTERP_SetHomingPosition(axis_id_t axis, float position); // Set home position

/* Callback Registration */
void INTERP_RegisterStepCallback(void (*callback)(axis_id_t axis, bool direction));
void INTERP_RegisterMotionCompleteCallback(void (*callback)(void));
void INTERP_RegisterErrorCallback(void (*callback)(const char *error_message));

/* Advanced Motion Control */
bool INTERP_PlanSCurveMove(position_t start, position_t end, float feed_rate);
bool INTERP_BlendMoves(position_t waypoints[], uint8_t point_count, float feed_rate);
bool INTERP_SetLookAheadDistance(float distance);

/* Look-ahead Planner Functions */
bool INTERP_PlannerAddBlock(position_t start, position_t end, float feed_rate, motion_profile_type_t profile);
bool INTERP_PlannerIsBufferFull(void);
bool INTERP_PlannerIsBufferEmpty(void);
uint8_t INTERP_PlannerGetBlockCount(void);
void INTERP_PlannerRecalculate(void);
void INTERP_PlannerOptimizeBuffer(void);
planner_block_t *INTERP_PlannerGetCurrentBlock(void);
void INTERP_PlannerAdvanceBlock(void);
void INTERP_PlannerClearBuffer(void);
bool INTERP_SetJunctionDeviation(float deviation);

/* Utility Functions */
float INTERP_CalculateDistance(position_t start, position_t end);
float INTERP_CalculateMoveTime(position_t start, position_t end, float feed_rate);
bool INTERP_IsPositionValid(position_t position);
void INTERP_LimitVelocity(velocity_t *velocity, float max_velocity);

/* Arc Interpolation Utilities */
bool INTERP_CalculateArcParameters(arc_parameters_t *arc);
bool INTERP_SegmentArc(arc_parameters_t *arc, position_t segments[], uint16_t max_segments);
float INTERP_CalculateArcLength(arc_parameters_t *arc);
bool INTERP_ValidateArcGeometry(arc_parameters_t *arc);
position_t INTERP_CalculateArcPoint(arc_parameters_t *arc, float angle);

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
typedef struct
{
    float acceleration_time;
    float constant_velocity_time;
    float deceleration_time;
    float acceleration_distance;
    float constant_velocity_distance;
    float deceleration_distance;
    float peak_velocity;
} trapezoidal_profile_t;

/* S-Curve Motion Profile */
typedef struct
{
    float jerk_time_accel;
    float accel_time;
    float jerk_time_decel;
    float constant_velocity_time;
    float constant_velocity_distance;
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

/* S-Curve Profile Functions */
float INTERP_GetSCurveVelocity(float time, const scurve_profile_t *profile);
float INTERP_GetSCurvePosition(float time, const scurve_profile_t *profile);

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