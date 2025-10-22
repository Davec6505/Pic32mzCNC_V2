/*******************************************************************************
  Motion Mathematics and Kinematics Library - Implementation

  Description:
    Core math library for CNC motion control.
    All functions are pure (no side effects) for easy testing.

  MISRA C:2012 Compliance:
    - Bounds checking on all array accesses
    - NULL pointer validation
    - Floating point safety checks (NaN, infinity)
    - Const-qualified lookup tables
*******************************************************************************/

#include "motion/motion_math.h"
#include "motion/multiaxis_control.h" /* For MultiAxis_GetStepCount() */
#include <stddef.h>
#include <math.h>
#include <assert.h>
#include <float.h>
#include <stdio.h> /* For snprintf() */

// *****************************************************************************
// Constants
// *****************************************************************************

#define MM_PER_INCH 25.4f
#define MIN_TO_SEC 60.0f
#define PI 3.14159265359f
#define EPSILON 1e-6f // Floating point comparison tolerance

// OCR hardware constraints (PIC32MZ timer configuration)
// TMR_CLOCK_HZ defined in motion_types.h (1.5625 MHz = 25 MHz ÷ 16 prescaler)
#define OCR_MAX_PERIOD 65485 // 16-bit timer with safety margin (41.91ms @ 1.5625MHz)

// DRV8825 stepper driver constraints
#define DRV8825_MAX_STEP_RATE_HZ 250000UL                                                // 250 kHz maximum step frequency
#define DRV8825_MIN_PERIOD_US 4.0f                                                       // Minimum 4µs per step (1/250kHz = 4µs)
#define OCR_MIN_PERIOD ((uint32_t)(DRV8825_MIN_PERIOD_US * (TMR_CLOCK_HZ / 1000000.0f))) // 7 counts @ 1.5625MHz (round up to 50 for safety)

// Hardware configuration constants defined in motion_types.h (single source of truth):
//   TMR_CLOCK_HZ, STEPPER_STEPS_PER_REV, MICROSTEPPING_MODE
//   BELT_PITCH_MM, PULLEY_TEETH, SCREW_PITCH_MM
//   STEPS_PER_MM_BELT, STEPS_PER_MM_LEADSCREW

// *****************************************************************************
// Global Settings Instance
// *****************************************************************************

motion_settings_t motion_settings;

// *****************************************************************************
// Global Coordinate System Instance (GRBL v1.1f)
// *****************************************************************************

/**
 * @brief Work coordinate system offsets (G54-G59) - PERSISTENT
 *
 * These are stored in EEPROM and survive reset/power cycle.
 * Each WCS is an offset from machine coordinates.
 *
 * Formula: WPos = MPos - work_offsets[active_wcs] - g92_offset
 */
float work_offsets[6][NUM_AXES] = {{0.0f}}; /* 6 WCS (G54-G59) × 4 axes */

/**
 * @brief G28/G30 predefined positions - PERSISTENT
 *
 * Stored in machine coordinates, not offsets.
 */
float predefined_positions[2][NUM_AXES] = {{0.0f}}; /* 2 positions × 4 axes */

/**
 * @brief G92 temporary coordinate offset - NON-PERSISTENT
 *
 * Cleared on reset/power cycle.
 */
float g92_offset[NUM_AXES] = {0.0f}; /* 4 axes */

/**
 * @brief Active work coordinate system (0=G54, 1=G55, ..., 5=G59)
 */
uint8_t active_wcs = 0U; /* Default: G54 */

// *****************************************************************************
// Helper Functions (Static/Private)
// *****************************************************************************

/*! \brief Safe floating point comparison
 *
 *  \param a First value
 *  \param b Second value
 *  \return true if values within EPSILON tolerance
 */
static inline bool float_equals(float a, float b)
{
    return fabsf(a - b) < EPSILON;
}

/*! \brief Check if float is valid (not NaN or infinity)
 *
 *  \param value Value to check
 *  \return true if valid, false if NaN or infinity
 */
static inline bool float_is_valid(float value)
{
    return !isnan(value) && !isinf(value);
}

/*! \brief Clamp value to range
 *
 *  \param value Input value
 *  \param min Minimum allowed value
 *  \param max Maximum allowed value
 *  \return Clamped value
 */
static inline float clamp(float value, float min, float max)
{
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}

// *****************************************************************************
// Settings Management
// *****************************************************************************

void MotionMath_InitializeSettings(void)
{
    MotionMath_LoadDefaultSettings();
}

