/*******************************************************************************
  Main Source File - CNC Controller Entry Point

  Description:
    Main entry point for PIC32MZ CNC Controller V2
    - Initializes Harmony3 peripherals (SYS_Initialize)
    - Initializes application layer (APP_Initialize)
    - Initializes G-code parser and UGS interface
    - Runs main loop with polling-based G-code processing

  Architecture:
    main.c (this file)
      ↓
    UGS Interface (serial communication)
      ↓
    G-code Parser (polling-based command processing)
      ↓
    Motion Buffer (ring buffer with look-ahead planning)
      ↓
    Multi-Axis Control (time-synchronized S-curve motion)
      ↓
    Harmony3 peripherals (OCR, TMR, GPIO, UART)
*******************************************************************************/

// TEMPORARY: Enable debug output for motion execution investigation
#define DEBUG_MOTION_BUFFER

// *****************************************************************************
// Section: Included Files
// *****************************************************************************

#include <stddef.h>                   // Defines NULL
#include <stdbool.h>                  // Defines true
#include <stdlib.h>                   // Defines EXIT_FAILURE
#include <stdio.h>                    // For sscanf
#include <string.h>                   // For strcat, strncat
#include "definitions.h"              // SYS function prototypes
#include "app.h"                      // Application layer
#include "serial_wrapper.h"           // GRBL serial using MCC plib_uart2
#include "ugs_interface.h"            // UGS protocol layer (uses serial_wrapper)
#include "gcode_parser.h"             // G-code parsing
#include "command_buffer.h"           // Command separation and buffering (NEW!)
#include "motion/motion_buffer.h"     // Motion planning ring buffer
#include "motion/motion_math.h"       // GRBL settings management
#include "motion/multiaxis_control.h" // Multi-axis coordinated motion

// *****************************************************************************
// External Test Functions (test_ocr_direct.c)
// *****************************************************************************
extern void TestOCR_CheckCommands(void);
extern void TestOCR_ExecuteTest(void);
extern void TestOCR_ResetCounters(void);

// *****************************************************************************
// Debug: UART RX Callback (DISABLED - causes race conditions with polling)
// *****************************************************************************
#ifdef DEBUG_MOTION_BUFFER_DISABLED_FOR_NOW
static volatile uint32_t uart_rx_int_count = 0;
static void UART_RxDebugCallback(UART_EVENT event, uintptr_t context)
{
  (void)event;   // Unused
  (void)context; // Unused
  uart_rx_int_count++;
  /* This proves RX interrupts are firing */
}
#endif // *****************************************************************************
// G-code Processing (Three-Stage Pipeline with Command Separation)
// *****************************************************************************

/**
 * @brief Process incoming serial data and split into commands (DMA-based)
 *
 * Three-stage pipeline architecture:
 *   Stage 1: DMA RX → Ring Buffer → Command Separation (this function)
 *   Stage 2: Command Buffer → Parsing → Motion Buffer (ProcessCommandBuffer)
 *   Stage 3: Motion Buffer → Multi-Axis Execution (ExecuteMotion)
 *
 * DMA Architecture (matches mikroC V1):
 *   - DMA Channel 0 receives UART2 data with pattern match on '?'
 *   - Pattern match triggers on '?' OR buffer full (500 bytes)
 *   - DMA ISR copies complete line to ring buffer
 *   - This function retrieves lines from ring buffer (zero-copy!)
 *
 * Flow:
 *   1. Check Get_Difference() for available data (mikroC V1 API)
 *   2. Get_Line() copies from ring buffer to line_buffer
 *   3. Check index[0] for control chars (?, !, ~, Ctrl-X) → Immediate response
 *   4. Check for $ system commands → Immediate response
 *   5. Tokenize line: "G92G0X10Y10" → ["G92", "G0", "X10", "Y10"]
 *   6. Split into commands: ["G92"], ["G0", "X10", "Y10"]
 *   7. Add to command buffer (64-entry ring buffer)
 *   8. Send "ok" immediately (non-blocking!)
 *
 * Benefits:
 *   - Hardware-guaranteed complete lines (DMA pattern match on '?')
 *   - Handles UGS initialization correctly (first message is '?' without '\n')
 *   - No byte loss, no timing dependencies, no race conditions
 *   - Proven architecture from mikroC V1 (worked for years)
 *   - Fast "ok" response (~175µs: tokenize + split, no parsing wait)
 *   - 64-command look-ahead window (plus 16 motion blocks = 80 total!)
 */
