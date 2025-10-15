/*******************************************************************************
  True 7-Segment S-Curve Motion Control
*******************************************************************************/

#include "motion/stepper_control.h"
#include "definitions.h"
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

// *****************************************************************************
// Multi-Axis Configuration
// *****************************************************************************/

#define NUM_AXES 3 // X, Y, Z

typedef enum
{
    AXIS_X = 0,
    AXIS_Y = 1,
    AXIS_Z = 2
} axis_id_t;

// *****************************************************************************
// S-Curve Motion Parameters
// *****************************************************************************/

static float max_velocity = 5000.0f; // steps/sec
static float max_accel = 10000.0f;   // steps/sec²
static float max_jerk = 50000.0f;    // steps/sec³

#define OCMP_PULSE_WIDTH 40
#define UPDATE_FREQ_HZ 1000.0f
#define UPDATE_PERIOD_SEC (1.0f / UPDATE_FREQ_HZ)

// *****************************************************************************
// S-Curve State
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

    // Velocity milestones (for absolute calculations)
    float v_end_segment1; // End of jerk accel
    float v_end_segment2; // End of const accel
    float v_end_segment3; // End of jerk decel accel (should equal cruise)
    float v_end_segment5; // End of jerk accel decel
    float v_end_segment6; // End of const decel

    // Step tracking
    volatile uint32_t step_count;
    uint32_t total_steps;
    bool direction_forward;

    // Axis identification
    axis_id_t axis_id;
    bool active;

} scurve_state_t;

static scurve_state_t scurve[NUM_AXES]; // One state per axis
static volatile bool motion_running = false;

// *****************************************************************************
// Math Helpers
// *****************************************************************************/

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

static bool calculate_scurve_profile(uint32_t distance)
{
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
        scurve->cruise_velocity = max_velocity;

        scurve->t1_jerk_accel = t_jerk;
        scurve->t3_jerk_decel_accel = t_jerk;
        scurve->t5_jerk_accel_decel = t_jerk;
        scurve->t7_jerk_decel_decel = t_jerk;

        if (v_between_jerks > 0.0f)
        {
            scurve->t2_const_accel = v_between_jerks / max_accel;
            scurve->t6_const_decel = scurve->t2_const_accel;
        }
        else
        {
            scurve->t2_const_accel = 0.0f;
            scurve->t6_const_decel = 0.0f;
        }

        float d_cruise = d_total - d_accel_total - d_decel_total;
        scurve->t4_cruise = d_cruise / scurve->cruise_velocity;

        scurve->v_end_segment1 = v_jerk;
        scurve->v_end_segment2 = scurve->v_end_segment1 + max_accel * scurve->t2_const_accel;
        scurve->v_end_segment3 = scurve->cruise_velocity;

        // Deceleration milestones (symmetric to acceleration)
        scurve->v_end_segment5 = scurve->cruise_velocity - v_jerk;
        scurve->v_end_segment6 = scurve->v_end_segment5 - max_accel * scurve->t6_const_decel;
    }
    else
    {
        // SHORT MOVE
        if (d_total <= 4.0f * d_jerk)
        {
            // VERY SHORT - only jerk segments
            float t_jerk_reduced = cbrt_approx(d_total / (4.0f * (1.0f / 6.0f) * max_jerk));
            scurve->cruise_velocity = 0.5f * max_jerk * t_jerk_reduced * t_jerk_reduced;

            scurve->t1_jerk_accel = t_jerk_reduced;
            scurve->t2_const_accel = 0.0f;
            scurve->t3_jerk_decel_accel = t_jerk_reduced;
            scurve->t4_cruise = 0.0f;
            scurve->t5_jerk_accel_decel = t_jerk_reduced;
            scurve->t6_const_decel = 0.0f;
            scurve->t7_jerk_decel_decel = t_jerk_reduced;

            scurve->v_end_segment1 = 0.5f * max_jerk * t_jerk_reduced * t_jerk_reduced;
            scurve->v_end_segment2 = scurve->v_end_segment1;
            scurve->v_end_segment3 = scurve->cruise_velocity;

            // Deceleration milestones (symmetric)
            scurve->v_end_segment5 = scurve->v_end_segment1;
            scurve->v_end_segment6 = 0.0f;
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

            scurve->cruise_velocity = 2.0f * v_jerk + max_accel * t_const;

            scurve->t1_jerk_accel = t_jerk;
            scurve->t2_const_accel = t_const;
            scurve->t3_jerk_decel_accel = t_jerk;
            scurve->t4_cruise = 0.0f;
            scurve->t5_jerk_accel_decel = t_jerk;
            scurve->t6_const_decel = t_const;
            scurve->t7_jerk_decel_decel = t_jerk;

            scurve->v_end_segment1 = v_jerk;
            scurve->v_end_segment2 = scurve->v_end_segment1 + max_accel * t_const;
            scurve->v_end_segment3 = scurve->cruise_velocity;

            // Deceleration milestones (symmetric)
            scurve->v_end_segment5 = scurve->cruise_velocity - v_jerk;
            scurve->v_end_segment6 = scurve->v_end_segment5 - max_accel * t_const;
        }
    }

    scurve->total_steps = distance;

    return true;
}