void MotionMath_LoadDefaultSettings(void)
{
    // $100-$103: Steps per mm (calculated from hardware configuration)
    //
    // Current configuration:
    //   1.8° stepper (200 steps/rev), 1/16 microstepping
    //
    //   Timing Belt (X/Y/A axes):
    //     Formula: (steps_per_rev * microsteps) / (pulley_teeth * belt_pitch_mm)
    //     Example with GT2 belt: (200 * 16) / (20 * 2) = 80 steps/mm
    //     Example with GT3 belt: (200 * 16) / (20 * 3) = 53.3 steps/mm
    //     Example with GT5 belt: (200 * 16) / (20 * 5) = 32 steps/mm
    //
    //   Leadscrew (Z axis):
    //     Formula: (steps_per_rev * microsteps) / screw_pitch_mm
    //     Example with 2.5mm pitch: (200 * 16) / 2.5 = 1280 steps/mm
    //     Example with 5mm pitch:   (200 * 16) / 5.0 = 640 steps/mm
    //     Example with 8mm pitch:   (200 * 16) / 8.0 = 400 steps/mm
    //
    // To change: Edit defines at top of file:
    //   STEPPER_STEPS_PER_REV, MICROSTEPPING_MODE
    //   BELT_PITCH_MM, PULLEY_TEETH (for belt drives)
    //   SCREW_PITCH_MM (for leadscrew drives)

    motion_settings.steps_per_mm[AXIS_X] = STEPS_PER_MM_BELT;      // Belt-driven (X axis)
    motion_settings.steps_per_mm[AXIS_Y] = STEPS_PER_MM_BELT;      // Belt-driven (Y axis)
    motion_settings.steps_per_mm[AXIS_Z] = STEPS_PER_MM_LEADSCREW; // Leadscrew-driven (Z axis)
    motion_settings.steps_per_mm[AXIS_A] = STEPS_PER_MM_BELT;      // Belt-driven (A axis) or configure as steps/degree

    // $110-$113: Max rate (mm/min) - Balanced for smooth accurate motion
    motion_settings.max_rate[AXIS_X] = 1000.0f;
    motion_settings.max_rate[AXIS_Y] = 1000.0f;
    motion_settings.max_rate[AXIS_Z] = 800.0f; // Z typically slower
    motion_settings.max_rate[AXIS_A] = 1000.0f;

    // $120-$123: Acceleration (mm/sec²) - Smooth and reliable
    motion_settings.acceleration[AXIS_X] = 100.0f;
    motion_settings.acceleration[AXIS_Y] = 100.0f;
    motion_settings.acceleration[AXIS_Z] = 50.0f; // Z typically slower
    motion_settings.acceleration[AXIS_A] = 100.0f;

    // $130-$133: Max travel (mm) - Machine-specific
    motion_settings.max_travel[AXIS_X] = 300.0f;
    motion_settings.max_travel[AXIS_Y] = 300.0f;
    motion_settings.max_travel[AXIS_Z] = 100.0f;
    motion_settings.max_travel[AXIS_A] = 360.0f; // Full rotation if rotary

    // $11: Junction deviation (mm) - Cornering tolerance
    // Smaller = sharper corners (slower), Larger = smoother corners (faster)
    motion_settings.junction_deviation = 0.01f; // 0.01mm = tight corners

    // $12: Arc tolerance (mm) - G2/G3 interpolation
    motion_settings.arc_tolerance = 0.002f; // 0.002mm = smooth arcs

    // Advanced parameters
    motion_settings.jerk_limit = 1000.0f;          // mm/sec³ - For S-curves (balanced smooth/responsive)
    motion_settings.minimum_planner_speed = 10.0f; // mm/min - Minimum junction speed
}