static void ProcessSerialRx(void)
{
  static char line_buffer[256] = {0}; /* Initialize to zeros on first use */
  static size_t line_pos = 0;         /* Track position across calls */
  static bool line_complete = false;  /* Flag for complete line */

  /* Check if serial data available in RX ring buffer
   * GRBL serial: Interrupt-driven RX (Priority 5/0), 256-byte buffer
   */
  if (Serial_Available() > 0 || line_complete)
  {
    /* Read characters until newline or buffer full */
    int16_t c;

    /* Only read if we don't already have a complete line */
    if (!line_complete)
    {
      while ((c = Serial_Read()) != -1)
      {
        if (c == '\n' || c == '\r')
        {
          if (line_pos > 0)
          {
            line_buffer[line_pos] = '\0'; /* Null terminate */
            line_complete = true;         /* Mark line as complete */
            break;                        /* Process this line */
          }
          continue; /* Skip empty lines (multiple \r\n) */
        }

        if (line_pos < (sizeof(line_buffer) - 1))
        {
          line_buffer[line_pos++] = (char)c;
        }
        else
        {
          /* Buffer overflow - discard line */
          line_pos = 0;
          memset(line_buffer, 0, sizeof(line_buffer));
          UGS_SendError(1, "Line too long");
          return;
        }
      }
    }

    /* If no complete line yet, continue */
    if (!line_complete || line_buffer[0] == '\0')
    {
      return;
    }

    /* Trim leading whitespace (newlines, spaces, tabs) */
    char *line_start = line_buffer;
    while (*line_start == ' ' || *line_start == '\t' ||
           *line_start == '\r' || *line_start == '\n')
    {
      line_start++;
    }

    /* Skip empty lines */
    if (*line_start == '\0')
    {
      memset(line_buffer, 0, sizeof(line_buffer));
      return;
    }

    /* Check for control characters at START of trimmed line
     * Control characters are already handled in GCode_BufferLine(),
     * but we need to skip tokenization for them here. */
    if (GCode_IsControlChar(line_start[0]))
    {
      /* Control char already handled in GCode_BufferLine() */
      memset(line_buffer, 0, sizeof(line_buffer)); /* Clear buffer */
      line_pos = 0;
      line_complete = false;
      return; /* Return early */
    }

    /* Check for $ system commands (GRBL protocol) */
    if (line_start[0] == '$')
    {
      /* Handle $I - Build info (CRITICAL for UGS version detection!) */
      if (line_start[1] == 'I')
      {
        /* $I - Build info (CRITICAL for UGS version detection!) */
        UGS_SendBuildInfo();
        UGS_SendOK();
      }
      /* Handle $xxx=value (setting assignment) */
      else if (line_start[1] >= '0' && line_start[1] <= '9')
      {
        uint32_t setting_id;
        float value;

        /* Parse "$100=250.0" format */
        if (sscanf(&line_start[1], "%u=%f", &setting_id, &value) == 2)
        {
          if (setting_id <= 255 && MotionMath_SetSetting((uint8_t)setting_id, value))
          {
            /* Setting updated successfully */
            UGS_SendOK();
          }
          else
          {
            /* Invalid setting ID or value */
            UGS_SendError(3, "Invalid setting or value");
          }
        }
        else
        {
          /* Parse error - invalid format */
          UGS_SendError(3, "Invalid $ command format");
        }
      }
      else if (line_start[1] == 'G')
      {
        /* $G - Parser state */
        UGS_SendParserState();
        UGS_SendOK();
      }
      else if (line_start[1] == '$')
      {
        /* $$ - View all settings */
        const uint8_t setting_ids[] = {
            11, 12,             // Junction deviation, arc tolerance
            100, 101, 102, 103, // Steps per mm (X, Y, Z, A)
            110, 111, 112, 113, // Max rate (X, Y, Z, A)
            120, 121, 122, 123, // Acceleration (X, Y, Z, A)
            130, 131, 132, 133  // Max travel (X, Y, Z, A)
        };

        for (uint8_t i = 0; i < sizeof(setting_ids); i++)
        {
          uint8_t id = setting_ids[i];
          float value = MotionMath_GetSetting(id);
          UGS_SendSetting(id, value);
        }
        UGS_SendOK();
      }
      else if (line_start[1] == '#')
      {
        /* $# - View coordinate offsets (GRBL v1.1f) */
        MotionMath_PrintCoordinateParameters();
        UGS_SendOK();
      }
      else if (line_start[1] == 'N')
      {
        /* $N - Startup lines */
        if (line_start[2] == '0')
        {
          UGS_SendStartupLine(0);
        }
        else if (line_start[2] == '1')
        {
          UGS_SendStartupLine(1);
        }
        else
        {
          /* $N with no number - show both */
          UGS_SendStartupLine(0);
          UGS_SendStartupLine(1);
        }
        UGS_SendOK();
      }
      else if (line_start[1] == '\0' || line_start[1] == '\r' || line_start[1] == '\n')
      {
        /* $ - Help */
        UGS_SendHelp();
        UGS_SendOK();
      }
      else
      {
        /* Unknown $ command */
        UGS_SendError(3, "$ command not recognized");
      }

      /* Clear buffer and reset state after $ command */
      memset(line_buffer, 0, sizeof(line_buffer));
      line_pos = 0;
      line_complete = false;
      return;
    }

    /* Tokenize regular G-code line */
    gcode_line_t tokenized;
    if (GCode_TokenizeLine(line_start, &tokenized))
    {
#ifdef DEBUG_MOTION_BUFFER
      UGS_Printf("[TOKEN] Line: '%s' -> %u tokens\r\n", line_start, tokenized.token_count);
#endif

      /* Split into individual commands
       *
       * Example: "G92G0X10Y10F200G1X20" splits into:
       *   Command 1: G92
       *   Command 2: G0 X10 Y10 F200
       *   Command 3: G1 X20
       *
       * This enables proper command separation for concatenated G-code.
       */
      uint8_t commands_added = CommandBuffer_SplitLine(&tokenized);

      if (commands_added > 0)
      {
        /* Commands successfully added to buffer
         *
         * GRBL Character-Counting Protocol (Phase 2 - Deep Look-Ahead)
         *
         * Send "ok" immediately after command separation (NOT after execution!)
         *
         * Pipeline:
         *   - 64-command buffer (filled here)
         *   - 16-motion buffer (filled by ProcessCommandBuffer)
         *   - Total: 80 commands in pipeline!
         *
         * Benefits:
         *   - Fast "ok" (~175µs: tokenize + split, no parse/execute wait)
         *   - Deep look-ahead (80 commands for advanced optimization)
         *   - Background parsing (commands parse while moving)
         *   - Proper command separation (handles concatenated G-code)
         *
         * Flow Control:
         *   - Commands added → send "ok" immediately
         *   - Buffer full → DON'T send "ok" (UGS retries)
         */
        UGS_SendOK();
      }
      else
      {
        /* Command buffer full - DON'T send "ok"
         * UGS will wait and retry automatically
         * Normal flow control for streaming protocol */
#ifdef DEBUG_MOTION_BUFFER
        UGS_Printf("[WARN] Command buffer full, no commands added\r\n");
#endif
      }
    }
    else
    {
      /* Tokenization error */
#ifdef DEBUG_MOTION_BUFFER
      UGS_Printf("[ERROR] Tokenization failed for: '%s'\r\n", line_buffer);
#endif
      UGS_SendError(1, "Tokenization error");
    }

    /* Clear buffer and reset state after processing */
    memset(line_buffer, 0, sizeof(line_buffer));
    line_pos = 0;
    line_complete = false;
  }
  /* If no complete line yet, return and wait for more data */
}

