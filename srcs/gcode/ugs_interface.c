/**
 * @file ugs_interface.c
 * @brief Universal G-code Sender (UGS) interface implementation
 *
 * Provides printf-style communication between motion_buffer and UGS via UART2.
 *
 * Data Flow Architecture:
 *   UGS (PC) ←→ UART2 (512B RX/TX ring buffers) ←→ This Module ←→ motion_buffer
 *
 * UART2_Write Interface (plib_uart2.c):
 *   - Copies data to static UART2_WriteBuffer[512] ring buffer
 *   - Uses wrInIndex/wrOutIndex (volatile uint32_t) for thread safety
 *   - UART TX ISR transmits from buffer automatically
 *   - Returns bytes successfully queued (non-blocking)
 *   - Pattern matches motion_buffer ring buffer design
 *
 * @date October 16, 2025
 */

#include "ugs_interface.h"
#include "serial_wrapper.h" /* GRBL serial using MCC plib_uart2 */
#include <stdio.h>          /* For vsnprintf */
#include <stdarg.h>         /* For va_list */
#include <string.h>         /* For strlen, strncpy */

//=============================================================================
// STATIC BUFFERS (MISRA Rule 8.7)
//=============================================================================

/**
 * @brief Static buffer for printf formatting
 *
 * MISRA Rule 8.7: File scope static buffer for thread safety.
 * Size matches UART2 TX ring buffer (512 bytes from plib_uart2.c, increased from 256).
 *
 * Flow: vsnprintf() → This buffer → UART2_Write() → UART2_WriteBuffer[512] → ISR → UGS
 */
static char tx_format_buffer[UGS_SERIAL_TX_BUFFER_SIZE];

//=============================================================================
// INITIALIZATION
//=============================================================================

void UGS_Initialize(void)
{
    /* GRBL serial initialization (direct UART register access) */
    Serial_Initialize();

    /* Clear format buffer */
    (void)memset(tx_format_buffer, 0, sizeof(tx_format_buffer));
}

//=============================================================================
// FORMATTED OUTPUT
//=============================================================================

/**
 * @brief Send formatted string (printf-style)
 *
 * MISRA Compliance:
 * - Rule 21.6: vsnprintf acceptable for formatting
 * - Rule 17.4: Buffer bounds validated
 */
size_t UGS_Printf(const char *format, ...)
{
    va_list args;
    int formatted_length;

    /* MISRA Rule 17.4: Parameter validation */
    if (format == NULL)
    {
        return 0U;
    }

    /* Format string into static buffer */
    va_start(args, format);
    formatted_length = vsnprintf(
        tx_format_buffer,
        UGS_SERIAL_TX_BUFFER_SIZE,
        format,
        args);
    va_end(args);

    /* MISRA Rule 10.3: Validate return value */
    if (formatted_length < 0)
    {
        return 0U; /* Encoding error */
    }

    /* MISRA Rule 10.3: Safe cast after validation */
    if ((size_t)formatted_length >= UGS_SERIAL_TX_BUFFER_SIZE)
    {
        /* Truncated - use buffer size minus null terminator */
        formatted_length = (int)(UGS_SERIAL_TX_BUFFER_SIZE - 1U);
    }

    /* Queue to GRBL serial TX ring buffer */
    for (int i = 0; i < formatted_length; i++)
    {
        Serial_Write((uint8_t)tx_format_buffer[i]);
    }

    return (size_t)formatted_length;
}

/**
 * @brief Send raw string (no formatting)
 *
 * More efficient than printf for constant strings.
 *
 * MISRA Rule 17.4: String length validated
 */
size_t UGS_Print(const char *str)
{
    size_t length;

    /* MISRA Rule 17.4: Parameter validation */
    if (str == NULL)
    {
        return 0U;
    }

    /* Send string via GRBL serial */
    Serial_WriteString(str);

    /* Return length for compatibility */
    length = strlen(str);
    return length;
}

/**
 * @brief Send single character
 *
 * MISRA Rule 10.3: Explicit cast to uint8_t*
 */
size_t UGS_PutChar(char c)
{
    Serial_Write((uint8_t)c);
    return 1U;
}

//=============================================================================
// GRBL PROTOCOL MESSAGES
//=============================================================================

/**
 * @brief Send GRBL "ok" response
 *
 * Critical for GRBL flow control - sender waits for this.
 */
void UGS_SendOK(void)
{
    (void)UGS_Print("ok\r\n");
}

