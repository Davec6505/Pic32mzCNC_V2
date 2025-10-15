/*******************************************************************************
  Stepper Control Layer - OCR Continuous Pulse Mode with S-Curve Profiles

  Architecture:
  - OCR hardware runs continuously generating step pulses at current rate
  - OCR interrupt: Lightweight step counter only (high frequency)
  - TMR1 @ 1kHz: All heavy computation (S-curve state machine, rate updates)

  Usage:
    StepperControl_Initialize();              // Setup hardware
    StepperControl_MoveSteps(5000, true);     // Move 5000 steps forward
    bool busy = StepperControl_IsBusy();      // Check motion status
*******************************************************************************/

#include "stepper_control.h"
#include "definitions.h"
#include <stdbool.h>
#include <stdint.h>

// *****************************************************************************
// Speed Control Constants (from speed_control.c)
// *****************************************************************************

// Timer frequency calculation constants
#define T1_FREQ_148 ((TICKS_PER_US * 1000000) / 100)
#define A_T_x100 ((long)(A_x10 * T1_FREQ_148))
#define T_FREQ_148 ((long)((TICKS_PER_US * 1000000) / 10000))
#define A_SQ (long)(A_x20000 * A_x20000)
#define A_x10 (long)(2 * 100 * ALPHA)
#define A_x20000 (long)(2 * 10000 * ALPHA)

// Motor physics
#define ALPHA ((float)2 * 3.14159 / 200) // 2*pi/spr (200 steps/rev)
#define TICKS_PER_US 100                 // 100MHz / 1MHz = 100

// OCR pulse width (DRV8825 requires 1.9µs minimum)
#define OCMP_PULSE_WIDTH 40 // 40µs pulse width

// *****************************************************************************
// Speed Ramp Data Structure
// *****************************************************************************

typedef struct
{
    // Speed ramp state
    run_state_t run_state;
    direction_t dir;

    // Step timing
    uint16_t step_delay;
    uint16_t min_delay;

    // Acceleration profile
    int32_t accel_count;
    int32_t decel_val;
    int32_t decel_start;
    int32_t rest;

    // Step counting
    volatile uint32_t step_count;
    volatile int16_t last_accel_delay;
} speed_ramp_data_t;

// Global speed ramp data
static speed_ramp_data_t srd;

// Motion control flags
static volatile bool motion_running = false;

// Motion parameters (can be adjusted via API)
static uint32_t current_accel = DEFAULT_ACCEL;
static uint32_t current_decel = DEFAULT_DECEL;
static uint32_t current_speed = DEFAULT_SPEED;

// *****************************************************************************
// Math Helper Functions
// *****************************************************************************

static uint32_t _sqrt(uint32_t x)
{
    uint32_t xr = 0;
    uint32_t q2 = 0x40000000L;
    uint8_t f;

    do
    {
        if ((xr + q2) <= x)
        {
            x -= xr + q2;
            f = 1;
        }
        else
        {
            f = 0;
        }
        xr >>= 1;
        if (f)
        {
            xr += q2;
        }
    } while (q2 >>= 2);

    if (xr < x)
    {
        return xr + 1;
    }
    else
    {
        return xr;
    }
}

static uint16_t calculate_step_delay(void)
{
    uint16_t new_step_delay = srd.step_delay -
                              (((2 * (long)srd.step_delay) + srd.rest) / (4 * srd.accel_count + 1));

    srd.rest = ((2 * (long)srd.step_delay) + srd.rest) % (4 * srd.accel_count + 1);

    return new_step_delay;
}

// *****************************************************************************
// OCR Interrupt - LIGHTWEIGHT STEP COUNTER ONLY
// *****************************************************************************

static void OCMP4_StepCounter(uintptr_t context)
{
    // ONLY increment step counter - TMR1 does everything else!
    srd.step_count++;
}

// *****************************************************************************
// TMR1 @ 1kHz - MOTION CONTROL ENGINE
// *****************************************************************************

