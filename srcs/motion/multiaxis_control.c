/*******************************************************************************
  Multi-Axis S-Curve Motion Control

  Coordinates X, Y, Z axes using individual S-curve profiles per axis.
  Hardware mapping:
    X-axis: OCMP4 + TMR2
    Y-axis: OCMP1 + TMR4
    Z-axis: OCMP5 + TMR3

  MISRA C:2012 Compliance:
    - Static assertions for compile-time validation
    - Runtime parameter validation with bounds checking
    - Const-qualified lookup tables
    - No dynamic memory allocation
    - Fixed-width integer types
*******************************************************************************/

#include "motion/multiaxis_control.h"
#include "config/default/peripheral/gpio/plib_gpio.h"

#include "definitions.h"
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>

// *****************************************************************************
// MISRA C Compile-Time Assertions
// *****************************************************************************

// Static assertion macro (C11 compatible)
#define STATIC_ASSERT(condition, message) \
    typedef char static_assert_##message[(condition) ? 1 : -1]

// Verify enum values match array indices
STATIC_ASSERT(AXIS_X == 0, axis_x_must_be_zero);
STATIC_ASSERT(AXIS_Y == 1, axis_y_must_be_one);
STATIC_ASSERT(AXIS_Z == 2, axis_z_must_be_two);
STATIC_ASSERT(AXIS_A == 3, axis_a_must_be_three);
// Verify array sizing
STATIC_ASSERT(NUM_AXES == 4, num_axes_must_be_four);

// *****************************************************************************
// Hardware Configuration
// *****************************************************************************/

// OCR/Timer assignments per PIC32MZ hardware
typedef struct
{
    void (*OCMP_Enable)(void);
    void (*OCMP_Disable)(void);
    void (*OCMP_CompareValueSet)(uint16_t);
    void (*OCMP_CompareSecondaryValueSet)(uint16_t);
    void (*OCMP_CallbackRegister)(void (*)(uintptr_t), uintptr_t);
    void (*TMR_Start)(void);
    void (*TMR_Stop)(void);
    void (*TMR_PeriodSet)(uint16_t);
} axis_hardware_t;

// Forward declarations for hardware functions
static void OCMP4_StepCounter_X(uintptr_t context);
static void OCMP1_StepCounter_Y(uintptr_t context);
static void OCMP5_StepCounter_Z(uintptr_t context);
#ifdef ENABLE_AXIS_A
static void OCMP3_StepCounter_A(uintptr_t context);
#endif
// Hardware configuration table
static const axis_hardware_t axis_hw[NUM_AXES] = {
    // AXIS_X: OCMP4 + TMR2 (TESTED - working)
    {
        .OCMP_Enable = OCMP4_Enable,
        .OCMP_Disable = OCMP4_Disable,
        .OCMP_CompareValueSet = OCMP4_CompareValueSet,
        .OCMP_CompareSecondaryValueSet = OCMP4_CompareSecondaryValueSet,
        .OCMP_CallbackRegister = OCMP4_CallbackRegister,
        .TMR_Start = TMR2_Start,
        .TMR_Stop = TMR2_Stop,
        .TMR_PeriodSet = TMR2_PeriodSet},
    // AXIS_Y: OCMP1 + TMR4 (using X hardware for now until wired)
    {
        .OCMP_Enable = OCMP1_Enable,
        .OCMP_Disable = OCMP1_Disable,
        .OCMP_CompareValueSet = OCMP1_CompareValueSet,
        .OCMP_CompareSecondaryValueSet = OCMP1_CompareSecondaryValueSet,
        .OCMP_CallbackRegister = OCMP1_CallbackRegister,
        .TMR_Start = TMR4_Start,
        .TMR_Stop = TMR4_Stop,
        .TMR_PeriodSet = TMR4_PeriodSet},
    // AXIS_Z: OCMP5 + TMR3 (using X hardware for now until wired)
    {
        .OCMP_Enable = OCMP5_Enable,
        .OCMP_Disable = OCMP5_Disable,
        .OCMP_CompareValueSet = OCMP5_CompareValueSet,
        .OCMP_CompareSecondaryValueSet = OCMP5_CompareSecondaryValueSet,
        .OCMP_CallbackRegister = OCMP5_CallbackRegister,
        .TMR_Start = TMR3_Start,
        .TMR_Stop = TMR3_Stop,
        .TMR_PeriodSet = TMR3_PeriodSet},
    // AXIS_A: OCMP3 + TMR1 (using X hardware for now until wired)
    {
        .OCMP_Enable = OCMP3_Enable,
        .OCMP_Disable = OCMP3_Disable,
        .OCMP_CompareValueSet = OCMP3_CompareValueSet,
        .OCMP_CompareSecondaryValueSet = OCMP3_CompareSecondaryValueSet,
        .OCMP_CallbackRegister = OCMP3_CallbackRegister,
        .TMR_Start = TMR5_Start,
        .TMR_Stop = TMR5_Stop,
        .TMR_PeriodSet = TMR5_PeriodSet}};

