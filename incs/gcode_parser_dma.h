/*******************************************************************************
  Enhanced G-code Parser with Harmony DMA Integration

  File Name:
    gcode_parser_dma.h

  Summary:
    Advanced G-code parser using your existing Harmony DMA setup

  Description:
    This implementation leverages your configured DMA channels (IRQ 146/147)
    for zero-overhead UART2 communication with ring buffer management.
    Designed to integrate seamlessly with your existing motion control system.
*******************************************************************************/

#ifndef GCODE_PARSER_DMA_H
#define GCODE_PARSER_DMA_H

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "definitions.h"

// *****************************************************************************
// Forward Declarations for DMA Integration
// *****************************************************************************

/* DMA Channel definitions - using your existing setup */
#ifndef DMAC_CHANNEL
#define DMAC_CHANNEL uint32_t
#define DMAC_CHANNEL_0 (0x0U)
#define DMAC_CHANNEL_1 (0x1U)
#endif

// *****************************************************************************
// *****************************************************************************
// Section: Configuration Constants
// *****************************************************************************
// *****************************************************************************

/* DMA Configuration - Using your existing setup */
#define GCODE_DMA_RX_CHANNEL        DMAC_CHANNEL_0  // IRQ 146 - UART2 RX
#define GCODE_DMA_TX_CHANNEL        DMAC_CHANNEL_1  // IRQ 147 - UART2 TX

/* Buffer Sizes - Optimized for CNC applications */
#define GCODE_DMA_RX_BUFFER_SIZE    512             // DMA receive buffer
#define GCODE_DMA_TX_BUFFER_SIZE    256             // DMA transmit buffer
#define GCODE_LINE_BUFFER_SIZE      128             // Single line buffer
#define GCODE_COMMAND_QUEUE_SIZE    16              // Command ring buffer
#define GCODE_MAX_TOKENS_PER_LINE   20              // G1 X10 Y20 F1000 etc.

/* Protocol Configuration */
#define GCODE_UART_BAUD_RATE        115200
#define GCODE_LINE_TERMINATOR       '\n'
#define GCODE_CARRIAGE_RETURN       '\r'
#define GCODE_COMMENT_START         '('
#define GCODE_COMMENT_END           ')'
#define GCODE_SEMICOLON_COMMENT     ';'

/* Real-time Commands (GRBL compatible) */
#define GCODE_RT_STATUS_QUERY       '?'             // Status report
#define GCODE_RT_FEED_HOLD          '!'             // Feed hold
#define GCODE_RT_CYCLE_START        '~'             // Resume
#define GCODE_RT_SOFT_RESET         0x18            // Ctrl-X
#define GCODE_RT_SAFETY_DOOR        0x84            // Safety door

/* Buffer Management Macros */
#define GCODE_BUFFER_NEXT(idx, size) (((idx) + 1) % (size))
#define GCODE_BUFFER_PREV(idx, size) (((idx) == 0) ? ((size) - 1) : ((idx) - 1))
#define GCODE_BUFFER_IS_EMPTY(head, tail) ((head) == (tail))
#define GCODE_BUFFER_IS_FULL(head, tail, size) (GCODE_BUFFER_NEXT(head, size) == (tail))
#define GCODE_BUFFER_COUNT(head, tail, size) \
    (((head) >= (tail)) ? ((head) - (tail)) : ((size) - (tail) + (head)))

// *****************************************************************************
// *****************************************************************************
// Section: Data Types
// *****************************************************************************
// *****************************************************************************

/* DMA Transfer States */
typedef enum {
    GCODE_DMA_STATE_IDLE = 0,
    GCODE_DMA_STATE_RX_ACTIVE,
    GCODE_DMA_STATE_TX_ACTIVE,
    GCODE_DMA_STATE_ERROR
} gcode_dma_state_t;

/* G-code Token Structure */
typedef struct {
    char letter;                                    // G, M, X, Y, Z, F, etc.
    float value;                                    // Numeric value
    bool has_value;                                 // Whether value is present
} gcode_token_t;