bool MotionMath_SetSetting(uint8_t setting_id, float value)
{
    // GRBL setting IDs: $100-$103 (steps/mm), $110-$113 (max rate), etc.

    if (value < 0.0f || !float_is_valid(value))
    {
        return false; // Reject negative or invalid values
    }

    // $100-$103: Steps per mm
    if (setting_id >= 100 && setting_id <= 103)
    {
        axis_id_t axis = (axis_id_t)(setting_id - 100);
        if (axis < NUM_AXES && value > 0.0f)
        {
            motion_settings.steps_per_mm[axis] = value;
            return true;
        }
    }

    // $110-$113: Max rate
    if (setting_id >= 110 && setting_id <= 113)
    {
        axis_id_t axis = (axis_id_t)(setting_id - 110);
        if (axis < NUM_AXES && value > 0.0f)
        {
            // Safety check: Ensure max_rate doesn't exceed DRV8825 250kHz limit
            // Max steps/sec = (max_rate_mm_min / 60) * steps_per_mm
            float steps_per_sec = (value / 60.0f) * motion_settings.steps_per_mm[axis];

            if (steps_per_sec > (float)DRV8825_MAX_STEP_RATE_HZ)
            {
                // Calculate safe max_rate that respects 250kHz limit
                float safe_max_rate = ((float)DRV8825_MAX_STEP_RATE_HZ / motion_settings.steps_per_mm[axis]) * 60.0f;
                motion_settings.max_rate[axis] = safe_max_rate;
                return false; // Indicate clamped value
            }

            motion_settings.max_rate[axis] = value;
            return true;
        }
    }

    // $120-$123: Acceleration
    if (setting_id >= 120 && setting_id <= 123)
    {
        axis_id_t axis = (axis_id_t)(setting_id - 120);
        if (axis < NUM_AXES && value > 0.0f)
        {
            motion_settings.acceleration[axis] = value;
            return true;
        }
    }

    // $130-$133: Max travel
    if (setting_id >= 130 && setting_id <= 133)
    {
        axis_id_t axis = (axis_id_t)(setting_id - 130);
        if (axis < NUM_AXES && value > 0.0f)
        {
            motion_settings.max_travel[axis] = value;
            return true;
        }
    }

    // $11: Junction deviation
    if (setting_id == 11)
    {
        if (value >= 0.001f && value <= 1.0f) // Reasonable range
        {
            motion_settings.junction_deviation = value;
            return true;
        }
    }

    // $12: Arc tolerance
    if (setting_id == 12)
    {
        if (value >= 0.001f && value <= 0.1f) // Reasonable range
        {
            motion_settings.arc_tolerance = value;
            return true;
        }
    }

    return false; // Unknown or invalid setting
}

float MotionMath_GetSetting(uint8_t setting_id)
{
    // $100-$103: Steps per mm
    if (setting_id >= 100 && setting_id <= 103)
    {
        axis_id_t axis = (axis_id_t)(setting_id - 100);
        return (axis < NUM_AXES) ? motion_settings.steps_per_mm[axis] : 0.0f;
    }

    // $110-$113: Max rate
    if (setting_id >= 110 && setting_id <= 113)
    {
        axis_id_t axis = (axis_id_t)(setting_id - 110);
        return (axis < NUM_AXES) ? motion_settings.max_rate[axis] : 0.0f;
    }

    // $120-$123: Acceleration
    if (setting_id >= 120 && setting_id <= 123)
    {
        axis_id_t axis = (axis_id_t)(setting_id - 120);
        return (axis < NUM_AXES) ? motion_settings.acceleration[axis] : 0.0f;
    }

    // $130-$133: Max travel
    if (setting_id >= 130 && setting_id <= 133)
    {
        axis_id_t axis = (axis_id_t)(setting_id - 130);
        return (axis < NUM_AXES) ? motion_settings.max_travel[axis] : 0.0f;
    }

    // $11: Junction deviation
    if (setting_id == 11)
    {
        return motion_settings.junction_deviation;
    }

    // $12: Arc tolerance
    if (setting_id == 12)
    {
        return motion_settings.arc_tolerance;
    }

    return 0.0f; // Unknown setting
}

float MotionMath_GetArcTolerance(void)
{
    return motion_settings.arc_tolerance;
}

// *****************************************************************************
// Unit Conversions
// *****************************************************************************

float MotionMath_MMToInch(float mm)
{
    return mm / MM_PER_INCH;
}

float MotionMath_InchToMM(float inch)
{
    return inch * MM_PER_INCH;
}

int32_t MotionMath_MMToSteps(float mm, axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES || !float_is_valid(mm))
    {
        return 0;
    }

    float steps = mm * motion_settings.steps_per_mm[axis];

    // Round to nearest integer (proper rounding, not truncation)
    return (int32_t)(steps + (steps >= 0.0f ? 0.5f : -0.5f));
}

float MotionMath_StepsToMM(int32_t steps, axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES)
    {
        return 0.0f;
    }

    float steps_per_mm = motion_settings.steps_per_mm[axis];

    if (float_equals(steps_per_mm, 0.0f))
    {
        return 0.0f; // Prevent divide by zero
    }

    return (float)steps / steps_per_mm;
}

// *****************************************************************************
// Feedrate and Velocity Calculations
// *****************************************************************************

float MotionMath_FeedrateToStepsPerSec(float feedrate_mm_min, axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES || !float_is_valid(feedrate_mm_min) || feedrate_mm_min < 0.0f)
    {
        return 0.0f;
    }

    // Convert mm/min → mm/sec → steps/sec
    float mm_per_sec = feedrate_mm_min / MIN_TO_SEC;
    float steps_per_sec = mm_per_sec * motion_settings.steps_per_mm[axis];

    return steps_per_sec;
}