// ******************************************************************************
// Drive Control Pin Macros (Dynamic Function Pointer Lookup)
// *****************************************************************************/
// Wrapper functions for direction pin macros (macros can't be used as function pointers)
static inline void enx_set_wrapper(void) { EnX_Set(); }
static inline void enx_clear_wrapper(void) { EnX_Clear(); }
static inline bool enx_get_wrapper(void) { return EnX_Get(); }
static inline void eny_set_wrapper(void) { EnY_Set(); }
static inline void eny_clear_wrapper(void) { EnY_Clear(); }
static inline bool eny_get_wrapper(void) { return EnY_Get(); }
static inline void enz_set_wrapper(void) { EnZ_Set(); }
static inline void enz_clear_wrapper(void) { EnZ_Clear(); }
static inline bool enz_get_wrapper(void) { return EnZ_Get(); }
#ifdef ENABLE_AXIS_A
static inline void ena_set_wrapper(void) { EnA_Set(); }
static inline void ena_clear_wrapper(void) { EnA_Clear(); }
static inline bool ena_get_wrapper(void) { return EnA_Get(); }
#endif

static void (*const en_set_funcs[NUM_AXES])(void) = {
    enx_set_wrapper, // AXIS_X
    eny_set_wrapper, // AXIS_Y
    enz_set_wrapper, // AXIS_Z
#ifdef ENABLE_AXIS_A
    ena_set_wrapper, // AXIS_A
#endif
};

static void (*const en_clear_funcs[NUM_AXES])(void) = {
    enx_clear_wrapper, // AXIS_X
    eny_clear_wrapper, // AXIS_Y
    enz_clear_wrapper, // AXIS_Z
#ifdef ENABLE_AXIS_A
    ena_clear_wrapper, // AXIS_A
#endif
};

static bool (*const en_get_funcs[NUM_AXES])(void) = {
    enx_get_wrapper, // AXIS_X
    eny_get_wrapper, // AXIS_Y
    enz_get_wrapper, // AXIS_Z
#ifdef ENABLE_AXIS_A
    ena_get_wrapper, // AXIS_A
#endif
};

// *****************************************************************************
// Driver Enable State Tracking
// *****************************************************************************
static volatile bool driver_enabled[NUM_AXES] = {false, false, false, false};

/*! \brief Enable stepper driver for specified axis using dynamic lookup
 *
 *  DRV8825 ENABLE pin is ACTIVE LOW:
 *    - LOW = Driver enabled (motor powered, torque applied)
 *    - HIGH = Driver disabled (motor unpowered, high-Z state)
 *
 *  This function makes the ENABLE pin LOW to activate the driver.
 *
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 *  MISRA Rule 1.3: Validate function pointer before call
 */
void MultiAxis_EnableDriver(axis_id_t axis)
{
    assert(axis < NUM_AXES); // Development-time check

    if (axis >= NUM_AXES || en_clear_funcs[axis] == NULL)
    {
        return; // Production safety check
    }

    if (!driver_enabled[axis]) // Only enable if currently disabled
    {
        en_clear_funcs[axis](); // ACTIVE LOW: Clear = Enable (pin goes LOW)
        driver_enabled[axis] = true;
    }
}

/*! \brief Disable stepper driver for specified axis using dynamic lookup
 *
 *  DRV8825 ENABLE pin is ACTIVE LOW:
 *    - LOW = Driver enabled (motor powered, torque applied)
 *    - HIGH = Driver disabled (motor unpowered, high-Z state)
 *
 *  This function makes the ENABLE pin HIGH to deactivate the driver.
 *
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 *  MISRA Rule 1.3: Validate function pointer before call
 */
void MultiAxis_DisableDriver(axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES || en_set_funcs[axis] == NULL)
    {
        return;
    }

    if (driver_enabled[axis]) // Only disable if currently enabled
    {
        en_set_funcs[axis](); // ACTIVE LOW: Set = Disable (pin goes HIGH)
        driver_enabled[axis] = false;
    }
}

/*! \brief Check if stepper driver is enabled for specified axis
 *
 *  Returns the software-tracked enable state (fast, no GPIO read).
 *
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 *  \return true if driver is enabled, false if disabled (LOGICAL STATE)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 */
bool MultiAxis_IsDriverEnabled(axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES)
        return false;

    return driver_enabled[axis];
}

/*! \brief Read enable pin GPIO state for specified axis
 *
 *  Reads the actual hardware pin state (for debugging/verification).
 *
 *  ⚠️ Returns RAW electrical state, not logical enabled/disabled!
 *  DRV8825 ENABLE is ACTIVE LOW:
 *    - Returns false (LOW) when driver is ENABLED  ✅
 *    - Returns true (HIGH) when driver is DISABLED ❌
 *
 *  For logical state, use MultiAxis_IsDriverEnabled() instead.
 *
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 *  \return Raw GPIO pin state (true = HIGH, false = LOW)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 *  MISRA Rule 1.3: Validate function pointer before call
 */

