/*******************************************************************************
  Motion G-code Parser Implementation File

  Company:
    Microchip Technology Inc.

  File Name:
    motion_gcode_parser.c

  Summary:
    This file contains the implementation of motion-focused G-code parsing.

  Description:
    This file implements functions for parsing G-code commands and converting
    them into motion blocks for the motion buffer system. It builds on the
    basic gcode_parser.h functions to provide motion-specific functionality.
*******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include "motion_gcode_parser.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// Parser state (global for this module)
static motion_parser_state_t parser_state;

// *****************************************************************************
// *****************************************************************************
// Section: Motion G-code Parser Functions
// *****************************************************************************
// *****************************************************************************

void MotionGCodeParser_Initialize(void)
{
    // Initialize parser state with defaults
    parser_state.current_position[0] = 0.0f; // X
    parser_state.current_position[1] = 0.0f; // Y
    parser_state.current_position[2] = 0.0f; // Z
    parser_state.current_feedrate = 100.0f;
    parser_state.current_spindle_state = 0;
    parser_state.current_spindle_speed = 0.0f;
    parser_state.current_coolant_state = 0;
    parser_state.current_plane = 17;             // XY plane
    parser_state.current_units = 21;             // Millimeters
    parser_state.current_distance_mode = 90;     // Absolute
    parser_state.current_feed_rate_mode = 94;    // Units per minute (default)
    parser_state.current_coordinate_system = 54; // G54
}

bool MotionGCodeParser_ParseMove(const char *command, motion_block_t *block)
{
    if (command == NULL || block == NULL)
    {
        return false;
    }

    // Initialize block with current position
    for (int i = 0; i < 3; i++)
    {
        block->target_pos[i] = parser_state.current_position[i];
    }

    block->feedrate = parser_state.current_feedrate;
    block->motion_type = (command[1] == '0') ? 0 : 1; // G0 = rapid, G1 = linear
    block->entry_velocity = 0.0f;
    block->exit_velocity = 0.0f;
    block->max_velocity = (block->motion_type == 0) ? DEFAULT_MAX_VELOCITY : parser_state.current_feedrate;
    block->distance = 0.0f;
    block->duration = 0.0f;
    block->is_valid = true;

    // Parse command using basic parser
    gcode_command_t parsed_command;
    if (!GCODE_ParseLine(command, &parsed_command))
    {
        return false;
    }

    // Extract coordinates if present
    if (parsed_command.words & WORD_X)
    {
        if (parser_state.current_distance_mode == 90)
        { // Absolute
            block->target_pos[0] = parsed_command.X;
        }
        else
        { // Incremental
            block->target_pos[0] = parser_state.current_position[0] + parsed_command.X;
        }
    }

    if (parsed_command.words & WORD_Y)
    {
        if (parser_state.current_distance_mode == 90)
        { // Absolute
            block->target_pos[1] = parsed_command.Y;
        }
        else
        { // Incremental
            block->target_pos[1] = parser_state.current_position[1] + parsed_command.Y;
        }
    }

    if (parsed_command.words & WORD_Z)
    {
        if (parser_state.current_distance_mode == 90)
        { // Absolute
            block->target_pos[2] = parsed_command.Z;
        }
        else
        { // Incremental
            block->target_pos[2] = parser_state.current_position[2] + parsed_command.Z;
        }
    }

    // Extract feedrate if present
    if (parsed_command.words & WORD_F)
    {
        if (parsed_command.F >= MIN_FEEDRATE && parsed_command.F <= MAX_FEEDRATE)
        {
            block->feedrate = parsed_command.F;
            parser_state.current_feedrate = parsed_command.F; // Update global feedrate
            if (block->motion_type == 1)
            { // Linear move
                block->max_velocity = parsed_command.F;
            }
        }
    }

    return true;
}

bool MotionGCodeParser_ParseArc(const char *command, motion_block_t *block)
{
    // For now, treat arcs as linear moves to endpoints
    // TODO: Implement proper arc interpolation with I, J, K parameters
    return MotionGCodeParser_ParseMove(command, block);
}

bool MotionGCodeParser_ParseDwell(const char *command, motion_block_t *block)
{
    if (command == NULL || block == NULL)
    {
        return false;
    }

    // Initialize as zero-movement block
    for (int i = 0; i < 3; i++)
    {
        block->target_pos[i] = parser_state.current_position[i]; // No movement
    }

    block->feedrate = 0.0f;
    block->motion_type = 4; // G4 dwell
    block->entry_velocity = 0.0f;
    block->exit_velocity = 0.0f;
    block->max_velocity = 0.0f;
    block->distance = 0.0f;
    block->is_valid = true;

    // Parse command to get dwell time
    gcode_command_t parsed_command;
    if (!GCODE_ParseLine(command, &parsed_command))
    {
        return false;
    }

    // Extract dwell time from P parameter
    if (parsed_command.words & WORD_P)
    {
        block->duration = parsed_command.P; // Store dwell time in duration field
        return true;
    }

    return false; // No valid P parameter found
}

bool MotionGCodeParser_ParseHome(const char *command, motion_block_t *block)
{
    if (command == NULL || block == NULL)
    {
        return false;
    }

    // G28/G30 - go to predefined position
    for (int i = 0; i < 3; i++)
    {
        block->target_pos[i] = 0.0f; // Home position
    }

    block->feedrate = DEFAULT_MAX_VELOCITY * 0.5f;      // Moderate homing speed
    block->motion_type = (command[2] == '8') ? 28 : 30; // G28 or G30
    block->entry_velocity = 0.0f;
    block->exit_velocity = 0.0f;
    block->max_velocity = block->feedrate;
    block->distance = 0.0f;
    block->duration = 0.0f;
    block->is_valid = true;

    return true;
}

void MotionGCodeParser_UpdateSpindleState(const char *command)
{
    if (command == NULL)
        return;

    gcode_command_t parsed_command;
    if (!GCODE_ParseLine(command, &parsed_command))
    {
        return;
    }

    if (parsed_command.words & WORD_M)
    {
        if (parsed_command.M == 3.0f)
        {
            parser_state.current_spindle_state = 1; // CW
        }
        else if (parsed_command.M == 4.0f)
        {
            parser_state.current_spindle_state = -1; // CCW
        }
        else if (parsed_command.M == 5.0f)
        {
            parser_state.current_spindle_state = 0; // Off
        }
    }

    // Parse spindle speed if present
    if (parsed_command.words & WORD_S)
    {
        parser_state.current_spindle_speed = parsed_command.S;
    }
}

void MotionGCodeParser_UpdateCoolantState(const char *command)
{
    if (command == NULL)
        return;

    gcode_command_t parsed_command;
    if (!GCODE_ParseLine(command, &parsed_command))
    {
        return;
    }

    if (parsed_command.words & WORD_M)
    {
        if (parsed_command.M == 8.0f)
        {
            parser_state.current_coolant_state = 1; // Flood coolant on
        }
        else if (parsed_command.M == 9.0f)
        {
            parser_state.current_coolant_state = 0; // All coolant off
        }
    }
}

void MotionGCodeParser_UpdatePlaneSelection(const char *command)
{
    if (command == NULL)
        return;

    gcode_command_t parsed_command;
    if (!GCODE_ParseLine(command, &parsed_command))
    {
        return;
    }

    if (parsed_command.words & WORD_G)
    {
        if (parsed_command.G == 17.0f)
        {
            parser_state.current_plane = 17; // XY plane
        }
        else if (parsed_command.G == 18.0f)
        {
            parser_state.current_plane = 18; // XZ plane
        }
        else if (parsed_command.G == 19.0f)
        {
            parser_state.current_plane = 19; // YZ plane
        }
    }
}

void MotionGCodeParser_UpdateUnits(const char *command)
{
    if (command == NULL)
        return;

    gcode_command_t parsed_command;
    if (!GCODE_ParseLine(command, &parsed_command))
    {
        return;
    }

    if (parsed_command.words & WORD_G)
    {
        if (parsed_command.G == 20.0f)
        {
            parser_state.current_units = 20; // Inches
        }
        else if (parsed_command.G == 21.0f)
        {
            parser_state.current_units = 21; // Millimeters
        }
    }
}

void MotionGCodeParser_UpdateDistanceMode(const char *command)
{
    if (command == NULL)
        return;

    gcode_command_t parsed_command;
    if (!GCODE_ParseLine(command, &parsed_command))
    {
        return;
    }

    if (parsed_command.words & WORD_G)
    {
        if (parsed_command.G == 90.0f)
        {
            parser_state.current_distance_mode = 90; // Absolute
        }
        else if (parsed_command.G == 91.0f)
        {
            parser_state.current_distance_mode = 91; // Incremental
        }
    }
}

void MotionGCodeParser_UpdateCoordinateOffset(const char *command)
{
    // G92 - Set coordinate system offset
    // For now, just acknowledge - full implementation would parse coordinates
    // and adjust the current position accordingly
    (void)command; // Suppress unused parameter warning
}

void MotionGCodeParser_UpdateWorkCoordinateSystem(const char *command)
{
    if (command == NULL)
        return;

    gcode_command_t parsed_command;
    if (!GCODE_ParseLine(command, &parsed_command))
    {
        return;
    }

    if (parsed_command.words & WORD_G)
    {
        int system = (int)parsed_command.G;
        if (system >= 54 && system <= 59)
        {
            parser_state.current_coordinate_system = system;
        }
    }
}

motion_parser_state_t MotionGCodeParser_GetState(void)
{
    return parser_state;
}

void MotionGCodeParser_UpdateFeedRateMode(const char *command)
{
    if (command == NULL)
        return;

    gcode_command_t parsed_command;
    if (!GCODE_ParseLine(command, &parsed_command))
    {
        return;
    }

    if (parsed_command.words & WORD_G)
    {
        if (parsed_command.G == 93.0f)
        {
            parser_state.current_feed_rate_mode = 93; // Inverse time mode
        }
        else if (parsed_command.G == 94.0f)
        {
            parser_state.current_feed_rate_mode = 94; // Units per minute mode (default)
        }
    }
}

void MotionGCodeParser_SetPosition(float x, float y, float z)
{
    parser_state.current_position[0] = x;
    parser_state.current_position[1] = y;
    parser_state.current_position[2] = z;
}

/*******************************************************************************
 End of File
 */