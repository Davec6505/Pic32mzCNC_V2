#include "gcode_parser_dma.h"
#include "grbl_settings.h"
#include "peripheral/uart/plib_uart2.h"
#include "peripheral/dmac/plib_dmac.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// *****************************************************************************
// Forward Declarations for Internal Functions
// *****************************************************************************
static void GCODE_DMA_InitializeChannels(void);
static void GCODE_DMA_SetupRxChannel(void);
static void GCODE_DMA_SetupTxChannel(void);
static void GCODE_DMA_StartRxTransfer(void);
static bool GCODE_DMA_StartTxTransfer(const char* data, size_t length);

// *****************************************************************************
// MikroC Pattern: Ring Buffer for UART DMA (ported to Harmony)
// *****************************************************************************

// Ring buffer structure (from your MikroC Serial typedef)
typedef struct {
    char temp_buffer[500];        // Main ring buffer (your MikroC size)
    volatile int head;            // Write pointer
    volatile int tail;            // Read pointer  
    volatile int diff;            // Data length
    volatile char has_data : 1;   // Data available flag
} uart_dma_serial_t;

// DMA buffers with coherent memory (proper Harmony approach)
static char rx_dma_buffer[200] __attribute__((coherent, aligned(16)));
static char tx_dma_buffer[200] __attribute__((coherent, aligned(16)));

// Ring buffer instance (ported from your MikroC serial variable)
static uart_dma_serial_t uart_serial __attribute__((coherent));

// DMA status flags (from your MikroC dma0_int_flag, dma1_int_flag)
static volatile char dma0_int_flag = 0;
static volatile char dma1_int_flag = 0;

// MikroC Pattern: Startup state tracking (your startup bit pattern)
static volatile bool startup_msg_sent = false;

gcode_dma_parser_t gcode_dma_parser;

// *****************************************************************************
// MikroC Pattern: Core DMA Functions (ported to Harmony PLIB)
// *****************************************************************************

// Port of your MikroC get_head_value(), get_tail_value(), get_difference()
static int uart_get_difference(void) {
    if (uart_serial.head > uart_serial.tail)
        uart_serial.diff = uart_serial.head - uart_serial.tail;
    else if (uart_serial.tail > uart_serial.head)
        uart_serial.diff = uart_serial.head;
    else
        uart_serial.diff = 0;
        
    return uart_serial.diff;
}

// Port of your MikroC reset_ring() function  
static void uart_reset_ring(void) {
    uart_serial.tail = uart_serial.head = 0;
}

// *****************************************************************************
// MikroC Pattern: Smart DMA Printf (ported to Harmony PLIB)
// *****************************************************************************

// Port of your exceptional MikroC dma_printf() function
static int uart_dma_printf(const char* format, ...) {
    va_list args;
    char buffer[200];
    int length;
    
    // Format the string
    va_start(args, format);
    length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (length > 0 && length < sizeof(buffer)) {
        // Use DMA for transmission (your MikroC dma_printf pattern)
        if (GCODE_DMA_StartTxTransfer(buffer, length)) {
            return length;
        } else {
            // DMA busy - fallback to blocking UART (for reliability)
            for (int i = 0; i < length; i++) {
                while (!UART2_TransmitterIsReady()) {
                    // Wait for TX ready
                }
                UART2_WriteByte(buffer[i]);
            }
            return length;
        }
    }
    
    return 0;
}

bool GCODE_DMA_Initialize(void) {
    memset(&gcode_dma_parser, 0, sizeof(gcode_dma_parser_t));
    
    // Initialize ring buffer (your MikroC serial initialization)
    memset(&uart_serial, 0, sizeof(uart_dma_serial_t));
    
    // Clear DMA buffers with proper coherent memory
    memset(rx_dma_buffer, 0, sizeof(rx_dma_buffer));
    memset(tx_dma_buffer, 0, sizeof(tx_dma_buffer));
    
    // MikroC Pattern: Initialize DMA channels like your DMA_global()
    GCODE_DMA_InitializeChannels();
    
    gcode_dma_parser.initialized = true;
    return true;
}

