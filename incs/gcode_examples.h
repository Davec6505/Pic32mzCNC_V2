/*******************************************************************************
  G-code Usage Examples and Testing

  File Name:
    gcode_examples.h

  Summary:
    Examples of G-code commands and usage patterns for testing

  Description:
    This file contains examples of how to use the G-code parser
    and test common CNC operations.
*******************************************************************************/

#ifndef GCODE_EXAMPLES_H
#define GCODE_EXAMPLES_H

#include "gcode_parser.h"

// *****************************************************************************
// Example G-code Commands for Testing
// *****************************************************************************

/*
 * Basic Motion Commands:
 * 
 * G21                  ; Set units to millimeters
 * G90                  ; Absolute positioning mode
 * G94                  ; Feed rate units per minute
 * 
 * G0 X10 Y10 Z5        ; Rapid move to X10, Y10, Z5
 * G1 X20 Y20 F1000     ; Linear move to X20, Y20 at 1000 mm/min
 * G1 Z-2 F500          ; Plunge to Z-2 at 500 mm/min
 * G1 X0 Y0             ; Move back to origin (feed rate from previous command)
 * G0 Z10               ; Rapid up to Z10
 * 
 * M3 S12000            ; Start spindle clockwise at 12000 RPM
 * M8                   ; Turn on flood coolant
 * M5                   ; Stop spindle
 * M9                   ; Turn off coolant
 * 
 * M30                  ; Program end and return
 */

/*
 * Real-time Commands (single character, no line ending):
 * 
 * ?                    ; Status report request
 * ~                    ; Cycle start (resume from hold)
 * !                    ; Feed hold (pause)
 * 0x18 (Ctrl-X)        ; Reset/Emergency stop
 * 0x84                 ; Safety door
 * 0x85                 ; Jog cancel
 */

/*
 * Example CNC Program - Square Pocket:
 * 
 * G21 G90 G94          ; Metric, absolute, feed rate mode
 * G0 Z5                ; Move to safe Z height
 * G0 X0 Y0             ; Move to start position
 * M3 S12000            ; Start spindle
 * G4 P2                ; Dwell 2 seconds for spindle to reach speed
 * 
 * G1 Z-1 F500          ; Plunge cut
 * G1 X10 F1000         ; Cut to X10
 * G1 Y10               ; Cut to Y10
 * G1 X0                ; Cut to X0
 * G1 Y0                ; Cut back to start
 * 
 * G0 Z5                ; Retract
 * M5                   ; Stop spindle
 * M30                  ; Program end
 */

/*
 * Feed Override Examples:
 * 
 * 0x90                 ; Reset feed override to 100%
 * 0x91                 ; Increase feed override by 10%
 * 0x92                 ; Decrease feed override by 10%
 * 0x93                 ; Increase feed override by 1%
 * 0x94                 ; Decrease feed override by 1%
 */

// *****************************************************************************
// Function Prototypes for Testing
// *****************************************************************************

/*******************************************************************************
  Function:
    void GCODE_RunTests(void)

  Summary:
    Run basic G-code parser tests

  Description:
    This function runs a series of tests to verify G-code parsing
    and command execution functionality.
*/
void GCODE_RunTests(void);

/*******************************************************************************
  Function:
    void GCODE_SimulateUGSConnection(void)

  Summary:
    Simulate Universal G-code Sender connection

  Description:
    This function simulates the startup sequence and basic commands
    that Universal G-code Sender typically sends.
*/
void GCODE_SimulateUGSConnection(void);

// *****************************************************************************
// Test Function Implementations
// *****************************************************************************

