/**
 * @file ugs_interface.h
 * @brief Universal G-code Sender (UGS) interface using UART2 ring buffer
 *
 * Provides printf-style communication for UGS (Universal G-code Sender).
 * This module bridges motion_buffer (G-code planning) and UART2 (serial output).
 *
 * Data Flow:
 *   1. UGS → UART2 RX ring buffer (256 bytes, ISR-driven)
 *   2. UGS_Interface_ReadLine() → Parse G-code → motion_buffer
 *   3. motion_buffer → multiaxis_control → stepper motion
 *   4. UGS_Interface_Printf() → vsnprintf() → UART2_Write() → TX ring buffer → UGS
 *
 * UART2_Write Interface:
 *   - Copies data to 256-byte TX ring buffer (UART2_WriteBuffer[256])
 *   - Returns bytes successfully queued (non-blocking)
 *   - UART TX ISR transmits from ring buffer automatically
 *   - Uses wrInIndex/wrOutIndex pattern (same as motion_buffer)
 *
 * @date October 16, 2025
 *
 * MISRA C Compliance:
 * - Rule 8.7: Static internal buffers
 * - Rule 21.6: stdio.h vsnprintf acceptable for formatting
 * - Rule 17.4: Array bounds validated
 */

#ifndef UGS_INTERFACE_H
#define UGS_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

//=============================================================================
// UGS/GRBL PROTOCOL CONSTANTS
//=============================================================================

/**
 * @brief Maximum length for formatted output strings
 *
 * Matches UART2 TX ring buffer size (256 bytes from plib_uart2.c).
 * GRBL v1.1f protocol typically uses 256 byte buffers for status reports.
 */
#define UGS_SERIAL_TX_BUFFER_SIZE 256U

/**
 * @brief GRBL real-time commands (single byte, bypass queue)
 *
 * UGS sends these single-character commands for real-time control.
 */
