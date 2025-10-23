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

// Treat a line as meaningful only if it contains at least one GRBL word letter
// Valid letters: G, M, X, Y, Z, A, B, C, I, J, K, F, S, T, P, L, N, R, D, H, $


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

    while (true)
    {
        /* Handle any real-time commands (e.g., status report '?') */
        uint8_t rt_cmd = Serial_GetRealtimeCommand();
        if (rt_cmd != 0)
        {
            GCode_HandleControlChar(rt_cmd);
        }

        /* Non-blocking check for a single character from the serial ring buffer */
        int16_t c_int = Serial_Read();

        if (c_int != -1)
        {
            char c = (char)c_int;

            if (c == '\n' || c == '\r')
            {
                /* Line terminator received, process the buffered line */
                line[line_pos] = '\0';

                // Trim leading whitespace to correctly identify empty/whitespace-only lines
                char *line_start = line;
                while (*line_start && isspace((unsigned char)*line_start))
                {
                    line_start++;
                }

                if (*line_start == '\0' || !LineHasGrblWordLetter(line_start))
                {
                    // Line is empty or was only whitespace. Send OK to keep UGS streaming.
                    UGS_SendOK();
                }
                else
                {
                    // Line has content, parse and execute
                    parsed_move_t move;
                    if (GCode_ParseLine(line_start, &move))
                    {
                        bool is_motion = (move.motion_mode <= 3) &&
                                         (move.axis_words[0] || move.axis_words[1] || move.axis_words[2] || move.axis_words[3]);

                        if (is_motion)
                        {
                            // Build absolute MACHINE-coordinate target in mm for GRBL planner
                            float target_mm[NUM_AXES];
                            // Start from current planner position to preserve unspecified axes
                            GRBLPlanner_GetPosition(target_mm);

                            for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
                            {
                                if (move.axis_words[axis])
                                {
                                    if (move.absolute_mode)
                                    {
                                        // G90: provided value is work coords; convert to machine coords
                                        target_mm[axis] = MotionMath_WorkToMachine(move.target[axis], axis);
                                    }
                                    else
                                    {
                                        // G91: relative offset (in work coords == machine offset magnitude)
                                        target_mm[axis] += move.target[axis];
                                    }
                                }
                            }

                            // Planner line data
                            grbl_plan_line_data_t pl_data;
                            pl_data.feed_rate = move.feedrate; // mm/min
                            pl_data.spindle_speed = 0.0f;       // not used yet
                            pl_data.condition = 0U;
                            if (move.motion_mode == 0) // G0 rapid
                            {
                                pl_data.condition |= PL_COND_FLAG_RAPID_MOTION | PL_COND_FLAG_NO_FEED_OVERRIDE;
                                // For rapids, ignore feed_rate (planner uses rapid_rate)
                            }

                            // Buffer the motion into GRBL planner
                            (void)GRBLPlanner_BufferLine(target_mm, &pl_data);

                            // Always acknowledge to keep sender streaming (GRBL character-counting)
                            UGS_SendOK();
                        }
                        else
                        {
                            // Non-motion commands (modal state changes, status queries, etc.)
                            UGS_SendOK();
                        }
                    }
                    else
                    {
                        UGS_SendError(1, GCode_GetLastError());
                    }
                }
                
                // Reset buffer for the next line
                line_pos = 0;
            }
            else if (line_pos < (GCODE_MAX_LINE_LENGTH - 1))
            {
                /* Add character to buffer */
                line[line_pos++] = c;
            }
            else
            {
                /* Line buffer overflow */
                UGS_SendError(20, "Line buffer overflow");
                line_pos = 0; // Reset
            }
        }

        /* Start segment execution when idle and segments are available (Phase 2B) */
        if (!MultiAxis_IsBusy())
        {
            if (GRBLStepper_GetBufferCount() > 0U)
            {
                (void)MultiAxis_StartSegmentExecution();
            }
        }

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