bool MultiAxis_ReadEnablePin(axis_id_t axis)
{
    // Defensive programming: validate parameters
    assert(axis < NUM_AXES);            // Development-time check
    assert(en_get_funcs[axis] != NULL); // Verify function pointer valid

    if ((axis < NUM_AXES) && (en_get_funcs[axis] != NULL))
    {
        return en_get_funcs[axis]();
    }

    return false; // Default return value
}

// *****************************************************************************
// Direction pin control Pin Macros (Dynamic Function Pointer Lookup)
// *****************************************************************************/
// *****************************************************************************/
// Wrapper functions for direction pin macros (macros can't be used as function pointers)
static inline void dirx_set_wrapper(void) { DirX_Set(); }
static inline void dirx_clear_wrapper(void) { DirX_Clear(); }
static inline void diry_set_wrapper(void) { DirY_Set(); }
static inline void diry_clear_wrapper(void) { DirY_Clear(); }
static inline void dirz_set_wrapper(void) { DirZ_Set(); }
static inline void dirz_clear_wrapper(void) { DirZ_Clear(); }
#ifdef ENABLE_AXIS_A
static inline void dira_set_wrapper(void) { DirA_Set(); }
static inline void dira_clear_wrapper(void) { DirA_Clear(); }
#endif

// Direction pin control function pointer tables
static void (*const dir_set_funcs[NUM_AXES])(void) = {
    dirx_set_wrapper, // AXIS_X
    diry_set_wrapper, // AXIS_Y
    dirz_set_wrapper, // AXIS_Z
#ifdef ENABLE_AXIS_A
    dira_set_wrapper, // AXIS_A
#endif
};

static void (*const dir_clear_funcs[NUM_AXES])(void) = {
    dirx_clear_wrapper, // AXIS_X
    diry_clear_wrapper, // AXIS_Y
    dirz_clear_wrapper, // AXIS_Z
#ifdef ENABLE_AXIS_A
    dira_clear_wrapper, // AXIS_A
#endif
};

/*! \brief Set direction for specified axis using dynamic lookup
 *
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 *  MISRA Rule 1.3: Validate function pointer before call
 */
void MultiAxis_SetDirection(axis_id_t axis)
{
    // Defensive programming: validate parameters
    assert(axis < NUM_AXES);             // Development-time check
    assert(dir_set_funcs[axis] != NULL); // Verify function pointer valid

    if ((axis < NUM_AXES) && (dir_set_funcs[axis] != NULL))
    {
        dir_set_funcs[axis]();
    }
}

/*! \brief Clear direction for specified axis using dynamic lookup
 *
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 *  MISRA Rule 1.3: Validate function pointer before call
 */
void MultiAxis_ClearDirection(axis_id_t axis)
{
    // Defensive programming: validate parameters
    assert(axis < NUM_AXES);               // Development-time check
    assert(dir_clear_funcs[axis] != NULL); // Verify function pointer valid

    if ((axis < NUM_AXES) && (dir_clear_funcs[axis] != NULL))
    {
        dir_clear_funcs[axis]();
    }
}

// *****************************************************************************
// S-Curve Motion Parameters (shared across axes)
// *****************************************************************************/

static float max_velocity = 5000.0f; // steps/sec
static float max_accel = 10000.0f;   // steps/sec²
static float max_jerk = 50000.0f;    // steps/sec³

#define OCMP_PULSE_WIDTH 40
#define UPDATE_FREQ_HZ 1000.0f
#define UPDATE_PERIOD_SEC (1.0f / UPDATE_FREQ_HZ)

// *****************************************************************************
// S-Curve State (per axis)
// *****************************************************************************/

typedef enum
{
    SEGMENT_IDLE = 0,
    SEGMENT_JERK_ACCEL,
    SEGMENT_CONST_ACCEL,
    SEGMENT_JERK_DECEL_ACCEL,
    SEGMENT_CRUISE,
    SEGMENT_JERK_ACCEL_DECEL,
    SEGMENT_CONST_DECEL,
    SEGMENT_JERK_DECEL_DECEL,
    SEGMENT_COMPLETE
} scurve_segment_t;

typedef struct
{
    scurve_segment_t current_segment;
    float elapsed_time;
    float total_elapsed;

    // Segment times
    float t1_jerk_accel;
    float t2_const_accel;
    float t3_jerk_decel_accel;
    float t4_cruise;
    float t5_jerk_accel_decel;
    float t6_const_decel;
    float t7_jerk_decel_decel;

    // Motion state
    float current_velocity;
    float current_accel;
    float cruise_velocity;

    // Velocity milestones
    float v_end_segment1;
    float v_end_segment2;
    float v_end_segment3;
    float v_end_segment5;
    float v_end_segment6;

    // Step tracking
    volatile uint32_t step_count;
    uint32_t total_steps;
    bool direction_forward;
    bool active;

} scurve_state_t;

