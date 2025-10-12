// uart_grbl_simple.c - HYBRID UART/DMA GRBL-compatible UGS communication
// HYBRID VERSION - UART polling for immediate ? response, DMA for line processing

#include "definitions.h"
#include "uart_grbl_simple.h"
#include "uart_debug.h"
#include "grbl_settings.h"
#include "peripheral/dmac/plib_dmac.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int handshake_query_count = 0;

// Buffer state helpers for GRBL status
static inline int get_planner_buffer_state(void)
{
    // TODO: Replace with actual planner buffer state
    return 15; // Example: 15 slots available
}
static inline int get_rx_buffer_state(void)
{
    // TODO: Replace with actual RX buffer state
    return 128; // Example: 128 bytes available
}

// DMA Configuration Constants
#define UART_RX_DMA_CHANNEL DMAC_CHANNEL_0
#define UART_TX_DMA_CHANNEL DMAC_CHANNEL_1
#define RX_BUFFER_SIZE 256
#define LINE_BUFFER_SIZE 100
#define MAX_PENDING_LINES 8

// HYBRID Approach: UART Buffer for immediate polling + DMA Buffer for line processing
static char uart_rx_buffer[16] __attribute__((coherent, aligned(16)));            // Small buffer for polling
static char dma_rx_buffer[RX_BUFFER_SIZE] __attribute__((coherent, aligned(16))); // DMA line buffer
static volatile uint32_t dma_rx_write_count = 0;                                  // Total bytes written by DMA
static volatile uint32_t dma_rx_read_count = 0;                                   // Total bytes read by CPU

// Operation Mode Control
typedef enum
{
    UART_MODE_POLLING,  // Direct UART polling (for ? discovery)
    UART_MODE_DMA_LINES // DMA pattern matching (for line commands)
} uart_operation_mode_t;

static uart_operation_mode_t uart_mode = UART_MODE_POLLING;

// Line Processing System
typedef struct
{
    char lines[MAX_PENDING_LINES][LINE_BUFFER_SIZE];
    volatile int head;
    volatile int tail;
    volatile int count;
} line_queue_t;

static line_queue_t line_queue = {0};
static char current_line_buffer[LINE_BUFFER_SIZE];
static int current_line_pos = 0;

// UGS Connection State Management
typedef enum
{
    UGS_STATE_DISCONNECTED,    // No UGS connection detected
    UGS_STATE_INITIAL_QUERIES, // UGS is sending initial ? queries
    UGS_STATE_BANNER_SENT,     // We sent the Grbl banner
    UGS_STATE_CONNECTED        // Full UGS connection established
} ugs_connection_state_t;

static ugs_connection_state_t ugs_state = UGS_STATE_DISCONNECTED;
static uint32_t status_query_count = 0;
static uint32_t last_status_time = 0;

// DMA State Management
static bool dma_system_initialized = false;
static uint32_t last_activity_time = 0;
static uint32_t ugs_timeout_ms = 5000; // Reduced timeout for faster detection

// Callback functions for application integration
static void (*motion_callback)(const char *) = NULL;
static void (*status_callback)(void) = NULL;
static void (*emergency_callback)(void) = NULL;

// HYBRID MODE SWITCHING FUNCTIONS
static void switch_to_uart_polling_mode(void);
/*
static void switch_to_dma_line_mode(void)
{
    // Stop any active DMA transfers
    DMAC_ChannelDisable(UART_RX_DMA_CHANNEL);
    // Clear buffers
    memset(uart_rx_buffer, 0, sizeof(uart_rx_buffer));
    memset(dma_rx_buffer, 0, sizeof(dma_rx_buffer));
    // Additional DMA setup logic if needed
}
*/

// Forward declarations
static void UART_DMA_RxEventHandler(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle);
static void UART_DMA_TxEventHandler(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle);
static void process_received_characters(void);
static void queue_complete_line(const char *line);
static bool get_next_line(char *line_buffer);
static void handle_real_time_character(char c);

