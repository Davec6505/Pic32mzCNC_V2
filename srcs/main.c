/*******************************#include "grbl_serial.h"      // GRBL serial communication

// Serial settings and callbacks
uintptr_t ctx = 0;
uint8_t rxBuffer[1];

void UART_Read(uintptr_t context)
{
  GRBL_Process_Char(rxBuffer[0]);
  UART2_Read(&rxBuffer, 1);
}*************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description: am starting a
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

#include "grbl_serial.h" // GRBL serial communication

// Serial settings and callbacks
uintptr_t ctx = 0;
uint8_t rxBuffer[1];

void UART_Read(uintptr_t context)
{
  GRBL_Process_Char(rxBuffer[0]);
  UART2_Read(&rxBuffer, 1);
}

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

  /* Initialize UART2 for communication */
  UART2_ReadCallbackRegister(UART_Read, ctx);
  UART2_Read(&rxBuffer[0], 1); // Start first read

  /* Initialize the application */
  APP_Initialize();

  CORETIMER_CallbackSet(CORETIMER_Fp, 0);
  CORETIMER_Start();

  /* Main application loop */
  while (true)
  {
    /* Maintain state machines of all polled MPLAB Harmony modules. */
    SYS_Tasks();

    /* Maintain the application's state machine. */
    APP_Tasks();
  }

  /* Execution should not come here during normal operation */
  return (EXIT_FAILURE);
}

/*******************************************************************************
 End of File
*/
