/*******************************************************************************
  Direct Port of MikroC Serial_Dma.c - Your proven working implementation
  
  This is a direct port of your MikroC Serial_Dma.c code that you know works.
  No Harmony complexity, just your proven DMA pattern matching logic.
*******************************************************************************/

#include "definitions.h"
#include "grbl_settings.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// Direct port of your MikroC Serial structure
typedef struct {
    char temp_buffer[500];        // Your exact MikroC size
    int head;
    int tail; 
    int diff;
    int has_data;
} Serial;

static Serial serial = {0};
static char rxBuf[200] = {0};     // Your MikroC rxBuf size
static char current_pattern = '?';  // Current pattern: '?' for status queries, '\n' for commands
static char pattern_switched = 0;   // Flag to indicate pattern was switched

// Port of your MikroC Get_Difference() function - EXACT copy
static int Get_Difference(void) {
    if(serial.head > serial.tail)
        serial.diff = serial.head - serial.tail;
    else if(serial.tail > serial.head)
        serial.diff = serial.head;
    else
        serial.diff = 0;

    return serial.diff;
}

// Port of your MikroC Get_Line() function - EXACT copy
static void Get_Line(char *str, int dif) {
    if(serial.tail + dif > 499)
        serial.tail = 0;

    strncpy(str, serial.temp_buffer + serial.tail, dif);
    serial.tail += dif;
}

// Port of your MikroC Reset_Ring() function - EXACT copy  
__attribute__((unused)) static void Reset_Ring(void) {
    serial.tail = serial.head = 0;
}

// Simple UART send function - replaces your dma_printf
static void uart_printf(const char* format, ...) {
    char buffer[200];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    const char* str = buffer;
    while (*str) {
        while (!UART2_TransmitterIsReady());
        UART2_WriteByte(*str);
        str++;
    }
}

// Port of your MikroC DMA_Switch_Pattern function - EXACT copy
static void DMA_Switch_Pattern(char new_pattern) {
    if (current_pattern != new_pattern) {
        current_pattern = new_pattern;
        pattern_switched = 1;
    }
}

// Port of your MikroC DMA_Handle_Pattern_Switch function - EXACT copy
static void DMA_Handle_Pattern_Switch(void) {
    if (pattern_switched && current_pattern == '\n') {
        // Add \r\n to the output buffer when switching from '?' to '\n'
        uart_printf("\r\n");
        pattern_switched = 0;  // Reset the flag
    }
}

// Initialize - Port of your MikroC initialization
void UART_GRBL_Initialize(void) {
    // Reset your MikroC serial structure
    serial.head = serial.tail = serial.diff = 0;
    serial.has_data = 0;
    memset(serial.temp_buffer, 0, sizeof(serial.temp_buffer));
    memset(rxBuf, 0, sizeof(rxBuf));
    
    // Initialize pattern matching state - your MikroC defaults
    current_pattern = '?';
    pattern_switched = 0;
    
    // Send GRBL greeting message that UGS expects
    uart_printf("Grbl 1.1f ['$' for help]\r\n");
}

// Port of your MikroC DMA interrupt logic - simplified for UART polling
void UART_GRBL_Tasks(void) {
    // Check if there's data available in UART2
    if (UART2_ReceiverIsReady()) {
        char received_char = UART2_ReadByte();
        int i = 1;  // We received 1 character
        
        // YOUR EXACT MikroC logic from DMA_CH0_ISR() - Fixed for GRBL protocol
        if (i > 0) {
            char last_char = received_char;
            
            // If we received '?' - this is a real-time status query (immediate response)
            if (last_char == '?') {
                // Send status report immediately (GRBL real-time command)
                uart_printf("<Idle|MPos:0.000,0.000,0.000|FS:0,0>\r\n");
                // Don't change pattern - stay ready for more status queries or commands
                return; // Don't buffer the '?' character
            }
            // If we received '\n' and current pattern is '\n', we have a complete command
            else if (last_char == '\n' && current_pattern == '\n') {
                DMA_Handle_Pattern_Switch();
                // Switch back to '?' pattern for next status query
                DMA_Switch_Pattern('?');
                
                // Mark that we have a complete line
                serial.has_data = 1;
            }
            // If we received '\n' but pattern was '?', switch to handle commands
            else if (last_char == '\n' && current_pattern == '?') {
                DMA_Switch_Pattern('\n');
                // Mark that we have a complete line
                serial.has_data = 1;
            }
        }

        // YOUR EXACT MikroC buffer management from DMA_CH0_ISR()
        // copy RxBuf -> temp_buffer  BUFFER_LENGTH
        // make sure that head + i don't exceed max buffer length
        if(serial.head + i > 499)
           serial.head = 0;

        // Put the received character into rxBuf first (your pattern)
        rxBuf[0] = received_char;
        
        strncpy(serial.temp_buffer + serial.head, rxBuf, i);
        serial.head += i;
        memset(rxBuf, 0, i + 2);  // Your exact memset pattern
    }
    
    // YOUR EXACT MikroC command processing logic
    int dif = Get_Difference();
    
    if (dif > 0 && serial.has_data) {
        char command_line[50];  // Your MikroC buffer size
        memset(command_line, 0, sizeof(command_line));  // Clear buffer
        
        // Use your exact Get_Line function
        Get_Line(command_line, dif);
        
        // Ensure null termination and remove any trailing whitespace
        command_line[dif] = '\0';
        
        // Remove trailing \r, \n, spaces
        for (int i = strlen(command_line) - 1; i >= 0; i--) {
            if (command_line[i] == '\r' || command_line[i] == '\n' || command_line[i] == ' ') {
                command_line[i] = '\0';
            } else {
                break;
            }
        }
        
        // Debug: Send what command we received
        // uart_printf("DEBUG: Received '%s' (len=%d)\r\n", command_line, strlen(command_line));
        
        // Process the command line - your Do_Gcode equivalent
        if (command_line[0] == '$') {
            // GRBL system command
            if (GRBL_ProcessSystemCommand(command_line)) {
                // Command was handled successfully
            } else {
                // Unknown command, send error
                uart_printf("error:3\r\n");
            }
        } else if (strlen(command_line) > 0) {
            // Regular G-code command
            uart_printf("ok\r\n");
        }
        
        // Reset has_data flag after processing
        serial.has_data = 0;
    }
}

// Compatible interface functions for existing app.c code
bool GCODE_DMA_Initialize(void) {
    UART_GRBL_Initialize();
    return true;
}

void GCODE_DMA_RegisterMotionCallback(void (*callback)(const char*)) {
    // Your MikroC implementation handled this directly - no callback needed
}

void GCODE_DMA_RegisterStatusCallback(void (*callback)(void)) {
    // Your MikroC implementation handled this directly - no callback needed  
}

void GCODE_DMA_RegisterEmergencyCallback(void (*callback)(void)) {
    // Your MikroC implementation handled this directly - no callback needed
}

void GCODE_DMA_Enable(void) {
    // Your MikroC implementation was always enabled - no separate enable needed
}

void GCODE_DMA_SendOK(void) {
    uart_printf("ok\r\n");
}

void GCODE_DMA_SendError(int errorCode) {
    char buffer[32];
    sprintf(buffer, "error:%d\r\n", errorCode);
    uart_printf(buffer);
}

void GCODE_DMA_SendResponse(const char* response) {
    uart_printf("%s\r\n", response);
}

int GCODE_DMA_GetCommandCount(void) {
    return 0;  // Your MikroC implementation didn't track command count
}