/**
 * @brief Process commands from command buffer (background parsing)
 *
 * Dequeues commands from command buffer and parses them into motion blocks.
 * This happens in background while machine executes previous moves.
 *
 * Flow:
 *   1. Check if motion buffer has space (keep some buffer margin)
 *   2. Get next command from command buffer
 *   3. Parse command tokens → parsed_move_t
 *   4. Add to motion buffer for execution
 *
 * Benefits:
 *   - Parsing happens in parallel with motion execution
 *   - Motion buffer stays full for continuous motion
 *   - Errors detected before motion execution
 */
static void ProcessCommandBuffer(void)
{
  /* Only process commands if motion buffer has space
   * Process aggressively - only block when buffer is truly full (15/16 blocks) */
  uint8_t motion_count = MotionBuffer_GetCount();

#ifdef DEBUG_MOTION_BUFFER
  static uint32_t debug_counter = 0;
  if (++debug_counter > 10000)
  {
    if (motion_count >= 15)
    {
      UGS_Printf("[PROCBUF] Motion buffer near full (%d blocks), waiting...\r\n", motion_count);
    }
    debug_counter = 0;
  }
#endif

  if (motion_count < 15) /* Changed from 12 to 15 - process more aggressively */
  {
    command_entry_t cmd;
    if (CommandBuffer_GetNext(&cmd))
    {
#ifdef DEBUG_MOTION_BUFFER
      UGS_Printf("[PROCBUF] Got command from buffer (motion_count=%d)\r\n", motion_count);
#endif

      /* Reconstruct line from tokens for parser
       * Example: ["G0", "X10", "Y20", "F1500"] → "G0 X10 Y20 F1500"
       */
      static char reconstructed_line[256];
      reconstructed_line[0] = '\0';

      for (uint8_t i = 0; i < cmd.token_count && i < MAX_TOKENS_PER_COMMAND; i++)
      {
        if (i > 0)
        {
          strcat(reconstructed_line, " "); // Add space between tokens
        }
        strncat(reconstructed_line, cmd.tokens[i],
                sizeof(reconstructed_line) - strlen(reconstructed_line) - 1);
      }

      /* Parse command line into parsed_move_t */
      parsed_move_t move;
      if (GCode_ParseLine(reconstructed_line, &move))
      {
        /* Check if motion command */
        bool has_motion = move.axis_words[AXIS_X] ||
                          move.axis_words[AXIS_Y] ||
                          move.axis_words[AXIS_Z] ||
                          move.axis_words[AXIS_A];

#ifdef DEBUG_MOTION_BUFFER
        UGS_Printf("[PARSE] '%s' -> motion=%d (X:%d Y:%d Z:%d)\r\n",
                   reconstructed_line, has_motion,
                   move.axis_words[AXIS_X], move.axis_words[AXIS_Y], move.axis_words[AXIS_Z]);
#endif

        if (has_motion)
        {
          /* Add to motion buffer */
          if (!MotionBuffer_Add(&move))
          {
            /* Motion buffer full (shouldn't happen with 4-block margin)
             * Log error but continue (command already processed) */
            UGS_Print("[MSG:Motion buffer full during background parse]\r\n");
          }
        }
        /* Modal-only commands (G90, M3, etc.) don't need motion buffer */
      }
      else
      {
        /* Parse error - log and continue */
        const char *error = GCode_GetLastError();
        if (error != NULL)
        {
          UGS_SendError(1, error);
        }
        GCode_ClearError();
      }
    }
  }
}

