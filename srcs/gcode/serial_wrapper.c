/*******************************************************************************
  Serial Wrapper for GRBL Protocol

  File Name:
    serial_wrapper.c

  Summary:
    Thin wrapper around MCC-generated plib_uart2 for GRBL compatibility

  Description:
    This file provides a simple ring buffer wrapper around Microchip's
    plib_uart2 to provide GRBL-style serial API without custom ISR bugs.

  Architecture:
    - Uses MCC-generated UART2 with callback-based reading
    - Ring buffer maintained in main code (not ISR)
    - Real-time commands detected in main loop
    - Avoids ISR timing issues that blocked TMR1
*******************************************************************************/

#include "serial_wrapper.h"
#include "gcode_parser.h"
#include "config/default/peripheral/uart/plib_uart2.h"
#include "config/default/peripheral/gpio/plib_gpio.h"  // For LED debug
#include <string.h>

// *****************************************************************************
// Ring Buffer Configuration
// *****************************************************************************

#define SERIAL_RX_BUFFER_SIZE 256
#define SERIAL_TX_BUFFER_SIZE 256

typedef struct
{
    volatile uint8_t data[SERIAL_RX_BUFFER_SIZE];
    volatile uint8_t head;
    volatile uint8_t tail;
} ring_buffer_t;

// *****************************************************************************
// Local Variables
// *****************************************************************************

static ring_buffer_t rx_buffer = {{0}, 0, 0};
static volatile uint8_t realtime_cmd = 0; // Real-time command pending (set in ISR, read in main loop)
static uint8_t uart_rx_byte;

// *****************************************************************************
// UART2 Read Callback (called from ISR context)
// *****************************************************************************

void Serial_RxCallback(uintptr_t context)
{
    // This is called when UART2_Read() completes (1 byte received)
    uint8_t data = uart_rx_byte;

    // DEBUG: Toggle LED2 to prove ISR is being called
    LED2_Toggle();

    // GRBL Pattern: Check for real-time commands and SET FLAG (don't execute in ISR!)
    // ISR context cannot safely call UART blocking functions
    if (GCode_IsControlChar(data))
    {
        // Store the command - main loop will handle it
        realtime_cmd = data;

        // Do NOT add to ring buffer - real-time commands are single-byte
        // and will be processed by main loop checking Serial_GetRealtimeCommand()
    }
    else
    {
        // Regular data - add to ring buffer for main loop processing
        uint8_t next_head = (rx_buffer.head + 1) & (SERIAL_RX_BUFFER_SIZE - 1);
        if (next_head != rx_buffer.tail)
        {
            rx_buffer.data[rx_buffer.head] = data;
            rx_buffer.head = next_head;
        }
        // If buffer full, data is silently dropped (GRBL behavior)
    }

    // Start next read immediately (re-enable RX interrupt)
    UART2_Read(&uart_rx_byte, 1);
} // *****************************************************************************
// Public API Implementation
// *****************************************************************************

void Serial_Initialize(void)
{
    // NOTE: UART2_Initialize() is called by SYS_Initialize() in initialization.c
    // Do NOT call it again here!

    // Clear ring buffer
    rx_buffer.head = 0;
    rx_buffer.tail = 0;
    realtime_cmd = 0;

    // Register callback and start first read
    UART2_ReadCallbackRegister(Serial_RxCallback, 0);
    UART2_Read(&uart_rx_byte, 1);
}

int16_t Serial_Read(void)
{
    if (rx_buffer.head == rx_buffer.tail)
    {
        return -1; // No data
    }

    uint8_t data = rx_buffer.data[rx_buffer.tail];
    rx_buffer.tail = (rx_buffer.tail + 1) & (SERIAL_RX_BUFFER_SIZE - 1);
    return (int16_t)data;
}

void Serial_Write(uint8_t data)
{
    // Use blocking write (plib_uart2 handles queueing)
    UART2_Write(&data, 1);

    // Wait for write to complete
    while (UART2_WriteIsBusy())
    {
        // Blocking is OK for single byte
    }
}

void Serial_WriteString(const char *str)
{
    if (str == NULL)
    {
        return;
    }

    size_t len = strlen(str);
    UART2_Write((void *)str, len);

    // Wait for write to complete
    while (UART2_WriteIsBusy())
    {
        // Blocking is OK for small strings
    }
}

uint8_t Serial_Available(void)
{
    uint8_t available = (uint8_t)((rx_buffer.head - rx_buffer.tail) & (SERIAL_RX_BUFFER_SIZE - 1));
    return available;
}

uint8_t Serial_GetRealtimeCommand(void)
{
    uint8_t cmd = realtime_cmd;
    realtime_cmd = 0;
    return cmd;
}

void Serial_ResetReadBuffer(void)
{
    rx_buffer.head = 0;
    rx_buffer.tail = 0;
}

void Serial_ResetWriteBuffer(void)
{
    // Not needed with plib_uart2
}