// Per-axis state (accessed from main code AND TMR1 interrupt @ 1kHz)
static volatile scurve_state_t axis_state[NUM_AXES];

// *****************************************************************************
// Math Helpers
// *****************************************************************************

static float cbrt_approx(float x)
{
    if (x == 0.0f)
        return 0.0f;

    float sign = (x < 0.0f) ? -1.0f : 1.0f;
    x = fabsf(x);

    float guess = expf(logf(x) / 3.0f);

    for (int i = 0; i < 3; i++)
    {
        guess = (2.0f * guess + x / (guess * guess)) / 3.0f;
    }

    return sign * guess;
}

// *****************************************************************************
// S-Curve Profile Calculation
// *****************************************************************************/

static bool calculate_scurve_profile(axis_id_t axis, uint32_t distance)
{
    volatile scurve_state_t *s = &axis_state[axis];
    float d_total = (float)distance;

    float t_jerk = max_accel / max_jerk;
    float v_jerk = 0.5f * max_accel * t_jerk;
    float d_jerk = (1.0f / 6.0f) * max_jerk * t_jerk * t_jerk * t_jerk;

    float v_between_jerks = max_velocity - 2.0f * v_jerk;
    float d_const_accel = 0.0f;

    if (v_between_jerks > 0.0f)
    {
        d_const_accel = v_between_jerks * v_between_jerks / (2.0f * max_accel);
    }

    float d_accel_total = 2.0f * d_jerk + d_const_accel;
    float d_decel_total = d_accel_total;

    if (d_total >= d_accel_total + d_decel_total)
    {
        // LONG MOVE - reach max velocity
        s->cruise_velocity = max_velocity;

        s->t1_jerk_accel = t_jerk;
        s->t3_jerk_decel_accel = t_jerk;
        s->t5_jerk_accel_decel = t_jerk;
        s->t7_jerk_decel_decel = t_jerk;

        if (v_between_jerks > 0.0f)
        {
            s->t2_const_accel = v_between_jerks / max_accel;
            s->t6_const_decel = s->t2_const_accel;
        }
        else
        {
            s->t2_const_accel = 0.0f;
            s->t6_const_decel = 0.0f;
        }

        float d_cruise = d_total - d_accel_total - d_decel_total;
        s->t4_cruise = d_cruise / s->cruise_velocity;

        s->v_end_segment1 = v_jerk;
        s->v_end_segment2 = s->v_end_segment1 + max_accel * s->t2_const_accel;
        s->v_end_segment3 = s->cruise_velocity;

        // Deceleration milestones
        s->v_end_segment5 = s->cruise_velocity - v_jerk;
        s->v_end_segment6 = s->v_end_segment5 - max_accel * s->t6_const_decel;
    }
    else
    {
        // SHORT MOVE
        if (d_total <= 4.0f * d_jerk)
        {
            // VERY SHORT - only jerk segments
            float t_jerk_reduced = cbrt_approx(d_total / (4.0f * (1.0f / 6.0f) * max_jerk));
            s->cruise_velocity = 0.5f * max_jerk * t_jerk_reduced * t_jerk_reduced;

            s->t1_jerk_accel = t_jerk_reduced;
            s->t2_const_accel = 0.0f;
            s->t3_jerk_decel_accel = t_jerk_reduced;
            s->t4_cruise = 0.0f;
            s->t5_jerk_accel_decel = t_jerk_reduced;
            s->t6_const_decel = 0.0f;
            s->t7_jerk_decel_decel = t_jerk_reduced;

            s->v_end_segment1 = 0.5f * max_jerk * t_jerk_reduced * t_jerk_reduced;
            s->v_end_segment2 = s->v_end_segment1;
            s->v_end_segment3 = s->cruise_velocity;

            s->v_end_segment5 = s->v_end_segment1;
            s->v_end_segment6 = 0.0f;
        }
        else
        {
            // MEDIUM - jerk + reduced constant accel
            float d_remaining = d_total - 4.0f * d_jerk;

            float a = max_accel;
            float b = 4.0f * v_jerk;
            float c = -d_remaining;

            float discriminant = b * b - 4.0f * a * c;
            if (discriminant < 0.0f)
                discriminant = 0.0f;

            float t_const = (-b + sqrtf(discriminant)) / (2.0f * a);

            s->cruise_velocity = 2.0f * v_jerk + max_accel * t_const;

            s->t1_jerk_accel = t_jerk;
            s->t2_const_accel = t_const;
            s->t3_jerk_decel_accel = t_jerk;
            s->t4_cruise = 0.0f;
            s->t5_jerk_accel_decel = t_jerk;
            s->t6_const_decel = t_const;
            s->t7_jerk_decel_decel = t_jerk;

            s->v_end_segment1 = v_jerk;
            s->v_end_segment2 = s->v_end_segment1 + max_accel * t_const;
            s->v_end_segment3 = s->cruise_velocity;

            s->v_end_segment5 = s->cruise_velocity - v_jerk;
            s->v_end_segment6 = s->v_end_segment5 - max_accel * t_const;
        }
    }

    s->total_steps = distance;

    return true;
}