uint32_t MotionMath_FeedrateToOCRPeriod(float feedrate_mm_min, axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES || !float_is_valid(feedrate_mm_min) || feedrate_mm_min <= 0.0f)
    {
        return OCR_MAX_PERIOD; // Slowest possible
    }

    float steps_per_sec = MotionMath_FeedrateToStepsPerSec(feedrate_mm_min, axis);

    if (steps_per_sec <= 0.0f)
    {
        return OCR_MAX_PERIOD;
    }

    // OCR period = Timer_Clock / Steps_Per_Sec
    // Example: 1.5625MHz / 100 steps/sec = 15,625 counts = 10ms per step
    // Example: 1.5625MHz / 1000 steps/sec = 1,563 counts = 1ms per step
    uint32_t period = (uint32_t)(TMR_CLOCK_HZ / steps_per_sec);

    // Clamp to hardware limits:
    // - Max period: 65,485 counts = 41.91ms @ 1.5625MHz (slowest: 23.8 steps/sec)
    // - Min period: 50 counts = 32µs @ 1.5625MHz (fastest: 31,250 steps/sec)
    if (period > OCR_MAX_PERIOD)
    {
        period = OCR_MAX_PERIOD;
    }
    else if (period < OCR_MIN_PERIOD) // Enforce hardware/driver limits
    {
        period = OCR_MIN_PERIOD; // Safety minimum
    }

    return period;
}

float MotionMath_CalculateStepInterval(float velocity_steps_sec)
{
    if (!float_is_valid(velocity_steps_sec) || velocity_steps_sec <= 0.0f)
    {
        return 0.0f;
    }

    // Interval (µs) = 1,000,000 / steps_per_sec
    return 1000000.0f / velocity_steps_sec;
}

// *****************************************************************************
// Time-Based Interpolation Support (for multiaxis_control.c)
// *****************************************************************************

/*! \brief Get maximum velocity in steps/sec for specified axis
 *
 *  Converts GRBL max_rate (mm/min) to steps/sec for S-curve calculations.
 *
 *  \param axis Axis identifier
 *  \return Maximum velocity in steps/sec
 */
float MotionMath_GetMaxVelocityStepsPerSec(axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES)
    {
        return 0.0f;
    }

    return MotionMath_FeedrateToStepsPerSec(motion_settings.max_rate[axis], axis);
}

/*! \brief Get acceleration in steps/sec² for specified axis
 *
 *  Converts GRBL acceleration (mm/sec²) to steps/sec² for S-curve calculations.
 *
 *  \param axis Axis identifier
 *  \return Acceleration in steps/sec²
 */
float MotionMath_GetAccelStepsPerSec2(axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES)
    {
        return 0.0f;
    }

    // Convert mm/sec² to steps/sec²
    return motion_settings.acceleration[axis] * motion_settings.steps_per_mm[axis];
}

/*! \brief Get jerk limit in steps/sec³ for specified axis
 *
 *  Converts GRBL jerk limit (mm/sec³) to steps/sec³ for S-curve calculations.
 *  Note: Jerk is global (not per-axis), but scales with steps_per_mm.
 *
 *  \param axis Axis identifier
 *  \return Jerk limit in steps/sec³
 */
float MotionMath_GetJerkStepsPerSec3(axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES)
    {
        return 0.0f;
    }

    // Convert mm/sec³ to steps/sec³
    return motion_settings.jerk_limit * motion_settings.steps_per_mm[axis];
}

// *****************************************************************************
// GRBL Planner Helper Functions (GRBL v1.1f compatibility)
// *****************************************************************************

float MotionMath_GetAccelMMPerSec2(axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES)
    {
        return 0.0f;
    }

    return motion_settings.acceleration[axis];
}

float MotionMath_GetMaxVelocityMMPerMin(axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES)
    {
        return 0.0f;
    }

    return motion_settings.max_rate[axis];
}

float MotionMath_GetJunctionDeviation(void)
{
    return motion_settings.junction_deviation;
}

float MotionMath_GetStepsPerMM(axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES)
    {
        return 1.0f; /* Avoid divide-by-zero */
    }

    return motion_settings.steps_per_mm[axis];
}

