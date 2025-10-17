/*******************************************************************************
  Application Layer - CNC Controller System Management

  Responsibilities:
  - LED indicators (power-on, heartbeat)
  - System status monitoring
  - Application state machine

  Notes:
  - Motion control is now driven by G-code parser via serial (see main.c)
  - LED1 heartbeat is handled by TMR1 interrupt @ 1Hz (in multiaxis_control.c)
  - LED2 power-on indicator
*******************************************************************************/

#include "app.h"
#include "multiaxis_control.h"
#include "definitions.h"
#include <stdbool.h>

// *****************************************************************************
// Application Data
// *****************************************************************************

APP_DATA appData;

// *****************************************************************************
// Initialization
// *****************************************************************************

void APP_Initialize(void)
{
    appData.state = APP_STATE_INIT;

    // Initialize multi-axis stepper control subsystem
    MultiAxis_Initialize();

    // Enable all axis stepper drivers (DRV8825 ENABLE pin active LOW)
    MultiAxis_EnableDriver(AXIS_X);
    MultiAxis_EnableDriver(AXIS_Y);
    MultiAxis_EnableDriver(AXIS_Z);
    MultiAxis_EnableDriver(AXIS_A);

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
        // LED1 heartbeat is handled by TMR1 interrupt (1Hz toggle in multiaxis_control.c)
        // LED1 shows solid during motion, toggles when idle

        // LED2 shows power-on status (set in APP_Initialize)
        // Could be used for error indication in future

        // Motion control is now handled by G-code parser in main.c
        // No button handling needed - all commands via serial

        break;
    }

    case APP_STATE_MOTION_ERROR:
    {
        // Error state - could flash LEDs or trigger alarm
        // For now, just stay in this state until reset
        LED2_Toggle(); // Flash LED2 to indicate error
        break;
    }

    default:
        appData.state = APP_STATE_SERVICE_TASKS;
        break;
    }
}