/**
 * @file test_ocr_direct.c
 * @brief Direct hardware test for OCR period scaling
 *
 * This test bypasses the G-code parser and coordinate systems entirely.
 * It directly calls MultiAxis_ExecuteCoordinatedMove() with known step counts
 * to verify the OCR period scaling fix is working correctly.
 *
 * Usage:
 *   Send '$T' via serial to trigger test moves (GRBL system command style)
 *   Send '?' to check position
 *   Send '$R' to reset position counters
 *
 * Note: Uses '$T' instead of 'T' to avoid conflict with G-code T (tool change)
 */

#include "definitions.h"
#include "motion/multiaxis_control.h"
#include "motion/motion_math.h"
#include "ugs_interface.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h> // For PRId32 format specifier

// Test command flag (set by serial command handler)
static volatile bool test_trigger = false;
static volatile bool reset_trigger = false;
static char cmd_buffer[3] = {0}; // Buffer for $T or $R commands
static uint8_t cmd_index = 0;

/**
 * @brief Check for test commands from serial
 * 
 * Recognizes GRBL-style system commands:
 *   $T or $t - Trigger direct hardware test
 *   $R or $r - Reset position counters
 *   $D or $d - Debug axis state information
 */
void TestOCR_CheckCommands(void)
{
    if (!UGS_RxHasData())
    {
        return;
    }

    char cmd;
    if (UART2_Read((uint8_t *)&cmd, 1) == 1)
    {
        // Build command buffer for $X style commands
        if (cmd == '$')
        {
            cmd_buffer[0] = '$';
            cmd_index = 1;
        }
        else if (cmd_index == 1)
        {
            // Second character after '$'
            cmd_buffer[1] = cmd;
            cmd_buffer[2] = '\0';
            
            // Check for test commands
            if (cmd == 'T' || cmd == 't')
            {
                test_trigger = true;
            }
            else if (cmd == 'R' || cmd == 'r')
            {
                reset_trigger = true;
            }
            else if (cmd == 'D' || cmd == 'd')
            {
                // Debug: Print axis state information
                UGS_Print("\r\n[DEBUG] Axis State Information:\r\n");
                for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
                {
                    int32_t steps = MultiAxis_GetStepCount(axis);
                    float mm = MotionMath_StepsToMM(steps, axis);
                    char dbg[100];
                    const char *axis_names[] = {"X", "Y", "Z", "A"};
                    snprintf(dbg, sizeof(dbg), "  %s: %" PRId32 " steps = %.3fmm\r\n",
                             axis_names[axis], steps, mm);
                    UGS_Print(dbg);
                }
            }
            
            // Reset buffer
            cmd_index = 0;
        }
        else
        {
            // Not building a $ command, reset
            cmd_index = 0;
        }
    }
}

/**
 * @brief Execute direct hardware test moves
 *
 * This function tests:
 * 1. Single-axis move: Y=800 steps (should be exactly 10mm @ 80 steps/mm)
 * 2. Coordinated move: X=800, Y=800 (diagonal 10mm x 10mm)
 * 3. Return to origin: X=0, Y=0
 */