float MotionMath_CalculateMoveTime(float distance_mm, float feedrate_mm_min, float accel_mm_sec2)
{
    if (!float_is_valid(distance_mm) || !float_is_valid(feedrate_mm_min) ||
        !float_is_valid(accel_mm_sec2) || distance_mm <= 0.0f || feedrate_mm_min <= 0.0f || accel_mm_sec2 <= 0.0f)
    {
        return 0.0f;
    }

    float feedrate_mm_sec = feedrate_mm_min / MIN_TO_SEC;

    // Time to accelerate to feedrate
    float accel_time = feedrate_mm_sec / accel_mm_sec2;
    float accel_distance = 0.5f * accel_mm_sec2 * accel_time * accel_time;

    // If distance too short to reach full speed (triangular profile)
    if (accel_distance * 2.0f > distance_mm)
    {
        // Total time = 2 * sqrt(distance / accel)
        return 2.0f * sqrtf(distance_mm / accel_mm_sec2);
    }

    // Trapezoidal profile: accel + cruise + decel
    float cruise_distance = distance_mm - (accel_distance * 2.0f);
    float cruise_time = cruise_distance / feedrate_mm_sec;

    return (accel_time * 2.0f) + cruise_time;
}

// *****************************************************************************
// Vector Mathematics
// *****************************************************************************

float MotionMath_VectorLength(float dx, float dy, float dz)
{
    if (!float_is_valid(dx) || !float_is_valid(dy) || !float_is_valid(dz))
    {
        return 0.0f;
    }

    return sqrtf(dx * dx + dy * dy + dz * dz);
}

void MotionMath_VectorNormalize(float *x, float *y, float *z)
{
    assert(x != NULL && y != NULL && z != NULL);

    if (x == NULL || y == NULL || z == NULL)
    {
        return;
    }

    float length = MotionMath_VectorLength(*x, *y, *z);

    if (float_equals(length, 0.0f))
    {
        *x = 0.0f;
        *y = 0.0f;
        *z = 0.0f;
        return;
    }

    *x /= length;
    *y /= length;
    *z /= length;
}

float MotionMath_CalculateBlockLength(float dx, float dy, float dz)
{
    return MotionMath_VectorLength(dx, dy, dz);
}

void MotionMath_CalculateCoordinatedMove(int32_t steps[NUM_AXES], motion_coordinated_move_t *coord)
{
    assert(steps != NULL && coord != NULL);

    if (steps == NULL || coord == NULL)
    {
        return;
    }

    // Find dominant axis (largest absolute step count)
    int32_t max_steps = 0;
    coord->dominant_axis = AXIS_X;

    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        int32_t abs_steps = (steps[axis] < 0) ? -steps[axis] : steps[axis];

        if (abs_steps > max_steps)
        {
            max_steps = abs_steps;
            coord->dominant_axis = axis;
        }
    }

    // Calculate speed ratios relative to dominant axis
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        if (max_steps == 0)
        {
            coord->axis_ratios[axis] = 0.0f;
        }
        else
        {
            int32_t abs_steps = (steps[axis] < 0) ? -steps[axis] : steps[axis];
            coord->axis_ratios[axis] = (float)abs_steps / (float)max_steps;
        }
    }

    // Calculate total Euclidean distance (in mm)
    float dx = MotionMath_StepsToMM(steps[AXIS_X], AXIS_X);
    float dy = MotionMath_StepsToMM(steps[AXIS_Y], AXIS_Y);
    float dz = MotionMath_StepsToMM(steps[AXIS_Z], AXIS_Z);

    coord->total_distance = MotionMath_VectorLength(dx, dy, dz);
}

// *****************************************************************************
// Look-Ahead Planner Support
// *****************************************************************************

float MotionMath_CalculateJunctionAngle(
    float prev_dx, float prev_dy, float prev_dz,
    float next_dx, float next_dy, float next_dz)
{
    // Normalize both vectors
    float prev_len = MotionMath_VectorLength(prev_dx, prev_dy, prev_dz);
    float next_len = MotionMath_VectorLength(next_dx, next_dy, next_dz);

    if (float_equals(prev_len, 0.0f) || float_equals(next_len, 0.0f))
    {
        return PI; // 180° = full stop required
    }

    // Unit vectors
    float prev_ux = prev_dx / prev_len;
    float prev_uy = prev_dy / prev_len;
    float prev_uz = prev_dz / prev_len;

    float next_ux = next_dx / next_len;
    float next_uy = next_dy / next_len;
    float next_uz = next_dz / next_len;

    // Dot product = cos(angle)
    float dot = (prev_ux * next_ux) + (prev_uy * next_uy) + (prev_uz * next_uz);

    // Clamp to [-1, 1] to handle floating point errors
    dot = clamp(dot, -1.0f, 1.0f);

    // acos(dot) = angle in radians
    return acosf(dot);
}

