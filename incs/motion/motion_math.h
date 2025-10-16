/*******************************************************************************
  Motion Mathematics and Kinematics Library

  Description:
    Comprehensive math library for CNC motion control including:
    - Unit conversions (mm, inches, steps)
    - Feedrate and velocity calculations
    - Junction velocity for look-ahead planning
    - S-curve trajectory timing
    - Vector mathematics for multi-axis coordination
    
  GRBL Compatibility:
    Uses GRBL v1.1f settings structure ($100-$132)
    Supports junction deviation ($11) for cornering
    
  Design Philosophy:
    - Pure functions (no side effects)
    - MISRA C:2012 compliant
    - Extensive bounds checking
    - Easy to unit test
*******************************************************************************/

#ifndef _MOTION_MATH_H
#define _MOTION_MATH_H

#include "motion_types.h"  // Centralized type definitions

// Global settings instance
extern motion_settings_t motion_settings;

// *****************************************************************************
// Settings Management
// *****************************************************************************

/*! \brief Initialize motion settings with safe defaults
 *
 *  Loads factory defaults suitable for typical CNC machine.
 *  Must be called before using any motion math functions.
 */
void MotionMath_InitializeSettings(void);

/*! \brief Load default settings for typical CNC machine
 *
 *  Default profile:
 *  - 250 steps/mm (1.8° stepper, 1/16 microstepping, GT2 belt)
 *  - 5000 mm/min max feedrate
 *  - 500 mm/sec² acceleration
 *  - 0.01mm junction deviation
 */
void MotionMath_LoadDefaultSettings(void);

/*! \brief Set GRBL setting by ID
 *
 *  \param setting_id GRBL setting number ($100, $110, etc.)
 *  \param value New setting value
 *  \return true if setting valid, false if out of range
 */
bool MotionMath_SetSetting(uint8_t setting_id, float value);

/*! \brief Get GRBL setting by ID
 *
 *  \param setting_id GRBL setting number ($100, $110, etc.)
 *  \return Current setting value, or 0.0f if invalid ID
 */
float MotionMath_GetSetting(uint8_t setting_id);

// *****************************************************************************
// Unit Conversions
// *****************************************************************************

/*! \brief Convert millimeters to inches
 *
 *  \param mm Distance in millimeters
 *  \return Distance in inches
 */
float MotionMath_MMToInch(float mm);

/*! \brief Convert inches to millimeters
 *
 *  \param inch Distance in inches
 *  \return Distance in millimeters
 */
float MotionMath_InchToMM(float inch);

/*! \brief Convert millimeters to steps for specified axis
 *
 *  Uses $100-$103 settings (steps_per_mm)
 *
 *  \param mm Distance in millimeters
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 *  \return Step count (rounded to nearest integer)
 */
int32_t MotionMath_MMToSteps(float mm, axis_id_t axis);

/*! \brief Convert steps to millimeters for specified axis
 *
 *  Uses $100-$103 settings (steps_per_mm)
 *
 *  \param steps Step count
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 *  \return Distance in millimeters
 */
float MotionMath_StepsToMM(int32_t steps, axis_id_t axis);

// *****************************************************************************
// Feedrate and Velocity Calculations
// *****************************************************************************

/*! \brief Convert feedrate to steps per second
 *
 *  \param feedrate_mm_min Feedrate in mm/min (G-code F value)
 *  \param axis Axis identifier
 *  \return Velocity in steps/sec
 */
float MotionMath_FeedrateToStepsPerSec(float feedrate_mm_min, axis_id_t axis);

/*! \brief Convert feedrate to OCR timer period
 *
 *  Calculates TMR period for OCR module to generate step pulses.
 *  Assumes 1MHz timer clock (1µs tick).
 *
 *  \param feedrate_mm_min Feedrate in mm/min
 *  \param axis Axis identifier
 *  \return OCR period in timer counts (clamped to 16-bit max)
 */
uint32_t MotionMath_FeedrateToOCRPeriod(float feedrate_mm_min, axis_id_t axis);

/*! \brief Calculate step interval from velocity
 *
 *  \param velocity_steps_sec Velocity in steps/second
 *  \return Time between steps in microseconds
 */
float MotionMath_CalculateStepInterval(float velocity_steps_sec);

// *****************************************************************************
// Time-Based Interpolation Support
// *****************************************************************************

/*! \brief Get maximum velocity in steps/sec for specified axis
 *
 *  Converts GRBL max_rate (mm/min) to steps/sec for S-curve calculations.
 *  Used by multiaxis_control for time-based interpolation.
 *
 *  \param axis Axis identifier
 *  \return Maximum velocity in steps/sec
 */
