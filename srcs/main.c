/*******************************************************************************
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

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "definitions.h"                // SYS function prototypes
#include "speed_control.h"              // Speed control for motion systems
#include "app.h"                        // Application header
#include "plib_uart2.h"                 // Direct UART2 access for testing
// Temporarily keep uart_debug for fallback testing
// #include "uart_debug.h"                 // UART debug functions

void CORETIMER_Fp(uint32_t status, uintptr_t context){
    APP_CoreTimerCallback(status, context);
}


void OCMP_Fp (uintptr_t context){
    APP_OCMP4_Callback(context);
}
// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );
    
    /* Initialize the application */
    APP_Initialize();
    CORETIMER_CallbackSet ( CORETIMER_Fp, 0);
    CORETIMER_Start();
    
    /* Main application loop */
    while ( true )
    {
        /* Maintain state machines of all polled MPLAB Harmony modules. */
        SYS_Tasks ( );
        
        /* Maintain the application's state machine. */
        APP_Tasks();
    }

    /* Execution should not come here during normal operation */
    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/