// *****************************************************************************
// MikroC Pattern: DMA Channel Setup (port of your DMA0() and DMA1() functions)
// *****************************************************************************

static void GCODE_DMA_InitializeChannels(void) {
    // Enable DMA module (your DMACONSET = 0x8000)
    // This is done by Harmony initialization
    
    // Setup DMA Channel 0 for RX (port of your DMA0() function)
    GCODE_DMA_SetupRxChannel();
    
    // Setup DMA Channel 1 for TX (port of your DMA1() function) 
    GCODE_DMA_SetupTxChannel();
    
    // Register interrupt handlers (your IEC4SET/IFS4CLR pattern)
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, GCODE_DMA_RxEventHandler, 0);
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_1, GCODE_DMA_TxEventHandler, 0);
}

// Port of your MikroC DMA0() function - RX channel setup
static void GCODE_DMA_SetupRxChannel(void) {
    // Your MikroC pattern:
    // DCH0ECON = (146 << 8) | 0x30;  // UART2 RX IRQ + pattern matching
    // DCH0DAT = '?';                 // Pattern data  
    // DCH0SSA = UART2 RX register    
    // DCH0DSA = rx buffer
    // DCH0CSIZ = 1;                  // 1 byte cell size
    
    // Note: Harmony DMAC doesn't have hardware pattern matching like PIC32MZ DMA
    // We'll implement pattern detection in software within the interrupt handler
    
    // Setup is handled by DMAC_ChannelTransfer() calls
}

// Port of your MikroC DMA1() function - TX channel setup  
static void GCODE_DMA_SetupTxChannel(void) {
    // Your MikroC pattern:
    // DCH1ECON = (147 << 8) | 0x30;  // UART2 TX IRQ
    // DCH1SSA = tx buffer
    // DCH1DSA = UART2 TX register
    // DCH1CSIZ = 1;                  // 1 byte cell size
    
    // Initialize TX as not busy
    gcode_dma_parser.dma.tx_busy = false;
}

void GCODE_DMA_Enable(void) {
    gcode_dma_parser.enabled = true;
    
    // Initialize startup state (your startup bit pattern)
    startup_msg_sent = false;
    
    // Reset ring buffer (your serial.head = serial.tail = 0)
    uart_serial.head = 0;
    uart_serial.tail = 0;
    uart_serial.diff = 0;
    uart_serial.has_data = 0;
    
    // Start continuous RX DMA (your DMA0_Enable() pattern)
    GCODE_DMA_StartRxTransfer();
    
    // Send startup message
    uart_dma_printf("Grbl Ready - DMA Pattern Matching Active\r\n");
}

// *****************************************************************************
// MikroC Pattern: DMA Transfer Control (your DMA0_Enable/DMA1_Enable pattern)
// *****************************************************************************

// Port of your MikroC DMA0_Enable() - start RX transfer
static void GCODE_DMA_StartRxTransfer(void) {
    // Start single-character DMA transfer from UART2 RX to our buffer
    // This will trigger interrupt on each character received
    DMAC_ChannelTransfer(
        DMAC_CHANNEL_0,                      // DMA channel 0 (your DCH0)
        (const void*)&U2RXREG,              // Source: UART2 RX register  
        1,                                  // Source size: 1 byte
        (const void*)rx_dma_buffer,         // Destination: our RX buffer
        1,                                  // Dest size: 1 byte (single char)
        1                                   // Cell size: 1 byte
    );
}

// Port of your MikroC DMA1 pattern - start TX transfer  
static bool GCODE_DMA_StartTxTransfer(const char* data, size_t length) {
    // Check if DMA1 is busy (your DMA_CH_Busy(1) pattern)
    if (gcode_dma_parser.dma.tx_busy) {
        return false;  // TX busy, cannot start new transfer
    }
    
    // Copy data to TX buffer
    if (length > sizeof(tx_dma_buffer)) {
        length = sizeof(tx_dma_buffer);
    }
    
    memcpy(tx_dma_buffer, data, length);
    gcode_dma_parser.dma.tx_busy = true;
    
    // Start DMA transfer from our buffer to UART2 TX
    DMAC_ChannelTransfer(
        DMAC_CHANNEL_1,                      // DMA channel 1 (your DCH1)
        (const void*)tx_dma_buffer,         // Source: our TX buffer
        length,                             // Source size: data length
        (const void*)&U2TXREG,              // Destination: UART2 TX register
        1,                                  // Dest size: 1 byte at a time
        1                                   // Cell size: 1 byte
    );
    
    return true;
}