float MotionMath_CalculateJunctionVelocity(
    float entry_angle_rad,
    float feedrate1_mm_min,
    float feedrate2_mm_min,
    float junction_deviation_mm)
{
    if (!float_is_valid(entry_angle_rad) || !float_is_valid(feedrate1_mm_min) ||
        !float_is_valid(feedrate2_mm_min) || !float_is_valid(junction_deviation_mm))
    {
        return 0.0f;
    }

    // Clamp to reasonable range
    entry_angle_rad = clamp(entry_angle_rad, 0.0f, PI);

    // Junction velocity calculation (GRBL formula)
    // Based on centripetal acceleration at corner
    // v_junction = sqrt(R * a_max)
    // where R = junction_deviation / (1 - cos(angle))

    float sin_half_angle = sinf(entry_angle_rad * 0.5f);

    if (float_equals(sin_half_angle, 0.0f))
    {
        // 0° angle = straight line, no speed limit
        return fminf(feedrate1_mm_min, feedrate2_mm_min);
    }

    // Assume acceleration limit (use minimum of both blocks)
    // TODO: This should use actual block acceleration settings
    float accel_mm_sec2 = 500.0f; // Default conservative value

    // R = deviation / (1 - cos(angle)) = deviation / (2 * sin²(angle/2))
    float radius = junction_deviation_mm / (2.0f * sin_half_angle * sin_half_angle);

    // v = sqrt(R * a) in mm/sec, convert to mm/min
    float junction_vel_mm_sec = sqrtf(radius * accel_mm_sec2);
    float junction_vel_mm_min = junction_vel_mm_sec * MIN_TO_SEC;

    // Clamp to minimum of both feedrates
    float max_junction = fminf(feedrate1_mm_min, feedrate2_mm_min);

    return fminf(junction_vel_mm_min, max_junction);
}

float MotionMath_CalculateMaxEntryVelocity(float distance_mm, float exit_vel, float accel)
{
    if (!float_is_valid(distance_mm) || !float_is_valid(exit_vel) ||
        !float_is_valid(accel) || distance_mm <= 0.0f || accel <= 0.0f)
    {
        return 0.0f;
    }

    // v_entry² = v_exit² + 2 * a * d
    // Convert mm/min to mm/sec for calculation
    float exit_vel_mm_sec = exit_vel / MIN_TO_SEC;
    float v_entry_squared = (exit_vel_mm_sec * exit_vel_mm_sec) + (2.0f * accel * distance_mm);

    if (v_entry_squared < 0.0f)
    {
        return 0.0f;
    }

    float entry_vel_mm_sec = sqrtf(v_entry_squared);
    return entry_vel_mm_sec * MIN_TO_SEC; // Convert back to mm/min
}

float MotionMath_CalculateMaxExitVelocity(float distance_mm, float entry_vel, float accel)
{
    if (!float_is_valid(distance_mm) || !float_is_valid(entry_vel) ||
        !float_is_valid(accel) || distance_mm <= 0.0f || accel <= 0.0f)
    {
        return 0.0f;
    }

    // v_exit² = v_entry² + 2 * a * d
    // Convert mm/min to mm/sec for calculation
    float entry_vel_mm_sec = entry_vel / MIN_TO_SEC;
    float v_exit_squared = (entry_vel_mm_sec * entry_vel_mm_sec) + (2.0f * accel * distance_mm);

    if (v_exit_squared < 0.0f)
    {
        return 0.0f;
    }

    float exit_vel_mm_sec = sqrtf(v_exit_squared);
    return exit_vel_mm_sec * MIN_TO_SEC; // Convert back to mm/min
}

bool MotionMath_CalculateVelocityProfile(
    float distance_mm,
    float entry_velocity_mm_min,
    float exit_velocity_mm_min,
    float max_velocity_mm_min,
    float acceleration_mm_sec2,
    velocity_profile_t *profile)
{
    assert(profile != NULL);

    if (profile == NULL || !float_is_valid(distance_mm) || distance_mm <= 0.0f ||
        !float_is_valid(entry_velocity_mm_min) || !float_is_valid(exit_velocity_mm_min) ||
        !float_is_valid(max_velocity_mm_min) || !float_is_valid(acceleration_mm_sec2) ||
        acceleration_mm_sec2 <= 0.0f)
    {
        return false;
    }

    // TODO: Implement full trapezoidal profile calculation
    // For now, return simplified profile

    profile->entry_velocity = entry_velocity_mm_min;
    profile->exit_velocity = exit_velocity_mm_min;
    profile->peak_velocity = max_velocity_mm_min;
    profile->accel_distance = 0.0f;
    profile->cruise_distance = distance_mm;
    profile->decel_distance = 0.0f;
    profile->total_time = MotionMath_CalculateMoveTime(distance_mm, max_velocity_mm_min, acceleration_mm_sec2);

    return true;
}