// DMA RX Interrupt Handler - Called when DMA transfer completes or on errors
static void UART_DMA_RxEventHandler(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle)
{
    switch (event)
    {
    case DMAC_TRANSFER_EVENT_COMPLETE:
    {
        // DMA pattern match complete - we have a line ending
        // Process the complete line from DMA buffer
        char *line_start = dma_rx_buffer;
        char *line_end = strchr(line_start, '\n');

        if (line_end != NULL)
        {
            *line_end = '\0'; // Null terminate the line

            // Remove any trailing \r
            if (line_end > line_start && *(line_end - 1) == '\r')
            {
                *(line_end - 1) = '\0';
            }

            // Queue the complete line for processing
            if (strlen(line_start) > 0)
            {
                queue_complete_line(line_start);

                // UGS State Transition: First line command means UGS is connected
                if (ugs_state == UGS_STATE_BANNER_SENT)
                {
                    ugs_state = UGS_STATE_CONNECTED;
                }
            }

            // Clear buffer for next line
            memset(dma_rx_buffer, 0, RX_BUFFER_SIZE);
        }

        // Restart DMA pattern matching for next line
        if (uart_mode == UART_MODE_DMA_LINES)
        {
            DMAC_ChannelPatternMatchSetup(UART_RX_DMA_CHANNEL,
                                          DMAC_DATA_PATTERN_SIZE_1_BYTE,
                                          0x0A); // Match newline character

            DMAC_ChannelTransfer(UART_RX_DMA_CHANNEL,
                                 (const void *)&U2RXREG,
                                 1,
                                 (const void *)dma_rx_buffer,
                                 RX_BUFFER_SIZE,
                                 1);
        }
        break;
    }

    case DMAC_TRANSFER_EVENT_ERROR:
        // Handle DMA error - restart pattern matching if still in DMA mode
        if (uart_mode == UART_MODE_DMA_LINES)
        {
            memset(dma_rx_buffer, 0, RX_BUFFER_SIZE);
            DMAC_ChannelPatternMatchSetup(UART_RX_DMA_CHANNEL,
                                          DMAC_DATA_PATTERN_SIZE_1_BYTE,
                                          0x0A);

            DMAC_ChannelTransfer(UART_RX_DMA_CHANNEL,
                                 (const void *)&U2RXREG,
                                 1,
                                 (const void *)dma_rx_buffer,
                                 RX_BUFFER_SIZE,
                                 1);
        }
        break;

    default:
        break;
    }
}

// DMA TX Interrupt Handler - Called when TX DMA transfer completes
static void UART_DMA_TxEventHandler(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle)
{
    // TX DMA complete - could implement TX queue here if needed
    switch (event)
    {
    case DMAC_TRANSFER_EVENT_COMPLETE:
        // TX transfer complete
        break;

    case DMAC_TRANSFER_EVENT_ERROR:
        // Handle TX error
        break;

    default:
        break;
    }
}

// HYBRID Character Processing - UART polling mode for discovery, DMA for lines
static void process_received_characters(void)
{
    // Always poll UART RX for real-time characters and line buffering
    while (UART2_ReceiverIsReady())
    {
        char received_char = UART2_ReadByte();
        last_activity_time = CORETIMER_GetTickCounter();

        // Immediate response for real-time characters
        if (received_char == '?' || received_char == '!' || received_char == '~' || received_char == 0x18)
        {
            handle_real_time_character(received_char);
            continue; // Don't add to line buffer
        }

        // Buffer line-based commands
        if (received_char == '\n' || received_char == '\r')
        {
            if (current_line_pos > 0)
            {
                current_line_buffer[current_line_pos] = '\0';
                queue_complete_line(current_line_buffer);
                current_line_pos = 0;
                memset(current_line_buffer, 0, sizeof(current_line_buffer));
            }
            continue;
        }

        // Add character to current line (if space available)
        if (current_line_pos < (LINE_BUFFER_SIZE - 1))
        {
            current_line_buffer[current_line_pos] = received_char;
            current_line_pos++;
        }
    }
    // DMA can be used for bulk line transfers, but should not interfere with real-time character handling
}

// Queue a complete line for processing
static void queue_complete_line(const char *line)
{
    // Check if queue has space
    if (line_queue.count < MAX_PENDING_LINES)
    {
        // Copy line to queue
        strncpy(line_queue.lines[line_queue.head], line, LINE_BUFFER_SIZE - 1);
        line_queue.lines[line_queue.head][LINE_BUFFER_SIZE - 1] = '\0';

        // Update queue head and count
        line_queue.head = (line_queue.head + 1) % MAX_PENDING_LINES;
        line_queue.count++;
    }
    // If queue is full, drop the line (could implement overflow handling)
}

// Get next line from queue for processing
static bool get_next_line(char *line_buffer)
{
    if (line_queue.count > 0)
    {
        // Copy line from queue
        strcpy(line_buffer, line_queue.lines[line_queue.tail]);

        // Update queue tail and count
        line_queue.tail = (line_queue.tail + 1) % MAX_PENDING_LINES;
        line_queue.count--;

        return true;
    }
    return false;
}

