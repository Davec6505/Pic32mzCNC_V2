/*******************************************************************************
  G-code Helper Functions Header

  Company:
    Microchip Technology Inc.

  File Name:
    gcode_helpers.h

  Summary:
    Header file for G-code parsing and string manipulation utilities

  Description:
    This header defines the interface for G-code helper functions including:
    - String tokenization for compound G-code commands
    - G-code type enumeration and parsing
    - Motion command analysis structures
    - Utility functions for G-code processing
*******************************************************************************/

#ifndef GCODE_HELPERS_H
#define GCODE_HELPERS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// *****************************************************************************
// Configuration Constants
// *****************************************************************************

#define MAX_GCODE_TOKENS 16       // Maximum tokens in a compound G-code command
#define MAX_GCODE_TOKEN_LENGTH 32 // Maximum length of individual token

// *****************************************************************************
// G-code Type Enumeration
// *****************************************************************************

typedef enum
{
    GCODE_UNKNOWN = 999, // Use high number to avoid conflicts with actual G/M codes
    GCODE_G0 = 0,        // Rapid positioning
    GCODE_G1 = 1,        // Linear interpolation
    GCODE_G2 = 2,        // Circular interpolation CW
    GCODE_G3 = 3,        // Circular interpolation CCW
    GCODE_G4 = 4,        // Dwell
    GCODE_G10 = 10,      // Coordinate system data tool offset
    GCODE_G17 = 17,      // XY plane selection
    GCODE_G18 = 18,      // XZ plane selection
    GCODE_G19 = 19,      // YZ plane selection
    GCODE_G20 = 20,      // Programming in inches
    GCODE_G21 = 21,      // Programming in millimeters
    GCODE_G28 = 28,      // Return to home position
    GCODE_G30 = 30,      // Return to secondary home position
    GCODE_G54 = 54,      // Work coordinate system 1
    GCODE_G55 = 55,      // Work coordinate system 2
    GCODE_G56 = 56,      // Work coordinate system 3
    GCODE_G57 = 57,      // Work coordinate system 4
    GCODE_G58 = 58,      // Work coordinate system 5
    GCODE_G59 = 59,      // Work coordinate system 6
    GCODE_G90 = 90,      // Absolute positioning
    GCODE_G91 = 91,      // Incremental positioning
    GCODE_G92 = 92,      // Coordinate system offset
    GCODE_G93 = 93,      // Inverse time feed mode
    GCODE_G94 = 94,      // Units per minute feed mode
    GCODE_M3 = 103,      // Spindle on, clockwise (offset by 100 to avoid G-code conflicts)
    GCODE_M4 = 104,      // Spindle on, counterclockwise
    GCODE_M5 = 105,      // Spindle stop
    GCODE_M8 = 108,      // Coolant on
    GCODE_M9 = 109,      // Coolant off
    GCODE_F = 200        // Feed rate (use unique high number)
} gcode_type_t;

// *****************************************************************************
// Data Structures
// *****************************************************************************

// 3D coordinate structure
typedef struct
{
    float x;
    float y;
    float z;
} coordinate_t;

// Motion command analysis result
typedef struct
{
    // Motion information
    bool has_motion;
    gcode_type_t motion_type;

    // Coordinates
    coordinate_t coordinates;
    bool has_x;
    bool has_y;
    bool has_z;

    // Arc parameters (for G2/G3)
    coordinate_t arc_center; // I, J, K values
    bool has_arc_center;

    // Feed and spindle
    float feedrate;
    bool has_feedrate;
    float spindle_speed;
    bool has_spindle_speed;

    // Modal states
    bool units_inches;  // true = inches (G20), false = mm (G21)
    bool absolute_mode; // true = absolute (G90), false = incremental (G91)
} motion_analysis_t;

// *****************************************************************************
// Function Declarations
// *****************************************************************************

// String tokenization functions
int GCodeHelpers_TokenizeString(const char *input, char tokens[MAX_GCODE_TOKENS][MAX_GCODE_TOKEN_LENGTH]);

// G-code type detection and parsing
gcode_type_t GCodeHelpers_ParseGCodeType(const char *token);
bool GCodeHelpers_IsMotionCommand(gcode_type_t gcode);
bool GCodeHelpers_IsCoordinateToken(const char *token);

// G-code command analysis
void GCodeHelpers_SetDebugCallback(void (*callback)(const char *));
void GCodeHelpers_DebugTokens(char tokens[MAX_GCODE_TOKENS][MAX_GCODE_TOKEN_LENGTH], int token_count);
motion_analysis_t GCodeHelpers_AnalyzeMotionTokens(char tokens[MAX_GCODE_TOKENS][MAX_GCODE_TOKEN_LENGTH], int token_count);

// String utility functions
bool GCodeHelpers_IsValidCommand(const char *command);
void GCodeHelpers_CleanCommand(const char *input, char *output, size_t output_size);
const char *GCodeHelpers_GetGCodeTypeName(gcode_type_t gcode);

// Position tracking and conversion functions
void GCodeHelpers_GetCurrentPositionFromSteps(float *x, float *y, float *z);
void GCodeHelpers_GetPositionInSteps(int32_t *x_steps, int32_t *y_steps, int32_t *z_steps);
void GCodeHelpers_ConvertStepsToPosition(int32_t x_steps, int32_t y_steps, int32_t z_steps, float *x, float *y, float *z);

#endif // GCODE_HELPERS_H