// *****************************************************************************
// OCR Interrupt Handlers (Step Counters)
// *****************************************************************************/

static void OCMP4_StepCounter_X(uintptr_t context)
{
    axis_state[AXIS_X].step_count++;
}

static void OCMP1_StepCounter_Y(uintptr_t context)
{
    axis_state[AXIS_Y].step_count++;
}

static void OCMP5_StepCounter_Z(uintptr_t context)
{
    axis_state[AXIS_Z].step_count++;
}

#ifdef ENABLE_AXIS_A
static void OCMP3_StepCounter_A(uintptr_t context)
{
    axis_state[AXIS_A].step_count++;
}
#endif

// *****************************************************************************
// TMR1 @ 1kHz - Multi-Axis S-CURVE STATE MACHINE
// *****************************************************************************/

static void TMR1_MultiAxisControl(uint32_t status, uintptr_t context)
{
    // DEBUG: Heartbeat to show TMR1 is running
    static uint16_t heartbeat = 0;
    if (++heartbeat > 1000)
    {
        LED1_Toggle(); // Blink every second to show TMR1 alive
        heartbeat = 0;
    }

    // Update all active axes
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        volatile scurve_state_t *s = &axis_state[axis];

        if (!s->active || s->current_segment == SEGMENT_IDLE)
            continue;

        LED2_Toggle(); // DEBUG: Show we have an active axis being processed

        s->elapsed_time += UPDATE_PERIOD_SEC;
        s->total_elapsed += UPDATE_PERIOD_SEC;

        float new_velocity = 0.0f;
        float new_accel = 0.0f;

        switch (s->current_segment)
        {
        case SEGMENT_JERK_ACCEL:
            new_accel = max_jerk * s->elapsed_time;
            new_velocity = 0.5f * max_jerk * s->elapsed_time * s->elapsed_time;

            if (s->elapsed_time >= s->t1_jerk_accel)
            {
                s->current_segment = SEGMENT_CONST_ACCEL;
                s->elapsed_time = 0.0f;
                s->current_velocity = s->v_end_segment1;
            }
            break;

        case SEGMENT_CONST_ACCEL:
            new_accel = max_accel;
            new_velocity = s->v_end_segment1 + (max_accel * s->elapsed_time);

            if (s->elapsed_time >= s->t2_const_accel)
            {
                s->current_segment = SEGMENT_JERK_DECEL_ACCEL;
                s->elapsed_time = 0.0f;
                s->current_velocity = s->v_end_segment2;
            }
            break;

        case SEGMENT_JERK_DECEL_ACCEL:
            new_accel = max_accel - (max_jerk * s->elapsed_time);
            new_velocity = s->v_end_segment2 +
                           (max_accel * s->elapsed_time) -
                           (0.5f * max_jerk * s->elapsed_time * s->elapsed_time);

            if (s->elapsed_time >= s->t3_jerk_decel_accel)
            {
                s->current_segment = SEGMENT_CRUISE;
                s->elapsed_time = 0.0f;
                s->current_velocity = s->cruise_velocity;
            }
            break;

        case SEGMENT_CRUISE:
            new_accel = 0.0f;
            new_velocity = s->cruise_velocity;

            // Safety: if we've traveled the distance, start deceleration
            if (s->elapsed_time >= s->t4_cruise || s->step_count >= (s->total_steps * 0.6f))
            {
                s->current_segment = SEGMENT_JERK_ACCEL_DECEL;
                s->elapsed_time = 0.0f;
            }
            break;

        case SEGMENT_JERK_ACCEL_DECEL:
            new_accel = -(max_jerk * s->elapsed_time);
            new_velocity = s->cruise_velocity -
                           (0.5f * max_jerk * s->elapsed_time * s->elapsed_time);

            if (s->elapsed_time >= s->t5_jerk_accel_decel)
            {
                s->current_segment = SEGMENT_CONST_DECEL;
                s->elapsed_time = 0.0f;
                s->current_velocity = s->v_end_segment5;
            }
            break;

        case SEGMENT_CONST_DECEL:
            new_accel = -max_accel;
            new_velocity = s->v_end_segment5 - (max_accel * s->elapsed_time);

            if (s->elapsed_time >= s->t6_const_decel)
            {
                s->current_segment = SEGMENT_JERK_DECEL_DECEL;
                s->elapsed_time = 0.0f;
                s->current_velocity = s->v_end_segment6;
            }
            break;

        case SEGMENT_JERK_DECEL_DECEL:
            new_accel = -(max_accel - max_jerk * s->elapsed_time);
            new_velocity = s->v_end_segment6 -
                           (max_accel * s->elapsed_time) +
                           (0.5f * max_jerk * s->elapsed_time * s->elapsed_time);

            // Check time-based OR step-based completion
            if (s->elapsed_time >= s->t7_jerk_decel_decel ||
                new_velocity <= 0.1f ||
                s->step_count >= s->total_steps)
            {
                s->current_segment = SEGMENT_COMPLETE;
                new_velocity = 0.0f;
            }
            break;

        case SEGMENT_COMPLETE:
            // CRITICAL: Stop timer then disable OCR
            axis_hw[axis].TMR_Stop();
            axis_hw[axis].OCMP_Disable();
            LED2_Set(); // Indicate axis complete (for testing)
            // Clear state
            s->active = false;
            s->current_velocity = 0.0f;
            s->current_accel = 0.0f;
            s->current_segment = SEGMENT_IDLE;
            break; // Exit switch - per-axis control, no global flag needed

        default:
            break;
        }

        s->current_velocity = new_velocity;
        s->current_accel = new_accel;

        if (s->current_velocity < 0.0f)
            s->current_velocity = 0.0f;
        if (s->current_velocity > max_velocity)
            s->current_velocity = max_velocity;

        // Update OCR hardware for this axis (only if still active)
        if (s->active && s->current_velocity > 1.0f)
        {
            uint32_t period = (uint32_t)(1000000.0f / s->current_velocity);

            if (period > 65485)
                period = 65485;
            if (period <= OCMP_PULSE_WIDTH)
                period = OCMP_PULSE_WIDTH + 10;

            axis_hw[axis].TMR_PeriodSet(period);
            axis_hw[axis].OCMP_CompareValueSet(period - OCMP_PULSE_WIDTH);
            axis_hw[axis].OCMP_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);
        }

        // Direction control - TODO: will be handled by G-code parser later
        // For now, direction is set in MultiAxis_MoveSingleAxis()
    }

    // No global motion_running flag needed - each axis manages its own state
}

