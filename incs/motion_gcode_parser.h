/*******************************************************************************
  Motion G-code Parser Header File

  Company:
    Microchip Technology Inc.

  File Name:
    motion_gcode_parser.h

  Summary:
    This header file provides the prototypes and definitions for motion-focused
    G-code parsing functionality.

  Description:
    This file declares the functions used for parsing G-code commands and
    converting them into motion blocks for the motion buffer system. This is
    a higher-level parser that builds on the basic GCODE_ParseLine function.
*******************************************************************************/

#ifndef MOTION_GCODE_PARSER_H
#define MOTION_GCODE_PARSER_H

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************
#include <stdint.h>
#include <stdbool.h>
#include "app.h"          // For motion_block_t definition
#include "gcode_parser.h" // For basic parsing functions

// *****************************************************************************
// *****************************************************************************
// Section: Constants
// *****************************************************************************
// *****************************************************************************

// G-code parsing constants
#define DEFAULT_MAX_VELOCITY 1000.0f // Default rapid velocity
#define MIN_FEEDRATE 1.0f            // Minimum feedrate
#define MAX_FEEDRATE 10000.0f        // Maximum feedrate

// *****************************************************************************
// *****************************************************************************
// Section: Data Types
// *****************************************************************************
// *****************************************************************************

// Parser state structure
typedef struct
{
    float current_position[3];     // Current X, Y, Z position
    float current_feedrate;        // Current feedrate
    int current_spindle_state;     // 0=off, 1=CW, -1=CCW
    float current_spindle_speed;   // RPM
    int current_coolant_state;     // 0=off, 1=flood
    int current_plane;             // G17=XY, G18=XZ, G19=YZ
    int current_units;             // G20=inches, G21=mm
    int current_distance_mode;     // G90=absolute, G91=incremental
    int current_coordinate_system; // G54-G59
} motion_parser_state_t;

// *****************************************************************************
// *****************************************************************************
// Section: Interface Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void MotionGCodeParser_Initialize(void)

  Summary:
    Initializes the motion G-code parser system.

  Description:
    This function initializes the parser state with default values.

  Parameters:
    None

  Returns:
    None
*******************************************************************************/
void MotionGCodeParser_Initialize(void);

/*******************************************************************************
  Function:
    bool MotionGCodeParser_ParseMove(const char *command, motion_block_t *block)

  Summary:
    Parses a G0 or G1 move command.

  Description:
    This function parses linear move commands and converts them into motion blocks.

  Parameters:
    command - G-code command string to parse
    block - Pointer to motion block to populate

  Returns:
    true - Command parsed successfully
    false - Parse error occurred
*******************************************************************************/
bool MotionGCodeParser_ParseMove(const char *command, motion_block_t *block);

/*******************************************************************************
  Function:
    bool MotionGCodeParser_ParseArc(const char *command, motion_block_t *block)

  Summary:
    Parses a G2 or G3 arc command.

  Parameters:
    command - G-code command string to parse
    block - Pointer to motion block to populate

  Returns:
    true - Command parsed successfully
    false - Parse error occurred
*******************************************************************************/
bool MotionGCodeParser_ParseArc(const char *command, motion_block_t *block);

/*******************************************************************************
  Function:
    bool MotionGCodeParser_ParseDwell(const char *command, motion_block_t *block)

  Summary:
    Parses a G4 dwell command.

  Parameters:
    command - G-code command string to parse
    block - Pointer to motion block to populate

  Returns:
    true - Command parsed successfully
    false - Parse error occurred
*******************************************************************************/
bool MotionGCodeParser_ParseDwell(const char *command, motion_block_t *block);

/*******************************************************************************
  Function:
    bool MotionGCodeParser_ParseHome(const char *command, motion_block_t *block)

  Summary:
    Parses a G28 or G30 homing command.

  Parameters:
    command - G-code command string to parse
    block - Pointer to motion block to populate

  Returns:
    true - Command parsed successfully
    false - Parse error occurred
*******************************************************************************/
bool MotionGCodeParser_ParseHome(const char *command, motion_block_t *block);

/*******************************************************************************
  Function:
    void MotionGCodeParser_UpdateSpindleState(const char *command)

  Summary:
    Updates spindle state based on M3/M4/M5 commands.

  Parameters:
    command - G-code command string containing spindle command

  Returns:
    None
*******************************************************************************/
void MotionGCodeParser_UpdateSpindleState(const char *command);

/*******************************************************************************
  Function:
    void MotionGCodeParser_UpdateCoolantState(const char *command)

  Summary:
    Updates coolant state based on M8/M9 commands.

  Parameters:
    command - G-code command string containing coolant command

  Returns:
    None
*******************************************************************************/
void MotionGCodeParser_UpdateCoolantState(const char *command);

/*******************************************************************************
  Function:
    void MotionGCodeParser_UpdatePlaneSelection(const char *command)

  Summary:
    Updates plane selection based on G17/G18/G19 commands.

  Parameters:
    command - G-code command string containing plane command

  Returns:
    None
*******************************************************************************/
void MotionGCodeParser_UpdatePlaneSelection(const char *command);

/*******************************************************************************
  Function:
    void MotionGCodeParser_UpdateUnits(const char *command)

  Summary:
    Updates units based on G20/G21 commands.

  Parameters:
    command - G-code command string containing units command

  Returns:
    None
*******************************************************************************/
void MotionGCodeParser_UpdateUnits(const char *command);

/*******************************************************************************
  Function:
    void MotionGCodeParser_UpdateDistanceMode(const char *command)

  Summary:
    Updates distance mode based on G90/G91 commands.

  Parameters:
    command - G-code command string containing distance mode command

  Returns:
    None
*******************************************************************************/
void MotionGCodeParser_UpdateDistanceMode(const char *command);

/*******************************************************************************
  Function:
    void MotionGCodeParser_UpdateCoordinateOffset(const char *command)

  Summary:
    Updates coordinate system offset based on G92 commands.

  Parameters:
    command - G-code command string containing coordinate command

  Returns:
    None
*******************************************************************************/
void MotionGCodeParser_UpdateCoordinateOffset(const char *command);

/*******************************************************************************
  Function:
    void MotionGCodeParser_UpdateWorkCoordinateSystem(const char *command)

  Summary:
    Updates work coordinate system based on G54-G59 commands.

  Parameters:
    command - G-code command string containing coordinate system command

  Returns:
    None
*******************************************************************************/
void MotionGCodeParser_UpdateWorkCoordinateSystem(const char *command);

/*******************************************************************************
  Function:
    motion_parser_state_t MotionGCodeParser_GetState(void)

  Summary:
    Gets the current parser state.

  Parameters:
    None

  Returns:
    Current parser state structure
*******************************************************************************/
motion_parser_state_t MotionGCodeParser_GetState(void);

/*******************************************************************************
  Function:
    void MotionGCodeParser_SetPosition(float x, float y, float z)

  Summary:
    Sets the current position in the parser state.

  Parameters:
    x - X coordinate
    y - Y coordinate
    z - Z coordinate

  Returns:
    None
*******************************************************************************/
void MotionGCodeParser_SetPosition(float x, float y, float z);

#endif /* MOTION_GCODE_PARSER_H */