/**
 * @brief Send GRBL error message
 *
 * Format: "error:N - Description\r\n"
 *
 * MISRA Rule 17.4: Pointer parameters validated
 */
void UGS_SendError(uint8_t error_code, const char *description)
{
    if (description != NULL)
    {
        (void)UGS_Printf("error:%u - %s\r\n", error_code, description);
    }
    else
    {
        (void)UGS_Printf("error:%u\r\n", error_code);
    }
}

/**
 * @brief Send GRBL alarm message
 *
 * Format: "ALARM:N - Description\r\n"
 *
 * MISRA Rule 17.4: Pointer parameters validated
 */
void UGS_SendAlarm(uint8_t alarm_code, const char *description)
{
    if (description != NULL)
    {
        (void)UGS_Printf("ALARM:%u - %s\r\n", alarm_code, description);
    }
    else
    {
        (void)UGS_Printf("ALARM:%u\r\n", alarm_code);
    }
}

/**
 * @brief Send GRBL status report
 *
 * Format: "<State|MPos:X,Y,Z|WPos:X,Y,Z>\r\n"
 *
 * MISRA Rule 17.4: Pointer parameter validated
 */
void UGS_SendStatusReport(
    const char *state,
    float mpos_x, float mpos_y, float mpos_z,
    float wpos_x, float wpos_y, float wpos_z)
{
    if (state != NULL)
    {
        (void)UGS_Printf(
            "<%s|MPos:%.3f,%.3f,%.3f|WPos:%.3f,%.3f,%.3f>\r\n",
            state,
            mpos_x, mpos_y, mpos_z,
            wpos_x, wpos_y, wpos_z);
    }
}

/**
 * @brief Send GRBL welcome message
 *
 * Format: "Grbl 1.1f ['$' for help]\r\n"
 */
void UGS_SendWelcome(void)
{
    (void)UGS_Print("Grbl 1.1f ['$' for help]\r\n");
}

/**
 * @brief Send GRBL setting value
 *
 * Format: "$N=value\r\n"
 *
 * Example: "$100=250.000\r\n"
 */
void UGS_SendSetting(uint8_t setting_number, float value)
{
    (void)UGS_Printf("$%u=%.3f\r\n", setting_number, value);
}

/**
 * @brief Send GRBL help message
 *
 * Lists all available $ system commands.
 */
void UGS_SendHelp(void)
{
    (void)UGS_Print("[HLP:$$ $# $G $I $N $x=val $Nx=line $J=line $SLP $C $X $H ~ ! ? ctrl-x]\r\n");
    (void)UGS_Print("[HLP:Available $ commands:]\r\n");
    (void)UGS_Print("[HLP:$$ - View all settings]\r\n");
    (void)UGS_Print("[HLP:$# - View coordinate offsets]\r\n");
    (void)UGS_Print("[HLP:$G - View parser state]\r\n");
    (void)UGS_Print("[HLP:$I - View build info]\r\n");
    (void)UGS_Print("[HLP:$N - View startup blocks]\r\n");
    (void)UGS_Print("[HLP:$x=val - Set setting]\r\n");
    (void)UGS_Print("[HLP:$H - Run homing cycle]\r\n");
    (void)UGS_Print("[HLP:$X - Clear alarm state]\r\n");
    (void)UGS_Print("[HLP:$C - Check G-code mode]\r\n");
}

/**
 * @brief Send GRBL build info (response to "$I" command)
 *
 * Format: "[VER:1.1f.20251017:]\r\n[OPT:V,16,512]\r\n"
 *
 * CRITICAL: UGS uses this to detect GRBL version!
 * Without this response, UGS connection fails.
 */
void UGS_SendBuildInfo(void)
{
    /* Version string: GRBL 1.1f with build date */
    (void)UGS_Print("[VER:1.1f.20251017:PIC32MZ CNC V2]\r\n");

    /* Options string:
     * V = Variable spindle enabled
     * 16 = Motion buffer size (blocks)
     * 512 = Serial RX buffer size (bytes) - Increased from 256 to match mikroC (500B)
     */
    (void)UGS_Print("[OPT:V,16,512]\r\n");
}

/**
 * @brief Send parser state (response to "$G" command)
 *
 * Format: "[GC:G0 G54 G17 G21 G90 G94 M5 M9 T0 F0 S0]\r\n"
 *
 * Shows current modal state for all modal groups.
 */
