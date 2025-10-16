/**
 * @file gcode_parser.c
 * @brief G-code parser implementation
 *
 * Implements polling-based G-code parsing with priority control character detection.
 *
 * @date October 16, 2025
 */

#include "gcode_parser.h"
#include "ugs_interface.h"
#include "motion/motion_math.h"
#include "motion/motion_buffer.h"                      /* For MotionBuffer_Pause/Resume/Clear */
#include "motion/multiaxis_control.h"                  /* For MultiAxis_EmergencyStop */
#include "config/default/peripheral/uart/plib_uart2.h" /* For UART2 direct access */
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

//=============================================================================
// STATIC STATE
//=============================================================================

/**
 * @brief Modal state (persistent across commands)
 */
static parser_modal_state_t modal_state = {0};

/**
 * @brief Line buffering state
 */
static struct
{
    char buffer[GCODE_MAX_LINE_LENGTH]; /* Line accumulator */
    uint16_t index;                     /* Current buffer position */
    bool line_ready;                    /* Complete line available */
} line_buffer = {0};

/**
 * @brief Last error message
 */
static char last_error[128] = {0};

//=============================================================================
// CONTROL CHARACTER DETECTION
//=============================================================================

bool GCode_IsControlChar(char c)
{
    return (c == GCODE_CTRL_STATUS_REPORT) ||
           (c == GCODE_CTRL_CYCLE_START) ||
           (c == GCODE_CTRL_FEED_HOLD) ||
           (c == GCODE_CTRL_SOFT_RESET);
}

/**
 * @brief Handle real-time control character immediately
 *
 * @param c Control character
 */