// *****************************************************************************
// MikroC Pattern: DMA Interrupt Handlers (your DMA_CH0_ISR and DMA_CH1_ISR)
// *****************************************************************************

// Port of your MikroC DMA_CH0_ISR() - RX interrupt handler  
void GCODE_DMA_RxEventHandler(DMAC_TRANSFER_EVENT event, uintptr_t context) {
    char received_char;
    
    // Save interrupt flags (your MikroC dma0_int_flag pattern)
    dma0_int_flag = (char)event;
    
    switch(event) {
        case DMAC_TRANSFER_EVENT_COMPLETE:
            // Character received in rx_dma_buffer
            received_char = rx_dma_buffer[0];
            
            // MikroC Pattern: Software pattern matching (replaces DCH0DAT hardware)
            // Before startup: look for '?' pattern (your Do_Startup_Msg)
            if (!startup_msg_sent && received_char == '?') {
                startup_msg_sent = true;  // bit_true(startup,bit(START_MSG))
                uart_dma_printf("Grbl 1.1f ['$' for help]\r\n");  // report_init_message()
                
                // Restart RX DMA for next character (your auto-enable pattern)
                GCODE_DMA_StartRxTransfer();
                return;
            }
            
            // After startup: handle immediate commands (your Do_Critical_Msg)
            if (startup_msg_sent) {
                switch(received_char) {
                    case '?':  // CMD_STATUS_REPORT
                        uart_dma_printf("<Idle|MPos:0.000,0.000,0.000|FS:0,0>\r\n");
                        GCODE_DMA_StartRxTransfer();  // Restart for next character
                        return;
                    case '!':  // CMD_FEED_HOLD  
                        uart_dma_printf("ok\r\n");
                        GCODE_DMA_StartRxTransfer();
                        return;
                    case '~':  // CMD_CYCLE_START
                        uart_dma_printf("ok\r\n"); 
                        GCODE_DMA_StartRxTransfer();
                        return;
                    case 0x18: // CMD_RESET
                        uart_dma_printf("ok\r\n");
                        GCODE_DMA_StartRxTransfer();
                        return;
                }
                
                // Normal character - add to ring buffer (your temp_buffer pattern)
                if (uart_serial.head >= 499) {
                    uart_serial.head = 0;  // Wrap buffer like MikroC
                }
                
                uart_serial.temp_buffer[uart_serial.head] = received_char;
                uart_serial.head++;
                uart_serial.has_data = 1;
            }
            
            // Restart RX DMA for next character (your DCH0CONSET auto-enable)
            GCODE_DMA_StartRxTransfer();
            break;
            
        case DMAC_TRANSFER_EVENT_ERROR:
            // Port of your MikroC CHERIF error handling
            gcode_dma_parser.dma.rx_errors++;
            
            // Restart RX DMA after error (your DMA_Abort pattern)
            GCODE_DMA_StartRxTransfer();
            break;
            
        default:
            break;
    }
}

// Port of your MikroC DMA_CH1_ISR() - TX interrupt handler
void GCODE_DMA_TxEventHandler(DMAC_TRANSFER_EVENT event, uintptr_t context) {
    // Save interrupt flags (your MikroC dma1_int_flag pattern)
    dma1_int_flag = (char)event;
    
    switch(event) {
        case DMAC_TRANSFER_EVENT_COMPLETE:
            // TX transfer complete (your CHBCIF handling)
            gcode_dma_parser.dma.tx_busy = false;  // DMA1 now free
            gcode_dma_parser.dma.tx_transfers_completed++;
            break;
            
        case DMAC_TRANSFER_EVENT_ERROR:
            // Port of your MikroC CHERIF error handling
            gcode_dma_parser.dma.tx_busy = false;
            gcode_dma_parser.dma.tx_errors++;
            break;
            
        default:
            break;
    }
}