/**
 * @brief Execute planned moves from motion buffer
 *
 * Dequeues motion blocks and executes them when multi-axis controller is idle.
 */
static void ExecuteMotion(void)
{
  /* Only execute next move if controller is idle */
  if (!MultiAxis_IsBusy())
  {

    /* Check if motion buffer has planned moves */
    if (MotionBuffer_HasData())
    {
      motion_block_t block;

      if (MotionBuffer_GetNext(&block))
      {
#ifdef DEBUG_MOTION_BUFFER
        /* DEBUG: Print what we're executing */
        UGS_Printf("[EXEC] Steps: X=%ld Y=%ld Z=%ld (busy=%d)\r\n",
                   block.steps[AXIS_X], block.steps[AXIS_Y], block.steps[AXIS_Z],
                   MultiAxis_IsBusy());
#endif

        /* Check if this is a zero-step block (no actual motion)
         * These can occur when moving to current position (G0 X0 Y0 when already at origin)
         * Skip them to avoid clogging the execution pipeline */
        bool has_steps = (block.steps[AXIS_X] != 0) || (block.steps[AXIS_Y] != 0) ||
                         (block.steps[AXIS_Z] != 0) || (block.steps[AXIS_A] != 0);

        if (!has_steps)
        {
#ifdef DEBUG_MOTION_BUFFER
          UGS_Printf("[EXEC] Skipping zero-step block\r\n");
#endif
          /* Don't execute, just discard and continue */
        }
        else
        {
          /* Execute coordinated move with pre-calculated S-curve profile */
          MultiAxis_ExecuteCoordinatedMove(block.steps);

#ifdef DEBUG_MOTION_BUFFER
          UGS_Printf("[EXEC] Coordinated move started\r\n");
#endif
        }
      }
#ifdef DEBUG_MOTION_BUFFER
      else
      {
        UGS_Printf("[EXEC] MotionBuffer_GetNext() returned false!\r\n");
      }
#endif
    }
#ifdef DEBUG_MOTION_BUFFER
    else
    {
      // Only print occasionally to avoid flooding
      static uint32_t idle_count = 0;
      if (++idle_count > 10000)
      {
        UGS_Printf("[EXEC] No data in motion buffer (idle)\r\n");
        idle_count = 0;
      }
    }
#endif
  }
#ifdef DEBUG_MOTION_BUFFER
  else
  {
    // Motion system is busy - this is normal during moves
  }
#endif
}

// *****************************************************************************
// Optional Features (can be enabled for testing)
// *****************************************************************************

