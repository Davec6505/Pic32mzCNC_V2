/*******************************************************************************
  Application Layer - Multi-Axis Hardware Test with Button Control

  Responsibilities:
  - Button debouncing and detection (SW1/SW2)
  - LED indicators (power-on, heartbeat)
  - Calls into multiaxis_control for coordinated motion
  - Main application state machine

  Test patterns:
  - SW1: Move X and Y axes +50mm forward (coordinated move)
  - SW2: Move X and Y axes -50mm reverse (return to start)
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

    // Enable X and Y axis stepper drivers (DRV8825 ENABLE pin active LOW)
    MultiAxis_EnableDriver(AXIS_X);
    MultiAxis_EnableDriver(AXIS_Y);

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
        // Check SW1 (move X and Y axes +50mm forward) - active LOW
        bool sw1_pressed = !SW1_Get();
        if (sw1_pressed && !sw1_was_pressed)
        {
            LED2_Toggle(); // DEBUG: Show SW1 button press detected
            if (!MultiAxis_IsBusy())
            {
                LED2_Toggle(); // DEBUG: Show we're starting motion

                // Convert 50mm to steps for both axes (250 steps/mm = 12,500 steps)
                int32_t steps_x = MotionMath_MMToSteps(50.0f, AXIS_X);
                int32_t steps_y = MotionMath_MMToSteps(50.0f, AXIS_Y);

                // Create coordinated move array (X, Y, Z, A)
                int32_t steps[NUM_AXES] = {steps_x, steps_y, 0, 0};

                // Move both X and Y axes forward 50mm (time-synchronized coordinated motion)
                MultiAxis_ExecuteCoordinatedMove(steps);
            }
        }
        sw1_was_pressed = sw1_pressed;

        // Check SW2 (move X and Y axes -50mm reverse) - active LOW
        bool sw2_pressed = !SW2_Get();
        if (sw2_pressed && !sw2_was_pressed)
        {
            LED2_Toggle(); // DEBUG: Show SW2 button press detected
            if (!MultiAxis_IsBusy())
            {
                LED2_Toggle(); // DEBUG: Show we're starting motion

                // Convert 50mm to steps for both axes (250 steps/mm = 12,500 steps)
                int32_t steps_x = MotionMath_MMToSteps(50.0f, AXIS_X);
                int32_t steps_y = MotionMath_MMToSteps(50.0f, AXIS_Y);

                // Create coordinated move array (negative for reverse)
                int32_t steps[NUM_AXES] = {-steps_x, -steps_y, 0, 0};

                // Move both X and Y axes reverse 50mm (time-synchronized coordinated motion)
                MultiAxis_ExecuteCoordinatedMove(steps);
            }
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