float MotionMath_GetMaxVelocityStepsPerSec(axis_id_t axis);

/*! \brief Get acceleration in steps/sec² for specified axis
 *
 *  Converts GRBL acceleration (mm/sec²) to steps/sec² for S-curve calculations.
 *  Supports per-axis acceleration limits (Z typically slower than XY).
 *
 *  \param axis Axis identifier
 *  \return Acceleration in steps/sec²
 */
float MotionMath_GetAccelStepsPerSec2(axis_id_t axis);

/*! \brief Get jerk limit in steps/sec³ for specified axis
 *
 *  Converts GRBL jerk limit (mm/sec³) to steps/sec³ for S-curve calculations.
 *  Note: Jerk is global setting but scales with steps_per_mm per axis.
 *
 *  \param axis Axis identifier
 *  \return Jerk limit in steps/sec³
 */
float MotionMath_GetJerkStepsPerSec3(axis_id_t axis);

/*! \brief Calculate move duration with trapezoidal profile
 *
 *  \param distance_mm Move distance
 *  \param feedrate_mm_min Target feedrate
 *  \param accel_mm_sec2 Acceleration limit
 *  \return Total move time in seconds
 */
float MotionMath_CalculateMoveTime(float distance_mm, float feedrate_mm_min, float accel_mm_sec2);

// *****************************************************************************
// Vector Mathematics (Multi-Axis Coordination)
// *****************************************************************************

/*! \brief Calculate 3D vector length (magnitude)
 *
 *  \param dx X component
 *  \param dy Y component
 *  \param dz Z component
 *  \return Vector magnitude (Euclidean distance)
 */
float MotionMath_VectorLength(float dx, float dy, float dz);

/*! \brief Normalize 3D vector to unit length
 *
 *  Modifies input values in-place.
 *
 *  \param x Pointer to X component (modified)
 *  \param y Pointer to Y component (modified)
 *  \param z Pointer to Z component (modified)
 */
void MotionMath_VectorNormalize(float *x, float *y, float *z);

/*! \brief Calculate block length for multi-axis move
 *
 *  Euclidean distance for coordinated motion.
 *
 *  \param dx X delta (mm)
 *  \param dy Y delta (mm)
 *  \param dz Z delta (mm)
 *  \return Total distance (mm)
 */
float MotionMath_CalculateBlockLength(float dx, float dy, float dz);

/*! \brief Calculate coordinated move parameters
 *
 *  Determines dominant axis and speed ratios for multi-axis synchronization.
 *
 *  \param steps Array of step counts per axis
 *  \param coord Output coordination data
 */
void MotionMath_CalculateCoordinatedMove(int32_t steps[NUM_AXES], motion_coordinated_move_t *coord);

// *****************************************************************************
// Look-Ahead Planner Support
// *****************************************************************************

/*! \brief Calculate safe junction velocity between two blocks
 *
 *  Uses junction deviation ($11) to determine maximum cornering speed.
 *  Based on GRBL's centripetal acceleration formula.
 *
 *  \param entry_angle_rad Angle between move vectors (0 to π radians)
 *  \param feedrate1_mm_min First block feedrate
 *  \param feedrate2_mm_min Second block feedrate
 *  \param junction_deviation_mm Junction deviation setting ($11)
 *  \return Safe junction velocity (mm/min)
 */
float MotionMath_CalculateJunctionVelocity(
    float entry_angle_rad,
    float feedrate1_mm_min,
    float feedrate2_mm_min,
    float junction_deviation_mm
);

/*! \brief Calculate angle between two move vectors
 *
 *  \param prev_dx Previous block X delta
 *  \param prev_dy Previous block Y delta
 *  \param prev_dz Previous block Z delta
 *  \param next_dx Next block X delta
 *  \param next_dy Next block Y delta
 *  \param next_dz Next block Z delta
 *  \return Angle in radians (0 to π)
 */
float MotionMath_CalculateJunctionAngle(
    float prev_dx, float prev_dy, float prev_dz,
    float next_dx, float next_dy, float next_dz
);

/*! \brief Calculate maximum entry velocity given exit velocity
 *
 *  Reverse calculation: What's the fastest we can enter if we need
 *  to exit at exit_vel?
 *
 *  \param distance_mm Block length
 *  \param exit_vel Exit velocity (mm/min)
 *  \param accel Acceleration limit (mm/sec²)
 *  \return Maximum entry velocity (mm/min)
 */
float MotionMath_CalculateMaxEntryVelocity(float distance_mm, float exit_vel, float accel);

