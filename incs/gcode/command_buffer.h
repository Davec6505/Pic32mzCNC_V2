/**
 * @file command_buffer.h
 * @brief Command separation and buffering for G-code parser
 *
 * Purpose:
 *   Split concatenated G-code lines into individual executable commands.
 *
 * Example:
 *   Input:  "G92G0X10Y10F200G93G1Z1Y100F400M20"
 *   Output: ["G92", "G0 X10 Y10 F200", "G93", "G1 Z1 Y100 F400", "M20"]
 *
 * Architecture:
 *   Serial → GCode_TokenizeLine() → CommandBuffer_SplitLine() → command_buffer_t
 *         → CommandBuffer_GetNext() → GCode_ParseCommand() → MotionBuffer_Add()
 *
 * Memory Usage (with 2MB RAM):
 *   64 commands × 32 bytes = 2KB (0.1% of total RAM)
 *
 * @date October 17, 2025
 */

#ifndef COMMAND_BUFFER_H
#define COMMAND_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include "gcode_parser.h"  // For gcode_line_t

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// CONFIGURATION (Conservative for 2MB RAM)
//=============================================================================

/**
 * @brief Number of commands in ring buffer
 * 
 * With 2MB RAM, 64 commands is trivial (~2KB total).
 * Provides deep look-ahead window for optimization.
 */
#define COMMAND_BUFFER_SIZE 64U

/**
 * @brief Maximum tokens per command
 * 
 * Typical: 4-6 tokens per command
 * Example: "G1 X10 Y20 F1500" = 4 tokens
 */
#define MAX_TOKENS_PER_COMMAND 8U

//=============================================================================
// DATA STRUCTURES
//=============================================================================

/**
 * @brief Command types (for fast lookup)
 */
typedef enum {
    CMD_NONE = 0,
    CMD_G0,              // Rapid positioning
    CMD_G1,              // Linear interpolation
    CMD_G2,              // CW arc
    CMD_G3,              // CCW arc
    CMD_G4,              // Dwell
    CMD_G28,             // Go to predefined position
    CMD_G30,             // Go to predefined position (secondary)
    CMD_G92,             // Coordinate offset
    CMD_G90,             // Absolute mode
    CMD_G91,             // Relative mode
    CMD_G20,             // Inches
    CMD_G21,             // Millimeters
    CMD_G17,             // XY plane
    CMD_G18,             // XZ plane
    CMD_G19,             // YZ plane
    CMD_M0,              // Program stop
    CMD_M1,              // Optional stop
    CMD_M2,              // Program end
    CMD_M3,              // Spindle CW
    CMD_M4,              // Spindle CCW
    CMD_M5,              // Spindle off
    CMD_M7,              // Coolant mist
    CMD_M8,              // Coolant flood
    CMD_M9,              // Coolant off
    CMD_M30,             // Program end and rewind
    CMD_UNKNOWN
} command_type_t;

/**
 * @brief Single command entry
 * 
 * Contains tokens for one complete G-code command.
 * Example: "G0 X10 Y20 F1500" → tokens=["G0","X10","Y20","F1500"], count=4
 */
typedef struct {
    char tokens[MAX_TOKENS_PER_COMMAND][GCODE_MAX_TOKEN_LENGTH];  // Token strings
    uint8_t token_count;                                           // Number of tokens
    command_type_t type;                                           // Command classification
} command_entry_t;

/**
 * @brief Command ring buffer (64-entry FIFO)
 * 
 * Architecture:
 *   - Ring buffer using modulo arithmetic
 *   - Thread-safe for single producer/consumer
 *   - Non-blocking add/get operations
 */
typedef struct {
    command_entry_t commands[COMMAND_BUFFER_SIZE];  // Circular array
    volatile uint8_t head;                          // Write pointer (0-63)
    volatile uint8_t tail;                          // Read pointer (0-63)
    volatile uint8_t count;                         // Number of commands (0-64)
} command_buffer_t;

//=============================================================================
// API FUNCTIONS
//=============================================================================

/**
 * @brief Initialize command buffer
 * 
 * Clears all entries and resets pointers.
 */
void CommandBuffer_Initialize(void);

/**
 * @brief Split tokenized line into individual commands
 * 
 * Takes a line like "G92G0X10Y10F200G1X20" and splits into:
 *   Command 1: G92
 *   Command 2: G0 X10 Y10 F200
 *   Command 3: G1 X20
 * 
 * @param tokenized_line Tokenized G-code line from GCode_TokenizeLine()
 * @return Number of commands extracted (0 if buffer full)
 * 
 * Example:
 *   gcode_line_t line;
 *   GCode_TokenizeLine("G92G0X10Y10F200", &line);
 *   uint8_t count = CommandBuffer_SplitLine(&line);
 *   // count = 2 (G92, G0 X10 Y10 F200)
 */
uint8_t CommandBuffer_SplitLine(const gcode_line_t *tokenized_line);

/**
 * @brief Get next command from buffer
 * 
 * Dequeues oldest command (FIFO order).
 * 
 * @param command Output buffer for command
 * @return true if command available, false if buffer empty
 */
bool CommandBuffer_GetNext(command_entry_t *command);

/**
 * @brief Check if buffer has commands
 * 
 * @return true if commands pending
 */
bool CommandBuffer_HasData(void);

/**
 * @brief Get number of commands in buffer
 * 
 * @return Command count (0-64)
 */
uint8_t CommandBuffer_GetCount(void);

/**
 * @brief Clear all commands
 * 
 * Used for emergency stop (^X soft reset).
 */
void CommandBuffer_Clear(void);

/**
 * @brief Classify command type from first token
 * 
 * @param token First token (e.g., "G0", "M3")
 * @return Command type enum
 */
command_type_t CommandBuffer_ClassifyToken(const char *token);

//=============================================================================
// DEBUGGING (Optional - Enable in command_buffer.c)
//=============================================================================

/**
 * @brief Print command buffer contents (for debugging)
 * 
 * Outputs via UGS_Printf() for serial debugging.
 */
void CommandBuffer_DebugPrint(void);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_BUFFER_H */
