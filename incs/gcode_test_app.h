/*******************************************************************************
  G-code Test Application

  File Name:
    gcode_test_app.h

  Summary:
    Test application integration for G-code parser testing

  Description:
    Provides integration functions to run G-code parser tests within
    your main application. Can be used for both development testing
    and production validation.
*******************************************************************************/

#ifndef GCODE_TEST_APP_H
#define GCODE_TEST_APP_H

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include "gcode_test_framework.h"

// *****************************************************************************
// *****************************************************************************
// Section: Interface Functions
// *****************************************************************************
// *****************************************************************************

/* Test Application Control */
bool GCODE_TEST_Initialize(void);
bool GCODE_TEST_RunQuickTests(void);
bool GCODE_TEST_RunFullSuite(void);
bool GCODE_TEST_RunPerformanceTests(void);
void GCODE_TEST_Shutdown(void);

/* Interactive Testing */
bool GCODE_TEST_ParseSingleCommand(const char *gcode_line);
bool GCODE_TEST_RunContinuousTest(uint32_t duration_seconds);
bool GCODE_TEST_ValidateHardwareIntegration(void);

/* Test Reporting */
void GCODE_TEST_PrintQuickStatus(void);
void GCODE_TEST_PrintDetailedReport(void);
void GCODE_TEST_ExportTestResults(void);

/* Development Helpers */
bool GCODE_TEST_DebugParser(const char *gcode_line);
void GCODE_TEST_EnableVerboseMode(bool enable);
void GCODE_TEST_SetTestParameters(uint16_t max_commands, float tolerance);

#endif /* GCODE_TEST_APP_H */

/*******************************************************************************
 Usage Example:
 
 // In your main application:
 
 void APP_Initialize(void) {
     // ... other initialization
     
     #ifdef DEBUG_BUILD
     GCODE_TEST_Initialize();
     GCODE_TEST_RunQuickTests();
     #endif
 }
 
 void APP_Tasks(void) {
     // ... normal operation
     
     // Optional: Run periodic validation
     static uint32_t last_test_time = 0;
     if (current_time - last_test_time > 60000) { // Every minute
         GCODE_TEST_ParseSingleCommand("G1 X10 Y10 F100");
         last_test_time = current_time;
     }
 }
 
 *******************************************************************************/