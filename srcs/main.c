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

// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
static uint32_t clock_status = 0;



int main(void)
{
    /* Initialize the device */
    SYS_Initialize(NULL);
    UGS_Initialize();
    // Initialize GRBL planner/stepper pipeline (Phase 2B)
    GRBLPlanner_Initialize();
    MultiAxis_Initialize();
    GCode_Initialize();

    UGS_SendBuildInfo();

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
                        bool is_motion = (move.motion_mode <= 3) &&
                                         (move.axis_words[0] || move.axis_words[1] || 
                                          move.axis_words[2] || move.axis_words[3]);

                        #if DEBUG_MOTION_BUFFER >= DEBUG_LEVEL_VERBOSE
                        UGS_Printf("[MOTION] mode=%u, X=%d Y=%d Z=%d A=%d, is_motion=%d\r\n",
                                   move.motion_mode,
                                   move.axis_words[0], move.axis_words[1], 
                                   move.axis_words[2], move.axis_words[3],
                                   is_motion);
                        #endif

                        if (is_motion)
                        {
                            // Build absolute target
                            float target_mm[NUM_AXES];
                            GRBLPlanner_GetPosition(target_mm);

                            for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
                            {
                                if (move.axis_words[axis])
                                {
                                    if (move.absolute_mode)
                                    {
                                        target_mm[axis] = MotionMath_WorkToMachine(move.target[axis], axis);
                                    }
                                    else
                                    {
                                        target_mm[axis] += move.target[axis];
                                    }
                                }
                            }

                            // Planner line data
                            grbl_plan_line_data_t pl_data;
                            pl_data.feed_rate = move.feedrate;
                            pl_data.spindle_speed = 0.0f;
                            pl_data.condition = 0U;
                            if (move.motion_mode == 0)
                            {
                                pl_data.condition |= PL_COND_FLAG_RAPID_MOTION | PL_COND_FLAG_NO_FEED_OVERRIDE;
                            }

                            // Buffer the motion into GRBL planner
                            plan_status_t plan_result = GRBLPlanner_BufferLine(target_mm, &pl_data);
                            
                            if (plan_result == PLAN_OK)
                            {
                                // ✅ SUCCESS: Buffer accepted - send "ok" and reset
                                UGS_SendOK();
                                line_pos = 0;
                                pending_retry = false;  // Clear retry flag
                            }
                            else if (plan_result == PLAN_BUFFER_FULL)
                            {
                                // ⏳ TEMPORARY: Buffer full - set retry flag, keep line buffered
                                pending_retry = true;
                            }
                            else /* plan_result == PLAN_EMPTY_BLOCK */
                            {
                                // ❌ PERMANENT: Zero-length move - send "ok" but don't add to buffer
                                // This is NOT an error - just silently ignore redundant positioning
                                UGS_SendOK();
                                line_pos = 0;
                                pending_retry = false;  // Clear retry flag
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

        /* Start segment execution when idle and segments are available (Phase 2B)
         * 
         * CRITICAL FIX (October 25, 2025): Delayed execution start for look-ahead planning
         * 
         * Don't start NEW execution until planner buffer reaches threshold (4 blocks).
         * This ensures proper look-ahead window for junction velocity optimization.
         * 
         * Flow:
         *   1. UGS sends commands → Planner buffers them → Sends "ok" immediately
         *   2. Buffer fills to threshold (4 blocks)
         *   3. Planner recalculates all blocks (junction velocities optimized)
         *   4. THEN start execution
         *   5. Once started, continue draining even if buffer drops below threshold
         */
        if (!MultiAxis_IsBusy())
        {
            uint8_t planner_count = GRBLPlanner_GetBufferCount();
            uint8_t stepper_count = GRBLStepper_GetBufferCount();
            
            /* Start execution when:
             * 1. NEW execution: Planner buffer >= threshold (4 blocks for look-ahead), OR
             * 2. CONTINUE draining: Stepper buffer has prepared segments
             */
            bool should_start_new = (planner_count >= GRBLPlanner_GetPlanningThreshold());
            bool should_continue = (stepper_count > 0U);
            
            if (should_start_new || should_continue)  // ✅ Either condition starts motion
            {
                (void)MultiAxis_StartSegmentExecution();
            }
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

        /* Maintain system services */
       // SYS_Tasks();
       if(clock_status++ > 400000){
        clock_status = 0;
        LED1_Toggle();
       }
    }

    return (EXIT_FAILURE);
}

/*******************************************************************************
 End of File
*******************************************************************************/