// Uncomment to enable simple UART echo for serial testing
// #define ENABLE_UART_ECHO

#ifdef ENABLE_UART_ECHO
static uint8_t uart_rx_buffer[1];

static void UART_EchoCallback(uintptr_t context)
{
  // Simple echo - read byte and write it back
  UART2_Read(uart_rx_buffer, 1);
  UART2_Write(uart_rx_buffer, 1);
}
#endif

// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************

int main(void)
{
  /* Initialize all Harmony3 modules (generated by MCC) */
  SYS_Initialize(NULL);

#ifdef ENABLE_UART_ECHO
  /* Optional: Setup UART echo for serial testing */
  UART2_ReadCallbackRegister(UART_EchoCallback, 0);
  UART2_Read(uart_rx_buffer, 1); // Start first read
#endif

  /* Initialize UGS serial interface
   * This calls Serial_Initialize() internally for:
   *   - Direct UART2 register access (no MCC, no DMA)
   *   - 115200 baud, 8N1
   *   - RX interrupt (Priority 5/0)
   *   - TX interrupt (Priority 5/1, on-demand)
   *   - 256-byte ring buffers
   *   - Real-time command detection ('?', '!', '~', Ctrl-X)
   */
  UGS_Initialize();

  /* Initialize G-code parser with modal defaults */
  GCode_Initialize();

  /* Initialize command buffer (64-entry ring buffer for command separation) */
  CommandBuffer_Initialize();

  /* Initialize motion buffer ring buffer */
  MotionBuffer_Initialize();

  /* Initialize multi-axis control subsystem
   * This also calls MotionMath_InitializeSettings() for GRBL defaults
   * - Registers OCR callbacks
   * - Registers TMR1 motion control callback @ 1kHz
   * - Starts TMR1 control loop
   */
  MultiAxis_Initialize();

  /* Initialize application layer (button handling, LEDs) */
  APP_Initialize();

  /* Send startup message to UGS */
  UGS_Print("\r\n");
  UGS_Print("Grbl 1.1f ['$' for help]\r\n");
  UGS_SendOK(); /* Main application loop - Three-stage pipeline */
  while (true)
  {
    /* OCR Direct Hardware Test - Check for 'T' command */
    TestOCR_CheckCommands();
    TestOCR_ExecuteTest();
    TestOCR_ResetCounters();

    /* Stage 1: Process incoming serial data → Command Buffer (64 entries)
     * - Receives lines from UART
     * - Tokenizes: "G92G0X10" → ["G92", "G0", "X10"]
     * - Splits: ["G92"], ["G0", "X10"]
     * - Sends "ok" immediately (~175µs response time)
     */
    ProcessSerialRx();

    /* Real-Time Command Processing (GRBL Pattern)
     * ISR sets flag when control character received, main loop handles it.
     * Cannot call blocking functions (UGS_Print) from ISR context!
     */
    uint8_t realtime_cmd = Serial_GetRealtimeCommand();
    if (realtime_cmd != 0)
    {
      GCode_HandleControlChar(realtime_cmd);
    }

    /* Stage 2: Process Command Buffer → Motion Buffer (16 blocks)
     * - Dequeues commands from 64-entry buffer
     * - Parses tokens → parsed_move_t
     * - Adds to motion buffer for execution
     * - Happens in background while machine moves
     *
     * CRITICAL: Process ALL available commands each loop iteration!
     * This prevents commands from getting stuck in the command buffer
     * when the motion buffer fills up. GRBL pattern: drain command buffer
     * as fast as possible to maintain flow control.
     *
     * Limit to 16 iterations to prevent infinite loop if something goes wrong.
     */
    for (uint8_t i = 0; i < 16; i++) /* Process up to 16 commands per iteration */
    {
      /* ProcessCommandBuffer() will return early if motion buffer is full (15/16)
       * or if command buffer is empty, so this loop will exit naturally */
      ProcessCommandBuffer();
    }

    /* Stage 3: Execute Motion Buffer → Hardware
     * - Dequeues motion blocks
     * - Executes S-curve profiles via multi-axis control
     * - Hardware OCR modules generate step pulses
     */
    ExecuteMotion();

    /* Run application state machine
     * - LED heartbeat management
     * - System status monitoring
     */
    APP_Tasks();

    /* Maintain state machines of all polled Harmony modules
     * - UART, timers, GPIO, etc.
     */
    SYS_Tasks();
  }

  /* Execution should not come here during normal operation */
  return (EXIT_FAILURE);
}

/*******************************************************************************
 End of File
*******************************************************************************/