/* Parsed G-code Line */
typedef struct {
    gcode_token_t tokens[GCODE_MAX_TOKENS_PER_LINE];
    uint8_t token_count;
    uint8_t line_number;                            // N parameter if present
    uint8_t checksum;                               // Checksum if present
    bool has_line_number;
    bool has_checksum;
    bool is_valid;
    char original_line[GCODE_LINE_BUFFER_SIZE];     // For debugging
} gcode_parsed_line_t;

/* DMA Buffer Management */
typedef struct {
    /* RX Buffers - Double buffered for continuous reception */
    uint8_t rx_buffer_a[GCODE_DMA_RX_BUFFER_SIZE];
    uint8_t rx_buffer_b[GCODE_DMA_RX_BUFFER_SIZE];
    uint8_t *rx_active_buffer;
    uint8_t *rx_processing_buffer;
    volatile uint16_t rx_bytes_received;
    volatile bool rx_buffer_ready;
    
    /* TX Buffer */
    uint8_t tx_buffer[GCODE_DMA_TX_BUFFER_SIZE];
    volatile uint16_t tx_bytes_queued;
    volatile bool tx_busy;
    
    /* DMA States */
    volatile gcode_dma_state_t rx_state;
    volatile gcode_dma_state_t tx_state;
    
    /* Buffer Statistics */
    uint32_t rx_transfers_completed;
    uint32_t tx_transfers_completed;
    uint32_t rx_errors;
    uint32_t tx_errors;
} gcode_dma_buffers_t;

/* Line Assembly State */
typedef struct {
    char current_line[GCODE_LINE_BUFFER_SIZE];
    uint8_t line_position;
    bool in_comment;
    bool in_parentheses_comment;
    bool line_ready;
    uint8_t comment_depth;
} gcode_line_assembly_t;

/* Command Ring Buffer */
typedef struct {
    gcode_parsed_line_t commands[GCODE_COMMAND_QUEUE_SIZE];
    volatile uint8_t head;
    volatile uint8_t tail;
    volatile uint8_t count;
    volatile bool overflow_flag;
    uint32_t commands_processed;
    uint32_t commands_dropped;
} gcode_command_queue_t;

/* Statistics and Monitoring */
typedef struct {
    uint32_t lines_processed;
    uint32_t parse_errors;
    uint32_t buffer_overflows;
    uint32_t real_time_commands;
    uint32_t bytes_received;
    uint32_t bytes_transmitted;
    float cpu_utilization;                          // Percentage
} gcode_statistics_t;

/* Main Parser Structure */
typedef struct {
    /* DMA and UART Management */
    gcode_dma_buffers_t dma;
    
    /* Line Processing */
    gcode_line_assembly_t line_assembly;
    
    /* Command Queue */
    gcode_command_queue_t command_queue;
    
    /* Statistics */
    gcode_statistics_t stats;
    
    /* Status Flags */
    volatile bool initialized;
    volatile bool enabled;
    volatile bool emergency_stop;
    
    /* Real-time Command Handling */
    volatile bool status_report_requested;
    volatile bool feed_hold_active;
    volatile bool cycle_start_requested;
    
    /* Integration Points */
    void (*motion_system_callback)(gcode_parsed_line_t *command);
    void (*status_report_callback)(void);
    void (*emergency_stop_callback)(void);
} gcode_dma_parser_t;

// *****************************************************************************
// *****************************************************************************
// Section: Interface Functions
// *****************************************************************************
// *****************************************************************************

/* Initialization and Control */
bool GCODE_DMA_Initialize(void);
void GCODE_DMA_Enable(void);
void GCODE_DMA_Disable(void);
void GCODE_DMA_Reset(void);

/* Main Processing Loop - Call from APP_Tasks() */
void GCODE_DMA_Tasks(void);

/* Command Interface */
bool GCODE_DMA_GetCommand(gcode_parsed_line_t *command);
uint8_t GCODE_DMA_GetCommandCount(void);
bool GCODE_DMA_IsCommandQueueFull(void);
void GCODE_DMA_ClearCommandQueue(void);

