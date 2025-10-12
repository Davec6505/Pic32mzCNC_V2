/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stddef.h>           // Defines NULL
#include <stdbool.h>          // Defines true
#include <stdlib.h>           // Defines EXIT_FAILURE
#include "definitions.h"      // SYS function prototypes
#include "speed_control.h"    // Speed control for motion systems
#include "app.h"              // Application header
#include "plib_uart2.h"       // Direct UART2 access for testing
#include "plib_uart_common.h" // Common UART functions
#include "plib_coretimer.h"   // Core timer for system timing
#include "grbl_serial.h"      // GRBL serial communication

void CORETIMER_Fp(uint32_t status, uintptr_t context)
{
  APP_TrajectoryTimerCallback_CoreTimer(status, context);
}

void OCMP_Fp(uintptr_t context)
{
  APP_OCMP4_Callback(context);
}
// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

int main(void)
{
  /* Initialize all modules */
  SYS_Initialize(NULL);

  /* Initialize the application */
  APP_Initialize();

  /* Main application loop */
  while (true)
  {
    /* Maintain state machines of all polled MPLAB Harmony modules. */
    SYS_Tasks();

    /* Run GRBL serial tasks to process incoming data */
    /* GRBL_Tasks(); */ // Temporarily disabled to test direct UART handling

    /* Maintain the application's state machine. */
    APP_Tasks();
  }

  /* Execution should not come here during normal operation */
  return (EXIT_FAILURE);
}

/*******************************************************************************
 End of File
*/
