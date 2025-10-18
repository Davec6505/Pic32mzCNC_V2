/**
 * @file test_ocr_direct.c
 * @brief Direct hardware test for OCR period scaling
 *
 * This test bypasses the G-code parser and coordinate systems entirely.
 * It directly calls MultiAxis_ExecuteCoordinatedMove() with known step counts
 * to verify the OCR period scaling fix is working correctly.
 *
 * Usage:
 *   Send 'T' via serial to trigger test moves
 *   Send '?' to check position
 *   Send 'R' to reset position counters
 */

#include "definitions.h"
#include "motion/multiaxis_control.h"
#include "motion/motion_math.h"
#include "ugs_interface.h"
#include <string.h>

// Test command flag (set by serial command handler)
static volatile bool test_trigger = false;
static volatile bool reset_trigger = false;

/**
 * @brief Check for test commands from serial
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
        if (cmd == 'T' || cmd == 't')
        {
            test_trigger = true;
        }
        else if (cmd == 'R' || cmd == 'r')
        {
            reset_trigger = true;
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
    snprintf(msg, sizeof(msg), "  Result: Y=%ld steps = %.3fmm\r\n", y_steps, y_mm);
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
        snprintf(msg, sizeof(msg), "  ✗ FAIL: Expected 800 steps, got %ld\r\n", y_steps);
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

    snprintf(msg, sizeof(msg), "  Result: X=%ld steps = %.3fmm\r\n", x_steps, x_mm);
    UGS_Print(msg);
    snprintf(msg, sizeof(msg), "          Y=%ld steps = %.3fmm\r\n", y_steps2, y_mm2);
    UGS_Print(msg);

    if (x_steps == 800 && y_steps2 == 1600)
    {
        UGS_Print("  ✓ PASS: Both axes reached target steps!\r\n");
    }
    else
    {
        snprintf(msg, sizeof(msg), "  ✗ FAIL: Expected X=800, Y=1600, got X=%ld, Y=%ld\r\n",
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

    snprintf(msg, sizeof(msg), "  Final: X=%ld steps = %.3fmm\r\n", x_final, x_mm_final);
    UGS_Print(msg);
    snprintf(msg, sizeof(msg), "         Y=%ld steps = %.3fmm\r\n", y_final, y_mm_final);
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
        snprintf(msg, sizeof(msg), "  ✗ FAIL: Position error X=%ld, Y=%ld steps\r\n",
                 x_final, y_final);
        UGS_Print(msg);
    }

    UGS_Print("\r\n");
}

/**
 * @brief Reset step counters to zero
 */
void TestOCR_ResetCounters(void)
{
    if (!reset_trigger)
    {
        return;
    }

    reset_trigger = false;

    // Reset all axis step counters
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        MultiAxis_ResetStepCount(axis);
    }

    UGS_Print("Step counters reset to zero\r\n");
}
