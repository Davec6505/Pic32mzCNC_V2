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
// TOKENIZATION (SPLIT INTO STRING ARRAY)
//=============================================================================

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

    /* Tokenize on whitespace */
    uint8_t token_count = 0;
    const char *delimiters = " \t\r\n";
    char *token = strtok(work_buffer, delimiters);

    while (token != NULL && token_count < GCODE_MAX_TOKENS)
    {
        /* Skip empty tokens */
        if (strlen(token) > 0)
        {
            /* Convert to uppercase for consistency */
            size_t len = strlen(token);
            if (len >= GCODE_MAX_TOKEN_LENGTH)
            {
                snprintf(last_error, sizeof(last_error),
                         "Token too long: %s", token);
                return false;
            }

            for (size_t i = 0; i < len; i++)
            {
                tokenized_line->tokens[token_count][i] = (char)toupper((int)token[i]);
            }
            tokenized_line->tokens[token_count][len] = '\0';

            token_count++;
        }

        token = strtok(NULL, delimiters);
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
// PARSING (HIGH-LEVEL)
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

    /* Identify command type from first token */
    char first_char = tokenized_line.tokens[0][0];

    if (first_char == 'G')
    {
        return GCode_ParseGCommand(&tokenized_line, move);
    }
    else if (first_char == 'M')
    {
        return GCode_ParseMCommand(&tokenized_line);
    }
    else if (first_char == '$')
    {
        return GCode_ParseSystemCommand(&tokenized_line);
    }
    else
    {
        snprintf(last_error, sizeof(last_error),
                 "Unknown command: %s", tokenized_line.tokens[0]);
        return false;
    }
}

//=============================================================================
// G-COMMAND PARSING (G0, G1, G2, G3, G90, G91, etc.)
//=============================================================================

bool GCode_ParseGCommand(const gcode_line_t *tokenized_line, parsed_move_t *move)
{
    if (tokenized_line == NULL || move == NULL)
    {
        return false;
    }

    /* Extract G-code number (e.g., "G1" → 1) */
    float g_value;
    if (!GCode_ExtractTokenValue(tokenized_line->tokens[0], &g_value))
    {
        snprintf(last_error, sizeof(last_error),
                 "Invalid G-code: %s", tokenized_line->tokens[0]);
        return false;
    }

    uint8_t g_code = (uint8_t)g_value;

    /* Handle modal commands (G90/G91/G20/G21) */
    if (g_code == 90)
    {
        modal_state.absolute_mode = true;
        UGS_Print(">> G90 Absolute Mode\r\n");
        return true; /* Modal only, no motion */
    }
    if (g_code == 91)
    {
        modal_state.absolute_mode = false;
        UGS_Print(">> G91 Relative Mode\r\n");
        return true; /* Modal only, no motion */
    }
    if (g_code == 21)
    {
        modal_state.metric_mode = true;
        UGS_Print(">> G21 Metric (mm)\r\n");
        return true; /* Modal only, no motion */
    }
    if (g_code == 20)
    {
        modal_state.metric_mode = false;
        UGS_Print(">> G20 Imperial (inches)\r\n");
        return true; /* Modal only, no motion */
    }

    /* Handle motion commands (G0, G1) */
    if (g_code == 0 || g_code == 1)
    {
        modal_state.motion_mode = g_code;

        /* Extract axis coordinates */
        float x_val, y_val, z_val, a_val, f_val;

        if (GCode_FindToken(tokenized_line, 'X', &x_val))
        {
            move->target[AXIS_X] = x_val;
            move->axis_words[AXIS_X] = true;
        }
        if (GCode_FindToken(tokenized_line, 'Y', &y_val))
        {
            move->target[AXIS_Y] = y_val;
            move->axis_words[AXIS_Y] = true;
        }
        if (GCode_FindToken(tokenized_line, 'Z', &z_val))
        {
            move->target[AXIS_Z] = z_val;
            move->axis_words[AXIS_Z] = true;
        }
        if (GCode_FindToken(tokenized_line, 'A', &a_val))
        {
            move->target[AXIS_A] = a_val;
            move->axis_words[AXIS_A] = true;
        }

        /* Extract feedrate */
        if (GCode_FindToken(tokenized_line, 'F', &f_val))
        {
            modal_state.feedrate = f_val;
        }

        /* Set move properties */
        move->feedrate = modal_state.feedrate;
        move->absolute_mode = modal_state.absolute_mode;
        move->motion_mode = g_code;

        /* Check if any axis specified */
        if (!move->axis_words[AXIS_X] && !move->axis_words[AXIS_Y] &&
            !move->axis_words[AXIS_Z] && !move->axis_words[AXIS_A])
        {
            snprintf(last_error, sizeof(last_error), "No axis specified");
            return false;
        }

        return true; /* Move ready for motion buffer */
    }

    /* Unsupported G-code */
    snprintf(last_error, sizeof(last_error), "Unsupported G-code: G%d", g_code);
    return false;
}

//=============================================================================
// M-COMMAND PARSING (M3, M5, M8, M9, etc.)
//=============================================================================

bool GCode_ParseMCommand(const gcode_line_t *tokenized_line)
{
    if (tokenized_line == NULL)
    {
        return false;
    }

    /* Extract M-code number */
    float m_value;
    if (!GCode_ExtractTokenValue(tokenized_line->tokens[0], &m_value))
    {
        snprintf(last_error, sizeof(last_error),
                 "Invalid M-code: %s", tokenized_line->tokens[0]);
        return false;
    }

    uint8_t m_code = (uint8_t)m_value;

    switch (m_code)
    {
    case 3:
        /* M3 - Spindle CW */
        UGS_Print(">> M3 Spindle CW\r\n");
        /* TODO: Implement spindle control */
        break;

    case 5:
        /* M5 - Spindle off */
        UGS_Print(">> M5 Spindle Off\r\n");
        /* TODO: Implement spindle control */
        break;

    case 8:
        /* M8 - Coolant on */
        UGS_Print(">> M8 Coolant On\r\n");
        /* TODO: Implement coolant control */
        break;

    case 9:
        /* M9 - Coolant off */
        UGS_Print(">> M9 Coolant Off\r\n");
        /* TODO: Implement coolant control */
        break;

    default:
        snprintf(last_error, sizeof(last_error),
                 "Unsupported M-code: M%d", m_code);
        return false;
    }

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