// *****************************************************************************
// S-Curve Trajectory Planning
// *****************************************************************************

float MotionMath_CalculateCruiseVelocity(float distance, float max_vel, float accel, float jerk)
{
    // TODO: Implement full S-curve cruise velocity calculation
    // For now, return max velocity (simple trapezoidal assumption)

    if (!float_is_valid(distance) || !float_is_valid(max_vel) ||
        !float_is_valid(accel) || !float_is_valid(jerk))
    {
        return 0.0f;
    }

    return max_vel;
}

float MotionMath_CalculateSegmentTime(float velocity_delta, float accel, float jerk)
{
    // TODO: Implement S-curve segment time calculation

    if (!float_is_valid(velocity_delta) || !float_is_valid(accel) ||
        !float_is_valid(jerk) || jerk <= 0.0f)
    {
        return 0.0f;
    }

    // Simplified: t = velocity_delta / accel
    return fabsf(velocity_delta) / accel;
}

bool MotionMath_CalculateSCurveTiming(
    float distance_mm,
    float entry_velocity_mm_min,
    float exit_velocity_mm_min,
    float max_velocity_mm_min,
    float acceleration_mm_sec2,
    float jerk_mm_sec3,
    scurve_timing_t *timing)
{
    assert(timing != NULL);

    if (timing == NULL)
    {
        return false;
    }

    // TODO: Implement full 7-segment S-curve calculation
    // This is complex and requires careful math - placeholder for now

    timing->entry_velocity = entry_velocity_mm_min;
    timing->exit_velocity = exit_velocity_mm_min;
    timing->peak_velocity = max_velocity_mm_min;
    timing->t1_jerk_accel = 0.0f;
    timing->t2_const_accel = 0.0f;
    timing->t3_jerk_decel_accel = 0.0f;
    timing->t4_cruise = 0.0f;
    timing->t5_jerk_accel_decel = 0.0f;
    timing->t6_const_decel = 0.0f;
    timing->t7_jerk_decel_decel = 0.0f;

    return true;
}

// *****************************************************************************
// Look-Ahead Buffer Planning
// *****************************************************************************

void MotionMath_PlannerForwardPass(
    velocity_profile_t *blocks,
    uint8_t block_count,
    float junction_deviation_mm)
{
    assert(blocks != NULL);

    if (blocks == NULL || block_count == 0)
    {
        return;
    }

    // TODO: Implement forward pass algorithm
    // Propagates maximum achievable velocities forward through buffer
}

void MotionMath_PlannerReversePass(
    velocity_profile_t *blocks,
    uint8_t block_count)
{
    assert(blocks != NULL);

    if (blocks == NULL || block_count == 0)
    {
        return;
    }

    // TODO: Implement reverse pass algorithm
    // Propagates deceleration constraints backward through buffer
}

// *****************************************************************************
// Coordinate System Conversions (GRBL v1.1f)
// *****************************************************************************

float MotionMath_WorkToMachine(float work_pos, axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES)
    {
        return 0.0f; /* Defensive: invalid axis */
    }

    /* Formula: MPos = WPos + work_offset + g92_offset */
    float machine_pos = work_pos + work_offsets[active_wcs][axis] + g92_offset[axis];

    return machine_pos;
}

float MotionMath_MachineToWork(float machine_pos, axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES)
    {
        return 0.0f; /* Defensive: invalid axis */
    }

    /* Formula: WPos = MPos - work_offset - g92_offset */
    float work_pos = machine_pos - work_offsets[active_wcs][axis] - g92_offset[axis];

    return work_pos;
}

float MotionMath_GetMachinePosition(axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES)
    {
        return 0.0f; /* Defensive: invalid axis */
    }

    /* Get current position in steps from multiaxis_control */
    uint32_t steps = MultiAxis_GetStepCount(axis);

    /* Convert steps to mm */
    float machine_pos_mm = MotionMath_StepsToMM((int32_t)steps, axis);

    return machine_pos_mm;
}

float MotionMath_GetWorkPosition(axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES)
    {
        return 0.0f; /* Defensive: invalid axis */
    }

    /* Get machine position and convert to work coordinates */
    float machine_pos = MotionMath_GetMachinePosition(axis);
    float work_pos = MotionMath_MachineToWork(machine_pos, axis);

    return work_pos;
}

