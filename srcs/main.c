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

// *****************************************************************************
// Section: Included Files
// *****************************************************************************

#include <stddef.h>                   // Defines NULL
#include <stdbool.h>                  // Defines true
#include <stdlib.h>                   // Defines EXIT_FAILURE
#include "definitions.h"              // SYS function prototypes
#include "app.h"                      // Application layer
#include "ugs_interface.h"            // Universal G-code Sender communication
#include "gcode_parser.h"             // G-code parsing
#include "motion/motion_buffer.h"     // Motion planning ring buffer
#include "motion/multiaxis_control.h" // Multi-axis coordinated motion

// *****************************************************************************
// G-code Processing (Polling-Based)
// *****************************************************************************

/**
 * @brief Process incoming G-code commands with polling pattern
 *
 * User's polling strategy:
 *   1. Check UGS_RxHasData() for incoming data
 *   2. Buffer line into char array
 *   3. Check index[0] for control chars (?, !, ~, Ctrl-X)
 *   4. Respond immediately to control chars
 *   5. Parse regular commands into parsed_move_t
 *   6. Add to motion buffer (only send "ok" if buffer accepts)
 */
static void ProcessGCode(void)
{
  static char line_buffer[256];

  /* Poll for incoming G-code line */
  if (GCode_BufferLine(line_buffer, sizeof(line_buffer)))
  {

    /* Check if control character was handled */
    if (GCode_IsControlChar(line_buffer[0]))
    {
      /* Control char already handled in GCode_BufferLine() */
      return;
    }

    /* Parse regular G-code command */
    parsed_move_t move;
    if (GCode_ParseLine(line_buffer, &move))
    {

      /* Check if this is a motion command (has axis words) */
      bool has_motion = move.axis_words[AXIS_X] ||
                        move.axis_words[AXIS_Y] ||
                        move.axis_words[AXIS_Z] ||
                        move.axis_words[AXIS_A];

      if (has_motion)
      {
        /* Try to add to motion buffer */
        if (MotionBuffer_Add(&move))
        {
          /* Buffer accepted move - send "ok" for flow control */
          UGS_SendOK();
        }
        else
        {
          /* Buffer full - DON'T send "ok", UGS will wait and retry */
          /* This is normal during streaming - buffer will drain */
        }
      }
      else
      {
        /* Modal-only command (G90, G91, M commands) - no motion */
        UGS_SendOK();
      }
    }
    else
    {
      /* Parse error - send error message */
      const char *error = GCode_GetLastError();
      if (error != NULL)
      {
        UGS_SendError(1, error);
      }
      else
      {
        UGS_SendError(1, "Parse error");
      }
      GCode_ClearError();
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
        /* Execute coordinated move with pre-calculated S-curve profile */
        MultiAxis_ExecuteCoordinatedMove(block.steps);
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

  /* Initialize UGS serial interface */
  UGS_Initialize();

  /* Initialize G-code parser with modal defaults */
  GCode_Initialize();

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
  UGS_SendOK();

  /* Main application loop */
  while (true)
  {
    /* Process incoming G-code commands (polling pattern) */
    ProcessGCode();

    /* Execute planned moves from motion buffer */
    ExecuteMotion();

    /* Run application state machine
     * - Button debouncing (SW1/SW2)
     * - LED heartbeat management
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