void TestOCR_ExecuteTest(void)
{
    if (!test_trigger)
    {
        return;
    }

    test_trigger = false; // Clear flag

    UGS_Print("\r\n");
    UGS_Print("======================================\r\n");
    UGS_Print("  OCR PERIOD SCALING - DIRECT TEST\r\n");
    UGS_Print("======================================\r\n");
    UGS_Print("Bypassing G-code parser and coordinates\r\n");
    UGS_Print("Testing hardware directly with known step counts\r\n");
    UGS_Print("\r\n");

    // Wait for any pending motion to complete
    while (MultiAxis_IsBusy())
    {
        // Wait
    }

    // Test 1: Y-axis single move (800 steps = 10mm @ 80 steps/mm)
    UGS_Print("Test 1: Y-axis 800 steps (should be 10.000mm)\r\n");
    int32_t steps1[NUM_AXES] = {0, 800, 0, 0}; // Y only
    MultiAxis_ExecuteCoordinatedMove(steps1);

    // Wait for completion
    while (MultiAxis_IsBusy())
    {
        // Wait
    }

    // Report position
    int32_t y_steps = MultiAxis_GetStepCount(AXIS_Y);
    float y_mm = MotionMath_StepsToMM(y_steps, AXIS_Y);

    char msg[100];
    snprintf(msg, sizeof(msg), "  Result: Y=%" PRId32 " steps = %.3fmm\r\n", y_steps, y_mm);
    UGS_Print(msg);

    if (y_steps == 800)
    {
        UGS_Print("  ✓ PASS: Correct step count!\r\n");
        if (y_mm >= 9.99f && y_mm <= 10.01f)
        {
            UGS_Print("  ✓ PASS: Position accurate!\r\n");
        }
        else
        {
            UGS_Print("  ✗ FAIL: Position conversion wrong\r\n");
        }
    }
    else
    {
        snprintf(msg, sizeof(msg), "  ✗ FAIL: Expected 800 steps, got %" PRId32 "\r\n", y_steps);
        UGS_Print(msg);
    }

    UGS_Print("\r\n");

    // Test 2: Coordinated diagonal move (X=800, Y=800)
    UGS_Print("Test 2: Coordinated move X=800, Y=800 (diagonal)\r\n");
    int32_t steps2[NUM_AXES] = {800, 800, 0, 0}; // X and Y together
    MultiAxis_ExecuteCoordinatedMove(steps2);

    // Wait for completion
    while (MultiAxis_IsBusy())
    {
        // Wait
    }

    // Report positions
    int32_t x_steps = MultiAxis_GetStepCount(AXIS_X);
    int32_t y_steps2 = MultiAxis_GetStepCount(AXIS_Y);
    float x_mm = MotionMath_StepsToMM(x_steps, AXIS_X);
    float y_mm2 = MotionMath_StepsToMM(y_steps2, AXIS_Y);

    snprintf(msg, sizeof(msg), "  Result: X=%" PRId32 " steps = %.3fmm\r\n", x_steps, x_mm);
    UGS_Print(msg);
    snprintf(msg, sizeof(msg), "          Y=%" PRId32 " steps = %.3fmm\r\n", y_steps2, y_mm2);
    UGS_Print(msg);

    if (x_steps == 800 && y_steps2 == 1600)
    {
        UGS_Print("  ✓ PASS: Both axes reached target steps!\r\n");
    }
    else
    {
        snprintf(msg, sizeof(msg), "  ✗ FAIL: Expected X=800, Y=1600, got X=%" PRId32 ", Y=%" PRId32 "\r\n",
                 x_steps, y_steps2);
        UGS_Print(msg);
    }

    UGS_Print("\r\n");

    // Test 3: Return to origin (negative move)
    UGS_Print("Test 3: Return to origin X=-800, Y=-1600\r\n");
    int32_t steps3[NUM_AXES] = {-800, -1600, 0, 0};
    MultiAxis_ExecuteCoordinatedMove(steps3);

    // Wait for completion
    while (MultiAxis_IsBusy())
    {
        // Wait
    }

    // Report final positions
    int32_t x_final = MultiAxis_GetStepCount(AXIS_X);
    int32_t y_final = MultiAxis_GetStepCount(AXIS_Y);
    float x_mm_final = MotionMath_StepsToMM(x_final, AXIS_X);
    float y_mm_final = MotionMath_StepsToMM(y_final, AXIS_Y);

    snprintf(msg, sizeof(msg), "  Final: X=%" PRId32 " steps = %.3fmm\r\n", x_final, x_mm_final);
    UGS_Print(msg);
    snprintf(msg, sizeof(msg), "         Y=%" PRId32 " steps = %.3fmm\r\n", y_final, y_mm_final);
    UGS_Print(msg);

    if (x_final == 0 && y_final == 0)
    {
        UGS_Print("  ✓ PASS: Returned to origin perfectly!\r\n");
        UGS_Print("\r\n");
        UGS_Print("======================================\r\n");
        UGS_Print("  OCR PERIOD SCALING: VERIFIED! ✓\r\n");
        UGS_Print("======================================\r\n");
    }
    else
    {
        snprintf(msg, sizeof(msg), "  ✗ FAIL: Position error X=%" PRId32 ", Y=%" PRId32 " steps\r\n",
                 x_final, y_final);
        UGS_Print(msg);
    }

    UGS_Print("\r\n");
}

/**
 * @brief Reset step counters to zero
 * Note: This is a manual reset message only. Step counters are maintained
 * by hardware and reset only on system reset or firmware reload.
 */
void TestOCR_ResetCounters(void)
{
    if (!reset_trigger)
    {
        return;
    }

    reset_trigger = false;

    UGS_Print("Note: Step counters are hardware-maintained.\r\n");
    UGS_Print("To reset, perform a system reset or reload firmware.\r\n");
}
