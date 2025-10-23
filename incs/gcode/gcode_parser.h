/**
 * @file gcode_parser.h
 * @brief G-code parser for CNC motion control
 *
 * Implements GRBL v1.1f compatible G-code parsing with:
 * - Real-time command detection (?, !, ~, Ctrl-X)
 * - Line-based command buffering
 * - Token splitting (G1 X10 Y20 F1500 → ["G1", "X10", "Y20", "F1500"])
 * - Modal state tracking (G90/G91, absolute/relative)
 *
 * Parser Architecture:
 *   1. Poll UGS_RxHasData() for incoming bytes
 *   2. Check index[0] for control chars → Immediate response
 *   3. Buffer line until \n or \r
 *   4. Split line into token array (max 16 tokens per line)
 *   5. Parse tokens into parsed_move_t structure
 *   6. Return to caller (motion_buffer adds move)
 *
 * @date October 16, 2025
 */

#ifndef GCODE_PARSER_H
#define GCODE_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> /* For size_t */
#include "motion/motion_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

//=============================================================================
// PARSER CONFIGURATION
//=============================================================================

/**
 * @brief Maximum length of G-code line (GRBL v1.1f: 256 characters)
 */
#define GCODE_MAX_LINE_LENGTH 256U

/**
 * @brief Maximum number of tokens per G-code line
 *
 * Example: "G1 X10 Y20 Z5 F1500" = 5 tokens
 * Typical: 8-12 tokens for complex commands
 */
#define GCODE_MAX_TOKENS 16U

/**
 * @brief Maximum length of individual token
 *
 * Example: "X123.456" = 8 characters
 */
#define GCODE_MAX_TOKEN_LENGTH 32U

    //=============================================================================
    // PARSER STATE
    //=============================================================================

    /**
     * @brief Parser state machine states
     */
    typedef enum
    {
        PARSER_STATE_IDLE,       /* Waiting for data */
        PARSER_STATE_BUFFERING,  /* Accumulating line */
        PARSER_STATE_TOKENIZING, /* Splitting into tokens */
        PARSER_STATE_PARSING,    /* Parsing tokens */
        PARSER_STATE_COMPLETE,   /* Move ready */
        PARSER_STATE_ERROR       /* Parse error */
    } parser_state_t;

    /**
     * @brief Modal state (persistent across commands)
     *
     * GRBL v1.1f modal groups:
     * - Motion mode: G0, G1, G2, G3, G38.x, G80
     * - Plane: G17 (XY), G18 (XZ), G19 (YZ)
     * - Distance: G90 (absolute), G91 (relative)
     * - Arc distance: G90.1 (absolute), G91.1 (relative)
     * - Feed rate mode: G93 (inverse time), G94 (units/min)
     * - Units: G20 (inches), G21 (mm)
     * - Cutter radius comp: G40 (off), G41 (left), G42 (right)
     * - Tool length offset: G43.1, G49
     * - Coordinate system: G54-G59, G59.1-G59.3
     * - Path control: G61 (exact path), G61.1 (exact stop), G64 (continuous)
     * - Spindle: M3 (CW), M4 (CCW), M5 (off)
     * - Coolant: M7 (mist), M8 (flood), M9 (off)
     */
    typedef struct
    {
        /* Modal groups */
        uint8_t motion_mode;       /* G0, G1, G2, G3 (Group 1) */
        uint8_t plane;             /* G17, G18, G19 (Group 2) */
        bool absolute_mode;        /* G90/G91 (Group 3) */
        bool arc_absolute_mode;    /* G90.1/G91.1 (Group 4) */
        uint8_t feed_rate_mode;    /* G93, G94 (Group 5) */
        bool metric_mode;          /* G20/G21 (Group 6) */
        uint8_t cutter_comp;       /* G40, G41, G42 (Group 7) */
        uint8_t tool_offset;       /* G43.1, G49 (Group 8) */
        uint8_t coordinate_system; /* G54-G59.3 (Group 12) */
        uint8_t path_control;      /* G61, G61.1, G64 (Group 13) */

        /* Modal parameters */
        float feedrate;      /* Current feedrate (mm/min or in/min) */
        float spindle_speed; /* Current spindle RPM */
        uint8_t tool_number; /* Current tool (T parameter) */

        /* Spindle/Coolant state */
        uint8_t spindle_state; /* 0=off, 3=CW, 4=CCW */
        bool coolant_mist;     /* M7 */
        bool coolant_flood;    /* M8 */

        /* Work coordinate offsets (G92) */
        float g92_offset[NUM_AXES]; /* X, Y, Z, A coordinate offsets */

        /* Stored positions */
        float g28_position[NUM_AXES]; /* G28 home position */
        float g30_position[NUM_AXES]; /* G30 secondary home position */

        /* Work coordinate systems (G54-G59) */
        float wcs_offsets[6][NUM_AXES]; /* 6 work coordinate systems */
    } parser_modal_state_t;

    /**
     * @brief Tokenized G-code line
     *
     * Example: "G1 X10 Y20 F1500\n" becomes:
     *   tokens = ["G1", "X10", "Y20", "F1500"]
     *   token_count = 4
     */
    typedef struct
    {
        char tokens[GCODE_MAX_TOKENS][GCODE_MAX_TOKEN_LENGTH]; /* Array of token strings */
        uint8_t token_count;                                   /* Number of tokens */
        char raw_line[GCODE_MAX_LINE_LENGTH];                  /* Original line for debug */
    } gcode_line_t;

