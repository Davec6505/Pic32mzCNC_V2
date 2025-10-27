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
 *   make all                                           → Production build (no debug)
 *   make all BUILD_CONFIG=Debug DEBUG_MOTION_BUFFER=3  → Debug build with -DDEBUG_MOTION_BUFFER=x
 */

// *****************************************************************************
// Section: Included Files
// *****************************************************************************

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "definitions.h"
#include "config/default/peripheral/coretimer/plib_coretimer.h"
#include "gcode/gcode_parser.h"
#include "gcode/serial_wrapper.h"
#include "gcode/ugs_interface.h"
#include "motion/multiaxis_control.h"
#include "motion/grbl_planner.h"
#include "motion/grbl_stepper.h"
#include "motion/motion_math.h"
#include "motion/motion_buffer.h"
#include "motion/motion_manager.h"  // ✅ CRITICAL: TMR9 segment prep!
#include "motion/homing.h"          // ✅ Homing cycle support



// *****************************************************************************
// Section: Local Functions prototypes
// *****************************************************************************
static bool GetLimitSwitchState(axis_id_t axis, bool positive_direction);




// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************

int main(void)
{
    /* Initialize the device */
    SYS_Initialize(NULL);
    UGS_Initialize();
    // Initialize GRBL planner/stepper pipeline (Phase 2B)
    GRBLPlanner_Initialize();
    MotionManager_Initialize();  // ✅ CRITICAL: Start TMR9 for segment prep!
    MultiAxis_Initialize();
    GCode_Initialize();

    UGS_SendBuildInfo();

    Homing_Initialize(GetLimitSwitchState);  // ✅ Pass limit switch callback
 
    /* 256U max length of G-code line */
    char line[GCODE_MAX_LINE_LENGTH];
    size_t line_pos = 0;

    /* Runtime safety guard: if both planner and stepper queues are empty but
     * the machine remains in <Run> for an extended period, force a clean stop.
     * This protects against rare edge cases where execution state doesn't
     * unwind after the final segment (walkabout after file end).
     *
     * Enabled only for debug builds (DEBUG_MOTION_BUFFER).
     */
#ifdef DEBUG_MOTION_BUFFER
    static bool guard_busy_no_queue_active = false;
    static uint32_t guard_busy_no_queue_start = 0U; // CORETIMER ticks

    static bool guard_plan_empty_stepper_busy_active = false;
    static uint32_t guard_plan_empty_stepper_busy_start = 0U; // CORETIMER ticks
#endif

    /* Retry state for when buffer is full */
    static bool pending_retry = false;

    while (true)
    {
        /* Handle any real-time commands (e.g., status report '?') */
        uint8_t rt_cmd = Serial_GetRealtimeCommand();
        if (rt_cmd != 0)
        {
            GCode_HandleControlChar(rt_cmd);
        }

        /* RETRY LOGIC: Handle pending retry BEFORE reading new serial data */
        int16_t c_int;
        bool skip_serial_processing = false;  /* NEW: Flag to skip serial but still run system tasks */
        
        if (pending_retry && line_pos > 0)
        {
            /* Buffer was full on previous iteration - check if space available now */
            if (!GRBLPlanner_IsBufferFull())
            {
                /* Space available! Force re-processing by simulating newline */
                c_int = '\n';
                pending_retry = false;
            }
            else
            {
                /* CRITICAL: Buffer STILL full - DO NOT read new serial! */
                /* Set flag to skip serial processing but CONTINUE to system tasks */
                c_int = -1;
                skip_serial_processing = true;  /* ✅ Still run APP_Tasks/SYS_Tasks at bottom of loop */
            }
        }
        else
        {
            /* Normal operation: Read from serial */
            c_int = Serial_Read();
        }

        if ((c_int != -1) && !skip_serial_processing)
        {
            char c = (char)c_int;

            /* Filter out non-ASCII/high-bit bytes */
            unsigned char uc = (unsigned char)c;
            if ((uc >= 0x80U) ||
                ((uc < 0x20U) && (c != '\n') && (c != '\r') && (c != '\t') && (c != ' ')))
            {
                continue;
            }

            if (c == '\n' || c == '\r')
            {
                /* Line terminator received, process the buffered line */
                line[line_pos] = '\0';

                // Trim leading whitespace
                char *line_start = line;
                while (*line_start && isspace((unsigned char)*line_start))
                {
                    line_start++;
                }

                // ═══════════════════════════════════════════════════════════════
                // CRITICAL FIX (October 26, 2025): Filter out debug echo feedback!
                // Serial terminals often echo transmitted data back to receiver
                // Debug output (starting with '[') was being re-parsed as G-code!
                // This caused blocks to execute TWICE (double step count).
                // ═══════════════════════════════════════════════════════════════
                if (*line_start == '[' || *line_start == '<' || 
                    strncmp(line_start, "ok", 2) == 0 ||
                    strncmp(line_start, "error:", 6) == 0 ||
                    strncmp(line_start, "ERROR:", 6) == 0 ||
                    strncmp(line_start, ">>", 2) == 0)
                {
                    // Debug/status output being echoed back - ignore it!
                    line_pos = 0;
                    continue;
                }

                // ═══════════════════════════════════════════════════════════════
                // CRITICAL FIX (October 25, 2025): Send ONE "ok" per command!
                // ═══════════════════════════════════════════════════════════════
                
                if (*line_start == '\0' || !LineHasGrblWordLetter(line_start))
                {
                    // Empty line or whitespace-only - send "ok" to keep streaming
                    UGS_SendOK();
                    line_pos = 0;  // Reset for next command
                }
                else
                {
                    // Line has content - parse and execute
                    #if DEBUG_MOTION_BUFFER >= DEBUG_LEVEL_PARSE
                    UGS_Printf("[PARSE] '%s'\r\n", line_start);
                    #endif
                    
                    parsed_move_t move;
                    if (GCode_ParseLine(line_start, &move))
                    {
                        // CRITICAL FIX (Oct 25, 2025): G2/G3 arcs don't set axis_words, check arc flags!
                        // LINEAR: G0/G1 (modes 0-1) with axis words present
                        // ARC: G2/G3 (modes 2-3) with IJK or R parameters present
                        bool is_linear_motion = (move.motion_mode <= 1) &&
                                                (move.axis_words[0] || move.axis_words[1] || 
                                                 move.axis_words[2] || move.axis_words[3]);
                        bool is_arc_motion = ((move.motion_mode == 2) || (move.motion_mode == 3)) &&
                                             (move.arc_has_ijk || move.arc_has_radius);
                        bool is_motion = is_linear_motion || is_arc_motion;

                        #if DEBUG_MOTION_BUFFER >= DEBUG_LEVEL_VERBOSE
                        UGS_Printf("[MOTION] mode=%u, X=%d Y=%d Z=%d A=%d, is_motion=%d\r\n",
                                   move.motion_mode,
                                   move.axis_words[0], move.axis_words[1], 
                                   move.axis_words[2], move.axis_words[3],
                                   is_motion);
                        #endif

                        if (is_motion)
                        {
                            /* ═══════════════════════════════════════════════════════════════
                             * CRITICAL FIX (Oct 25, 2025): Arc Support via MotionBuffer
                             * ═══════════════════════════════════════════════════════════════
                             * BEFORE: Called GRBLPlanner_BufferLine() directly
                             * PROBLEM: Bypassed arc conversion in MotionBuffer_Add()
                             * AFTER: Use MotionBuffer_Add() which handles BOTH:
                             *   - Linear moves (G0/G1): Passes to GRBL planner
                             *   - Arc moves (G2/G3): Converts to segments, then buffers
                             * ═══════════════════════════════════════════════════════════════ */
                            
                            if (MotionBuffer_Add(&move))
                            {
                                /* ✅ SUCCESS: Buffer accepted move */
                                
                                /* CRITICAL (Oct 25, 2025): For arc commands (G2/G3),
                                 * DO NOT send "ok" immediately! Arc generator runs in
                                 * TMR1 ISR @ 1ms. Main loop will check MotionBuffer_CheckArcComplete()
                                 * and send "ok" when arc finishes.
                                 * 
                                 * For linear moves (G0/G1), send "ok" immediately.
                                 */
                                if (move.motion_mode == 2 || move.motion_mode == 3)
                                {
                                    /* Arc command - "ok" sent by main loop when MotionBuffer_CheckArcComplete() returns true */
#ifdef DEBUG_MOTION_BUFFER
                                    UGS_Printf("[MAIN] Arc G%d queued, waiting for TMR1 completion\r\n", 
                                               move.motion_mode);
#endif
                                }
                                else
                                {
                                    /* Linear move - send "ok" immediately */
                                    UGS_SendOK();
                                }
                                
                                line_pos = 0;
                                pending_retry = false;
                            }
                            else
                            {
                                /* ⏳ BUFFER FULL: Retry on next loop iteration */
                                pending_retry = true;
                            }
                        }
                        else
                        {
                            // Non-motion command - send "ok" and reset
                            UGS_SendOK();
                            line_pos = 0;
                            pending_retry = false;  // Clear retry flag for non-motion too
                        }
                    }
                    else
                    {
                        // Parse error - send error response (NOT "ok")
                        UGS_SendError(1, GCode_GetLastError());
                        line_pos = 0;  // Reset for next command
                    }
                }
            }
            else if (line_pos < (GCODE_MAX_LINE_LENGTH - 1))
            {
                /* Add character to buffer */
                line[line_pos++] = c;
            }
            else
            {
                /* Line buffer overflow - send error (NOT "ok") */
                UGS_SendError(20, "Line buffer overflow");
                line_pos = 0;
            }
        }

        /* CRITICAL (October 25, 2025): Check if arc generation completed
         * TMR1 ISR can't call UGS_SendOK() (blocking UART causes deadlock!)
         * Instead, ISR sets flag and main loop sends "ok" here in safe context.
         */
        (void)MotionBuffer_CheckArcComplete();
        
        /* CRITICAL (October 25, 2025): Flow control for arc generator
         * 
         * Signal to TMR1 ISR that planner buffer has space for more segments.
         * Prevents TMR1 from overflowing the planner buffer during arc execution.
         * 
         * This must be called AFTER draining the buffer (MultiAxis_StartSegmentExecution)
         * so TMR1 can see the available space and continue generating segments.
         */
        MotionBuffer_SignalArcCanContinue();

        /* Start segment execution when idle and segments are available (Phase 2B)
         * 
         * CRITICAL FIX (October 25, 2025 - Evening): Always drain planner buffer!
         * 
         * During arc generation, TMR1 ISR continuously adds segments to the planner buffer.
         * We must ALWAYS call MultiAxis_StartSegmentExecution() to drain planner → stepper,
         * even when machine is busy, otherwise the planner buffer fills up and arc generation stalls!
         * 
         * Flow:
         *   1. UGS/TMR1 sends commands → Planner buffers them
         *   2. Main loop ALWAYS drains planner → stepper buffer
         *   3. Stepper buffer → segment execution (only starts when threshold reached)
         */
        uint8_t planner_count = GRBLPlanner_GetBufferCount();
        uint8_t stepper_count = GRBLStepper_GetBufferCount();
        
        /* Always try to prepare segments if planner has data */
        if (planner_count > 0 || stepper_count > 0)
        {
            (void)MultiAxis_StartSegmentExecution();
        }

#ifdef DEBUG_MOTION_BUFFER
        /*
         * Walkabout guard: If there are no more blocks in the planner and no
         * prepared segments in the stepper buffer, but the machine still
         * reports busy for a long time, stop all motion. This should never
         * happen during normal operation: after the last segment completes,
         * MultiAxis_IsBusy() should fall to false quickly.
         *
         * Threshold: 5 seconds (in CORETIMER ticks @100MHz).
         * Chosen high to avoid tripping on legitimate long last segments.
         */
        {
            uint8_t plan_cnt = GRBLPlanner_GetBufferCount();
            uint8_t seg_cnt = GRBLStepper_GetBufferCount();
            bool stepperBusy = GRBLStepper_IsBusy();
            bool axisBusy = MultiAxis_IsBusy();

            // Guard A: Absolutely no queues left but axes still busy → stop after 2s
            if ((plan_cnt == 0U) && (seg_cnt == 0U) && axisBusy)
            {
                if (!guard_busy_no_queue_active)
                {
                    guard_busy_no_queue_active = true;
                    guard_busy_no_queue_start = CORETIMER_CounterGet();
                }
                else
                {
                    uint32_t now = CORETIMER_CounterGet();
                    const uint32_t GUARD_TICKS = 200000000UL; // 2s @ 100MHz
                    if ((now - guard_busy_no_queue_start) > GUARD_TICKS)
                    {
                        UGS_Printf("GUARD-A: plan=0 seg=0 but still running >2s. Forcing stop.\r\n");
                        MultiAxis_StopAll();
                        guard_busy_no_queue_active = false;
                    }
                }
            }
            else
            {
                guard_busy_no_queue_active = false;
            }

            // Guard B: Planner empty but stepper still busy (draining) for too long → reset stepper (2s window)
            if ((plan_cnt == 0U) && stepperBusy && axisBusy)
            {
                if (!guard_plan_empty_stepper_busy_active)
                {
                    guard_plan_empty_stepper_busy_active = true;
                    guard_plan_empty_stepper_busy_start = CORETIMER_CounterGet();
                }
                else
                {
                    uint32_t now = CORETIMER_CounterGet();
                    const uint32_t GUARD2_TICKS = 200000000UL; // 2s @ 100MHz
                    if ((now - guard_plan_empty_stepper_busy_start) > GUARD2_TICKS)
                    {
                        UGS_Printf("GUARD-B: planner empty but stepper busy >2s. Resetting stepper/halting motion.\r\n");
                        GRBLStepper_Reset();
                        MultiAxis_StopAll();
                        guard_plan_empty_stepper_busy_active = false;
                    }
                }
            }
            else
            {
                guard_plan_empty_stepper_busy_active = false;
            }
        }
#endif

        /* CRITICAL (Oct 25, 2025): Check if arc generation completed
         * 
         * TMR1 ISR can't call UGS_SendOK() directly (blocking UART write causes deadlock!)
         * Instead, ISR sets a flag, and we check it here in main loop to send "ok"
         * in a safe (non-ISR) context.
         */
        (void)MotionBuffer_CheckArcComplete();


        /* Homing cycle non-blocking update */
        if (Homing_IsActive()) {
            Homing_Update();  // Process one step of homing cycle
        }

        /* CRITICAL FIX (Oct 26, 2025): Timer-based LED heartbeat
         * 
         * Previous method counted main loop iterations (clock_status++ > 400000)
         * which broke during arc generation when loop was very busy processing
         * segments. Now uses CORETIMER @ 100MHz for consistent 1Hz blink.
         */
        static uint32_t last_led_toggle = 0;
        uint32_t now = CORETIMER_CounterGet();
        const uint32_t LED_PERIOD_TICKS = 50000000UL;  // 500ms @ 100MHz = 1Hz blink
        
        if ((now - last_led_toggle) >= LED_PERIOD_TICKS) {
            LED1_Toggle();
            last_led_toggle = now;
        }
    }

    return (EXIT_FAILURE);
}