static inline void GCODE_RunTests(void)
{
    gcode_status_t status;
    
    // Test basic setup commands
    status = GCODE_ParseLine("G21");  // Metric units
    // Should return STATUS_OK
    
    status = GCODE_ParseLine("G90");  // Absolute mode
    // Should return STATUS_OK
    
    status = GCODE_ParseLine("G94");  // Feed rate mode
    // Should return STATUS_OK
    
    // Test motion commands
    status = GCODE_ParseLine("G0 X10 Y10 Z5");  // Rapid move
    // Should return STATUS_OK and queue rapid move
    
    status = GCODE_ParseLine("G1 X20 Y20 F1000");  // Linear move
    // Should return STATUS_OK and queue linear move at 1000 mm/min
    
    status = GCODE_ParseLine("G1 Z-2 F500");  // Z plunge
    // Should return STATUS_OK and queue Z move at 500 mm/min
    
    // Test spindle and coolant commands
    status = GCODE_ParseLine("M3 S12000");  // Start spindle
    // Should return STATUS_OK
    
    status = GCODE_ParseLine("M8");  // Coolant on
    // Should return STATUS_OK
    
    status = GCODE_ParseLine("M5");  // Stop spindle
    // Should return STATUS_OK
    
    status = GCODE_ParseLine("M9");  // Coolant off
    // Should return STATUS_OK
    
    // Test error conditions
    status = GCODE_ParseLine("G99");  // Invalid G-code
    // Should return error status
    
    status = GCODE_ParseLine("X10 Y");  // Missing value
    // Should return STATUS_BAD_NUMBER_FORMAT
    
    // Test real-time commands
    GCODE_ProcessRealTimeCommand('?');  // Status report
    GCODE_ProcessRealTimeCommand('!');  // Feed hold
    GCODE_ProcessRealTimeCommand('~');  // Cycle start
}

static inline void GCODE_SimulateUGSConnection(void)
{
    // Simulate Universal G-code Sender startup sequence
    
    // UGS typically sends these on connection:
    GCODE_ProcessRealTimeCommand(CMD_RESET);     // Reset controller
    
    // Wait for "Grbl 1.1h ['$' for help]" response
    
    GCODE_ProcessRealTimeCommand('?');           // Request status
    // Should respond: <Idle|MPos:0.000,0.000,0.000|FS:0,0>
    
    // UGS may send settings requests (not implemented yet):
    // GCODE_ParseLine("$$");                    // View settings
    // GCODE_ParseLine("$G");                    // View G-code parser state
    
    // Test some basic movement commands UGS might send:
    GCODE_ParseLine("G21");                      // Metric mode
    GCODE_ParseLine("G90");                      // Absolute mode
    GCODE_ParseLine("G94");                      // Feed rate mode
    
    GCODE_ProcessRealTimeCommand('?');           // Status check
    
    GCODE_ParseLine("G0 X10");                   // Test move
    GCODE_ParseLine("G0 X0");                    // Return to origin
    
    // UGS jog commands (when in jog mode):
    // GCODE_ParseLine("$J=G91X1F100");          // Jog +X 1mm at 100mm/min
    // GCODE_ProcessRealTimeCommand(0x85);       // Jog cancel
}

/*
 * Integration Notes:
 * 
 * 1. UART Configuration:
 *    - Baud rate: 115200 (GRBL standard)
 *    - 8 data bits, 1 stop bit, no parity
 *    - Hardware or software flow control optional
 * 
 * 2. Universal G-code Sender Setup:
 *    - Select "GRBL" as controller type
 *    - Set baud rate to 115200
 *    - Enable status polling (default 200ms)
 * 
 * 3. Other Compatible Software:
 *    - bCNC
 *    - Candle
 *    - CNCjs
 *    - ChiliPeppr
 *    - Grbl Panel
 * 
 * 4. Real-time Performance:
 *    - Status reports should respond within 1ms
 *    - Feed hold should stop motion within servo period
 *    - Reset should stop motion immediately
 * 
 * 5. Motion Buffer Management:
 *    - Parser fills motion buffer
 *    - Motion planner processes buffer
 *    - Look-ahead optimizes velocity profiles
 */

#endif /* GCODE_EXAMPLES_H */

/*******************************************************************************
 End of File
 */