static void handle_control_char(char c)
{
    switch (c)
    {
    case GCODE_CTRL_STATUS_REPORT:
        /* Send status report immediately - use dummy positions for now */
        UGS_SendStatusReport("Idle", 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        break;

    case GCODE_CTRL_FEED_HOLD:
        /* Pause motion buffer */
        MotionBuffer_Pause();
        UGS_Print(">> Feed Hold\r\n");
        break;

    case GCODE_CTRL_CYCLE_START:
        /* Resume motion buffer */
        MotionBuffer_Resume();
        UGS_Print(">> Cycle Start\r\n");
        break;

    case GCODE_CTRL_SOFT_RESET:
        /* Emergency stop and reset */
        MultiAxis_StopAll();
        MotionBuffer_Clear();
        GCode_ResetModalState();
        UGS_Print(">> System Reset\r\n");
        break;

    default:
        /* Unknown control character - ignore */
        break;
    }
}

//=============================================================================
// INITIALIZATION
//=============================================================================

void GCode_Initialize(void)
{
    /* Set GRBL defaults */
    modal_state.motion_mode = 1;      /* G1 (linear motion) */
    modal_state.absolute_mode = true; /* G90 (absolute coordinates) */
    modal_state.metric_mode = true;   /* G21 (millimeters) */
    modal_state.plane = 17;           /* G17 (XY plane) */
    modal_state.feedrate = 1000.0f;   /* Default 1000 mm/min */
    modal_state.spindle_speed = 0.0f;
    modal_state.coordinate_system = 0;

    /* Clear line buffer */
    memset(&line_buffer, 0, sizeof(line_buffer));

    /* Clear error */
    memset(last_error, 0, sizeof(last_error));
}

//=============================================================================
// LINE BUFFERING (USER'S POLLING PATTERN)
//=============================================================================

bool GCode_BufferLine(char *line, size_t line_size)
{
    /* Check if data available (user's polling pattern) */
    if (!UGS_RxHasData())
    {
        return false;
    }

    /* Read incoming bytes one at a time */
    uint8_t byte;
    while (UART2_ReadCountGet() > 0)
    {
        UART2_Read(&byte, 1);
        char c = (char)byte;

        /* PRIORITY CHECK: Control character at index[0] (user's requirement) */
        if (line_buffer.index == 0 && GCode_IsControlChar(c))
        {
            handle_control_char(c);
            /* Return empty line to signal control char handled */
            line[0] = c;
            line[1] = '\0';
            return true;
        }

        /* Handle line terminators */
        if (c == GCODE_CTRL_LINE_FEED || c == GCODE_CTRL_CARRIAGE_RET)
        {
            if (line_buffer.index > 0)
            {
                /* Complete line available */
                line_buffer.buffer[line_buffer.index] = '\0';

                /* Copy to output buffer */
                size_t copy_size = (line_buffer.index < line_size - 1) ? line_buffer.index : (line_size - 1);
                memcpy(line, line_buffer.buffer, copy_size);
                line[copy_size] = '\0';

                /* Reset buffer for next line */
                line_buffer.index = 0;

                return true;
            }
            /* Ignore empty lines */
            continue;
        }

        /* Buffer character */
        if (line_buffer.index < GCODE_MAX_LINE_LENGTH - 1)
        {
            line_buffer.buffer[line_buffer.index++] = c;
        }
        else
        {
            /* Line too long - error */
            snprintf(last_error, sizeof(last_error),
                     "Line exceeds %d characters", GCODE_MAX_LINE_LENGTH);
            line_buffer.index = 0; /* Reset buffer */
            return false;
        }
    }

    return false; /* Line not complete yet */
}

//=============================================================================
// TOKENIZATION (SPLIT INTO STRING ARRAY - GRBL COMPLIANT)
//=============================================================================

/**
 * @brief Check if character is a valid GRBL word letter
 *
 * Valid letters: G, M, X, Y, Z, A, B, C, I, J, K, F, S, T, P, L, N, R, D, H
 *
 * @param c Character to check
 * @return true if valid word letter
 */
static bool is_word_letter(char c)
{
    char upper = (char)toupper((int)c);
    return (upper == 'G' || upper == 'M' ||
            upper == 'X' || upper == 'Y' || upper == 'Z' ||
            upper == 'A' || upper == 'B' || upper == 'C' ||
            upper == 'I' || upper == 'J' || upper == 'K' ||
            upper == 'F' || upper == 'S' || upper == 'T' ||
            upper == 'P' || upper == 'L' || upper == 'N' ||
            upper == 'R' || upper == 'D' || upper == 'H' ||
            upper == '$');
}

bool GCode_TokenizeLine(const char *line, gcode_line_t *tokenized_line)
{
    if (line == NULL || tokenized_line == NULL)
    {
        snprintf(last_error, sizeof(last_error), "NULL pointer in tokenize");
        return false;
    }

    /* Clear output structure */
    memset(tokenized_line, 0, sizeof(gcode_line_t));

    /* Copy raw line for debug */
    strncpy(tokenized_line->raw_line, line, GCODE_MAX_LINE_LENGTH - 1);

    /* Work buffer for parsing */
    char work_buffer[GCODE_MAX_LINE_LENGTH];
    strncpy(work_buffer, line, GCODE_MAX_LINE_LENGTH - 1);
    work_buffer[GCODE_MAX_LINE_LENGTH - 1] = '\0';

    /* Strip comments (everything after ';' or '(' ) */
    char *comment = strchr(work_buffer, ';');
    if (comment != NULL)
    {
        *comment = '\0';
    }
    comment = strchr(work_buffer, '(');
    if (comment != NULL)
    {
        *comment = '\0';
    }

    /* GRBL-compliant tokenization: Split on letters
     *
     * Examples:
     *   "G0 X50 F100"    → ["G0", "X50", "F100"]
     *   "G0X50F100"      → ["G0", "X50", "F100"]
     *   "G92G0X50F50M10" → ["G92", "G0", "X50", "F50", "M10"]
     *   "G1 X10.5Y20.3"  → ["G1", "X10.5", "Y20.3"]
     */
    uint8_t token_count = 0;
    size_t i = 0;
    size_t len = strlen(work_buffer);

    while (i < len && token_count < GCODE_MAX_TOKENS)
    {
        /* Skip whitespace */
        while (i < len && (work_buffer[i] == ' ' || work_buffer[i] == '\t'))
        {
            i++;
        }

        if (i >= len)
            break;

        /* Check if this is a word letter (start of token) */
        if (is_word_letter(work_buffer[i]))
        {
            /* Start new token */
            size_t token_idx = 0;

            /* Add the letter */
            tokenized_line->tokens[token_count][token_idx++] = (char)toupper((int)work_buffer[i]);
            i++;

            /* Collect following digits, decimal point, minus sign */
            while (i < len && token_idx < (GCODE_MAX_TOKEN_LENGTH - 1))
            {
                char c = work_buffer[i];

                /* Valid number characters */
                if (isdigit((int)c) || c == '.' || c == '-' || c == '+')
                {
                    tokenized_line->tokens[token_count][token_idx++] = c;
                    i++;
                }
                /* Stop at next letter or whitespace */
                else if (is_word_letter(c) || c == ' ' || c == '\t')
                {
                    break;
                }
                /* Invalid character */
                else
                {
                    snprintf(last_error, sizeof(last_error),
                             "Invalid character '%c' at position %zu", c, i);
                    return false;
                }
            }

            /* Null-terminate token */
            tokenized_line->tokens[token_count][token_idx] = '\0';

            /* Check token length */
            if (token_idx >= GCODE_MAX_TOKEN_LENGTH)
            {
                snprintf(last_error, sizeof(last_error),
                         "Token too long at position %zu", i);
                return false;
            }

            token_count++;
        }
        else
        {
            /* Skip invalid characters (shouldn't happen after comment stripping) */
            i++;
        }
    }

    tokenized_line->token_count = token_count;

    if (token_count == 0)
    {
        snprintf(last_error, sizeof(last_error), "No tokens found");
        return false;
    }

    return true;
}

//=============================================================================
// TOKEN EXTRACTION
//=============================================================================

bool GCode_ExtractTokenValue(const char *token, float *value)
{
    if (token == NULL || value == NULL || strlen(token) < 2)
    {
        return false;
    }

    /* Skip letter prefix (e.g., "X" in "X10.5") */
    const char *number_str = &token[1];

    /* Use strtof for floating point conversion */
    char *endptr;
    float result = strtof(number_str, &endptr);

    /* Check if conversion successful */
    if (endptr == number_str || *endptr != '\0')
    {
        return false;
    }

    *value = result;
    return true;
}

bool GCode_FindToken(const gcode_line_t *tokenized_line, char letter, float *value)
{
    if (tokenized_line == NULL || value == NULL)
    {
        return false;
    }

    char upper_letter = (char)toupper((int)letter);

    for (uint8_t i = 0; i < tokenized_line->token_count; i++)
    {
        if (tokenized_line->tokens[i][0] == upper_letter)
        {
            return GCode_ExtractTokenValue(tokenized_line->tokens[i], value);
        }
    }

    return false;
}

//=============================================================================
// PARSING (HIGH-LEVEL - COMPREHENSIVE ALL-TOKEN PROCESSING)
//=============================================================================

bool GCode_ParseLine(const char *line, parsed_move_t *move)
{
    if (line == NULL || move == NULL)
    {
        snprintf(last_error, sizeof(last_error), "NULL pointer in parse");
        return false;
    }

    /* Clear output structure */
    memset(move, 0, sizeof(parsed_move_t));

    /* Tokenize line */
    gcode_line_t tokenized_line;
    if (!GCode_TokenizeLine(line, &tokenized_line))
    {
        return false;
    }

    /* COMPREHENSIVE TOKEN PROCESSING
     *
     * GRBL allows multiple commands on one line, e.g.:
     *   "G90 G21 G0 X50 Y20 F1500"
     *   "G92G0X50F50M10G93"
     *
     * We process ALL tokens and categorize by type:
     *   - G-codes (motion, modal, coordinate system)
     *   - M-codes (spindle, coolant, program control)
     *   - Parameters (X, Y, Z, A, F, S, etc.)
     *   - System commands ($)
     *
     * Modal commands update state, motion commands generate moves.
     */

    bool has_motion_command = false;
    bool has_system_command = false;
    bool has_m_command = false;
    uint8_t motion_g_code = modal_state.motion_mode; /* Default to modal */

    /* First pass: Process all G-codes and identify motion command */
    for (uint8_t i = 0; i < tokenized_line.token_count; i++)
    {
        char letter = tokenized_line.tokens[i][0];

        if (letter == 'G')
        {
            float g_value;
            if (!GCode_ExtractTokenValue(tokenized_line.tokens[i], &g_value))
            {
                snprintf(last_error, sizeof(last_error),
                         "Invalid G-code: %s", tokenized_line.tokens[i]);
                return false;
            }

            uint8_t g_code = (uint8_t)g_value;

            /* Classify G-code type */
            if (g_code == 0 || g_code == 1 || g_code == 2 || g_code == 3)
            {
                /* Motion commands */
                has_motion_command = true;
                motion_g_code = g_code;
                modal_state.motion_mode = g_code;
            }
            else if (g_code == 90 || g_code == 91)
            {
                /* Distance mode */
                modal_state.absolute_mode = (g_code == 90);
            }
            else if (g_code == 20 || g_code == 21)
            {
                /* Units */
                modal_state.metric_mode = (g_code == 21);
            }
            else if (g_code == 17 || g_code == 18 || g_code == 19)
            {
                /* Plane selection */
                modal_state.plane = g_code;
            }
            else if (g_code == 92)
            {
                /* Coordinate system offset */
                /* TODO: Implement G92 */
                UGS_Printf(">> G92 (coordinate offset)\r\n");
            }
            else if (g_code == 93 || g_code == 94)
            {
                /* Feed rate mode */
                /* TODO: Implement inverse time mode */
                UGS_Printf(">> G%d (feed rate mode)\r\n", g_code);
            }
            else
            {
                snprintf(last_error, sizeof(last_error),
                         "Unsupported G-code: G%d", g_code);
                return false;
            }
        }
        else if (letter == 'M')
        {
            has_m_command = true;
        }
        else if (letter == '$')
        {
            has_system_command = true;
        }
    }

    /* Handle system commands (take priority) */
    if (has_system_command)
    {
        return GCode_ParseSystemCommand(&tokenized_line);
    }

    /* Second pass: Extract parameters (X, Y, Z, A, F, S, etc.) */
    for (uint8_t i = 0; i < tokenized_line.token_count; i++)
    {
        char letter = tokenized_line.tokens[i][0];
        float value;

        if (!GCode_ExtractTokenValue(tokenized_line.tokens[i], &value))
        {
            /* Skip tokens without values (already processed G/M codes) */
            continue;
        }

        switch (letter)
        {
        case 'X':
            move->target[AXIS_X] = value;
            move->axis_words[AXIS_X] = true;
            break;

        case 'Y':
            move->target[AXIS_Y] = value;
            move->axis_words[AXIS_Y] = true;
            break;

        case 'Z':
            move->target[AXIS_Z] = value;
            move->axis_words[AXIS_Z] = true;
            break;

        case 'A':
            move->target[AXIS_A] = value;
            move->axis_words[AXIS_A] = true;
            break;

        case 'F':
            modal_state.feedrate = value;
            break;

        case 'S':
            modal_state.spindle_speed = value;
            break;

        case 'I':
        case 'J':
        case 'K':
            /* Arc parameters (for G2/G3) */
            /* TODO: Implement arc support */
            break;

        case 'P':
        case 'L':
            /* Dwell time or loop count */
            /* TODO: Implement dwell (G4) */
            break;

        case 'N':
            /* Line number (ignore) */
            break;

        default:
            /* Already processed or unknown */
            break;
        }
    }

    /* Process M-commands (execute immediately) */
    if (has_m_command)
    {
        for (uint8_t i = 0; i < tokenized_line.token_count; i++)
        {
            if (tokenized_line.tokens[i][0] == 'M')
            {
                float m_value;
                if (GCode_ExtractTokenValue(tokenized_line.tokens[i], &m_value))
                {
                    uint8_t m_code = (uint8_t)m_value;

                    switch (m_code)
                    {
                    case 3:
                        UGS_Print(">> M3 Spindle CW\r\n");
                        /* TODO: Implement spindle control */
                        break;
                    case 4:
                        UGS_Print(">> M4 Spindle CCW\r\n");
                        break;
                    case 5:
                        UGS_Print(">> M5 Spindle Off\r\n");
                        break;
                    case 7:
                        UGS_Print(">> M7 Mist Coolant On\r\n");
                        break;
                    case 8:
                        UGS_Print(">> M8 Flood Coolant On\r\n");
                        break;
                    case 9:
                        UGS_Print(">> M9 Coolant Off\r\n");
                        break;
                    default:
                        UGS_Printf(">> M%d (unsupported)\r\n", m_code);
                        break;
                    }
                }
            }
        }
    }

    /* Generate motion block if needed */
    if (has_motion_command ||
        move->axis_words[AXIS_X] || move->axis_words[AXIS_Y] ||
        move->axis_words[AXIS_Z] || move->axis_words[AXIS_A])
    {
        move->motion_mode = motion_g_code;
        move->feedrate = modal_state.feedrate;
        move->absolute_mode = modal_state.absolute_mode;

        /* Validate motion */
        if (!move->axis_words[AXIS_X] && !move->axis_words[AXIS_Y] &&
            !move->axis_words[AXIS_Z] && !move->axis_words[AXIS_A])
        {
            /* No axes specified - this is valid for modal operation */
            /* Use previous position (will be handled by motion buffer) */
        }

        return true; /* Motion ready */
    }

    /* No motion, but modal state updated */
    return true;
}

//=============================================================================
// SYSTEM COMMAND PARSING ($$, $H, $X, $100=250, etc.)
//=============================================================================

bool GCode_ParseSystemCommand(const gcode_line_t *tokenized_line)
{
    if (tokenized_line == NULL)
    {
        return false;
    }

    const char *cmd = tokenized_line->tokens[0];

    if (strcmp(cmd, "$$") == 0)
    {
        /* Print all settings using GRBL setting IDs */
        UGS_Print(">> GRBL Settings:\r\n");
        UGS_Printf("$100=%.3f (X steps/mm)\r\n", MotionMath_GetSetting(100));
        UGS_Printf("$101=%.3f (Y steps/mm)\r\n", MotionMath_GetSetting(101));
        UGS_Printf("$102=%.3f (Z steps/mm)\r\n", MotionMath_GetSetting(102));
        UGS_Printf("$110=%.1f (X max rate mm/min)\r\n", MotionMath_GetSetting(110));
        UGS_Printf("$111=%.1f (Y max rate mm/min)\r\n", MotionMath_GetSetting(111));
        UGS_Printf("$112=%.1f (Z max rate mm/min)\r\n", MotionMath_GetSetting(112));
        /* TODO: Add more settings */
        return true;
    }

    if (strcmp(cmd, "$H") == 0)
    {
        /* Homing cycle */
        UGS_Print(">> $H Homing Cycle\r\n");
        /* TODO: Implement homing */
        return true;
    }

    if (strcmp(cmd, "$X") == 0)
    {
        /* Clear alarm */
        UGS_Print(">> $X Alarm Cleared\r\n");
        return true;
    }

    /* Setting assignment (e.g., $100=250) */
    if (strchr(cmd, '=') != NULL)
    {
        UGS_Printf(">> Setting: %s\r\n", cmd);
        /* TODO: Parse and update setting */
        return true;
    }

    snprintf(last_error, sizeof(last_error), "Unknown system command: %s", cmd);
    return false;
}

//=============================================================================
// MODAL STATE
//=============================================================================

const parser_modal_state_t *GCode_GetModalState(void)
{
    return &modal_state;
}

void GCode_ResetModalState(void)
{
    GCode_Initialize();
}

//=============================================================================
// ERROR REPORTING
//=============================================================================

const char *GCode_GetLastError(void)
{
    return (last_error[0] != '\0') ? last_error : NULL;
}

void GCode_ClearError(void)
{
    memset(last_error, 0, sizeof(last_error));
}

//=============================================================================
// DEBUGGING
//=============================================================================

#ifdef DEBUG
void GCode_DebugPrintTokens(const gcode_line_t *tokenized_line)
{
    if (tokenized_line == NULL)
    {
        return;
    }

    UGS_Printf("Raw line: %s\r\n", tokenized_line->raw_line);
    UGS_Printf("Tokens (%d):\r\n", tokenized_line->token_count);

    for (uint8_t i = 0; i < tokenized_line->token_count; i++)
    {
        UGS_Printf("  [%d]: %s\r\n", i, tokenized_line->tokens[i]);
    }
}
#endif