// *****************************************************************************
// Section: Local Function Implementations
// *****************************************************************************

/**
 * @brief Get limit switch state for homing module
 * 
 * Callback function passed to Homing_Initialize().
 * Limit switches use ACTIVE LOW logic (closed = LOW = triggered).
 * 
 * @param axis Axis to check (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 * @param positive_direction true = max limit, false = min limit
 * @return true if switch is triggered (closed/grounded)
 */
static bool GetLimitSwitchState(axis_id_t axis, bool positive_direction)
{
    /* Check appropriate limit switch GPIO based on axis and direction
     * Limit switches are ACTIVE LOW (closed = LOW = triggered)
     */
    switch (axis) {
        case AXIS_X:
            return positive_direction 
                ? false     /* X max limit - not implemented */
                : !LIMIT_X_PIN_Get();  /* X min limit */

        case AXIS_Y:
            return positive_direction
                ? false  /* Y max limit - not implemented */
                : !LIMIT_Y_PIN_Get(); /* Y min limit */

        case AXIS_Z:
            return positive_direction
                ? false  /* Z max limit - not implemented */
                : !LIMIT_Z_PIN_Get(); /* Z min limit */

        case AXIS_A:
            return false;  /* A-axis limits not implemented */
        
        default:
            return false;
    }
}

/*******************************************************************************
 End of File
*******************************************************************************/