// Handle real-time characters with UGS pattern recognition
static void handle_real_time_character(char c)
{
    uint32_t current_time = CORETIMER_GetTickCounter();
    switch (c)
    {
    case '?':
        status_query_count++;
        last_status_time = current_time;
        handshake_query_count++;
        if (handshake_query_count == 1)
        {
            // First '?' after connection: send status and version
            char status_msg[128];
            snprintf(status_msg, sizeof(status_msg),
                     "<Idle|MPos:0.000,0.000,0.000|WPos:0.000,0.000,0.000|Bf:%d,%d|FS:0,0>\r\n",
                     get_planner_buffer_state(), get_rx_buffer_state());
            UART_DEBUG_SendString(status_msg);
            UART_DEBUG_SendString("[VER:1.1f.20161014:]\r\n[OPT:VL,15,128]\r\nok\r\n");
        }
        else if (handshake_query_count == 2)
        {
            // Second '?': send firmware banner
            UART_DEBUG_SendString("Grbl 1.1f ['$' for help]\r\n");
        }
        else
        {
            // Subsequent '?': send status
            char status_msg[128];
            snprintf(status_msg, sizeof(status_msg),
                     "<Idle|MPos:0.000,0.000,0.000|WPos:0.000,0.000,0.000|Bf:%d,%d|FS:0,0>\r\n",
                     get_planner_buffer_state(), get_rx_buffer_state());
            UART_DEBUG_SendString(status_msg);
        }
        break;
    case '!':
        if (emergency_callback)
        {
            emergency_callback();
        }
        break;
    case '~':
        // Cycle start / resume
        break;
    case 0x18:
        if (emergency_callback)
        {
            emergency_callback();
        }
        break;
    default:
        break;
    }
}

// HYBRID MODE SWITCHING FUNCTIONS

// Switch to UART polling mode - for immediate ? response during UGS discovery
static void switch_to_uart_polling_mode(void)
{
    // Stop any active DMA transfers
    DMAC_ChannelDisable(UART_RX_DMA_CHANNEL);

    // Clear buffers
    memset(uart_rx_buffer, 0, sizeof(uart_rx_buffer));
    memset(dma_rx_buffer, 0, sizeof(dma_rx_buffer));

    // Set mode to polling
    uart_mode = UART_MODE_POLLING;

    // UART polling will be handled in process_received_characters()
    // No DMA setup needed - direct UART register reading
}

void UART_GRBL_Initialize(void)
{
    // Initialize DMA buffers and state
    memset(uart_rx_buffer, 0, sizeof(uart_rx_buffer));
    memset(dma_rx_buffer, 0, sizeof(dma_rx_buffer));
    memset(&line_queue, 0, sizeof(line_queue));
    memset(current_line_buffer, 0, sizeof(current_line_buffer));

    dma_rx_write_count = 0;
    dma_rx_read_count = 0;
    current_line_pos = 0;

    // Initialize UGS connection state machine
    ugs_state = UGS_STATE_DISCONNECTED;
    status_query_count = 0;
    last_status_time = 0;
    last_activity_time = CORETIMER_GetTickCounter();

    // Register DMA callbacks
    DMAC_ChannelCallbackRegister(UART_RX_DMA_CHANNEL, UART_DMA_RxEventHandler, (uintptr_t)NULL);
    DMAC_ChannelCallbackRegister(UART_TX_DMA_CHANNEL, UART_DMA_TxEventHandler, (uintptr_t)NULL);

    // HYBRID APPROACH: Start in UART polling mode for immediate ? response
    switch_to_uart_polling_mode();

    dma_system_initialized = true;
}