//=============================================================================
// CONTROL CHARACTER DETECTION
//=============================================================================

/**
 * @brief Control character codes (GRBL real-time commands)
 *
 * These bypass the normal command queue and execute immediately.
 */
#define GCODE_CTRL_STATUS_REPORT '?'  /* 0x3F - Query status */
#define GCODE_CTRL_CYCLE_START '~'    /* 0x7E - Resume motion */
#define GCODE_CTRL_FEED_HOLD '!'      /* 0x21 - Pause motion */
#define GCODE_CTRL_SOFT_RESET 0x18    /* Ctrl-X - Soft reset */
#define GCODE_CTRL_DEBUG_COUNTERS '@' /* 0x40 - Print debug counters (Y steps, segments) */
#define GCODE_CTRL_CARRIAGE_RET '\r'  /* 0x0D - Line terminator */
#define GCODE_CTRL_LINE_FEED '\n'     /* 0x0A - Line terminator */

    /**
     * @brief Check if character is a real-time control character
     *
     * @param c Character to check
     * @return true if control character (requires immediate action)
     */
    bool GCode_IsControlChar(char c);

/**
 * @brief Check if line contains at least one GRBL word letter
 *
 * @param s Line to check
 * @return true if line has at least one GRBL word letter, false otherwise
 *
 * Treat a line as meaningful only if it contains at least one GRBL word letter
 * Valid letters: G, M, X, Y, Z, A, B, C, I, J, K, F, S, T, P, L, N, R, D, H, $
 *
 */
    bool LineHasGrblWordLetter(const char *s);


    /**
     * @brief Handle real-time control character (called from ISR)
     *
     * GRBL Pattern: Processes real-time commands immediately in ISR context.
     * - '?' (0x3F) = Status report
     * - '!' (0x21) = Feed hold
     * - '~' (0x7E) = Cycle start/resume
     * - Ctrl-X (0x18) = Soft reset
     *
     * @param c Control character to handle
     */
    void GCode_HandleControlChar(char c);

    //=============================================================================
    // INITIALIZATION
    //=============================================================================

    /**
     * @brief Initialize G-code parser
     *
     * Sets up modal state defaults:
     * - G90 (absolute mode)
     * - G21 (metric/mm)
     * - G17 (XY plane)
     * - G94 (feed rate mode)
     */
    void GCode_Initialize(void);

    //=============================================================================
    // LINE BUFFERING
    //=============================================================================

    /**
     * @brief Poll for incoming G-code and buffer complete line
     *
     * Uses polling pattern:
     *   1. Check UGS_RxHasData() for incoming bytes
     *   2. Read first byte, check if control char
     *   3. If control char: Handle immediately, return true
     *   4. Else: Buffer until \n or \r
     *   5. When complete: Return true (line ready to parse)
     *
     * @param line Output buffer for complete G-code line
     * @param line_size Size of output buffer
     * @return true if complete line available or control char handled
     *
     * Example:
     *   char line[256];
     *   if (GCode_BufferLine(line, sizeof(line))) {
     *       if (line[0] == '?') {
     *           // Control char already handled
     *       } else {
     *           // Parse G-code line
     *           GCode_ParseLine(line, &move);
     *       }
     *   }
     */
    bool GCode_BufferLine(char *line, size_t line_size);

    //=============================================================================
    // TOKENIZATION
    //=============================================================================

    /**
     * @brief Split G-code line into array of token strings
     *
     * Splits on whitespace, handles comments, validates length.
     *
     * Example:
     *   Input:  "G1 X10.5 Y20 F1500 ; move to position\n"
     *   Output: tokens = ["G1", "X10.5", "Y20", "F1500"]
     *           token_count = 4
     *   (Comment stripped, line terminators removed)
     *
     * @param line Input G-code line (null-terminated)
     * @param tokenized_line Output structure with token array
     * @return true if tokenization successful, false if error
     */
    bool GCode_TokenizeLine(const char *line, gcode_line_t *tokenized_line);

    //=============================================================================
    // PARSING
    //=============================================================================

    /**
     * @brief Parse tokenized G-code line into motion structure
     *
     * High-level parser that:
     *   1. Tokenizes line using GCode_TokenizeLine()
     *   2. Identifies command type (G, M, $ commands)
     *   3. Extracts parameters (X, Y, Z, F, etc.)
     *   4. Updates modal state
     *   5. Generates parsed_move_t structure
     *
     * @param line G-code line string (e.g., "G1 X10 Y20 F1500")
     * @param move Output parsed move structure
     * @return true if parse successful, false if error
     *
     * MISRA Rule 17.4: Parameter validation (NULL checks)
     *
     * Example:
     *   parsed_move_t move;
     *   if (GCode_ParseLine("G1 X10 Y20 F1500", &move)) {
     *       // move.target[AXIS_X] = 10.0
     *       // move.target[AXIS_Y] = 20.0
     *       // move.feedrate = 1500.0
     *       // move.axis_words[AXIS_X] = true
     *       // move.axis_words[AXIS_Y] = true
     *   }
     */
    bool GCode_ParseLine(const char *line, parsed_move_t *move);

    /**
     * @brief Parse $-command tokens ($$, $H, $X, $100=250, etc.)
     *
     * @param tokenized_line Tokenized G-code line
     * @return true if parse successful
     */
    bool GCode_ParseSystemCommand(const gcode_line_t *tokenized_line);

    //=============================================================================
    // TOKEN EXTRACTION
    //=============================================================================

    /**
     * @brief Extract numeric value from token (e.g., "X10.5" → 10.5)
     *
     * @param token Token string (e.g., "X10.5", "F1500")
     * @param value Output numeric value
     * @return true if extraction successful
     *
     * Example:
     *   float value;
     *   GCode_ExtractTokenValue("X10.5", &value);  // value = 10.5
     *   GCode_ExtractTokenValue("F1500", &value);  // value = 1500.0
     */
    bool GCode_ExtractTokenValue(const char *token, float *value);

    /**
     * @brief Find token by letter prefix (e.g., find "X" in token array)
     *
     * @param tokenized_line Tokenized G-code line
     * @param letter Letter to search for ('X', 'Y', 'Z', 'F', etc.)
     * @param value Output numeric value if found
     * @return true if token found and value extracted
     *
     * Example:
     *   float x_value;
     *   if (GCode_FindToken(&tokens, 'X', &x_value)) {
     *       // x_value contains coordinate
     *   }
     */
    bool GCode_FindToken(const gcode_line_t *tokenized_line, char letter, float *value);

    //=============================================================================
    // MODAL STATE
    //=============================================================================

    /**
     * @brief Get current modal state (read-only)
     *
     * @return Pointer to current modal state structure
     */
    const parser_modal_state_t *GCode_GetModalState(void);

    /**
     * @brief Reset modal state to defaults (G90, G21, G17, G94)
     */
    void GCode_ResetModalState(void);

    //=============================================================================
    // ERROR REPORTING
    //=============================================================================

    /**
     * @brief Get last parser error message
     *
     * @return Pointer to error string (or NULL if no error)
     */
    const char *GCode_GetLastError(void);

    /**
     * @brief Clear last error message
     */
    void GCode_ClearError(void);

    /**
     * @brief Print current parser modal state to UGS
     */
    void GCode_PrintParserState(void);

    //=============================================================================
    // DEBUGGING
    //=============================================================================

#ifdef DEBUG
    /**
     * @brief Print tokenized line for debugging
     *
     * @param tokenized_line Tokenized G-code line to print
     */
    void GCode_DebugPrintTokens(const gcode_line_t *tokenized_line);
#endif

#ifdef __cplusplus
}
#endif

#endif /* GCODE_PARSER_H */