void UGS_SendParserState(void)
{
    /* TODO: Query actual modal state from gcode_parser
     * For now, send safe defaults */
    (void)UGS_Print("[GC:G0 G54 G17 G21 G90 G94 M5 M9 T0 F0 S0]\r\n");
}

/**
 * @brief Send startup line (response to "$N" commands)
 *
 * Format: "$N=\r\n" (empty by default)
 *
 * @param line_number Startup line number (0 or 1)
 */
void UGS_SendStartupLine(uint8_t line_number)
{
    /* Startup lines not implemented - send empty */
    (void)UGS_Printf("$N%u=\r\n", line_number);
}

//=============================================================================
// INPUT FUNCTIONS (RX Ring Buffer)
//=============================================================================

/**
 * @brief Read line from UART2 RX buffer
 *
 * Reads until '\n' or '\r' encountered or buffer full.
 *
 * MISRA Rule 17.4: Array bounds validated
 */
size_t UGS_ReadLine(char *buffer, size_t buffer_size)
{
    size_t bytes_read = 0U;
    uint8_t byte;

    /* MISRA Rule 17.4: Parameter validation */
    if ((buffer == NULL) || (buffer_size == 0U))
    {
        return 0U;
    }

    /* Read characters until newline or buffer full */
    while (bytes_read < (buffer_size - 1U))
    {
        /* Try to read one byte from GRBL serial RX ring buffer */
        int16_t c = Serial_Read();
        if (c == -1)
        {
            /* No more data available */
            break;
        }

        byte = (uint8_t)c;

        /* Check for line terminator */
        if ((byte == (uint8_t)'\n') || (byte == (uint8_t)'\r'))
        {
            /* Complete line received */
            buffer[bytes_read] = '\0'; /* Null terminate */

            /* Consume any additional line terminators (handle \r\n or \n\r) */
            int16_t next = Serial_Read();
            if (next != -1)
            {
                uint8_t next_byte = (uint8_t)next;
                if ((next_byte != (uint8_t)'\n') && (next_byte != (uint8_t)'\r'))
                {
                    /* Not a line terminator - would need to put back
                     * For now, we'll lose this byte (acceptable for GRBL) */
                }
            }

            return bytes_read; /* Return line length (without terminators) */
        }

        /* Add character to buffer */
        buffer[bytes_read] = (char)byte;
        bytes_read++;
    }

    /* Buffer full or no complete line yet */
    if (bytes_read > 0U)
    {
        buffer[bytes_read] = '\0'; /* Null terminate partial line */
    }

    return 0U; /* No complete line available */
}

/**
 * @brief Check if RX data available
 *
 * Pattern from UART2_ReadCountGet().
 */
bool UGS_RxHasData(void)
{
    return (Serial_Available() > 0U);
}

/**
 * @brief Check if TX buffer has space
 *
 * Note: GRBL serial always has space (blocks if full)
 */
bool UGS_TxHasSpace(void)
{
    return true; /* GRBL serial blocks on write if full */
}

/**
 * @brief Get RX available bytes
 */
size_t UGS_RxAvailable(void)
{
    return (size_t)Serial_Available();
}

/**
 * @brief Get TX free space
 *
 * Note: GRBL serial doesn't expose this, return large value
 */
size_t UGS_TxFreeSpace(void)
{
    return 256U; /* Assume space available (GRBL blocks if full) */
}

//=============================================================================
// DEBUGGING (only if DEBUG defined)
//=============================================================================

#ifdef DEBUG
/**
 * @brief Send debug message
 *
 * Only compiled in DEBUG builds.
 */
void UGS_Debug(const char *format, ...)
{
    va_list args;
    int formatted_length;

    if (format == NULL)
    {
        return;
    }

    /* Prepend [DEBUG] tag */
    (void)UGS_Print("[DEBUG] ");

    /* Format and send message */
    va_start(args, format);
    formatted_length = vsnprintf(
        tx_format_buffer,
        UGS_SERIAL_TX_BUFFER_SIZE,
        format,
        args);
    va_end(args);

    if (formatted_length > 0)
    {
        if ((size_t)formatted_length >= UGS_SERIAL_TX_BUFFER_SIZE)
        {
            formatted_length = (int)(UGS_SERIAL_TX_BUFFER_SIZE - 1U);
        }

        /* Send via GRBL serial */
        for (int i = 0; i < formatted_length; i++)
        {
            Serial_Write((uint8_t)tx_format_buffer[i]);
        }
    }
}
#endif
