/*******************************************************************************
  Serial Wrapper for GRBL Protocol - Header

  File Name:
    serial_wrapper.h

  Summary:
    GRBL-compatible serial API using MCC plib_uart2

  Description:
    Provides the same API as grbl_serial.h but uses MCC-generated
    plib_uart2 for reliable operation without custom ISR bugs.
*******************************************************************************/

#ifndef SERIAL_WRAPPER_H
#define SERIAL_WRAPPER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // *****************************************************************************
    // Real-Time Command Definitions (GRBL Protocol)
    // *****************************************************************************

#define CMD_RESET 0x18         // Ctrl-X
#define CMD_STATUS_REPORT 0x3F // '?'
#define CMD_CYCLE_START 0x7E   // '~'
#define CMD_FEED_HOLD 0x21     // '!'

    // *****************************************************************************
    // Public API
    // *****************************************************************************

    /**
     * @brief Initialize serial communication using MCC plib_uart2
     */
    void Serial_Initialize(void);

    /**
     * @brief Read one byte from receive buffer
     * @return Byte value (0-255) or -1 if no data available
     */
    int16_t Serial_Read(void);

    /**
     * @brief Write one byte to transmit buffer (blocking)
     * @param data Byte to send
     */
    void Serial_Write(uint8_t data);

    /**
     * @brief Write string to transmit buffer (blocking)
     * @param str Null-terminated string to send
     */
    void Serial_WriteString(const char *str);

    /**
     * @brief Get number of bytes available in receive buffer
     * @return Number of bytes available to read
     */
    uint8_t Serial_Available(void);

    /**
     * @brief Get pending real-time command (if any)
     *
     * Real-time commands are detected in ISR and stored as a flag.
     * This function retrieves and clears the flag (atomic operation).
     *
     * @return Command byte (?, !, ~, Ctrl-X) or 0 if none pending
     */
    uint8_t Serial_GetRealtimeCommand(void);

    /**
     * @brief Clear receive buffer
     */
    void Serial_ResetReadBuffer(void);

    /**
     * @brief Clear transmit buffer (no-op with plib_uart2)
     */
    void Serial_ResetWriteBuffer(void);

    // *****************************************************************************
    // Helper Macros
    // *****************************************************************************

#define Serial_Newline() Serial_WriteString("\r\n")
#define Serial_SendOK() Serial_WriteString("ok\r\n")
#define Serial_SendError(code, msg)   \
    do                                \
    {                                 \
        Serial_WriteString("error:"); \
        Serial_Write('0' + (code));   \
        Serial_WriteString(" (");     \
        Serial_WriteString(msg);      \
        Serial_WriteString(")\r\n");  \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif // SERIAL_WRAPPER_H