// *****************************************************************************
// Public API
// *****************************************************************************/

void MultiAxis_Initialize(void)
{
    // Initialize all axis states
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        axis_state[axis].current_segment = SEGMENT_IDLE;
        axis_state[axis].step_count = 0;
        axis_state[axis].active = false;

        // Disable all drivers on startup (safe default)
        MultiAxis_DisableDriver(axis);
    }

    // Register OCR callbacks
    axis_hw[AXIS_X].OCMP_CallbackRegister(OCMP4_StepCounter_X, 0);
    axis_hw[AXIS_Y].OCMP_CallbackRegister(OCMP1_StepCounter_Y, 0);
    axis_hw[AXIS_Z].OCMP_CallbackRegister(OCMP5_StepCounter_Z, 0);

    // Register TMR1 callback for multi-axis control
    TMR1_CallbackRegister(TMR1_MultiAxisControl, 0);
    TMR1_Start();

    LED2_Set(); // Indicate initialized
}

/*! \brief Execute single-axis motion with S-curve profile
 *
 *  \param axis Axis identifier (must be < NUM_AXES)
 *  \param steps Number of steps to move (absolute value used)
 *  \param forward Direction: true = forward, false = reverse
 *
 *  MISRA Rule 8.7: Parameter validation with early return
 *  MISRA Rule 17.4: Array bounds checking
 */
void MultiAxis_MoveSingleAxis(axis_id_t axis, int32_t steps, bool forward)
{
    LED1_Toggle(); // DEBUG: Show function called

    // MISRA-compliant parameter validation
    assert(axis < NUM_AXES); // Development-time check

    if (axis >= NUM_AXES)
    {
        return; // Defensive: reject invalid axis
    }

    volatile scurve_state_t *s = &axis_state[axis];

    assert(s != NULL); // Verify pointer validity

    uint32_t abs_steps = (uint32_t)((steps < 0) ? -steps : steps);

    if (abs_steps == 0U)
    {
        return; // No motion required
    }

    // Ensure hardware is stopped before restarting
    axis_hw[axis].OCMP_Disable();
    axis_hw[axis].TMR_Stop();

    // Enable driver before motion (DRV8825 active-low enable)
    MultiAxis_EnableDriver(axis);

    // Calculate S-curve profile
    calculate_scurve_profile(axis, abs_steps);

    // Initialize state
    s->current_segment = SEGMENT_JERK_ACCEL;
    s->elapsed_time = 0.0f;
    s->total_elapsed = 0.0f;
    s->current_velocity = 0.0f;
    s->current_accel = 0.0f;
    s->step_count = 0U;
    s->direction_forward = forward;
    s->active = true;

    LED1_Set();   // DEBUG: Show motion starting
    LED2_Clear(); // DEBUG: Clear completion indicator

    // Set direction pin using dynamic lookup
    if (forward)
    {
        MultiAxis_SetDirection(axis);
    }
    else
    {
        MultiAxis_ClearDirection(axis);
    }

    // Start OCR - set initial period for first segment
    uint32_t initial_period = 65000U; // Slow start for S-curve
    axis_hw[axis].TMR_PeriodSet(initial_period);
    axis_hw[axis].OCMP_CompareValueSet(initial_period - OCMP_PULSE_WIDTH);
    axis_hw[axis].OCMP_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);

    axis_hw[axis].OCMP_Enable();
    axis_hw[axis].TMR_Start();

    // Per-axis control - no global motion_running flag needed
}

