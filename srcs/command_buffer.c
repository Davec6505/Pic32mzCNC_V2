/**
 * @file command_buffer.c
 * @brief Command separation and buffering implementation
 *
 * Splits concatenated G-code commands into individual executable units.
 *
 * Algorithm:
 *   1. Start with tokenized line (e.g., ["G92", "G0", "X10", "Y10", "F200"])
 *   2. Classify each token (is it a command or parameter?)
 *   3. Group tokens into commands based on GRBL modal groups
 *   4. Store commands in ring buffer
 *
 * Example Flow:
 *   Input:  "G92G0X10Y10F200G1X20"
 *   Tokens: ["G92", "G0", "X10", "Y10", "F200", "G1", "X20"]
 *   Split:  Command 1: ["G92"]
 *           Command 2: ["G0", "X10", "Y10", "F200"]
 *           Command 3: ["G1", "X20"]
 *
 * @date October 17, 2025
 */

// Debug configuration
// *****************************************************************************
// Debug Configuration
// *****************************************************************************
// #define DEBUG_MOTION_BUFFER // Enable debug output (DISABLED - floods UGS)

// *****************************************************************************

#include "command_buffer.h"
#include "ugs_interface.h" // For debug output
#include <stdio.h>         // For sscanf
#include <string.h>
#include <ctype.h>

//=============================================================================
// STATIC BUFFER
//=============================================================================

static command_buffer_t cmd_buffer;

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Check if token is a parameter (X, Y, Z, F, S, etc.)
 *
 * Parameters attach to the previous command.
 *
 * @param token Token string (e.g., "X10", "F1500")
 * @return true if parameter
 */
static bool is_parameter_token(const char *token)
{
    if (token == NULL || token[0] == '\0')
    {
        return false;
    }

    char first = (char)toupper((int)token[0]);

    // Axis parameters
    if (first == 'X' || first == 'Y' || first == 'Z' ||
        first == 'A' || first == 'B' || first == 'C')
    {
        return true;
    }

    // IJK arc parameters
    if (first == 'I' || first == 'J' || first == 'K')
    {
        return true;
    }

    // Feedrate, Spindle, Tool, etc.
    if (first == 'F' || first == 'S' || first == 'T' ||
        first == 'P' || first == 'L' || first == 'R' ||
        first == 'D' || first == 'H' || first == 'N')
    {
        return true;
    }

    return false;
}

/**
 * @brief Check if token starts a new command
 *
 * New commands: G-codes, M-codes
 *
 * @param token Token string
 * @return true if command starter
 */
static bool is_command_token(const char *token)
{
    if (token == NULL || token[0] == '\0')
    {
        return false;
    }

    char first = (char)toupper((int)token[0]);

    return (first == 'G' || first == 'M');
}

//=============================================================================
// INITIALIZATION
//=============================================================================

void CommandBuffer_Initialize(void)
{
    memset(&cmd_buffer, 0, sizeof(command_buffer_t));
    cmd_buffer.head = 0;
    cmd_buffer.tail = 0;
    cmd_buffer.count = 0;
}

//=============================================================================
// COMMAND CLASSIFICATION
//=============================================================================

command_type_t CommandBuffer_ClassifyToken(const char *token)
{
    if (token == NULL || token[0] == '\0')
    {
        return CMD_UNKNOWN;
    }

    char first = (char)toupper((int)token[0]);

    if (first == 'G')
    {
        // Parse G-code number
        int code = 0;
        if (sscanf(&token[1], "%d", &code) == 1)
        {
            switch (code)
            {
            case 0:
                return CMD_G0;
            case 1:
                return CMD_G1;
            case 2:
                return CMD_G2;
            case 3:
                return CMD_G3;
            case 4:
                return CMD_G4;
            case 17:
                return CMD_G17;
            case 18:
                return CMD_G18;
            case 19:
                return CMD_G19;
            case 20:
                return CMD_G20;
            case 21:
                return CMD_G21;
            case 28:
                return CMD_G28;
            case 30:
                return CMD_G30;
            case 90:
                return CMD_G90;
            case 91:
                return CMD_G91;
            case 92:
                return CMD_G92;
            default:
                return CMD_UNKNOWN;
            }
        }
    }
    else if (first == 'M')
    {
        // Parse M-code number
        int code = 0;
        if (sscanf(&token[1], "%d", &code) == 1)
        {
            switch (code)
            {
            case 0:
                return CMD_M0;
            case 1:
                return CMD_M1;
            case 2:
                return CMD_M2;
            case 3:
                return CMD_M3;
            case 4:
                return CMD_M4;
            case 5:
                return CMD_M5;
            case 7:
                return CMD_M7;
            case 8:
                return CMD_M8;
            case 9:
                return CMD_M9;
            case 30:
                return CMD_M30;
            default:
                return CMD_UNKNOWN;
            }
        }
    }

    return CMD_UNKNOWN;
}

