/**
 * @file motion_types.h
 * @brief Centralized type definitions for CNC motion control system
 *
 * This header provides all shared data structures for the motion control
 * subsystem, preventing duplicate type definitions across multiple modules.
 *
 * Design Philosophy:
 * - Single source of truth for all motion-related types
 * - Supports time-based S-curve interpolation (NOT Bresenham)
 * - GRBL v1.1f compatible data structures
 * - Clear separation between parsed G-code and planned motion
 *
 * @date October 16, 2025
 */

#ifndef MOTION_TYPES_H
#define MOTION_TYPES_H

#include <stdint.h>
#include <stdbool.h>

//=============================================================================
// HARDWARE CONFIGURATION CONSTANTS
//=============================================================================

/**
 * @brief Timer clock frequency for OCR modules (TMR2/3/4/5)
 *
 * Configuration from MCC:
 * - Peripheral clock (PBCLK3): 50 MHz
 * - Prescaler: 1:32
 * - Effective frequency: 1.5625 MHz (640ns per tick)
 *
 * This constant is used for calculating OCR periods for step pulse generation.
 *
 * NOTE: 1:32 prescaler chosen to prevent 16-bit timer overflow at slow speeds.
 * Min step rate: 1.5625MHz / 65535 = 23.8 steps/sec (excellent for slow Z-axis)
 * Max step rate: 1.5625MHz / 50 = 31,250 steps/sec (adequate for rapids)
 */
#define TMR_CLOCK_HZ 1562500UL // 1.5625 MHz (50 MHz PBCLK3 ÷ 32 prescaler)

/**
 * @brief Stepper motor configuration
 */
#define STEPPER_STEPS_PER_REV 200.0f // 1.8° stepper = 200 steps/revolution (0.9° = 400)
#define MICROSTEPPING_MODE 16.0f     // DRV8825 microstepping: 1, 2, 4, 8, 16, 32

/**
 * @brief Timing belt drive configuration
 *
 * Common belt types and pitches:
 *   GT2: 2mm pitch (most common for 3D printers/CNC)
 *   GT3: 3mm pitch (higher torque)
 *   GT5: 5mm pitch (even higher torque)
 *   HTD 3mm: 3mm pitch (High Torque Drive)
 *   HTD 5mm: 5mm pitch (High Torque Drive)
 */
#define BELT_PITCH_MM 2.5f // Belt pitch in mm (2.5 for T2.5, 2.0 for GT2, 3.0 for GT3, 5.0 for GT5/HTD5)
#define PULLEY_TEETH 20.0f // Number of teeth on pulley (16, 20, 24, etc.)

/**
 * @brief Leadscrew/ballscrew configuration
 *
 * Common pitches: 2mm, 2.5mm, 5mm, 8mm
 * Imperial: 0.5" (12.7mm), 0.25" (6.35mm)
 */
#define SCREW_PITCH_MM 2.5f // Leadscrew pitch in mm (distance per revolution)

/**
 * @brief Calculate steps per mm for different drive systems
 *
 * Belt drive: (steps_per_rev * microstepping) / (pulley_teeth * belt_pitch_mm)
 * Leadscrew:  (steps_per_rev * microstepping) / screw_pitch_mm
 */
#define STEPS_PER_MM_BELT ((STEPPER_STEPS_PER_REV * MICROSTEPPING_MODE) / (PULLEY_TEETH * BELT_PITCH_MM))
#define STEPS_PER_MM_LEADSCREW ((STEPPER_STEPS_PER_REV * MICROSTEPPING_MODE) / SCREW_PITCH_MM)

//=============================================================================
// AXIS DEFINITIONS
//=============================================================================

/**
 * @brief Axis identifiers for 4-axis CNC system
 *
 * X/Y: Horizontal plane (gantry motion)
 * Z: Vertical axis (tool height)
 * A: Rotary axis (4th axis, degrees)
 */
typedef enum
{
    AXIS_X = 0,
    AXIS_Y = 1,
    AXIS_Z = 2,
    AXIS_A = 3,
    NUM_AXES = 4
} axis_id_t;

//=============================================================================
// POSITION TRACKING
//=============================================================================

/**
 * @brief Multi-axis position in steps
 *
 * Used for:
 * - Current machine position tracking
 * - Target position storage
 * - Start/end positions for motion profiles
 */
