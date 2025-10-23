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

/* DEBUG_MOTION_BUFFER is controlled by Makefile:
 *   make all         → Production build (no debug)
 *   make all DEBUG=1 → Debug build with -DDEBUG_MOTION_BUFFER
 */

// *****************************************************************************
// Section: Included Files
// *****************************************************************************

#include <stddef.h>                   // Defines NULL
#include <stdbool.h>                  // Defines true
#include <stdlib.h>                   // Defines EXIT_FAILURE
#include <stdio.h>                    // For sscanf
#include <string.h>                   // For strcat, strncat
#include "definitions.h"              // SYS function prototypes
#include "serial_wrapper.h"           // GRBL serial using MCC plib_uart2
#include "ugs_interface.h"            // UGS protocol layer (uses serial_wrapper)
#include "gcode_parser.h"             // G-code parsing
#include "arc_converter.h"            // Arc-to-segment conversion (G2/G3)
#include "command_buffer.h"           // Command separation and buffering (NEW!)
#include "motion/motion_buffer.h"     // Motion planning ring buffer (OLD - being replaced)
#include "motion/grbl_planner.h"      // GRBL planner (NEW - Phase 1)
#include "motion/motion_math.h"       // GRBL settings management
#include "motion/multiaxis_control.h" // Multi-axis coordinated motion

// *****************************************************************************
// External Test Functions (test_ocr_direct.c) - NOW IN LIBS FOLDER
// *****************************************************************************
// Note: test_ocr_direct.c moved to libs/ folder
// To use: build shared library with 'make shared_lib' and link with USE_SHARED_LIB=1
// Or copy back to srcs/ for direct compilation
// extern void TestOCR_CheckCommands(void);
// extern void TestOCR_ExecuteTest(void);
// extern void TestOCR_ResetCounters(void);

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
  //static uint32_t serial_rx_call_count = 0;  /* DEBUG: Count function calls */

  /* DEBUG: Print every 1000 calls to show this function executes */
  //if (++serial_rx_call_count % 1000 == 0)
  //{
  //  UGS_Printf("[DEBUG_RX] ProcessSerialRx called %lu times\r\n", serial_rx_call_count);
  //}

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
#ifdef DEBUG_MOTION_BUFFER
        UGS_Printf("[CMD_ADD] Added %d commands, buffer now has %d\r\n",
                   commands_added, CommandBuffer_GetCount());
#endif
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
  /* DEBUG: Prove this function is called */
  LED2_Toggle();
  
  /* Modal position tracking - preserve unspecified axes (CRITICAL for GRBL planner!)
   * In G90 absolute mode, when "G1 X10" is sent, Y/Z should maintain last position.
   * Parser only fills in specified axes, so we must merge with modal position. */
  static float modal_position[NUM_AXES] = {0.0f, 0.0f, 0.0f, 0.0f};

  /* Only process commands if motion buffer has space
   * CRITICAL FIX (October 23, 2025): Use GRBL planner buffer, NOT MotionBuffer!
   * Process aggressively - only block when buffer is truly full (15/16 blocks) */
  uint8_t motion_count = GRBLPlanner_GetBufferCount();