//=============================================================================
// COMMAND SPLITTING (CORE ALGORITHM)
//=============================================================================

uint8_t CommandBuffer_SplitLine(const gcode_line_t *tokenized_line)
{
    if (tokenized_line == NULL)
    {
        return 0;
    }

    uint8_t commands_added = 0;
    command_entry_t current_command;
    memset(&current_command, 0, sizeof(command_entry_t));

    bool building_command = false;

    // Iterate through all tokens
    for (uint8_t i = 0; i < tokenized_line->token_count; i++)
    {
        const char *token = tokenized_line->tokens[i];

        if (is_command_token(token))
        {
            // Check if we need to save previous command
            if (building_command && current_command.token_count > 0)
            {
                // Buffer full check
                if (cmd_buffer.count >= COMMAND_BUFFER_SIZE)
                {
                    return commands_added; // Buffer full, stop adding
                }

                // Add previous command to buffer
                memcpy(&cmd_buffer.commands[cmd_buffer.head],
                       &current_command,
                       sizeof(command_entry_t));

                cmd_buffer.head = (cmd_buffer.head + 1U) % COMMAND_BUFFER_SIZE;
                cmd_buffer.count++;
                commands_added++;

                // Reset for new command
                memset(&current_command, 0, sizeof(command_entry_t));
            }

            // Start new command
            building_command = true;
            current_command.type = CommandBuffer_ClassifyToken(token);
            strncpy(current_command.tokens[0], token, GCODE_MAX_TOKEN_LENGTH - 1);
            current_command.token_count = 1;
        }
        else if (is_parameter_token(token) && building_command)
        {
            // Add parameter to current command
            if (current_command.token_count < MAX_TOKENS_PER_COMMAND)
            {
                strncpy(current_command.tokens[current_command.token_count],
                        token,
                        GCODE_MAX_TOKEN_LENGTH - 1);
                current_command.token_count++;
            }
        }
        else
        {
            // Unknown token - ignore or treat as parameter
            if (building_command && current_command.token_count < MAX_TOKENS_PER_COMMAND)
            {
                strncpy(current_command.tokens[current_command.token_count],
                        token,
                        GCODE_MAX_TOKEN_LENGTH - 1);
                current_command.token_count++;
            }
        }
    }

    // Don't forget the last command!
    if (building_command && current_command.token_count > 0)
    {
        if (cmd_buffer.count < COMMAND_BUFFER_SIZE)
        {
            memcpy(&cmd_buffer.commands[cmd_buffer.head],
                   &current_command,
                   sizeof(command_entry_t));

            cmd_buffer.head = (cmd_buffer.head + 1U) % COMMAND_BUFFER_SIZE;
            cmd_buffer.count++;
            commands_added++;
        }
    }

#ifdef DEBUG_MOTION_BUFFER
    if (commands_added > 0)
    {
        UGS_Printf("[SPLIT] Added %u commands to buffer (total: %u)\r\n",
                   commands_added, cmd_buffer.count);
    }
#endif

    return commands_added;
}

//=============================================================================
// BUFFER OPERATIONS
//=============================================================================

bool CommandBuffer_GetNext(command_entry_t *command)
{
    if (command == NULL || cmd_buffer.count == 0)
    {
        return false;
    }

    // Copy command from tail
    memcpy(command,
           &cmd_buffer.commands[cmd_buffer.tail],
           sizeof(command_entry_t));

    // Advance tail pointer
    cmd_buffer.tail = (cmd_buffer.tail + 1U) % COMMAND_BUFFER_SIZE;
    cmd_buffer.count--;

    return true;
}

bool CommandBuffer_HasData(void)
{
    return (cmd_buffer.count > 0);
}

uint8_t CommandBuffer_GetCount(void)
{
    return cmd_buffer.count;
}

void CommandBuffer_Clear(void)
{
    cmd_buffer.head = 0;
    cmd_buffer.tail = 0;
    cmd_buffer.count = 0;
}

//=============================================================================
// DEBUGGING
//=============================================================================

void CommandBuffer_DebugPrint(void)
{
    UGS_Printf("[CMD_BUF] Count: %u/%u\r\n", cmd_buffer.count, COMMAND_BUFFER_SIZE);

    uint8_t idx = cmd_buffer.tail;
    for (uint8_t i = 0; i < cmd_buffer.count; i++)
    {
        UGS_Printf("[%u] Type: %u, Tokens: ", i, cmd_buffer.commands[idx].type);

        for (uint8_t j = 0; j < cmd_buffer.commands[idx].token_count; j++)
        {
            UGS_Printf("%s ", cmd_buffer.commands[idx].tokens[j]);
        }
        UGS_Printf("\r\n");

        idx = (idx + 1U) % COMMAND_BUFFER_SIZE;
    }
}