typedef struct
{
    int32_t x;
    int32_t y;
    int32_t z;
    int32_t a;
} position_t;

//=============================================================================
// S-CURVE MOTION PROFILE
//=============================================================================

/**
 * @brief 7-segment S-curve motion profile parameters
 *
 * Segments: J+ -> A+ -> J- -> Const -> J- -> A- -> J+
 *
 * Used by: multiaxis_control.c for time-based interpolation
 */
typedef struct
{
    float total_time;     // Total move time (seconds)
    float accel_time;     // Acceleration phase time
    float const_time;     // Constant velocity time
    float decel_time;     // Deceleration phase time
    float peak_velocity;  // Maximum velocity reached (steps/sec)
    float acceleration;   // Acceleration rate (steps/sec²)
    float distance;       // Total distance (steps)
    bool use_scurve;      // Use S-curve vs trapezoidal
    position_t start_pos; // Starting position
    position_t end_pos;   // Ending position
} scurve_motion_profile_t;

//=============================================================================
// COORDINATED MOVE REQUEST
//=============================================================================

/**
 * @brief Coordinated multi-axis move command
 *
 * Used by: MultiAxis_MoveCoordinated() for simultaneous axis motion
 * All active axes finish at the same time (time-synchronized)
 */
typedef struct
{
    int32_t steps[NUM_AXES];    // Step count per axis (signed for direction)
    bool axis_active[NUM_AXES]; // Which axes participate in move
} coordinated_move_t;

//=============================================================================
// MULTI-AXIS COORDINATION DATA
//=============================================================================

/**
 * @brief Multi-axis coordination analysis for synchronized motion
 *
 * Calculates dominant axis and speed ratios for coordinated moves.
 * Used by both look-ahead planner and multiaxis_control for synchronization.
 */
typedef struct
{
    axis_id_t dominant_axis;             // Axis with longest travel distance
    float axis_ratios[NUM_AXES];         // Relative speeds [0.0 - 1.0]
    float total_distance;                // Vector magnitude (mm)
    float total_move_time;               // Total time for move (from dominant axis) (seconds)
    float axis_velocity_scale[NUM_AXES]; // Velocity scaling factors (for time-based interpolation)
} motion_coordinated_move_t;

//=============================================================================
// VELOCITY PROFILE (for Look-Ahead Planning)
//=============================================================================

/**
 * @brief Velocity profile for a motion segment
 *
 * Used by: motion_buffer look-ahead planner to optimize cornering
 * speeds and minimize total move time while respecting acceleration limits.
 */
typedef struct
{
    float nominal_speed;   // Requested speed (mm/min)
    float entry_speed;     // Speed entering this segment (mm/min) - ALIAS: entry_velocity
    float exit_speed;      // Speed exiting this segment (mm/min) - ALIAS: exit_velocity
    float max_entry_speed; // Maximum physically possible (mm/min)
    float max_exit_speed;  // Limited by junction angle (mm/min)
    float acceleration;    // Max acceleration for this move (mm/sec²)

    // Additional fields for compatibility with motion_math.c
    float entry_velocity;  // ALIAS for entry_speed
    float exit_velocity;   // ALIAS for exit_speed
    float peak_velocity;   // Peak velocity achieved (mm/min)
    float accel_distance;  // Distance spent accelerating (mm)
    float cruise_distance; // Distance at constant velocity (mm)
    float decel_distance;  // Distance spent decelerating (mm)
    float total_time;      // Total time for this segment (seconds)
} velocity_profile_t;

//=============================================================================
// MOTION BLOCK (Ring Buffer Entry)
//=============================================================================

/**
 * @brief Planned motion block ready for execution
 *
 * This is the fundamental unit in the motion buffer ring buffer.
 * Contains all pre-calculated parameters needed by multiaxis_control
 * for immediate execution without further planning.
 *
 * Flow: parsed_move_t → (planner) → motion_block_t → (execution)
 */
typedef struct
{
    int32_t steps[NUM_AXES];         // Target position in steps (absolute)
    float feedrate;                  // Requested feedrate (mm/min)
    float entry_velocity;            // Planned entry velocity (mm/min) - from look-ahead
    float exit_velocity;             // Planned exit velocity (mm/min) - from look-ahead
    float max_entry_velocity;        // Junction velocity limit (mm/min)
    bool recalculate_flag;           // Needs replanning (set when new block added)
    bool axis_active[NUM_AXES];      // Which axes move in this block
    scurve_motion_profile_t profile; // Pre-calculated S-curve profile
} motion_block_t;