static void TMR1_MotionControl(uint32_t status, uintptr_t context)
{
    uint16_t new_step_delay = 0;

    // Update direction pin based on current state
    if (srd.dir == CW)
    {
        DirX_Set();
    }
    else
    {
        DirX_Clear();
    }

    // S-curve state machine
    switch (srd.run_state)
    {
    case STOP:
        // Motion complete - stop OCR hardware
        srd.step_count = 0;
        srd.rest = 0;
        OCMP4_Disable();
        TMR2_Stop();
        motion_running = false;
        LED2_Clear();
        break;

    case ACCEL:
        srd.accel_count++;
        new_step_delay = calculate_step_delay();

        // Check if we should start deceleration
        if (srd.step_count >= (uint32_t)srd.decel_start)
        {
            srd.accel_count = srd.decel_val;
            srd.run_state = DECEL;
        }
        // Check if we hit max speed
        else if (new_step_delay <= srd.min_delay)
        {
            srd.last_accel_delay = new_step_delay;
            new_step_delay = srd.min_delay;
            srd.rest = 0;
            srd.run_state = RUN;
        }

        // Update OCR period dynamically (hardware keeps running!)
        srd.step_delay = new_step_delay;
        TMR2_PeriodSet(new_step_delay);
        OCMP4_CompareValueSet(new_step_delay - OCMP_PULSE_WIDTH);
        OCMP4_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);
        break;

    case RUN:
        new_step_delay = srd.min_delay;

        // Check if we should start deceleration
        if (srd.step_count >= (uint32_t)srd.decel_start)
        {
            srd.accel_count = srd.decel_val;
            new_step_delay = srd.last_accel_delay;
            srd.run_state = DECEL;

            // Update OCR period
            srd.step_delay = new_step_delay;
            TMR2_PeriodSet(new_step_delay);
            OCMP4_CompareValueSet(new_step_delay - OCMP_PULSE_WIDTH);
            OCMP4_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);
        }
        break;

    case DECEL:
        srd.accel_count++;
        new_step_delay = calculate_step_delay();

        // Check if we're at last step
        if (srd.accel_count >= 0)
        {
            srd.run_state = STOP;
        }

        // Update OCR period
        srd.step_delay = new_step_delay;
        TMR2_PeriodSet(new_step_delay);
        OCMP4_CompareValueSet(new_step_delay - OCMP_PULSE_WIDTH);
        OCMP4_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);
        break;
    }
}

// *****************************************************************************
// Public API Implementation
// *****************************************************************************

void StepperControl_Initialize(void)
{
    // Register callbacks
    OCMP4_CallbackRegister(OCMP4_StepCounter, (uintptr_t)NULL);
    TMR1_CallbackRegister(TMR1_MotionControl, (uintptr_t)NULL);

    // Initialize motion state
    srd.run_state = STOP;
    srd.step_count = 0;
    motion_running = false;

    // Start TMR1 @ 1kHz for motion control
    TMR1_Start();
}

void StepperControl_MoveSteps(int32_t steps, bool forward)
{
    uint32_t abs_steps;
    uint32_t max_s_lim;
    uint32_t accel_lim;

    // Don't start new move if already moving
    if (motion_running)
    {
        return;
    }

    // Set direction
    srd.dir = forward ? CW : CCW;
    abs_steps = forward ? steps : -steps;

    // Special case: single step
    if (abs_steps == 1)
    {
        srd.accel_count = -1;
        srd.run_state = DECEL;
        srd.step_delay = 1000;
        srd.step_count = 0;
        motion_running = true;

        // Set direction and start
        if (srd.dir == CW)
        {
            DirX_Set();
        }
        else
        {
            DirX_Clear();
        }

        LED2_Set();
        TMR2_PeriodSet(srd.step_delay);
        OCMP4_CompareValueSet(srd.step_delay - OCMP_PULSE_WIDTH);
        OCMP4_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);
        OCMP4_Enable();
        TMR2_Start();
        return;
    }

    if (abs_steps == 0)
    {
        return;
    }

    // Calculate S-curve profile
    srd.min_delay = A_T_x100 / current_speed;
    srd.step_delay = (T1_FREQ_148 * _sqrt(A_SQ / current_accel)) / 100;

    max_s_lim = (long)current_speed * current_speed /
                (long)(((long)A_x20000 * current_accel) / 100);
    if (max_s_lim == 0)
        max_s_lim = 1;

    accel_lim = ((long)abs_steps * current_decel) / (current_accel + current_decel);
    if (accel_lim == 0)
        accel_lim = 1;

    if (accel_lim <= max_s_lim)
    {
        srd.decel_val = accel_lim - abs_steps;
    }
    else
    {
        srd.decel_val = -((long)max_s_lim * current_accel) / current_decel;
    }
    if (srd.decel_val == 0)
        srd.decel_val = -1;

    srd.decel_start = abs_steps + srd.decel_val;

    if (srd.step_delay <= srd.min_delay)
    {
        srd.step_delay = srd.min_delay;
        srd.run_state = RUN;
    }
    else
    {
        srd.run_state = ACCEL;
    }

    // Reset counters
    srd.accel_count = 0;
    srd.rest = 0;
    srd.step_count = 0;
    srd.last_accel_delay = 0;
    motion_running = true;

    // Set direction BEFORE starting pulses
    if (srd.dir == CW)
    {
        DirX_Set();
    }
    else
    {
        DirX_Clear();
    }

    // Start OCR in continuous mode
    LED2_Set();
    TMR2_PeriodSet(srd.step_delay);
    OCMP4_CompareValueSet(srd.step_delay - OCMP_PULSE_WIDTH);
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
    current_accel = accel;
    current_decel = decel;
    current_speed = speed;
}

void StepperControl_Stop(void)
{
    srd.run_state = STOP;
}

uint32_t StepperControl_GetStepCount(void)
{
    return srd.step_count;
}