/*! \brief Execute coordinated multi-axis motion
 *
 *  \param steps Array of step counts for each axis [X, Y, Z]
 *               Positive = forward, Negative = reverse, 0 = no motion
 *
 *  MISRA Rule 17.4: Array parameter must be validated
 *  MISRA Rule 8.13: Pointer should be const-qualified (cannot due to API)
 */
void MultiAxis_MoveCoordinated(int32_t steps[NUM_AXES])
{
    // MISRA-compliant parameter validation
    assert(steps != NULL); // Development-time check

    if (steps == NULL)
    {
        return; // Defensive: reject null pointer
    }

    // Start all axes simultaneously for coordinated motion
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        if (steps[axis] != 0)
        {
            bool forward = (steps[axis] > 0);
            int32_t step_val = steps[axis];
            uint32_t abs_steps = (uint32_t)((step_val < 0) ? -step_val : step_val);
            MultiAxis_MoveSingleAxis(axis, (int32_t)abs_steps, forward);
        }
    }
}

/*! \brief Check if any axis is currently moving
 *
 *  \return true if motion in progress, false if all axes idle
 */
bool MultiAxis_IsBusy(void)
{
    // Check if any axis is active
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        if (axis_state[axis].active)
        {
            return true;
        }
    }
    return false;
}

/*! \brief Check if specific axis is moving
 *
 *  \param axis Axis identifier to check
 *  \return true if axis active, false if idle or invalid axis
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 */
bool MultiAxis_IsAxisBusy(axis_id_t axis)
{
    assert(axis < NUM_AXES); // Development-time check

    if (axis >= NUM_AXES)
    {
        return false; // Defensive: invalid axis always idle
    }

    return axis_state[axis].active;
}

/*! \brief Emergency stop all axes
 *
 *  Immediately halts all motion and disables OCR hardware.
 *  Also disables all stepper drivers for safety.
 *  Safe to call at any time, including from interrupt context.
 */
void MultiAxis_StopAll(void)
{
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        assert(axis < NUM_AXES); // Loop bounds check

        axis_hw[axis].OCMP_Disable();
        axis_hw[axis].TMR_Stop();
        axis_state[axis].current_segment = SEGMENT_COMPLETE;
        axis_state[axis].active = false;

        // Disable driver for safety (DRV8825 high-Z state)
        MultiAxis_DisableDriver(axis);
    }

    LED1_Clear();
    LED2_Clear();
    // No global motion_running flag - each axis manages its own state
}

/*! \brief Get current step count for axis
 *
 *  \param axis Axis identifier to query
 *  \return Step count, or 0 if invalid axis
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 */
uint32_t MultiAxis_GetStepCount(axis_id_t axis)
{
    assert(axis < NUM_AXES); // Development-time check

    if (axis >= NUM_AXES)
    {
        return 0U; // Defensive: return safe value for invalid axis
    }

    return axis_state[axis].step_count;
}

/*******************************************************************************
  Time-Based Vector Interpolation for Multi-Axis S-Curve Motion

  Algorithm:
  1. Find dominant axis (longest distance)
  2. Calculate S-curve profile for dominant axis
  3. Scale other axes' velocities to match dominant axis timing
  4. All axes use same segment times, different velocities
*******************************************************************************/

typedef struct
{
    axis_id_t dominant_axis;             // Axis with longest distance
    float total_move_time;               // Total time for move (from dominant axis)
    float axis_velocity_scale[NUM_AXES]; // Velocity scaling factors
} coordinated_move_t;

static coordinated_move_t coord_move;

/*! \brief Calculate coordinated multi-axis move with time synchronization
 *
 *  \param steps Array of target steps for each axis [X, Y, Z, A]
 *
 *  Algorithm:
 *    1. Find dominant axis (max absolute steps)
 *    2. Calculate S-curve profile for dominant axis → determines total time
 *    3. Scale velocities of other axes: v_axis = (distance_axis / total_time)
 *    4. All axes share same segment timing (t1-t7 from dominant axis)
 */
