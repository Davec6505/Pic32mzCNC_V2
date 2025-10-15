/*******************************************************************************
  Application Layer - Hardware Test with Button Control

  Responsibilities:
  - Button debouncing and detection (SW1/SW2)
  - LED indicators (power-on, heartbeat)
  - Calls into stepper_control for motion execution
  - Main application state machine
*******************************************************************************/

#include "app.h"
#include "stepper_control.h"
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

    // Initialize stepper control subsystem
    StepperControl_Initialize();

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
        // Check SW1 (forward motion) - active LOW
        bool sw1_pressed = !SW1_Get(); // Active LOW
        if (sw1_pressed && !sw1_was_pressed && !StepperControl_IsBusy())
        {
            // Request 5000 steps forward with default profile
            StepperControl_MoveSteps(5000, true);
        }
        sw1_was_pressed = sw1_pressed;

        // Check SW2 (reverse motion) - active LOW
        bool sw2_pressed = !SW2_Get(); // Active LOW;
        if (sw2_pressed && !sw2_was_pressed && !StepperControl_IsBusy())
        {
            // Request 5000 steps reverse with default profile
            StepperControl_MoveSteps(5000, false);
        }
        sw2_was_pressed = sw2_pressed;

        // Heartbeat LED when idle
        if (!StepperControl_IsBusy())
        {
            static uint32_t heartbeat_counter = 0;
            if (++heartbeat_counter > 500000)
            {
                LED1_Toggle();
                heartbeat_counter = 0;
            }
        }
        else
        {
            LED1_Set(); // Solid during motion
        }

        break;
    }

    default:
        appData.state = APP_STATE_SERVICE_TASKS;
        break;
    }
}