/* Transmission Functions */
bool GCODE_DMA_SendResponse(const char *response);
bool GCODE_DMA_SendFormattedResponse(const char *format, ...);
void GCODE_DMA_SendStatusReport(void);
void GCODE_DMA_SendOK(void);
void GCODE_DMA_SendError(const char *error_message);

/* Real-time Command Handling */
void GCODE_DMA_ProcessRealTimeCommands(void);
bool GCODE_DMA_IsEmergencyStopActive(void);
bool GCODE_DMA_IsFeedHoldActive(void);
void GCODE_DMA_ClearFeedHold(void);

/* Statistics and Monitoring */
gcode_statistics_t GCODE_DMA_GetStatistics(void);
void GCODE_DMA_ResetStatistics(void);
float GCODE_DMA_GetBufferUtilization(void);

/* Callback Registration */
void GCODE_DMA_RegisterMotionCallback(void (*callback)(gcode_parsed_line_t *command));
void GCODE_DMA_RegisterStatusCallback(void (*callback)(void));
void GCODE_DMA_RegisterEmergencyCallback(void (*callback)(void));

/* DMA Event Handlers - Called from Harmony DMA callbacks */
void GCODE_DMA_RxEventHandler(DMAC_TRANSFER_EVENT event, uintptr_t context);
void GCODE_DMA_TxEventHandler(DMAC_TRANSFER_EVENT event, uintptr_t context);

/* Utility Functions */
bool GCODE_DMA_ParseLine(const char *line, gcode_parsed_line_t *parsed);
bool GCODE_DMA_ValidateChecksum(const char *line);
void GCODE_DMA_CalculateChecksum(const char *line, uint8_t *checksum);

/* Debug and Testing */
void GCODE_DMA_PrintBufferStatus(void);
void GCODE_DMA_PrintStatistics(void);
bool GCODE_DMA_SelfTest(void);

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Access
// *****************************************************************************
// *****************************************************************************

/* Access to parser instance - defined in implementation */
extern gcode_dma_parser_t gcode_dma_parser;

/* Status Access Macros */
#define GCODE_DMA_IsInitialized()      (gcode_dma_parser.initialized)
#define GCODE_DMA_IsEnabled()          (gcode_dma_parser.enabled)
#define GCODE_DMA_GetCommandsQueued()  (gcode_dma_parser.command_queue.count)
#define GCODE_DMA_GetRxBytesReady()    (gcode_dma_parser.dma.rx_bytes_received)
#define GCODE_DMA_IsTxBusy()           (gcode_dma_parser.dma.tx_busy)

// *****************************************************************************
// *****************************************************************************
// Section: Configuration Helpers
// *****************************************************************************
// *****************************************************************************

/* UART2 DMA Trigger Source Verification */
#define GCODE_VERIFY_DMA_CONFIG() \
    do { \
        /* Verify DMA channels are configured for UART2 */ \
        /* IRQ 146 should be UART2_RX, IRQ 147 should be UART2_TX */ \
        /* This is verified during initialization */ \
    } while(0)

/* Integration Example:
 * 
 * // In APP_Initialize():
 * GCODE_DMA_Initialize();
 * GCODE_DMA_RegisterMotionCallback(my_motion_handler);
 * GCODE_DMA_Enable();
 * 
 * // In APP_Tasks():
 * GCODE_DMA_Tasks();
 * 
 * // Motion handler example:
 * void my_motion_handler(gcode_parsed_line_t *command) {
 *     // Extract G-code parameters and execute motion
 *     for(int i = 0; i < command->token_count; i++) {
 *         switch(command->tokens[i].letter) {
 *             case 'G': // G command
 *             case 'X': // X coordinate
 *             case 'Y': // Y coordinate
 *             case 'F': // Feed rate
 *             // etc.
 *         }
 *     }
 * }
 */

#endif /* GCODE_PARSER_DMA_H */

/*******************************************************************************
 End of File
 */