// *****************************************************************************
// OCR Interrupt
// *****************************************************************************/

static void OCMP4_StepCounter(uintptr_t context)
{
    scurve->step_count++;

    /*  if (scurve->step_count >= scurve->total_steps)
        {
            scurve->current_segment = SEGMENT_COMPLETE;
        }
    */
}

// *****************************************************************************
// TMR1 @ 1kHz - S-CURVE STATE MACHINE
// *****************************************************************************/

static void TMR1_SCurveControl(uint32_t status, uintptr_t context)
{
    if (!motion_running)
        return;

    scurve->elapsed_time += UPDATE_PERIOD_SEC;
    scurve->total_elapsed += UPDATE_PERIOD_SEC;

    float new_velocity = 0.0f;
    float new_accel = 0.0f;

    switch (scurve->current_segment)
    {
    case SEGMENT_JERK_ACCEL:
        new_accel = max_jerk * scurve->elapsed_time;
        new_velocity = 0.5f * max_jerk * scurve->elapsed_time * scurve->elapsed_time;

        if (scurve->elapsed_time >= scurve->t1_jerk_accel)
        {
            scurve->current_segment = SEGMENT_CONST_ACCEL;
            scurve->elapsed_time = 0.0f;
            scurve->current_velocity = scurve->v_end_segment1;
        }
        break;

    case SEGMENT_CONST_ACCEL:
        new_accel = max_accel;
        new_velocity = scurve->v_end_segment1 + (max_accel * scurve->elapsed_time);

        if (scurve->elapsed_time >= scurve->t2_const_accel)
        {
            scurve->current_segment = SEGMENT_JERK_DECEL_ACCEL;
            scurve->elapsed_time = 0.0f;
            scurve->current_velocity = scurve->v_end_segment2;
        }
        break;

    case SEGMENT_JERK_DECEL_ACCEL:
        new_accel = max_accel - (max_jerk * scurve->elapsed_time);
        new_velocity = scurve->v_end_segment2 +
                       (max_accel * scurve->elapsed_time) -
                       (0.5f * max_jerk * scurve->elapsed_time * scurve->elapsed_time);

        if (scurve->elapsed_time >= scurve->t3_jerk_decel_accel)
        {
            scurve->current_segment = SEGMENT_CRUISE;
            scurve->elapsed_time = 0.0f;
            scurve->current_velocity = scurve->cruise_velocity;
        }
        break;

    case SEGMENT_CRUISE:
        new_accel = 0.0f;
        new_velocity = scurve->cruise_velocity;

        if (scurve->elapsed_time >= scurve->t4_cruise)
        {
            scurve->current_segment = SEGMENT_JERK_ACCEL_DECEL;
            scurve->elapsed_time = 0.0f;
        }
        break;

    case SEGMENT_JERK_ACCEL_DECEL:
        // Segment 5: Decelerate with increasing negative jerk (mirrored segment 1)
        new_accel = -(max_jerk * scurve->elapsed_time);
        new_velocity = scurve->cruise_velocity -
                       (0.5f * max_jerk * scurve->elapsed_time * scurve->elapsed_time);

        if (scurve->elapsed_time >= scurve->t5_jerk_accel_decel)
        {
            scurve->current_segment = SEGMENT_CONST_DECEL;
            scurve->elapsed_time = 0.0f;
            scurve->current_velocity = scurve->v_end_segment5; // Use pre-calculated milestone
        }
        break;

    case SEGMENT_CONST_DECEL:
        // Segment 6: Constant deceleration (mirrored segment 2)
        // ABSOLUTE velocity calculation from END of segment 5 (pre-calculated milestone)
        new_accel = -max_accel;
        new_velocity = scurve->v_end_segment5 - (max_accel * scurve->elapsed_time);

        if (scurve->elapsed_time >= scurve->t6_const_decel)
        {
            scurve->current_segment = SEGMENT_JERK_DECEL_DECEL;
            scurve->elapsed_time = 0.0f;
            scurve->current_velocity = scurve->v_end_segment6; // Use pre-calculated milestone
        }
        break;

    case SEGMENT_JERK_DECEL_DECEL:
        // Segment 7: Final deceleration with decreasing negative jerk (mirrored segment 3)
        // ABSOLUTE velocity calculation from END of segment 6 (pre-calculated milestone)
        new_accel = -(max_accel - max_jerk * scurve->elapsed_time);
        new_velocity = scurve->v_end_segment6 -
                       (max_accel * scurve->elapsed_time) +
                       (0.5f * max_jerk * scurve->elapsed_time * scurve->elapsed_time);

        if (scurve->elapsed_time >= scurve->t7_jerk_decel_decel || new_velocity <= 0.1f)
        {
            scurve->current_segment = SEGMENT_COMPLETE;
            new_velocity = 0.0f;
        }
        break;

    case SEGMENT_COMPLETE:
        OCMP4_Disable();
        TMR2_Stop();
        motion_running = false;
        scurve->current_velocity = 0.0f;
        scurve->current_accel = 0.0f;
        LED2_Clear();
        return;

    default:
        break;
    }

    scurve->current_velocity = new_velocity;
    scurve->current_accel = new_accel;

    if (scurve->current_velocity < 0.0f)
        scurve->current_velocity = 0.0f;
    if (scurve->current_velocity > max_velocity)
        scurve->current_velocity = max_velocity;

    if (scurve->current_velocity > 1.0f)
    {
        uint32_t period = (uint32_t)(1000000.0f / scurve->current_velocity);

        if (period > 65485)
            period = 65485;
        if (period <= OCMP_PULSE_WIDTH)
            period = OCMP_PULSE_WIDTH + 10;

        TMR2_PeriodSet(period);
        OCMP4_CompareValueSet(period - OCMP_PULSE_WIDTH);
        OCMP4_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);
    }

    if (scurve->direction_forward)
    {
        DirX_Set();
    }
    else
    {
        DirX_Clear();
    }
}

