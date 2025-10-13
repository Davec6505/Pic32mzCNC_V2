// grbl_serial.c - GRBL communica    // APP_UARTPrint("Grbl 1.1f ['$' for help]\r\n"); // This is now sent by APP_Initializeing character interrupts

#include "definitions.h"
#include "grbl_serial.h"
#include "app.h" // For APP_UARTPrint
#include <string.h>
#include "peripheral/uart/plib_uart2.h"

#define RX_BUFFER_SIZE 128

// --- Static function prototypes ---
/* Function prototypes disabled - using app.c UART handling instead
static void handle_real_time_character(uint8_t c);
static void process_line(void);
*/
static void serial_write(const char *s);

// --- Private variables ---
/* Disabled - using app.c UART handling instead
static uint8_t rx_buffer[RX_BUFFER_SIZE];
static volatile uint16_t rx_buffer_head;
static volatile uint16_t rx_buffer_tail;
static volatile uint8_t rt_command; // Real-time command character

// Buffer for assembling a line of G-code
static char line_buffer[RX_BUFFER_SIZE];
static uint8_t line_char_count;
static bool line_ready;
*/

// Persistent buffer for UART read - disabled
// static uint8_t rx_data;

// --- Callback function pointers ---
static grbl_write_callback_t write_callback = NULL;
static grbl_motion_callback_t motion_callback = NULL;
static grbl_status_callback_t status_callback = NULL;
static grbl_emergency_callback_t emergency_callback = NULL;

/* Callback function disabled - using app.c UART handling instead
static void UART2_Read_Callback(uintptr_t context)
{
    // The data is now in rx_data. Process it.
    if (UART2_ErrorGet() == UART_ERROR_NONE)
    {
        // Handle real-time commands immediately
        if (rx_data == '?' || rx_data == '~' || rx_data == '!')
        {
            rt_command = rx_data;
        }
        else
        {
            // Buffer other characters
            uint16_t next_head = (rx_buffer_head + 1) % RX_BUFFER_SIZE;
            if (next_head != rx_buffer_tail)
            {
                rx_buffer[rx_buffer_head] = rx_data;
                rx_buffer_head = next_head;
            }
        }
    }

    // IMPORTANT: Re-arm the UART read interrupt for the next byte
    UART2_Read(&rx_data, 1);
}
*/

/* GRBL Serial functions disabled - using app.c UART handling instead
void GRBL_Serial_Initialize(void)
{
    rx_buffer_head = 0;
    rx_buffer_tail = 0;
    rt_command = 0;
    line_char_count = 0;
    line_ready = false;

    // Register the callback and arm the first read
    // UART2_ReadCallbackRegister(UART2_Read_Callback, (uintptr_t)NULL);
    // UART2_Read(&rx_data, 1); // Initial read
}
*/

void GRBL_RegisterWriteCallback(grbl_write_callback_t callback)
{
    write_callback = callback;
}

void GRBL_RegisterMotionCallback(grbl_motion_callback_t callback)
{
    motion_callback = callback;
}

void GRBL_RegisterStatusCallback(grbl_status_callback_t callback)
{
    status_callback = callback;
}

void GRBL_RegisterEmergencyCallback(grbl_emergency_callback_t callback)
{
    emergency_callback = callback;
}

static void serial_write(const char *s)
{
    if (write_callback)
    {
        write_callback(s);
    }
}

/* GRBL Tasks function disabled - using app.c UART handling instead
void GRBL_Tasks(void)
{
    // Process real-time commands first, these are high priority
    if (rt_command != 0)
    {
        handle_real_time_character(rt_command);
        rt_command = 0; // Clear the command after handling
    }

    // Process buffered characters for line commands
    while (rx_buffer_tail != rx_buffer_head)
    {
        char c = rx_buffer[rx_buffer_tail];
        rx_buffer_tail = (rx_buffer_tail + 1) % RX_BUFFER_SIZE;

        if (c == '\n' || c == '\r')
        {
            // End of line character received
            if (line_char_count > 0)
            {
                line_buffer[line_char_count] = '\0'; // Null terminate the string
                line_ready = true;
            }
        }
        else
        {
            // Add character to the line buffer
            if (line_char_count < (RX_BUFFER_SIZE - 1))
            {
                line_buffer[line_char_count++] = c;
            }
        }

        if (line_ready)
        {
            process_line();
            line_char_count = 0; // Reset for next line
            line_ready = false;
        }
    }
}
*/

/* Handle real-time commands function disabled
static void handle_real_time_character(uint8_t c)
{
    switch (c)
    {
    case '?': // Status report
        if (status_callback)
        {
            status_callback();
        }
        break;
    case '!': // Feed hold
        // TODO: Implement feed hold
        serial_write("ALARM:Feed Hold\r\n");
        break;
    case '~': // Cycle start/resume
        // TODO: Implement cycle start
        serial_write("ALARM:Cycle Start\r\n");
        break;
    case 0x18: // Ctrl-X, reset
        if (emergency_callback)
        {
            emergency_callback();
        }
        break;
    }
}

static void process_line(void)
{
    if (motion_callback)
    {
        motion_callback(line_buffer);
    }
}
*/

void grbl_send_response(const char *message)
{
    serial_write(message);
}

// Deprecated functions, no longer used in this model
void GRBL_Process_Char(char c) {}
void GRBL_Serial_Tasks(void) {}