/*! \brief Calculate maximum exit velocity given entry velocity
 *
 *  Forward calculation: What's the fastest we can exit if we enter
 *  at entry_vel?
 *
 *  \param distance_mm Block length
 *  \param entry_vel Entry velocity (mm/min)
 *  \param accel Acceleration limit (mm/sec²)
 *  \return Maximum exit velocity (mm/min)
 */
float MotionMath_CalculateMaxExitVelocity(float distance_mm, float entry_vel, float accel);

/*! \brief Calculate trapezoidal velocity profile
 *
 *  Determines acceleration, cruise, and deceleration phases.
 *  Handles cases where block is too short to reach peak velocity.
 *
 *  \param distance_mm Block length
 *  \param entry_velocity_mm_min Starting velocity (from look-ahead)
 *  \param exit_velocity_mm_min Ending velocity (from look-ahead)
 *  \param max_velocity_mm_min Feedrate limit
 *  \param acceleration_mm_sec2 Acceleration limit
 *  \param profile Output velocity profile
 *  \return true if profile valid, false if parameters invalid
 */
bool MotionMath_CalculateVelocityProfile(
    float distance_mm,
    float entry_velocity_mm_min,
    float exit_velocity_mm_min,
    float max_velocity_mm_min,
    float acceleration_mm_sec2,
    velocity_profile_t *profile
);

// *****************************************************************************
// S-Curve Trajectory Planning
// *****************************************************************************

/*! \brief Calculate S-curve 7-segment timing profile
 *
 *  Generates jerk-limited trajectory with smooth acceleration/deceleration.
 *  Integrates with look-ahead planner via entry/exit velocities.
 *
 *  Profile segments:
 *  1. Jerk-limited acceleration ramp-up
 *  2. Constant acceleration
 *  3. Jerk-limited acceleration ramp-down
 *  4. Constant velocity cruise
 *  5. Jerk-limited deceleration ramp-up
 *  6. Constant deceleration
 *  7. Jerk-limited deceleration ramp-down
 *
 *  \param distance_mm Move distance
 *  \param entry_velocity_mm_min Starting velocity (from look-ahead)
 *  \param exit_velocity_mm_min Ending velocity (from look-ahead)
 *  \param max_velocity_mm_min Feedrate limit
 *  \param acceleration_mm_sec2 Acceleration limit
 *  \param jerk_mm_sec3 Jerk limit
 *  \param timing Output timing profile
 *  \return true if profile valid, false if parameters invalid
 */
bool MotionMath_CalculateSCurveTiming(
    float distance_mm,
    float entry_velocity_mm_min,
    float exit_velocity_mm_min,
    float max_velocity_mm_min,
    float acceleration_mm_sec2,
    float jerk_mm_sec3,
    scurve_timing_t *timing
);

/*! \brief Calculate cruise velocity for S-curve profile
 *
 *  Determines maximum velocity achievable given distance constraints
 *  and acceleration/jerk limits.
 *
 *  \param distance Move distance (mm)
 *  \param max_vel Feedrate limit (mm/min)
 *  \param accel Acceleration limit (mm/sec²)
 *  \param jerk Jerk limit (mm/sec³)
 *  \return Achievable cruise velocity (mm/min)
 */
float MotionMath_CalculateCruiseVelocity(float distance, float max_vel, float accel, float jerk);

/*! \brief Calculate S-curve segment duration
 *
 *  Time to change velocity by velocity_delta with given accel/jerk.
 *
 *  \param velocity_delta Change in velocity (mm/min)
 *  \param accel Acceleration (mm/sec²)
 *  \param jerk Jerk (mm/sec³)
 *  \return Segment time (seconds)
 */
float MotionMath_CalculateSegmentTime(float velocity_delta, float accel, float jerk);

// *****************************************************************************
// Look-Ahead Buffer Planning (Forward/Reverse Passes)
// *****************************************************************************

/*! \brief Forward pass: Calculate maximum achievable velocities
 *
 *  First pass of look-ahead planner.
 *  Propagates velocity constraints forward through buffer.
 *
 *  \param blocks Array of velocity profiles to plan
 *  \param block_count Number of blocks in buffer
 *  \param junction_deviation_mm Junction deviation setting ($11)
 */
void MotionMath_PlannerForwardPass(
    velocity_profile_t *blocks,
    uint8_t block_count,
    float junction_deviation_mm
);

/*! \brief Reverse pass: Propagate velocity constraints backward
 *
 *  Second pass of look-ahead planner.
 *  Ensures deceleration constraints satisfied.
 *
 *  \param blocks Array of velocity profiles to plan
 *  \param block_count Number of blocks in buffer
 */
void MotionMath_PlannerReversePass(
    velocity_profile_t *blocks,
    uint8_t block_count
);

#endif // _MOTION_MATH_H
