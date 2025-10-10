#include "gcode_parser_dma.h"
#include "grbl_settings.h"
#include "peripheral/uart/plib_uart2.h"
#include "peripheral/dmac/plib_dmac.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

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

// Port of your MikroC get_line() function
static void uart_get_line(char *str, int diff) {
    if (uart_serial.tail + diff > 499)
        uart_serial.tail = 0;
        
    strncpy(str, uart_serial.temp_buffer + uart_serial.tail, diff);
    uart_serial.tail += diff;
}

// Port of your MikroC reset_ring() function  
static void uart_reset_ring(void) {
    uart_serial.tail = uart_serial.head = 0;
}

// Port of your MikroC DMA1_IsBusy() check
static bool uart_dma_tx_is_busy(void) {
    return gcode_dma_parser.dma.tx_busy;
}

// *****************************************************************************
// MikroC Pattern: Smart DMA Printf (ported to Harmony PLIB)
// *****************************************************************************

// Port of your exceptional MikroC dma_printf() function
static int uart_dma_printf(const char* format, ...) {
    va_list args;
    char buffer[200];
    int length;
    
    // Key MikroC pattern: Check if DMA1 TX is busy first!
    if (uart_dma_tx_is_busy()) {
        return 0;  // Don't send if busy - critical for stability
    }
    
    // Format the string
    va_start(args, format);
    length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (length > 0 && length < sizeof(buffer)) {
        // Copy to TX buffer (your MikroC txBuf pattern)
        memcpy(tx_dma_buffer, buffer, length);
        
        // Start DMA1 transmission using Harmony PLIB
        gcode_dma_parser.dma.tx_busy = true;
        
        bool result = DMAC_ChannelTransfer(
            DMAC_CHANNEL_1,                    // DMA1 (your MikroC channel)
            (const void*)tx_dma_buffer,       // Source: txBuf equivalent
            length,                           // Dynamic block size (your MikroC DCH1SSIZ pattern)
            (const void*)&U2TXREG,           // Dest: UART2 TX register
            1,                               // Dest size
            1                                // Cell size
        );
        
        if (!result) {
            gcode_dma_parser.dma.tx_busy = false;
            return 0;
        }
        
        return length;
    }
    
    return 0;
}

bool GCODE_DMA_Initialize(void) {
    memset(&gcode_dma_parser, 0, sizeof(gcode_dma_parser_t));
    
    // Initialize ring buffer (your MikroC serial initialization)
    memset(&uart_serial, 0, sizeof(uart_dma_serial_t));
    
    // Clear DMA buffers 
    memset(rx_dma_buffer, 0, sizeof(rx_dma_buffer));
    memset(tx_dma_buffer, 0, sizeof(tx_dma_buffer));
    
    // Register DMA event handlers (Harmony PLIB callbacks)
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, GCODE_DMA_RxEventHandler, 0);
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_1, GCODE_DMA_TxEventHandler, 0);
    
    gcode_dma_parser.initialized = true;
    return true;
}

void GCODE_DMA_Enable(void) {
    gcode_dma_parser.enabled = true;
    
    // Start continuous RX DMA (port of your MikroC DMA0_Enable pattern)
    DMAC_ChannelTransfer(
        DMAC_CHANNEL_0,                      // DMA0 (your MikroC RX channel)  
        (const void*)&U2RXREG,              // Source: UART2 RX register
        1,                                  // Source size (1 byte at a time)
        (const void*)rx_dma_buffer,         // Dest: rxBuf equivalent
        sizeof(rx_dma_buffer),              // Dest size (your MikroC 200 bytes)
        1                                   // Cell size
    );
}

// *****************************************************************************
// MikroC Pattern: DMA Event Handlers (ported from your interrupt handlers)
// *****************************************************************************

// Port of your MikroC DMA_CH0_ISR() - RX interrupt handler  
void GCODE_DMA_RxEventHandler(DMAC_TRANSFER_EVENT event, uintptr_t context) {
    int length = 0;
    
    // Save interrupt flags (your MikroC dma0_int_flag pattern)
    dma0_int_flag = (char)event;
    
    switch(event) {
        case DMAC_TRANSFER_EVENT_COMPLETE:
            // Port of your MikroC block complete logic
            length = strlen(rx_dma_buffer);
            
            if (length > 0) {
                // Check for buffer wrap (your MikroC head pointer logic)
                if (uart_serial.head + length > 499) {
                    uart_serial.head = 0;
                }
                
                // Copy to ring buffer (your MikroC temp_buffer pattern)
                strncpy(uart_serial.temp_buffer + uart_serial.head, rx_dma_buffer, length);
                uart_serial.head += length;
                uart_serial.has_data = 1;
                
                // Check for immediate GRBL commands (your pattern matching concept)
                for (int i = 0; i < length; i++) {
                    if (rx_dma_buffer[i] == '?') {
                        // Immediate response (your MikroC pattern for responsiveness)
                        GRBL_ProcessSystemCommand("?");
                        break;
                    }
                }
                
                // Clear RX buffer (your MikroC memset pattern)
                memset(rx_dma_buffer, 0, length + 2);
            }
            
            // Restart RX DMA for continuous reception (auto-enable concept)
            DMAC_ChannelTransfer(
                DMAC_CHANNEL_0,
                (const void*)&U2RXREG,
                1,
                (const void*)rx_dma_buffer,
                sizeof(rx_dma_buffer),
                1
            );
            break;
            
        case DMAC_TRANSFER_EVENT_ERROR:
            // Port of your MikroC CHERIF error handling
            gcode_dma_parser.dma.rx_errors++;
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
            // Port of your MikroC block complete logic
            gcode_dma_parser.dma.tx_busy = false;  // DMA1 now free
            gcode_dma_parser.dma.tx_transfers_completed++;
            break;
            
        case DMAC_TRANSFER_EVENT_ERROR:
            // Port of your MikroC CHERIF error handling with CABORT
            gcode_dma_parser.dma.tx_busy = false;
            gcode_dma_parser.dma.tx_errors++;
            break;
            
        default:
            break;
    }
}

void GCODE_DMA_Tasks(void) {
    if (!gcode_dma_parser.enabled) return;
    
    // Port of your MikroC main processing loop using ring buffer
    int data_length = uart_get_difference();
    
    if (data_length > 0 && uart_serial.has_data) {
        char command_line[100];
        
        // Get line from ring buffer (your MikroC get_line pattern)
        uart_get_line(command_line, data_length);
        command_line[data_length] = '\0';
        
        // Process GRBL commands (enhanced from your simple polling)
        if (command_line[0] == '$') {
            GRBL_ProcessSystemCommand(command_line);
        }
        
        // Reset data flag if buffer empty (your MikroC logic)
        if (uart_get_difference() == 0) {
            uart_serial.has_data = 0;
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
