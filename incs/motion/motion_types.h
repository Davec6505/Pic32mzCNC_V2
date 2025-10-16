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
// AXIS DEFINITIONS
//=============================================================================

/**
 * @brief Axis identifiers for 4-axis CNC system
 * 
 * X/Y: Horizontal plane (gantry motion)
 * Z: Vertical axis (tool height)
 * A: Rotary axis (4th axis, degrees)
 */
typedef enum {
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
typedef struct {
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
typedef struct {
    float total_time;               // Total move time (seconds)
    float accel_time;               // Acceleration phase time
    float const_time;               // Constant velocity time
    float decel_time;               // Deceleration phase time
    float peak_velocity;            // Maximum velocity reached (steps/sec)
    float acceleration;             // Acceleration rate (steps/sec²)
    float distance;                 // Total distance (steps)
    bool use_scurve;                // Use S-curve vs trapezoidal
    position_t start_pos;           // Starting position
    position_t end_pos;             // Ending position
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
typedef struct {
    int32_t steps[NUM_AXES];        // Step count per axis (signed for direction)
    bool axis_active[NUM_AXES];     // Which axes participate in move
} coordinated_move_t;

//=============================================================================
// MULTI-AXIS COORDINATION DATA
//=============================================================================

/**
 * @brief Multi-axis coordination analysis for synchronized motion
 * 
 * Calculates dominant axis and speed ratios for coordinated moves.
 * Used by look-ahead planner for multi-axis synchronization.
 */
typedef struct {
    axis_id_t dominant_axis;         // Axis with longest travel distance
    float axis_ratios[NUM_AXES];     // Relative speeds [0.0 - 1.0]
    float total_distance;            // Vector magnitude (mm)
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
typedef struct {
    float nominal_speed;            // Requested speed (mm/min)
    float entry_speed;              // Speed entering this segment (mm/min)
    float exit_speed;               // Speed exiting this segment (mm/min)
    float max_entry_speed;          // Maximum physically possible (mm/min)
    float max_exit_speed;           // Limited by junction angle (mm/min)
    float acceleration;             // Max acceleration for this move (mm/sec²)
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
typedef struct {
    int32_t steps[NUM_AXES];        // Target position in steps (absolute)
    float feedrate;                 // Requested feedrate (mm/min)
    float entry_velocity;           // Planned entry velocity (mm/min) - from look-ahead
    float exit_velocity;            // Planned exit velocity (mm/min) - from look-ahead
    float max_entry_velocity;       // Junction velocity limit (mm/min)
    bool recalculate_flag;          // Needs replanning (set when new block added)
    bool axis_active[NUM_AXES];     // Which axes move in this block
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
typedef struct {
    float target[NUM_AXES];         // Target position in mm (or degrees for A-axis)
    float feedrate;                 // Feed rate in mm/min
    bool absolute_mode;             // G90 (true) or G91 (false)
    bool axis_words[NUM_AXES];      // Which axes specified in command
    uint8_t motion_mode;            // G0, G1, G2, G3, etc.
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
typedef struct {
    float t1, t2, t3, t4, t5, t6, t7;  // Time for each segment (seconds)
    float v_max;                        // Peak velocity (mm/sec)
    float a_max;                        // Peak acceleration (mm/sec²)
    float j_max;                        // Jerk limit (mm/sec³)
    bool valid;                         // Calculation succeeded
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
typedef struct {
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
    
    // Homing settings (future use)
    bool homing_enabled;
    float homing_feed_rate;
    float homing_seek_rate;
} motion_settings_t;

#endif // MOTION_TYPES_H
