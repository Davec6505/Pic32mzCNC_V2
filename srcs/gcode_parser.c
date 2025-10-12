#include "gcode_parser.h"
#include <stdlib.h> // For strtof
#include <string.h> // For memset
#include <ctype.h>  // For toupper

/**
 * @brief Parses a line of G-code text into a structured command.
 *
 * This function implements the logic to iterate through a G-code string,
 * identify command words (e.g., 'G', 'X', 'F'), and parse their associated
 * floating-point values. It populates the gcode_command_t structure, which
 * can then be used by the motion planner.
 *
 * The parser is designed to be robust, handling whitespace, comments,
 * and ignoring unknown command words.
 */
bool GCODE_ParseLine(const char *line, gcode_command_t *command)
{
    if (!line || !command)
    {
        return false;
    }

    // Initialize the command structure to a known state (all zeros)
    memset(command, 0, sizeof(gcode_command_t));

    int i = 0;
    bool has_word = false;

    while (line[i] != '\0')
    {
        // Skip leading whitespace
        while (isspace((unsigned char)line[i]))
        {
            i++;
        }

        // Stop parsing if a comment is found (standard G-code practice)
        if (line[i] == ';' || line[i] == '(')
        {
            break;
        }

        // Check for end of line
        if (line[i] == '\0')
        {
            break;
        }

        // Get the G-code address (the letter).
        char letter = toupper((unsigned char)line[i]);
        i++; // Consume the letter

        // Use strtof to parse the floating-point value that follows.
        char *end_ptr;
        float value;

        // G and M codes are special, they are integers.
        if (letter == 'G' || letter == 'M')
        {
            value = (float)strtol(&line[i], &end_ptr, 10);
        }
        else
        {
            value = strtof(&line[i], &end_ptr);
        }

        // If end_ptr is the same as the starting pointer, it means no number was parsed.
        if (end_ptr == &line[i])
        {
            // No number followed the letter. This could be a standalone command word.
            // We'll just move on.
            continue;
        }

        // A valid word and value were found
        has_word = true;
        i = end_ptr - line; // Move the index past the parsed number

        // Store the value in the appropriate field of the command struct
        // and set the corresponding bit in the 'words' bitmask.
        switch (letter)
        {
        case 'G':
            command->G = value;
            command->words |= WORD_G;
            break;
        case 'M':
            command->M = value;
            command->words |= WORD_M;
            break;
        case 'X':
            command->X = value;
            command->words |= WORD_X;
            break;
        case 'Y':
            command->Y = value;
            command->words |= WORD_Y;
            break;
        case 'Z':
            command->Z = value;
            command->words |= WORD_Z;
            break;
        case 'I':
            command->I = value;
            command->words |= WORD_I;
            break;
        case 'J':
            command->J = value;
            command->words |= WORD_J;
            break;
        case 'K':
            command->K = value;
            command->words |= WORD_K;
            break;
        case 'F':
            command->F = value;
            command->words |= WORD_F;
            break;
        case 'S':
            command->S = value;
            command->words |= WORD_S;
            break;
        case 'P':
            command->P = value;
            command->words |= WORD_P;
            break;
        case 'T':
            command->T = value;
            command->words |= WORD_T;
            break;
            // Default: Ignore any unknown letters, as per GRBL standard.
        }
    }

    // Return true if at least one valid G-code word was found, false otherwise.
    return has_word;
}