//=============================================================================
// PARSED G-CODE MOVE (Parser Output)
//=============================================================================

/**
 * @brief Parsed G-code move command (before conversion to steps)
 *
 * Output from G-code parser, input to motion buffer.
 * Contains move in user units (mm) before conversion to steps.
 *
 * Example: "G1 X10 Y20 F1500" → parsed_move_t
 */
typedef struct
{
    float target[NUM_AXES];    // Target position in mm (or degrees for A-axis)
    float feedrate;            // Feed rate in mm/min
    bool absolute_mode;        // G90 (true) or G91 (false)
    bool axis_words[NUM_AXES]; // Which axes specified in command
    uint8_t motion_mode;       // G0, G1, G2, G3, etc.
    
    // Arc parameters (for G2/G3)
    float arc_center_offset[3]; // I, J, K offsets from current position (mm)
    float arc_radius;           // R parameter (mm) - alternative to IJK
    bool arc_has_ijk;           // True if IJK specified (center format)
    bool arc_has_radius;        // True if R specified (radius format)
} parsed_move_t;

//=============================================================================
// S-CURVE TIMING STRUCTURE (for Complex Profiles)
//=============================================================================

/**
 * @brief Detailed timing for 7-segment S-curve calculation
 *
 * Used by: MotionMath_CalculateSCurveTiming() for advanced planning
 *
 * Segments:
 *   t1: Jerk positive (acceleration increasing)
 *   t2: Constant acceleration
 *   t3: Jerk negative (acceleration decreasing to zero)
 *   t4: Constant velocity
 *   t5: Jerk negative (deceleration increasing)
 *   t6: Constant deceleration
 *   t7: Jerk positive (deceleration decreasing to zero)
 */
typedef struct
{
    float t1, t2, t3, t4, t5, t6, t7; // Time for each segment (seconds)
    float v_max;                      // Peak velocity (mm/sec)
    float a_max;                      // Peak acceleration (mm/sec²)
    float j_max;                      // Jerk limit (mm/sec³)
    bool valid;                       // Calculation succeeded

    // Additional fields for compatibility with motion_math.c
    float entry_velocity;      // Entry velocity (mm/min)
    float exit_velocity;       // Exit velocity (mm/min)
    float peak_velocity;       // Peak velocity (mm/min)
    float t1_jerk_accel;       // Segment 1: Jerk increasing acceleration
    float t2_const_accel;      // Segment 2: Constant acceleration
    float t3_jerk_decel_accel; // Segment 3: Jerk decreasing acceleration
    float t4_cruise;           // Segment 4: Constant velocity cruise
    float t5_jerk_accel_decel; // Segment 5: Jerk increasing deceleration
    float t6_const_decel;      // Segment 6: Constant deceleration
    float t7_jerk_decel_decel; // Segment 7: Jerk decreasing deceleration
} scurve_timing_t;

//=============================================================================
// GRBL SETTINGS STRUCTURE (from motion_math.h)
//=============================================================================

/**
 * @brief GRBL v1.1f compatible settings structure
 *
 * Centralized storage for all motion parameters.
 * These are the $100-$133 settings in GRBL protocol.
 */
typedef struct
{
    // $100-$103: Steps per mm
    float steps_per_mm[NUM_AXES];

    // $110-$113: Max rate (mm/min)
    float max_rate[NUM_AXES];

    // $120-$123: Acceleration (mm/sec²)
    float acceleration[NUM_AXES];

    // $130-$133: Max travel (mm)
    float max_travel[NUM_AXES];

    // $11: Junction deviation (mm)
    float junction_deviation;

    // $12: Arc tolerance (mm)
    float arc_tolerance;

    // Custom: Jerk limit (mm/sec³)
    float jerk_limit;

    // Custom: Minimum planner speed (mm/min)
    float minimum_planner_speed;

    // Homing settings (future use)
    bool homing_enabled;
    float homing_feed_rate;
    float homing_seek_rate;
} motion_settings_t;

#endif // MOTION_TYPES_H