// *****************************************************************************
// Public API
// *****************************************************************************/

void StepperControl_Initialize(void)
{
    OCMP4_CallbackRegister(OCMP4_StepCounter, (uintptr_t)NULL);
    TMR1_CallbackRegister(TMR1_SCurveControl, (uintptr_t)NULL);

    scurve->current_segment = SEGMENT_IDLE;
    scurve->step_count = 0;
    motion_running = false;

    TMR1_Start();
}

void StepperControl_MoveSteps(int32_t steps, bool forward)
{
    if (motion_running)
        return;

    uint32_t abs_steps = (steps < 0) ? -steps : steps;
    if (abs_steps == 0)
        return;

    if (!calculate_scurve_profile(abs_steps))
        return;

    scurve->current_segment = SEGMENT_JERK_ACCEL;
    scurve->elapsed_time = 0.0f;
    scurve->total_elapsed = 0.0f;
    scurve->current_velocity = 0.0f;
    scurve->current_accel = 0.0f;
    scurve->step_count = 0;
    scurve->direction_forward = forward;
    motion_running = true;

    if (forward)
    {
        DirX_Set();
    }
    else
    {
        DirX_Clear();
    }

    LED2_Set();
    uint32_t initial_period = 10000;
    TMR2_PeriodSet(initial_period);
    OCMP4_CompareValueSet(initial_period - OCMP_PULSE_WIDTH);
    OCMP4_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);
    OCMP4_Enable();
    TMR2_Start();
}

bool StepperControl_IsBusy(void)
{
    return motion_running;
}

void StepperControl_SetProfile(uint32_t accel, uint32_t decel, uint32_t speed)
{
    max_velocity = (float)speed;
    max_accel = (float)accel;
    max_jerk = max_accel * 5.0f;
}

void StepperControl_Stop(void)
{
    scurve->current_segment = SEGMENT_COMPLETE;
}

uint32_t StepperControl_GetStepCount(void)
{
    return scurve->step_count;
}