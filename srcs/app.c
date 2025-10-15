/*******************************************************************************
  Application Layer - Multi-Axis Hardware Test with Button Control

  Responsibilities:
  - Button debouncing and detection (SW1/SW2)
  - LED indicators (power-on, heartbeat)
  - Calls into multiaxis_control for coordinated motion
  - Main application state machine

  Test patterns:
  - SW1: Single axis motion (X-axis 5000 steps forward)
  - SW2: Multi-axis diagonal motion (X+Y coordinated)
*******************************************************************************/

#include "app.h"
#include "multiaxis_control.h"
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
        // Check SW1 (single X-axis motion) - active LOW
        bool sw1_pressed = !SW1_Get();
        if (sw1_pressed && !sw1_was_pressed)
        {
            LED1_Toggle(); // DEBUG: Show button detected
            if (!MultiAxis_IsBusy())
            {
                LED2_Toggle(); // DEBUG: Show not busy, starting move
                // Single axis test: 5000 steps forward on X-axis only
                MultiAxis_MoveSingleAxis(AXIS_X, 5000, true);
            }
            else
            {
                LED2_Set(); // DEBUG: Show blocked by IsBusy
            }
        }
        sw1_was_pressed = sw1_pressed;

        // Check SW2 (test different motion) - active LOW
        bool sw2_pressed = !SW2_Get();
        if (sw2_pressed && !sw2_was_pressed && !MultiAxis_IsBusy())
        {
            // SW2: Different X-axis motion - 10000 steps forward
            // (Y/Z hardware not wired yet, so only test X for now)
            int32_t move[NUM_AXES] = {
                5000, // X: 5000 steps
                5000, // Y: 5000 steps
                10000 // Z: 10000 steps
            };
            MultiAxis_MoveCoordinated(move);
            //  MultiAxis_MoveSingleAxis(AXIS_X, 10000, true);
        }
        sw2_was_pressed = sw2_pressed;

        // Heartbeat LED when idle
        if (!MultiAxis_IsBusy())
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