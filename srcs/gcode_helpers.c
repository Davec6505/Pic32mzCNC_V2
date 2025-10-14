/*******************************************************************************
  G-code Helper Functions

  Company:
    Microchip Technology Inc.

  File Name:
    gcode_helpers.c

  Summary:
    G-code parsing and string manipulation helper functions

  Description:
    This file contains utility functions for G-code parsing including:
    - MikroC-style string tokenizer for compound G-code commands
    - G-code type parsing and validation
    - String manipulation utilities for motion commands

    This modular approach keeps the main application cleaner and provides
    reusable utilities for complex G-code parsing operations.
*******************************************************************************/

#include "gcode_helpers.h"
#include "grbl_settings.h"
#include "app.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// Forward declarations for callback function
static void (*debug_print_callback)(const char *) = NULL;

// *****************************************************************************
// MikroC-Style String Tokenizer Implementation
// *****************************************************************************

int GCodeHelpers_TokenizeString(const char *input, char tokens[MAX_GCODE_TOKENS][MAX_GCODE_TOKEN_LENGTH])
{
    int token_count = 0;
    int token_pos = 0;
    int i = 0;
    bool in_token = false;

    // Clear all tokens first
    for (int j = 0; j < MAX_GCODE_TOKENS; j++)
    {
        tokens[j][0] = '\0';
    }

    // Skip leading whitespace and special characters
    while (input[i] != '\0' && (input[i] == ' ' || input[i] == '\t' || input[i] == '$' || input[i] == '='))
    {
        i++;
    }

    while (input[i] != '\0' && token_count < MAX_GCODE_TOKENS)
    {
        char c = input[i];

        // Skip spaces and tabs - they separate tokens
        if (c == ' ' || c == '\t')
        {
            if (in_token)
            {
                // End current token
                tokens[token_count][token_pos] = '\0';
                token_count++;
                token_pos = 0;
                in_token = false;
            }
            i++;
            continue;
        }

        // Check for letter transitions (G21G91 -> G21, G91)
        // This handles compound commands without spaces
        if (in_token && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
        {
            // Check if current token already starts with a letter
            char first_char = tokens[token_count][0];
            if ((first_char >= 'A' && first_char <= 'Z') || (first_char >= 'a' && first_char <= 'z'))
            {
                // End current token and start new one
                tokens[token_count][token_pos] = '\0';
                token_count++;
                token_pos = 0;

                if (token_count >= MAX_GCODE_TOKENS)
                    break;
            }
        }

        // Add character to current token
        if (token_pos < MAX_GCODE_TOKEN_LENGTH - 1)
        {
            tokens[token_count][token_pos] = c;
            token_pos++;
            in_token = true;
        }

        i++;
    }

    // Close last token if we were building one
    if (in_token && token_count < MAX_GCODE_TOKENS)
    {
        tokens[token_count][token_pos] = '\0';
        token_count++;
    }

    return token_count;
}

// *****************************************************************************
// G-code Type Detection and Parsing
// *****************************************************************************

gcode_type_t GCodeHelpers_ParseGCodeType(const char *token)
{
    if (token == NULL || token[0] == '\0')
    {
        return GCODE_UNKNOWN;
    }

    char first_char = token[0];

    if (first_char == 'G' || first_char == 'g')
    {
        int code_num = atoi(&token[1]);
        switch (code_num)
        {
        case 0:
            return GCODE_G0;
        case 1:
            return GCODE_G1;
        case 2:
            return GCODE_G2;
        case 3:
            return GCODE_G3;
        case 4:
            return GCODE_G4;
        case 10:
            return GCODE_G10;
        case 17:
            return GCODE_G17;
        case 18:
            return GCODE_G18;
        case 19:
            return GCODE_G19;
        case 20:
            return GCODE_G20;
        case 21:
            return GCODE_G21;
        case 28:
            return GCODE_G28;
        case 30:
            return GCODE_G30;
        case 54:
            return GCODE_G54;
        case 55:
            return GCODE_G55;
        case 56:
            return GCODE_G56;
        case 57:
            return GCODE_G57;
        case 58:
            return GCODE_G58;
        case 59:
            return GCODE_G59;
        case 90:
            return GCODE_G90;
        case 91:
            return GCODE_G91;
        case 92:
            return GCODE_G92;
        case 93:
            return GCODE_G93;
        case 94:
            return GCODE_G94;
        default:
            return GCODE_UNKNOWN;
        }
    }
    else if (first_char == 'M' || first_char == 'm')
    {
        int code_num = atoi(&token[1]);
        switch (code_num)
        {
        case 3:
            return GCODE_M3;
        case 4:
            return GCODE_M4;
        case 5:
            return GCODE_M5;
        case 8:
            return GCODE_M8;
        case 9:
            return GCODE_M9;
        default:
            return GCODE_UNKNOWN;
        }
    }
    else if (first_char == 'F' || first_char == 'f')
    {
        return GCODE_F;
    }

    return GCODE_UNKNOWN;
}

bool GCodeHelpers_IsMotionCommand(gcode_type_t gcode)
{
    return (gcode == GCODE_G0 || gcode == GCODE_G1 || gcode == GCODE_G2 || gcode == GCODE_G3);
}

bool GCodeHelpers_IsCoordinateToken(const char *token)
{
    if (token == NULL || token[0] == '\0')
    {
        return false;
    }

    char first_char = token[0];
    return (first_char == 'X' || first_char == 'x' ||
            first_char == 'Y' || first_char == 'y' ||
            first_char == 'Z' || first_char == 'z' ||
            first_char == 'I' || first_char == 'i' ||
            first_char == 'J' || first_char == 'j' ||
            first_char == 'K' || first_char == 'k');
}

// *****************************************************************************
// G-code Command Analysis and Processing
// *****************************************************************************

void GCodeHelpers_SetDebugCallback(void (*callback)(const char *))
{
    debug_print_callback = callback;
}

void GCodeHelpers_DebugTokens(char tokens[MAX_GCODE_TOKENS][MAX_GCODE_TOKEN_LENGTH], int token_count)
{
    if (debug_print_callback == NULL)
    {
        return; // No debug output if callback not set
    }

    char debug_msg[512];
    sprintf(debug_msg, "[DEBUG: Tokenized %d items: ", token_count);

    for (int i = 0; i < token_count && i < MAX_GCODE_TOKENS; i++)
    {
        strcat(debug_msg, "'");
        strncat(debug_msg, tokens[i], MAX_GCODE_TOKEN_LENGTH - 1);
        strcat(debug_msg, "'");
        if (i < token_count - 1)
        {
            strcat(debug_msg, " ");
        }
    }
    strcat(debug_msg, "]\r\n");

    debug_print_callback(debug_msg);
}

motion_analysis_t GCodeHelpers_AnalyzeMotionTokens(char tokens[MAX_GCODE_TOKENS][MAX_GCODE_TOKEN_LENGTH], int token_count)
{
    motion_analysis_t analysis = {0}; // Initialize all fields to 0/false

    for (int i = 0; i < token_count; i++)
    {
        char *token = tokens[i];
        if (token[0] == '\0')
            continue; // Skip empty tokens

        gcode_type_t gcode_type = GCodeHelpers_ParseGCodeType(token);

        // Check for motion commands
        if (GCodeHelpers_IsMotionCommand(gcode_type))
        {
            analysis.has_motion = true;
            analysis.motion_type = gcode_type;
        }

        // Parse coordinates
        char first_char = token[0];
        if (GCodeHelpers_IsCoordinateToken(token))
        {
            float value = atof(&token[1]);

            switch (first_char)
            {
            case 'X':
            case 'x':
                analysis.coordinates.x = value;
                analysis.has_x = true;
                break;
            case 'Y':
            case 'y':
                analysis.coordinates.y = value;
                analysis.has_y = true;
                break;
            case 'Z':
            case 'z':
                analysis.coordinates.z = value;
                analysis.has_z = true;
                break;
            case 'I':
            case 'i':
                analysis.arc_center.x = value;
                analysis.has_arc_center = true;
                break;
            case 'J':
            case 'j':
                analysis.arc_center.y = value;
                analysis.has_arc_center = true;
                break;
            case 'K':
            case 'k':
                analysis.arc_center.z = value;
                analysis.has_arc_center = true;
                break;
            }
        }
        else if (first_char == 'F' || first_char == 'f')
        {
            analysis.feedrate = atof(&token[1]);
            analysis.has_feedrate = true;
        }
        else if (first_char == 'S' || first_char == 's')
        {
            analysis.spindle_speed = atof(&token[1]);
            analysis.has_spindle_speed = true;
        }

        // Check for modal commands (units, positioning mode, etc.)
        switch (gcode_type)
        {
        case GCODE_G20:
            analysis.units_inches = true;
            break;
        case GCODE_G21:
            analysis.units_inches = false;
            break;
        case GCODE_G90:
            analysis.absolute_mode = true;
            break;
        case GCODE_G91:
            analysis.absolute_mode = false;
            break;
        default:
            break;
        }
    }

    // Detect implicit motion commands (for jogging)
    // If we have coordinates and feedrate but no explicit motion command,
    // assume G1 (linear interpolation)
    if (!analysis.has_motion &&
        (analysis.has_x || analysis.has_y || analysis.has_z) &&
        analysis.has_feedrate)
    {
        analysis.has_motion = true;
        analysis.motion_type = GCODE_G1; // Default to linear motion for jogging
    }

    return analysis;
}

// *****************************************************************************
// String Utility Functions
// *****************************************************************************

bool GCodeHelpers_IsValidCommand(const char *command)
{
    if (command == NULL || command[0] == '\0')
    {
        return false;
    }

    // Skip leading whitespace
    while (*command == ' ' || *command == '\t')
    {
        command++;
    }

    // Check for comment lines
    if (*command == ';' || *command == '(' || *command == '%')
    {
        return false;
    }

    // Check for empty line
    if (*command == '\0' || *command == '\r' || *command == '\n')
    {
        return false;
    }

    return true;
}

void GCodeHelpers_CleanCommand(const char *input, char *output, size_t output_size)
{
    if (input == NULL || output == NULL || output_size == 0)
    {
        return;
    }

    size_t j = 0;
    bool in_comment = false;

    for (size_t i = 0; i < strlen(input) && j < output_size - 1; i++)
    {
        char c = input[i];

        // Handle comments
        if (c == ';' || c == '(')
        {
            in_comment = true;
            continue;
        }
        if (c == ')')
        {
            in_comment = false;
            continue;
        }
        if (in_comment)
        {
            continue;
        }

        // Skip line endings
        if (c == '\r' || c == '\n')
        {
            break;
        }

        // Convert to uppercase for consistency
        if (c >= 'a' && c <= 'z')
        {
            c = c - 'a' + 'A';
        }

        output[j++] = c;
    }

    output[j] = '\0';

    // Trim trailing whitespace
    while (j > 0 && (output[j - 1] == ' ' || output[j - 1] == '\t'))
    {
        output[--j] = '\0';
    }
}

const char *GCodeHelpers_GetGCodeTypeName(gcode_type_t gcode)
{
    switch (gcode)
    {
    case GCODE_G0:
        return "G0 (Rapid)";
    case GCODE_G1:
        return "G1 (Linear)";
    case GCODE_G2:
        return "G2 (Arc CW)";
    case GCODE_G3:
        return "G3 (Arc CCW)";
    case GCODE_G4:
        return "G4 (Dwell)";
    case GCODE_G10:
        return "G10 (Coordinate)";
    case GCODE_G17:
        return "G17 (XY Plane)";
    case GCODE_G18:
        return "G18 (XZ Plane)";
    case GCODE_G19:
        return "G19 (YZ Plane)";
    case GCODE_G20:
        return "G20 (Inches)";
    case GCODE_G21:
        return "G21 (Millimeters)";
    case GCODE_G28:
        return "G28 (Home)";
    case GCODE_G30:
        return "G30 (Home2)";
    case GCODE_G90:
        return "G90 (Absolute)";
    case GCODE_G91:
        return "G91 (Incremental)";
    case GCODE_G92:
        return "G92 (Coordinate Set)";
    case GCODE_M3:
        return "M3 (Spindle CW)";
    case GCODE_M4:
        return "M4 (Spindle CCW)";
    case GCODE_M5:
        return "M5 (Spindle Stop)";
    case GCODE_M8:
        return "M8 (Coolant On)";
    case GCODE_M9:
        return "M9 (Coolant Off)";
    case GCODE_F:
        return "F (Feedrate)";
    default:
        return "Unknown";
    }
}

// *****************************************************************************
// Position Tracking and Conversion Functions
// *****************************************************************************

void GCodeHelpers_GetCurrentPositionFromSteps(float *x, float *y, float *z)
{
    /* Convert step counts to real-world coordinates on-demand
     * This is much more efficient than continuously updating position calculations
     * Only called when UGS requests status - not every step!
     */

    // Get steps per mm settings using GRBL function with safety checks
    float x_steps_per_mm = GRBL_GetSetting(SETTING_X_STEPS_PER_MM);
    float y_steps_per_mm = GRBL_GetSetting(SETTING_Y_STEPS_PER_MM);
    float z_steps_per_mm = GRBL_GetSetting(SETTING_Z_STEPS_PER_MM);

    // Fallback to defaults if GRBL settings invalid (matches app.c execution defaults)
    // CRITICAL: Must match steps_per_mm used in APP_ExecuteMotionBlock() for accurate status reports
    if (x_steps_per_mm <= 0.0f || isnan(x_steps_per_mm))
    {
        x_steps_per_mm = 250.0f; // Default: matches GRBL $100 setting and app.c fallback
    }
    if (y_steps_per_mm <= 0.0f || isnan(y_steps_per_mm))
    {
        y_steps_per_mm = 250.0f; // Default: matches GRBL $101 setting and app.c fallback
    }
    if (z_steps_per_mm <= 0.0f || isnan(z_steps_per_mm))
    {
        z_steps_per_mm = 250.0f; // Default: matches GRBL $102 setting and app.c fallback
    }

    // Get current step positions
    int32_t x_steps = APP_GetAxisCurrentPosition(0);
    int32_t y_steps = APP_GetAxisCurrentPosition(1);
    int32_t z_steps = APP_GetAxisCurrentPosition(2);

    // Convert step counts to millimeters using validated steps_per_mm settings
    *x = (float)x_steps / x_steps_per_mm;
    *y = (float)y_steps / y_steps_per_mm;
    *z = (float)z_steps / z_steps_per_mm;
}

void GCodeHelpers_GetPositionInSteps(int32_t *x_steps, int32_t *y_steps, int32_t *z_steps)
{
    /* Get current position in raw step counts
     * Useful for debugging and step-based calculations
     */
    *x_steps = APP_GetAxisCurrentPosition(0);
    *y_steps = APP_GetAxisCurrentPosition(1);
    *z_steps = APP_GetAxisCurrentPosition(2);
}

void GCodeHelpers_ConvertStepsToPosition(int32_t x_steps, int32_t y_steps, int32_t z_steps, float *x, float *y, float *z)
{
    /* Convert any step counts to real-world coordinates
     * Utility function for converting steps to position values
     */

    // Get steps per mm settings using GRBL function with safety checks
    float x_steps_per_mm = GRBL_GetSetting(SETTING_X_STEPS_PER_MM);
    float y_steps_per_mm = GRBL_GetSetting(SETTING_Y_STEPS_PER_MM);
    float z_steps_per_mm = GRBL_GetSetting(SETTING_Z_STEPS_PER_MM);

    // Safety check: Use default values if GRBL settings return invalid values (0, NaN, or negative)
    // TEMPORARY FIX: Use motion planner values (400 steps/mm) since GRBL_GetSetting returns 0
    if (x_steps_per_mm <= 0.0f || isnan(x_steps_per_mm))
    {
        x_steps_per_mm = 400.0f; // Match motion planner value (was GRBL_DEFAULT_X_STEPS_PER_MM 160.0f)
    }
    if (y_steps_per_mm <= 0.0f || isnan(y_steps_per_mm))
    {
        y_steps_per_mm = 400.0f; // Match motion planner value (was GRBL_DEFAULT_Y_STEPS_PER_MM 160.0f)
    }
    if (z_steps_per_mm <= 0.0f || isnan(z_steps_per_mm))
    {
        z_steps_per_mm = 400.0f; // Match motion planner value (was GRBL_DEFAULT_Z_STEPS_PER_MM 160.0f)
    }

    // Convert step counts to millimeters using validated steps_per_mm settings
    *x = (float)x_steps / x_steps_per_mm;
    *y = (float)y_steps / y_steps_per_mm;
    *z = (float)z_steps / z_steps_per_mm;
}