#define UGS_CMD_STATUS_REPORT '?' /* Query current status */
#define UGS_CMD_CYCLE_START '~'   /* Resume from feed hold */
#define UGS_CMD_FEED_HOLD '!'     /* Pause motion */
#define UGS_CMD_SOFT_RESET 0x18   /* Ctrl-X - immediate reset */

    //=============================================================================
    // INITIALIZATION
    //=============================================================================

    /**
     * @brief Initialize UGS serial interface
     *
     * Wraps UART2_Initialize() which sets up:
     * - 256-byte TX ring buffer (UART2_WriteBuffer)
     * - 256-byte RX ring buffer (UART2_ReadBuffer)
     * - Interrupt-driven TX/RX handlers
     * - wrInIndex/wrOutIndex for ring buffer management
     *
     * Must be called before any UGS_* functions.
     *
     * MISRA Rule 8.7: Calls MCC-generated UART2_Initialize()
     */
    void UGS_Initialize(void);

    //=============================================================================
    // FORMATTED OUTPUT (printf-style)
    //=============================================================================

    /**
     * @brief Send formatted string to UART2 TX ring buffer (printf-style)
     *
     * Implementation:
     *   1. vsnprintf() → Format string into static buffer (256 bytes)
     *   2. UART2_Write(buffer, length) → Copy to TX ring buffer
     *   3. UART TX ISR → Transmit from ring buffer to UGS
     *
     * Thread-safe: Static buffer is file-scoped (not reentrant).
     * ISR-safe: UART2_Write() uses interrupt-safe ring buffer.
     *
     * @param format Printf-style format string
     * @param ... Variable arguments for format string
     * @return Number of bytes queued to TX buffer (0 if buffer full)
     *
     * MISRA Compliance:
     * - Rule 21.6: vsnprintf acceptable for formatting
     * - Rule 17.4: String length validated before UART2_Write()
     *
     * Example:
     *   UGS_Printf("X:%.3f Y:%.3f Z:%.3f\r\n", x, y, z);
     */
    size_t UGS_Printf(const char *format, ...);

    /**
     * @brief Send raw string to UART2 (no formatting)
     *
     * More efficient than printf for constant strings.
     * Directly calls UART2_Write() → TX ring buffer.
     *
     * @param str Null-terminated string to send
     * @return Number of bytes queued to TX buffer
     *
     * Example:
     *   UGS_Print("ok\r\n");
     */
    size_t UGS_Print(const char *str);

    /**
     * @brief Send single character to UART2
     *
     * @param c Character to send
     * @return 1 if queued, 0 if TX buffer full
     */
    size_t UGS_PutChar(char c);

    //=============================================================================
    // UGS/GRBL PROTOCOL OUTPUT FUNCTIONS
    //=============================================================================

    /**
     * @brief Send "ok" response to UGS
     *
     * Tells UGS that last G-code command was accepted and queued to motion_buffer.
     * Critical for GRBL flow control - UGS waits for "ok" before sending next command.
     *
     * Integration with motion_buffer:
     *   1. UGS_ReadLine() → Parse G-code → parsed_move_t
     *   2. MotionBuffer_Add(&move) → Returns true if space available
     *   3. If true: UGS_SendOK() → UGS sends next command
     *   4. If false: Don't send "ok" → UGS waits → Retry next loop
     *
     * Example:
     *   if (GCode_ParseLine(line, &move)) {
     *       if (MotionBuffer_Add(&move)) {
     *           UGS_SendOK();  // ✅ Accepted - UGS sends next
     *       }
     *       // ❌ Buffer full - DON'T send ok, UGS will retry
     *   }
     */
    void UGS_SendOK(void);

    /**
     * @brief Send error message to UGS
     *
     * Format: "error:N - Description\r\n"
     * Where N is GRBL error code (1-254).
     *
     * @param error_code GRBL error code (1-254)
     * @param description Human-readable error description
     *
     * GRBL Error Codes (v1.1f):
     *   1 = G-code command letter not found
     *   2 = G-code command value invalid
     *   3 = $ system command not recognized
     *   20 = Soft limit exceeded
     *   21 = Hard limit triggered
     *   ... (see GRBL v1.1f documentation)
     *
     * Example:
     *   UGS_SendError(20, "Soft limit");
     *   // Outputs: "error:20 - Soft limit\r\n"
     */
    void UGS_SendError(uint8_t error_code, const char *description);

    /**
     * @brief Send alarm message to UGS
     *
     * Format: "ALARM:N - Description\r\n"
     * Where N is GRBL alarm code (1-254).
     *
     * @param alarm_code GRBL alarm code
     * @param description Human-readable alarm description
     *
     * GRBL Alarm Codes (v1.1f):
     *   1 = Hard limit triggered
     *   2 = Soft limit exceeded
     *   3 = Abort during cycle
     *   9 = Homing failed
     *
     * Example:
     *   UGS_SendAlarm(1, "Hard limit");
     *   // Outputs: "ALARM:1 - Hard limit\r\n"
     */
    void UGS_SendAlarm(uint8_t alarm_code, const char *description);

    /**
     * @brief Send status report to UGS
     *
     * Format: "<Idle|MPos:0.000,0.000,0.000|WPos:0.000,0.000,0.000>\r\n"
     *
     * Sent in response to '?' real-time command from UGS.
     * Uses multiaxis_control for step counts, motion_math for step→mm conversion.
     *
     * Integration example:
     *   float mpos_x = MultiAxis_GetStepCount(AXIS_X) / 80.0f;  // 80 steps/mm
     *   UGS_SendStatusReport("Run", mpos_x, ...);
     *
     * @param state Machine state ("Idle", "Run", "Hold", "Alarm", "Home")
     * @param mpos_x Machine position X (absolute from origin)
     * @param mpos_y Machine position Y
     * @param mpos_z Machine position Z
     * @param wpos_x Work position X (relative to G54 offset)
     * @param wpos_y Work position Y
     * @param wpos_z Work position Z
     *
     * Example:
     *   UGS_SendStatusReport("Run", 10.5, 20.3, 5.0, 10.5, 20.3, 5.0);
     *   // Outputs: "<Run|MPos:10.500,20.300,5.000|WPos:10.500,20.300,5.000>\r\n"
     */
    void UGS_SendStatusReport(
        const char *state,
        float mpos_x, float mpos_y, float mpos_z,
        float wpos_x, float wpos_y, float wpos_z);

    /**
     * @brief Send welcome message to UGS
     *
     * Format: "Grbl 1.1f ['$' for help]\r\n"
     *
     * Sent on startup or after soft reset (Ctrl-X).
     */
    void UGS_SendWelcome(void);

    /**
     * @brief Send setting value to UGS
     *
     * Format: "$N=value\r\n"
     *
     * Example: $100=250.000 (X steps/mm)
     *
     * Uses motion_math for getting GRBL settings ($100-$133).
     *
     * @param setting_number GRBL setting number ($100-$133)
     * @param value Setting value (float)
     *
     * Example:
     *   float steps_per_mm = MotionMath_GetStepsPerMM(AXIS_X);
     *   UGS_SendSetting(100, steps_per_mm);
     *   // Outputs: "$100=250.000\r\n"
     */
    void UGS_SendSetting(uint8_t setting_number, float value);

    /**
     * @brief Send help message to UGS (response to "$" command)
     *
     * Lists all available $ system commands.
     */
    void UGS_SendHelp(void);

    /**
     * @brief Send build info to UGS (response to "$I" command)
     *
     * Format: "[VER:1.1f.20251017:]\r\n[OPT:]\r\n"
     *
     * GRBL v1.1f requires this for UGS version detection.
     * UGS sends "$I" on connection to identify firmware.
     *
     * Example output:
     *   [VER:1.1f.20251017:]
     *   [OPT:V,15,128]
     *   ok
     *
     * Where:
     *   VER = GRBL version and build date
     *   OPT = Compile-time options (V=variable spindle, 15=buffer blocks, 128=buffer size)
     */
    void UGS_SendBuildInfo(void);

    /**
     * @brief Send parser state (response to "$G" command)
     *
     * Format: "[GC:G0 G54 G17 G21 G90 G94 M5 M9 T0 F0 S0]\r\n"
     *
     * Shows current modal state for all modal groups.
     * Uses GCode_GetModalState() to query parser state.
     */
    void UGS_SendParserState(void);

    /**
     * @brief Send startup line (response to "$N" commands)
     *
     * Format: "$N=G-code line\r\n"
     * Where N is 0 or 1 (two startup lines supported in GRBL)
     *
     * @param line_number Startup line number (0 or 1)
     */
    void UGS_SendStartupLine(uint8_t line_number);

    //=============================================================================
    // INPUT (RX Buffer Read)
    //=============================================================================

    /**
     * @brief Read line from UART2 RX ring buffer
     *
     * Reads characters until '\n' or '\r' encountered or buffer full.
     * Implements GRBL line-based protocol.
     *
     * Integration with motion_buffer:
     *   1. UGS_ReadLine() → Complete G-code line
     *   2. GCode_ParseLine() → parsed_move_t structure
     *   3. MotionBuffer_Add(&move) → Queue for execution
     *   4. If successful: UGS_SendOK()
     *
     * @param buffer Output buffer for line
     * @param buffer_size Size of output buffer
     * @return Number of bytes read (0 if no complete line available)
     *
     * MISRA Rule 17.4: Buffer bounds validated
     *
     * Example:
     *   char line[128];
     *   parsed_move_t move;
     *   if (UGS_ReadLine(line, sizeof(line))) {
     *       if (GCode_ParseLine(line, &move)) {
     *           if (MotionBuffer_Add(&move)) {
     *               UGS_SendOK();
     *           }
     *       }
     *   }
     */
    size_t UGS_ReadLine(char *buffer, size_t buffer_size);

    /**
     * @brief Check if RX data available
     *
     * Uses UART2_ReadCountGet() from plib_uart2.c.
     *
     * @return true if characters available to read
     */
    bool UGS_RxHasData(void);

    /**
     * @brief Check if TX buffer has space
     *
     * Uses UART2_WriteFreeBufferCountGet() from plib_uart2.c.
     *
     * @return true if TX buffer has space for more data
     */
    bool UGS_TxHasSpace(void);

    /**
     * @brief Get number of bytes waiting in RX buffer
     *
     * @return Number of unread bytes
     */
    size_t UGS_RxAvailable(void);

    /**
     * @brief Get free space in TX buffer
     *
     * @return Number of bytes available for writing
     */
    size_t UGS_TxFreeSpace(void);

//=============================================================================
// DEBUGGING & DIAGNOSTICS
//=============================================================================

/**
 * @brief Send debug message (only if DEBUG defined)
 *
 * Useful for development debugging - disabled in production builds.
 * Can show motion_buffer state, multiaxis_control status, etc.
 *
 * @param format Printf-style format string
 * @param ... Variable arguments
 *
 * Example:
 *   UGS_Debug("MotionBuffer: %d blocks, MultiAxis busy: %d\r\n",
 *             MotionBuffer_GetCount(), MultiAxis_IsBusy());
 */
#ifdef DEBUG
    void UGS_Debug(const char *format, ...);
#else
#define UGS_Debug(...) ((void)0) /* No-op in release builds */
#endif

#ifdef __cplusplus
}
#endif

#endif /* UGS_INTERFACE_H */