#ifdef DEBUG_MOTION_BUFFER
  static uint32_t debug_counter = 0;
  if (++debug_counter > 10000)
  {
    if (motion_count >= 15)
    {
      UGS_Printf("[PROCBUF] GRBL planner near full (%d blocks), waiting...\r\n", motion_count);
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
      UGS_Printf("[PROCBUF] Got command from buffer (planner=%d, cmd_buf=%d)\r\n", 
                 motion_count, CommandBuffer_GetCount());
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

        if (has_motion)
        {
          /* ═══════════════════════════════════════════════════════════════
           * GRBL PLANNER INTEGRATION (Phase 1)
           * ═══════════════════════════════════════════════════════════════
           * Convert parsed_move_t (G-code parser) to GRBL planner format:
           *   - target[NUM_AXES]: float array in mm (absolute machine coords)
           *   - grbl_plan_line_data_t: feedrate, spindle, condition flags
           *
           * CRITICAL: Merge with modal position to preserve unspecified axes!
           * Example: "G1 X10" should move X to 10 while Y/Z stay at current position.
           */

          /* Merge parsed move with modal position (preserve unspecified axes) */
          float target_work[NUM_AXES];    /* Work coordinates from G-code */
          float target_machine[NUM_AXES]; /* Machine coordinates for planner */

#ifdef DEBUG_MOTION_BUFFER
          UGS_Printf("[MODAL] Pre-merge modal: X=%.3f Y=%.3f | Parsed: X=%.3f(%d) Y=%.3f(%d)\r\n",
                     modal_position[AXIS_X], modal_position[AXIS_Y],
                     move.target[AXIS_X], move.axis_words[AXIS_X],
                     move.target[AXIS_Y], move.axis_words[AXIS_Y]);


          UGS_Printf("[MODAL] Before merge: modal=(%.3f,%.3f,%.3f) parsed=(%.3f,%.3f,%.3f) words=(%d,%d,%d)\r\n",
                     modal_position[AXIS_X], modal_position[AXIS_Y], modal_position[AXIS_Z],
                     move.target[AXIS_X], move.target[AXIS_Y], move.target[AXIS_Z],
                     move.axis_words[AXIS_X], move.axis_words[AXIS_Y], move.axis_words[AXIS_Z]);
#endif

          /* CRITICAL (Oct 22, 2025): Save starting position BEFORE merge for arc conversion
           * Arc commands (G2/G3) need the ORIGINAL starting position, not the endpoint.
           * modal_position will be updated during merge, so capture it now. */
          float start_position[NUM_AXES];
          for (uint8_t axis = 0; axis < NUM_AXES; axis++)
          {
            start_position[axis] = modal_position[axis];
          }
          
#ifdef DEBUG_MOTION_BUFFER
          UGS_Printf("[MODAL] start_position (work coords before merge): X=%.3f Y=%.3f Z=%.3f\r\n",
                     start_position[AXIS_X], start_position[AXIS_Y], start_position[AXIS_Z]);
#endif

          for (uint8_t axis = 0; axis < NUM_AXES; axis++)
          {
            if (move.axis_words[axis])
            {
              /* CRITICAL FIX (Oct 21, 2025): Handle G91 (relative mode) */
              if (move.absolute_mode)
              {
                /* G90 (absolute): Use parsed value as-is */
                target_work[axis] = move.target[axis];
                modal_position[axis] = move.target[axis]; /* Update modal state */
              }
              else
              {
                /* G91 (relative): Add offset to current modal position */
                target_work[axis] = modal_position[axis] + move.target[axis];
                modal_position[axis] = target_work[axis]; /* Update modal state */
              }
            }
            else
            {
              /* Axis not specified - use modal position */
              target_work[axis] = modal_position[axis];
            }

            /* CRITICAL: Convert work coordinates to machine coordinates!
             * The GRBL planner expects absolute machine coordinates (MPos).
             * Parser provides work coordinates (WPos = what user specified).
             * Formula: MPos = WPos + work_offset + g92_offset */
            target_machine[axis] = MotionMath_WorkToMachine(target_work[axis], (axis_id_t)axis);
          }

 #ifdef DEBUG_MOTION_BUFFER         
          UGS_Printf("[MODAL] Post-merge: target_work=(%.3f,%.3f) target_machine=(%.3f,%.3f) mode=%s\r\n",
                     target_work[AXIS_X], target_work[AXIS_Y],
                     target_machine[AXIS_X], target_machine[AXIS_Y],
                     move.absolute_mode ? "G90" : "G91");


          UGS_Printf("[MODAL] After merge: target_work=(%.3f,%.3f,%.3f) target_machine=(%.3f,%.3f,%.3f)\r\n",
                     target_work[AXIS_X], target_work[AXIS_Y], target_work[AXIS_Z],
                     target_machine[AXIS_X], target_machine[AXIS_Y], target_machine[AXIS_Z]);
#endif

          /* Prepare GRBL planner input data */
          grbl_plan_line_data_t pl_data;
          pl_data.feed_rate = move.feedrate; /* mm/min from F word */
          pl_data.spindle_speed = 0.0f;      /* Future: M3/M4 spindle control */

          /* Set condition flags based on motion mode */
          pl_data.condition = 0;
          if (move.motion_mode == 0) /* G0 rapid positioning */
          {
            /* G0 rapid positioning - ignore feedrate, use machine max */
            pl_data.condition |= PL_COND_FLAG_RAPID_MOTION;
          }
          /* Future: PL_COND_FLAG_SYSTEM_MOTION for G28/G30 */

          /* ═══════════════════════════════════════════════════════════════
           * ARC CONVERSION: G2/G3 → Multiple G1 Segments (October 22, 2025)
           * ═══════════════════════════════════════════════════════════════
           * Convert circular arcs into linear segments and buffer to GRBL planner
           * 
           * CRITICAL (Oct 23, 2025): Arc converter needs MACHINE coordinates!
           * - start_position: Work coords before modal merge → convert to machine
           * - target_machine: Machine coords after work-to-machine conversion
           * - center_offset: I,J,K offsets from parsed move (relative, in mm)
           * ═══════════════════════════════════════════════════════════════ */
          if (move.motion_mode == 2 || move.motion_mode == 3)
          {
            #ifdef DEBUG_MOTION_BUFFER
            UGS_Printf("[MAIN] Arc detected: mode=G%u, I=%.3f J=%.3f, target=(%.3f,%.3f)\r\n",
                       move.motion_mode, 
                       move.arc_center_offset[AXIS_X], move.arc_center_offset[AXIS_Y],
                       move.target[AXIS_X], move.target[AXIS_Y]);
            #endif
            
            /* Convert start_position from work coordinates to machine coordinates
             * CRITICAL FIX (Oct 23, 2025): start_position is in work coords, but arc
             * converter needs machine coords! */
            float start_machine[NUM_AXES];
            for (uint8_t axis = 0; axis < NUM_AXES; axis++)
            {
                start_machine[axis] = MotionMath_WorkToMachine(start_position[axis], (axis_id_t)axis);
            }
            
#ifdef DEBUG_MOTION_BUFFER
            UGS_Printf("[ARC] start_machine=(%.3f,%.3f,%.3f) target_machine=(%.3f,%.3f,%.3f)\r\n",
                       start_machine[AXIS_X], start_machine[AXIS_Y], start_machine[AXIS_Z],
                       target_machine[AXIS_X], target_machine[AXIS_Y], target_machine[AXIS_Z]);
#endif
            
            /* Convert arc to segments using arc converter module
             * CRITICAL: Pass machine coordinates, not work coordinates!
             * - start_machine: Start position in machine coords
             * - target_machine: Target position in machine coords
             * - move.arc_center_offset: I,J,K from parser (relative offsets)
             */
            bool arc_success = ArcConverter_ConvertToSegments(
                move.motion_mode,           /* G2 or G3 */
                start_machine,              /* Start MACHINE position */
                target_machine,             /* Target MACHINE position */
                move.arc_center_offset,     /* I,J,K offsets */
                &pl_data                    /* Feedrate, spindle, etc. */
            );
            
            // Update modal position to arc endpoint (even if arc failed, position should update)
            if (arc_success)
            {
              for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
              {
                if (move.axis_words[axis])
                {
                  modal_position[axis] = target_work[axis];
                }
              }
            }
          }
          else
          {
            /* Add to GRBL planner (replaces MotionBuffer_Add) */
            if (!GRBLPlanner_BufferLine(target_machine, &pl_data))
          {
            /* Planner rejected move (zero-length or buffer full)
             * Log warning but continue (command already parsed) */
#ifdef DEBUG_MOTION_BUFFER
            UGS_Print("[MSG:GRBL planner rejected move - zero length or buffer full]\r\n");
#endif

#ifdef DEBUG_MOTION_BUFFER
            UGS_Printf("[GRBL] Rejected: Work(%.3f,%.3f,%.3f) -> Machine(%.3f,%.3f,%.3f) F:%.1f\r\n",
                       target_work[AXIS_X], target_work[AXIS_Y], target_work[AXIS_Z],
                       target_machine[AXIS_X], target_machine[AXIS_Y], target_machine[AXIS_Z],
                       pl_data.feed_rate);
#endif
          }
#ifdef DEBUG_MOTION_BUFFER
          else
          {
            /* Get planner's internal position after buffering */
            float planner_pos[NUM_AXES];
            GRBLPlanner_GetPosition(planner_pos);
            uint8_t buffer_count = GRBLPlanner_GetBufferCount();

            UGS_Printf("[GRBL] Buffered: Work(%.3f,%.3f,%.3f) -> Machine(%.3f,%.3f,%.3f) F:%.1f rapid:%d\r\n",
                       target_work[AXIS_X], target_work[AXIS_Y], target_work[AXIS_Z],
                       target_machine[AXIS_X], target_machine[AXIS_Y], target_machine[AXIS_Z],
                       pl_data.feed_rate,
                       (pl_data.condition & PL_COND_FLAG_RAPID_MOTION) ? 1 : 0);
            UGS_Printf("       Planner pos: X:%.3f Y:%.3f Z:%.3f (buffer:%d/16)\r\n",
                       planner_pos[AXIS_X], planner_pos[AXIS_Y], planner_pos[AXIS_Z],
                       buffer_count);
          }
#endif
          } /* End of else block for non-arc motion */
        }
        /* Modal-only commands (G90, M3, etc.) don't need planner */
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

  /* ═══════════════════════════════════════════════════════════════
   * GRBL PLANNER INITIALIZATION (Phase 1)
   * ═══════════════════════════════════════════════════════════════
   * Initialize GRBL v1.1f motion planner:
   *   - 16-block ring buffer for look-ahead planning
   *   - Junction deviation algorithm for smooth cornering
   *   - Forward/reverse pass optimization
   *   - Position tracking in steps (pl.sys_position[])
   */
  GRBLPlanner_Initialize();

  /* Initialize old motion buffer (Phase 2: will be removed) */
  MotionBuffer_Initialize();

  /* Initialize multi-axis control subsystem
   * This also calls MotionMath_InitializeSettings() for GRBL defaults
   * - Registers OCR callbacks
   * - Registers TMR1 motion control callback @ 1kHz
   * - Starts TMR1 control loop
   */
  MultiAxis_Initialize();

  /* Send startup message to UGS */
  UGS_Print("\r\n");
  UGS_Print("Grbl 1.1f ['$' for help]\r\n");
  UGS_SendOK(); /* Main application loop - Three-stage pipeline */
  while (true)
  {
    /* OCR Direct Hardware Test - Check for 'T' command */
    // Note: test_ocr_direct.c moved to libs/ folder - functions not available
    // TestOCR_CheckCommands();
    // TestOCR_ExecuteTest();
    // TestOCR_ResetCounters();

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
     * ADAPTIVE PROCESSING (October 19, 2025):
     * Drain command buffer until one of three conditions:
     * 1. Motion buffer nearly full (15/16 blocks) - backpressure
     * 2. Command buffer empty - nothing left to process
     * 3. Safety limit reached (16 iterations) - prevent infinite loop
     *
     * CRITICAL FIX (October 23, 2025): Use GRBLPlanner buffer, NOT MotionBuffer!
     * The old MotionBuffer is unused - all moves go through GRBL planner now.
     *
     * This is NON-BLOCKING: Max execution time ~100µs (16 × ~6µs per command)
     * Main loop returns every ~1ms to check real-time commands (E-stop, feed hold)
     *
     * Critical for safety: Never use blocking while loops that prevent
     * real-time command processing!
     */
    uint8_t cmd_count = 0;
    uint8_t planner_count = GRBLPlanner_GetBufferCount();
    bool has_cmds = CommandBuffer_HasData();
    uint8_t cmd_buf_count = CommandBuffer_GetCount();

#ifdef DEBUG_MOTION_BUFFER
    static uint32_t loop_debug = 0;
    /* Print debug MORE frequently during testing - every ~10000 iterations
     * to catch timing issues with command processing */
    if (++loop_debug > 10000 && (planner_count > 0 || cmd_buf_count > 0 || has_cmds))
    {
      UGS_Printf("[LOOP] planner=%d, cmd_buf=%d, hasData=%d, will_run=%d\r\n",
                 planner_count, cmd_buf_count, has_cmds ? 1 : 0, 
                 (planner_count < 15 && has_cmds) ? 1 : 0);
      loop_debug = 0;
    }
#endif

    while (cmd_count < 16 && planner_count < 15 && has_cmds)
    {
      ProcessCommandBuffer();
      cmd_count++;
      planner_count = GRBLPlanner_GetBufferCount();  /* Update after each iteration */
      has_cmds = CommandBuffer_HasData();
    }
    
#ifdef DEBUG_MOTION_BUFFER
    /* CRITICAL DEBUG (Oct 23): Why isn't 5th command being processed?
     * This will print when the while loop exits to show WHY it stopped */
    if (has_cmds && cmd_count == 0)
    {
      UGS_Printf("[LOOP_EXIT] Commands waiting but loop didn't run! planner=%d, has_cmds=%d\r\n",
                 planner_count, has_cmds ? 1 : 0);
    }
#endif

    /* Stage 3: Motion Execution → Hardware
     * CRITICAL FIX (October 20, 2025):
     * Segment execution start MUST be in main loop, NOT ISR!
     *
     * TMR9 ISR @ 10ms (motion_manager.c):
     *   - Prepares segments in background (tactical planning)
     *   - Adds segments to buffer for execution
     *
     * Main Loop (here):
     *   - Checks if machine idle AND segments available
     *   - Starts OCR hardware execution
     *   - Non-blocking: Runs every main loop iteration (~1ms)
     *
     * Why main loop?
     *   - Hardware configuration (OCR/TMR setup) not safe in ISR
     *   - Avoids race conditions between ISRs
     *   - Clean separation: ISR prepares, main loop executes
     */
    if (!MultiAxis_IsBusy() && GRBLStepper_GetBufferCount() > 0)
    {
      UGS_Printf("[EXEC] Starting segment (seg_count=%d)\r\n", GRBLStepper_GetBufferCount());
      MultiAxis_StartSegmentExecution();
    }

    /* LED1 Heartbeat - Simple CPU alive indicator
     * Toggles every ~32768 loops (approximately 1Hz at typical loop rate)
     * Shows main loop is running (not stuck in ISR or crashed)
     */
    static uint16_t heartbeat_counter = 0;
    if (++heartbeat_counter == 0)
    { // Rolls over every 65536 iterations
      LED1_Toggle();
    }

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