bool MultiAxis_CalculateCoordinatedMove(int32_t steps[NUM_AXES])
{
    assert(steps != NULL);

    if (steps == NULL)
    {
        return false;
    }

    // Step 1: Find dominant axis (longest distance)
    uint32_t max_steps = 0;
    axis_id_t dominant = AXIS_X;

    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        uint32_t abs_steps = (uint32_t)abs(steps[axis]);

        if (abs_steps > max_steps)
        {
            max_steps = abs_steps;
            dominant = axis;
        }
    }

    if (max_steps == 0U)
    {
        return false; // No motion required
    }

    coord_move.dominant_axis = dominant;

    // Step 2: Calculate S-curve profile for dominant axis
    // This determines the total move time and segment times (t1-t7)
    volatile scurve_state_t *dominant_state = &axis_state[dominant];

    if (!calculate_scurve_profile(dominant, max_steps))
    {
        return false;
    }

    // Calculate total move time from dominant axis S-curve
    coord_move.total_move_time = dominant_state->t1_jerk_accel +
                                 dominant_state->t2_const_accel +
                                 dominant_state->t3_jerk_decel_accel +
                                 dominant_state->t4_cruise +
                                 dominant_state->t5_jerk_accel_decel +
                                 dominant_state->t6_const_decel +
                                 dominant_state->t7_jerk_decel_decel;

    // Step 3: Calculate velocity scaling for subordinate axes
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        if (axis == dominant)
        {
            coord_move.axis_velocity_scale[axis] = 1.0f; // Dominant runs at full profile
        }
        else
        {
            uint32_t axis_steps = (uint32_t)abs(steps[axis]);

            if (axis_steps == 0U)
            {
                coord_move.axis_velocity_scale[axis] = 0.0f; // Axis not moving
            }
            else
            {
                // Scale velocity so this axis completes in same time as dominant
                // scale_factor = axis_distance / dominant_distance
                coord_move.axis_velocity_scale[axis] =
                    (float)axis_steps / (float)max_steps;
            }
        }
    }

    return true;
}

/*! \brief Execute coordinated move with synchronized timing
 *
 *  All axes:
 *  - Share same segment times (t1-t7) from dominant axis
 *  - Scale their velocities proportionally
 *  - Finish simultaneously (coordinated motion)
 */
void MultiAxis_ExecuteCoordinatedMove(int32_t steps[NUM_AXES])
{
    assert(steps != NULL);

    if (!MultiAxis_CalculateCoordinatedMove(steps))
    {
        return; // Invalid move
    }

    // Get dominant axis profile (this determines timing for all axes)
    volatile scurve_state_t *dominant = &axis_state[coord_move.dominant_axis];

    // Start all axes with synchronized profiles
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        if (coord_move.axis_velocity_scale[axis] == 0.0f)
        {
            continue; // Skip axes with no motion
        }

        volatile scurve_state_t *s = &axis_state[axis];

        // Copy segment times from dominant axis (ALL axes use same timing)
        s->t1_jerk_accel = dominant->t1_jerk_accel;
        s->t2_const_accel = dominant->t2_const_accel;
        s->t3_jerk_decel_accel = dominant->t3_jerk_decel_accel;
        s->t4_cruise = dominant->t4_cruise;
        s->t5_jerk_accel_decel = dominant->t5_jerk_accel_decel;
        s->t6_const_decel = dominant->t6_const_decel;
        s->t7_jerk_decel_decel = dominant->t7_jerk_decel_decel;

        // Scale velocities for this axis
        float velocity_scale = coord_move.axis_velocity_scale[axis];

        s->cruise_velocity = dominant->cruise_velocity * velocity_scale;
        s->v_end_segment1 = dominant->v_end_segment1 * velocity_scale;
        s->v_end_segment2 = dominant->v_end_segment2 * velocity_scale;
        s->v_end_segment3 = dominant->v_end_segment3 * velocity_scale;
        s->v_end_segment5 = dominant->v_end_segment5 * velocity_scale;
        s->v_end_segment6 = dominant->v_end_segment6 * velocity_scale;

        // Set step counts and direction
        uint32_t abs_steps = (uint32_t)abs(steps[axis]);
        s->total_steps = abs_steps;
        s->step_count = 0U;
        s->direction_forward = (steps[axis] > 0);

        // Initialize state
        s->current_segment = SEGMENT_JERK_ACCEL;
        s->elapsed_time = 0.0f;
        s->total_elapsed = 0.0f;
        s->current_velocity = 0.0f;
        s->current_accel = 0.0f;
        s->active = true;

        // Set direction and start hardware
        if (s->direction_forward)
        {
            MultiAxis_SetDirection(axis);
        }
        else
        {
            MultiAxis_ClearDirection(axis);
        }

        // Start OCR with slow initial period
        uint32_t initial_period = 65000U;
        axis_hw[axis].TMR_PeriodSet(initial_period);
        axis_hw[axis].OCMP_CompareValueSet(initial_period - OCMP_PULSE_WIDTH);
        axis_hw[axis].OCMP_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);

        axis_hw[axis].OCMP_Enable();
        axis_hw[axis].TMR_Start();
    }

    LED1_Set(); // Indicate coordinated motion active
}
