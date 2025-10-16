/*******************************************************************************
  Application Layer - Multi-Axis Hardware Test with Button Control

  Responsibilities:
  - Button debouncing and detection (SW1/SW2)
  - LED indicators (power-on, heartbeat)
  - Calls into multiaxis_control for coordinated motion
  - Main application state machine

  Test patterns:
  - SW1: Move X-axis +50mm forward (12,500 steps @ 250 steps/mm)
  - SW2: Move X-axis -50mm reverse (return to start)
*******************************************************************************/

#include "app.h"
#include "multiaxis_control.h"
#include "motion_math.h" // For MotionMath_MMToSteps()
#include "definitions.h"
#include <stdbool.h>

// *****************************************************************************
// Application Data
// *****************************************************************************

APP_DATA appData;

// Button debouncing
static bool sw1_was_pressed = false;
static bool sw2_was_pressed = false;

// *****************************************************************************
// Initialization
// *****************************************************************************

void APP_Initialize(void)
{
    appData.state = APP_STATE_INIT;

    // Initialize multi-axis stepper control subsystem
    MultiAxis_Initialize();

    // Enable X-axis stepper driver (DRV8825 ENABLE pin active LOW)
    MultiAxis_EnableDriver(AXIS_X);

    // Power-on indicator
    LED2_Set();

    // Ready for service
    appData.state = APP_STATE_SERVICE_TASKS;
}

// *****************************************************************************
// Application Tasks - Main Loop
// *****************************************************************************

void APP_Tasks(void)
{
    switch (appData.state)
    {
    case APP_STATE_INIT:
        // Initialization done in APP_Initialize()
        break;

    case APP_STATE_SERVICE_TASKS:
    {
        // Check SW1 (move X-axis +50mm forward) - active LOW
        bool sw1_pressed = !SW1_Get();
        if (sw1_pressed && !sw1_was_pressed)
        {
            LED2_Toggle(); // DEBUG: Show button detected
            if (!MultiAxis_IsBusy())
            {
                // Convert 50mm to steps (250 steps/mm = 12,500 steps)
                int32_t steps_50mm = MotionMath_MMToSteps(50.0f, AXIS_X);

                // Move X-axis forward 50mm at decent speed
                MultiAxis_MoveSingleAxis(AXIS_X, steps_50mm, true);
            }
        }
        sw1_was_pressed = sw1_pressed;

        // Check SW2 (move X-axis -50mm reverse) - active LOW
        bool sw2_pressed = !SW2_Get();
        if (sw2_pressed && !sw2_was_pressed && !MultiAxis_IsBusy())
        {
            // Convert 50mm to steps (250 steps/mm = 12,500 steps)
            int32_t steps_50mm = MotionMath_MMToSteps(50.0f, AXIS_X);

            // Move X-axis reverse 50mm to return to start position
            MultiAxis_MoveSingleAxis(AXIS_X, steps_50mm, false);
        }
        sw2_was_pressed = sw2_pressed;

        // LED1 heartbeat is handled by TMR1 interrupt (1Hz toggle in multiaxis_control.c)
        // LED1 shows solid during motion (set in MultiAxis_MoveSingleAxis)

        // LED2 shows motion activity (toggled by TMR1 when processing axes)

        break;
    }

    default:
        appData.state = APP_STATE_SERVICE_TASKS;
        break;
    }
}