void MotionMath_SetActiveWCS(uint8_t wcs_number)
{
    assert(wcs_number < 6U); /* Must be 0-5 for G54-G59 */

    if (wcs_number < 6U)
    {
        active_wcs = wcs_number;
    }
}

uint8_t MotionMath_GetActiveWCS(void)
{
    return active_wcs;
}

void MotionMath_SetWorkOffset(uint8_t wcs_number, const float offsets[NUM_AXES])
{
    assert(wcs_number < 6U);
    assert(offsets != NULL);

    if (wcs_number >= 6U || offsets == NULL)
    {
        return; /* Invalid parameters */
    }

    /* Copy offsets for specified WCS */
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        work_offsets[wcs_number][axis] = offsets[axis];
    }
}

float MotionMath_GetWorkOffset(uint8_t wcs_number, axis_id_t axis)
{
    assert(wcs_number < 6U);
    assert(axis < NUM_AXES);

    if (wcs_number >= 6U || axis >= NUM_AXES)
    {
        return 0.0f; /* Invalid parameters */
    }

    return work_offsets[wcs_number][axis];
}

void MotionMath_SetG92Offset(const float offsets[NUM_AXES])
{
    assert(offsets != NULL);

    if (offsets == NULL)
    {
        return;
    }

    /* Copy G92 offsets */
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        g92_offset[axis] = offsets[axis];
    }
}

void MotionMath_ClearG92Offset(void)
{
    /* Clear all G92 offsets (G92.1 command) */
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        g92_offset[axis] = 0.0f;
    }
}

void MotionMath_SetPredefinedPosition(uint8_t position_index, const float positions[NUM_AXES])
{
    assert(position_index < 2U); /* Must be 0 or 1 (G28 or G30) */
    assert(positions != NULL);

    if (position_index >= 2U || positions == NULL)
    {
        return; /* Invalid parameters */
    }

    /* Copy predefined position in machine coordinates */
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        predefined_positions[position_index][axis] = positions[axis];
    }
}

float MotionMath_GetPredefinedPosition(uint8_t position_index, axis_id_t axis)
{
    assert(position_index < 2U);
    assert(axis < NUM_AXES);

    if (position_index >= 2U || axis >= NUM_AXES)
    {
        return 0.0f; /* Invalid parameters */
    }

    return predefined_positions[position_index][axis];
}

void MotionMath_PrintCoordinateParameters(void)
{
    /* Need to include ugs_interface.h for UGS_Print */
    extern void UGS_Print(const char *str);

    char buffer[80];

    /* Print G54-G59 work coordinate offsets */
    for (uint8_t i = 0U; i < 6U; i++)
    {
        (void)snprintf(buffer, sizeof(buffer), "[G%d:%.3f,%.3f,%.3f,%.3f]\r\n",
                       54 + (int)i,
                       (double)work_offsets[i][AXIS_X],
                       (double)work_offsets[i][AXIS_Y],
                       (double)work_offsets[i][AXIS_Z],
                       (double)work_offsets[i][AXIS_A]);
        UGS_Print(buffer);
    }

    /* Print G28 predefined position */
    (void)snprintf(buffer, sizeof(buffer), "[G28:%.3f,%.3f,%.3f,%.3f]\r\n",
                   (double)predefined_positions[0][AXIS_X],
                   (double)predefined_positions[0][AXIS_Y],
                   (double)predefined_positions[0][AXIS_Z],
                   (double)predefined_positions[0][AXIS_A]);
    UGS_Print(buffer);

    /* Print G30 predefined position */
    (void)snprintf(buffer, sizeof(buffer), "[G30:%.3f,%.3f,%.3f,%.3f]\r\n",
                   (double)predefined_positions[1][AXIS_X],
                   (double)predefined_positions[1][AXIS_Y],
                   (double)predefined_positions[1][AXIS_Z],
                   (double)predefined_positions[1][AXIS_A]);
    UGS_Print(buffer);

    /* Print G92 offset */
    (void)snprintf(buffer, sizeof(buffer), "[G92:%.3f,%.3f,%.3f,%.3f]\r\n",
                   (double)g92_offset[AXIS_X],
                   (double)g92_offset[AXIS_Y],
                   (double)g92_offset[AXIS_Z],
                   (double)g92_offset[AXIS_A]);
    UGS_Print(buffer);

    /* Print tool length offset (TLO) - not implemented yet */
    (void)snprintf(buffer, sizeof(buffer), "[TLO:0.000]\r\n");
    UGS_Print(buffer);

    /* Print probe result (PRB) - not implemented yet */
    (void)snprintf(buffer, sizeof(buffer), "[PRB:0.000,0.000,0.000,0.000:0]\r\n");
    UGS_Print(buffer);
}