// TRUE DMA ASYNC Main task function with UGS Pattern Recognition
void UART_GRBL_Tasks(void)
{
    if (!dma_system_initialized)
    {
        return;
    }

    uint32_t current_time = CORETIMER_GetTickCounter();

    // UGS Connection State Machine Timeout Handling
    if (ugs_state != UGS_STATE_DISCONNECTED)
    {
        uint32_t time_since_activity = current_time - last_activity_time;
        uint32_t ms_since_activity = time_since_activity / 100000; // Convert to milliseconds

        uint32_t time_since_status = current_time - last_status_time;
        uint32_t ms_since_status = time_since_status / 100000;

        // Timeout if no activity for extended period OR no status queries for shorter period
        if (ms_since_activity > ugs_timeout_ms || ms_since_status > 3000)
        {
            // UGS connection lost - reset state machine completely for reconnection
            UART_GRBL_ResetForNextConnection();
            // HYBRID: Switch back to UART polling mode for reconnection
            switch_to_uart_polling_mode();
            return;
        }
    }

    // ASYNC PROCESSING: Process any characters received in background
    process_received_characters();

    // ASYNC LINE PROCESSING: Handle any complete lines in the queue
    char command_line[LINE_BUFFER_SIZE];
    while (get_next_line(command_line))
    {
        // Remove any trailing whitespace
        for (int k = strlen(command_line) - 1; k >= 0; k--)
        {
            if (command_line[k] == ' ' || command_line[k] == '\t')
            {
                command_line[k] = '\0';
            }
            else
            {
                break;
            }
        }

        // Process the command line based on UGS connection state
        if (command_line[0] == '?')
        {
            // UGS Status query command (after connection established - comes with \n)
            char status_msg[128];
            snprintf(status_msg, sizeof(status_msg),
                     "<Idle|MPos:0.000,0.000,0.000|WPos:0.000,0.000,0.000|Bf:%d,%d|FS:0,0>\r\n",
                     get_planner_buffer_state(), get_rx_buffer_state());
            UART_DEBUG_SendString(status_msg);
        }
        else if (command_line[0] == '$')
        {
            // GRBL system command - mark connection as established if not already
            if (ugs_state == UGS_STATE_BANNER_SENT || ugs_state == UGS_STATE_INITIAL_QUERIES)
            {
                ugs_state = UGS_STATE_CONNECTED;
            }
            if (GRBL_ProcessSystemCommand(command_line))
            {
                // Command was handled successfully
                UART_DEBUG_SendString("ok\r\n");
            }
            else
            {
                UART_DEBUG_SendString("error:3\r\n");
            }
        }
        else if (strlen(command_line) > 0)
        {
            // Regular G-code command - mark connection as established
            if (ugs_state == UGS_STATE_BANNER_SENT || ugs_state == UGS_STATE_INITIAL_QUERIES)
            {
                ugs_state = UGS_STATE_CONNECTED;
            }
            if (motion_callback)
            {
                motion_callback(command_line);
            }
            // Always send ok/error only for G-code/system commands
            UART_DEBUG_SendString("ok\r\n");
        }
    }
}

// TRUE DMA ASYNC Reset for next UGS connection cycle
void UART_GRBL_ResetForNextConnection(void)
{
    ugs_state = UGS_STATE_DISCONNECTED;
    status_query_count = 0;
    last_status_time = 0;
    last_activity_time = CORETIMER_GetTickCounter();
    handshake_query_count = 0;
    // Send GRBL banner immediately on connection reset
    UART_DEBUG_SendString("Grbl 1.1f ['$' for help]\r\n");

    // Clear DMA buffers and queues
    dma_rx_write_count = 0;
    dma_rx_read_count = 0;
    current_line_pos = 0;
    memset(&line_queue, 0, sizeof(line_queue));
    memset(current_line_buffer, 0, sizeof(current_line_buffer));

    // Restart DMA transfer to ensure clean state
    DMAC_ChannelTransfer(UART_RX_DMA_CHANNEL,
                         (const void *)&U2RXREG,      // Source: UART2 RX register
                         1,                           // Source size: 1 byte
                         (const void *)dma_rx_buffer, // Destination: Our circular buffer
                         RX_BUFFER_SIZE,              // Destination size: Full buffer
                         1);                          // Cell size: 1 byte
}

// Manual reset function - same functionality as ResetForNextConnection
void UART_GRBL_ManualReset(void)
{
    UART_GRBL_ResetForNextConnection();
}

// Compatible interface functions for existing app.c code
bool GCODE_DMA_Initialize(void)
{
    UART_GRBL_Initialize();
    return true;
}

void GCODE_DMA_RegisterMotionCallback(void (*callback)(const char *))
{
    motion_callback = callback;
}

void GCODE_DMA_RegisterStatusCallback(void (*callback)(void))
{
    status_callback = callback;
}

void GCODE_DMA_RegisterEmergencyCallback(void (*callback)(void))
{
    emergency_callback = callback;
}

void GCODE_DMA_Enable(void)
{
    // Your MikroC implementation was always enabled - no separate enable needed
}

void GCODE_DMA_SendOK(void)
{
    UART_DEBUG_SendString("ok\r\n");
}

void GCODE_DMA_SendError(int errorCode)
{
    char buffer[32];
    sprintf(buffer, "error:%d\r\n", errorCode);
    UART_DEBUG_SendString(buffer);
}

void GCODE_DMA_SendResponse(const char *response)
{
    char buffer[256];
    sprintf(buffer, "%s\r\n", response);
    UART_DEBUG_SendString(buffer);
}
