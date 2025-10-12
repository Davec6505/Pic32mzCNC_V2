#ifndef GCODE_PARSER_H
#define GCODE_PARSER_H

#include <stdint.h>
#include <stdbool.h>

// Bitmask to track which G-code words were found on a line
typedef enum
{
    WORD_G = 1 << 0,
    WORD_M = 1 << 1,
    WORD_X = 1 << 2,
    WORD_Y = 1 << 3,
    WORD_Z = 1 << 4,
    WORD_I = 1 << 5,
    WORD_J = 1 << 6,
    WORD_K = 1 << 7,
    WORD_F = 1 << 8,
    WORD_S = 1 << 9,
    WORD_P = 1 << 10,
    WORD_T = 1 << 11,
} gcode_word_t;

// Structure to hold a parsed G-code command
typedef struct
{
    uint32_t words; // Bitmask of words found in the command
    float G;        // G-command (e.g., 0.0, 1.0, 90.0)
    float M;        // M-command
    float X;        // X-axis coordinate
    float Y;        // Y-axis coordinate
    float Z;        // Z-axis coordinate
    float I;        // Arc I-offset
    float J;        // Arc J-offset
    float K;        // Arc K-offset
    float F;        // Feedrate
    float S;        // Spindle speed
    float P;        // Misc parameter (e.g., dwell time)
    float T;        // Tool number
} gcode_command_t;

/**
 * @brief Parses a line of G-code text into a structured command.
 *
 * This function takes a null-terminated string containing a G-code command
 * and populates a gcode_command_t structure with the parsed values. It handles
 * various G-code words (G, M, X, Y, Z, F, etc.) and their numeric values.
 *
 * @param line A pointer to the character string containing the G-code line.
 * @param command A pointer to the gcode_command_t structure to be populated.
 * @return true if parsing was successful, false if the line was empty or invalid.
 */
bool GCODE_ParseLine(const char *line, gcode_command_t *command);

#endif // GCODE_PARSER_H