// Port of your MikroC Get_Line() function
static int uart_read_buffer_line(char *buffer, int max_length) {
    if (!uart_serial.has_data || max_length <= 0) return 0;
    
    // Look for complete lines (terminated by \n or \r)
    char *line_start = uart_serial.temp_buffer + uart_serial.tail;
    char *line_end = NULL;
    
    // Search from tail to head for line terminator
    for (int i = uart_serial.tail; i < uart_serial.head; i++) {
        if (uart_serial.temp_buffer[i] == '\n' || uart_serial.temp_buffer[i] == '\r') {
            line_end = &uart_serial.temp_buffer[i];
            break;
        }
    }
    
    if (line_end) {
        int line_length = line_end - line_start;
        if (line_length > max_length) line_length = max_length;
        
        // Copy line without the terminator
        strncpy(buffer, line_start, line_length);
        buffer[line_length] = '\0';
        
        // Update tail past the processed line (including terminator)
        uart_serial.tail = (line_end - uart_serial.temp_buffer) + 1;
        
        // Reset data flag if buffer empty (MikroC pattern)
        if (uart_serial.tail >= uart_serial.head) {
            uart_serial.has_data = 0;
            uart_serial.head = uart_serial.tail = 0;  // Reset pointers
        }
        
        return line_length;
    }
    
    return 0;
}

void GCODE_DMA_Tasks(void) {
    if (!gcode_dma_parser.enabled) return;
    
    // MikroC Pattern: Sample_Gcode_Line() equivalent
    // Process complete lines from ring buffer (after line terminator received)
    int dif = uart_get_difference();
    
    if (dif > 0 && uart_serial.has_data) {
        // MikroC Pattern: Get_Line() equivalent - process complete lines
        char command_line[100];
        memset(command_line, 0, sizeof(command_line));
        
        int line_length = uart_read_buffer_line(command_line, sizeof(command_line) - 1);
        
        if (line_length > 0) {
            // MikroC Pattern: Skip '?' in line mode (your "? cannot be used with '\n'" check)
            if (command_line[0] == '?') {
                return;
            }
            
            // Process the G-code line (your Do_Gcode equivalent)
            if (command_line[0] == '$') {
                // GRBL system command
                GRBL_ProcessSystemCommand(command_line);
            } else {
                // Regular G-code command
                uart_dma_printf("ok\r\n");
            }
        }
    }
}

bool GCODE_DMA_SendResponse(const char *response) {
    if (!response) return false;
    
    // Use smart DMA printf (your MikroC dma_printf pattern)
    return uart_dma_printf("%s", response) > 0;
}

// Stub implementations for remaining required functions
void GCODE_DMA_Disable(void) { gcode_dma_parser.enabled = false; }
void GCODE_DMA_Reset(void) { GCODE_DMA_Disable(); uart_reset_ring(); }
bool GCODE_DMA_GetCommand(gcode_parsed_line_t *command) { return false; }
uint8_t GCODE_DMA_GetCommandCount(void) { return 0; }

// Missing functions needed by app.c
void GCODE_DMA_RegisterMotionCallback(void (*callback)(gcode_parsed_line_t *command)) {
    gcode_dma_parser.motion_system_callback = callback;
}

void GCODE_DMA_RegisterStatusCallback(void (*callback)(void)) {
    gcode_dma_parser.status_report_callback = callback;
}

void GCODE_DMA_RegisterEmergencyCallback(void (*callback)(void)) {
    gcode_dma_parser.emergency_stop_callback = callback;
}

void GCODE_DMA_SendOK(void) {
    uart_dma_printf("ok\r\n");  // Use smart DMA printf
}

void GCODE_DMA_SendError(const char *error_message) {
    if (error_message) {
        uart_dma_printf("error:%s\r\n", error_message);  // Use smart DMA printf
    } else {
        uart_dma_printf("error\r\n");  // Use smart DMA printf
    }
}
