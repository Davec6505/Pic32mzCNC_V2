// grbl_serial.c - GRBL communication using character interrupts

#include "definitions.h"
#include "grbl_serial.h"
#include "app.h" // For APP_UARTPrint
#include "grbl_settings.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define LINE_BUFFER_SIZE 100

static char current_line_buffer[LINE_BUFFER_SIZE];
static int current_line_pos = 0;
static int handshake_query_count = 0;

// Callback functions for application integration
static void (*motion_callback)(const char *) = NULL;
static void (*status_callback)(void) = NULL;
static void (*emergency_callback)(void) = NULL;

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

void GRBL_Serial_Initialize(void)
{
    memset(current_line_buffer, 0, sizeof(current_line_buffer));
    current_line_pos = 0;
    handshake_query_count = 0;
    APP_UARTPrint("Grbl 1.1f ['$' for help]\r\n"); // Send handshake on startup
}

void GRBL_Process_Char(char c)
{
    // Handle real-time characters immediately
    if (c == '?' || c == '!' || c == '~' || c == 0x18)
    {
        handle_real_time_character(c);
        return;
    }

    // Buffer line-based commands
    if (c == '\n' || c == '\r')
    {
        if (current_line_pos > 0)
        {
            current_line_buffer[current_line_pos] = '\0';
            process_line(current_line_buffer);
            current_line_pos = 0;
            memset(current_line_buffer, 0, sizeof(current_line_buffer));
        }
    }
    else if (current_line_pos < (LINE_BUFFER_SIZE - 1))
    {
        current_line_buffer[current_line_pos++] = c;
    }
}

void process_line(const char *line)
{
    if (strlen(line) > 0)
    {
        if (motion_callback)
        {
            motion_callback(line);
        }
        // "ok" response is now handled by the motion_callback (APP_ExecuteMotionCommand)
    }
}

void handle_real_time_character(char c)
{
    switch (c)
    {
    case '?':
        // UGS handshake and status query
        handshake_query_count++;
        if (handshake_query_count == 2)
        {
            APP_UARTPrint("Grbl 1.1f ['$' for help]\r\n");
        }
        else
        {
            if (status_callback)
            {
                status_callback();
            }
        }
        break;
    case '!':
        // Feed hold - Not yet implemented
        break;
    case 0x18: // Ctrl-X, soft-reset
        if (emergency_callback)
        {
            emergency_callback();
        }
        break;
    case '~':
        // Cycle start/resume - Not yet implemented
        break;
    }
}

void GRBL_RegisterMotionCallback(void (*callback)(const char *))
{
    motion_callback = callback;
}

void GRBL_RegisterStatusCallback(void (*callback)(void))
{
    status_callback = callback;
}

void GRBL_RegisterEmergencyCallback(void (*callback)(void))
{
    emergency_callback = callback;
}
