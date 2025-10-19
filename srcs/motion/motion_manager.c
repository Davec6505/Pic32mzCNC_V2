/*******************************************************************************
  Motion Manager Implementation

  Company:
    Microchip Technology Inc.

  File Name:
    motion_manager.c

  Summary:
    GRBL-style automatic motion buffer feeding using TMR9 @ 10ms.

  Description:
    This module implements the GRBL st_prep_buffer() pattern for continuous
    motion without gaps between moves. TMR9 ISR runs at 10ms intervals
    (Priority 1 - lowest) and automatically starts the next move when the
    machine becomes idle.

    CRITICAL FIX (October 19, 2025): Switched from CoreTimer to TMR9!
    PIC32MZ CoreTimer has special CPU-coupled behavior that prevents reliable
    interrupt disable/enable in ISR context. TMR9 is a standard peripheral
    timer with clean interrupt handling.

    ISR Pattern (NO DISABLE/ENABLE NEEDED):
      1. Check if machine idle AND buffer has data
      2. Dequeue block and start coordinated move
      3. Hardware clears interrupt flag automatically

    TMR9 Configuration (MCC):
      - Prescaler: 1:256
      - Period: ~19531 (10ms @ 50MHz PBCLK)
      - Priority: 1 (lowest)
      - Callback: Enabled

    MISRA C:2012 Compliance:
      - Rule 8.7: Static functions where external linkage not required
      - Rule 2.7: Unused parameters explicitly cast to void
      - Rule 10.1: Explicit operand type conversions
      - Rule 17.7: All return values checked before use
*******************************************************************************/

// *****************************************************************************
// Section: Included Files
// *****************************************************************************

#include "definitions.h"              // SYS function prototypes
#include "motion/motion_manager.h"    // Own header (MISRA Rule 8.8)
#include "motion/motion_buffer.h"     // MotionBuffer_GetNext(), HasData() (OLD - Phase 2: remove)
#include "motion/grbl_planner.h"      // GRBLPlanner_GetCurrentBlock() (NEW - Phase 1)
#include "motion/multiaxis_control.h" // MultiAxis_IsBusy(), ExecuteCoordinatedMove()
#include "ugs_interface.h"            // UGS_Printf() for debug output

// *****************************************************************************
// Section: Local Variables
// *****************************************************************************

/*! \brief Flag to track if current block has been executed
 *
 *  CRITICAL (October 19, 2025): GRBL blocks must remain in planner until
 *  motion completes. This flag prevents premature discard.
 *
 *  Pattern:
 *    1. Get block from planner
 *    2. Start motion, set flag = false
 *    3. Wait for motion to complete (MultiAxis_IsBusy() returns false)
 *    4. Discard block, set flag = true
 *    5. Get next block
 *
 * CRITICAL: volatile required for MIPS compiler optimization!
 * - This flag is accessed by TMR9 ISR and modified during motion execution
 * - Without volatile, XC32 compiler may cache value in register causing stale reads
 * - MIPS architecture requires explicit volatile for ISR-shared variables
 */
static volatile bool block_discarded = true; /* Start true (no block active) */

/*! \brief Last move's step deltas for position update
 *
 *  CRITICAL FIX (October 19, 2025): Save step deltas before starting move,
 *  then update machine_position when move completes!
 *
 *  Without this, MultiAxis_GetStepCount() returns step_count (move progress)
 *  instead of absolute machine position, causing position feedback to be wrong.
 *
 *  Pattern:
 *    1. Convert GRBL block to signed steps[] array
 *    2. Save to last_move_steps[] BEFORE calling MultiAxis_ExecuteCoordinatedMove()
 *    3. When motion completes, call MultiAxis_UpdatePosition(last_move_steps)
 *    4. This adds delta to machine_position[], keeping absolute position accurate
 */
static int32_t last_move_steps[NUM_AXES] = {0, 0, 0, 0};

// *****************************************************************************
// Section: TMR9 ISR - Motion Buffer Feeding
// *****************************************************************************

/*! \brief TMR9 ISR - Automatic motion buffer feeding (GRBL-style)
 *
 *  Called every 10ms by TMR9 interrupt at Priority 1 (lowest).
 *
 *  CRITICAL FIX (October 19, 2025): Switched from CoreTimer to TMR9!
 *  - PIC32MZ CoreTimer has CPU-coupled behavior (disable/enable unreliable)
 *  - TMR9 is standard peripheral timer with clean interrupt handling
 *  - No manual interrupt disable/enable needed (hardware handles it)
 *
 *  GRBL Pattern:
 *    1. Check if ALL axes idle (no motion in progress)
 *    2. If idle AND motion buffer has blocks:
 *       - Dequeue next block from ring buffer
 *       - Filter zero-step blocks (no actual motion)
 *       - Start coordinated move with pre-calculated S-curve
 *    3. Result: Continuous motion without gaps between moves
 *
 *  Timing:
 *    - 10ms interval (100 Hz update rate)
 *    - Typical execution: <100µs (check + dequeue + start)
 *    - Hardware guarantees: No re-entry, automatic flag clear
 *
 *  \param status TMR9 interrupt status (unused)
 *  \param context User context (unused)
 *
 *  \return None
 *
 *  MISRA Rule 2.7: Unused parameters explicitly documented
 *  MISRA Rule 8.7: External linkage required (registered as callback)
 */
static void MotionManager_TMR9_ISR(uint32_t status, uintptr_t context)
{
    /* MISRA Rule 2.7: Explicitly document unused parameters */
    (void)status;  // TMR9 status not used
    (void)context; // User context not used

    /* ═══════════════════════════════════════════════════════════════════════
     * Motion Buffer Feeding Logic (GRBL st_prep_buffer() pattern)
     *
     * CRITICAL (October 19, 2025): On MIPS PIC32MZ, do NOT disable/enable
     * timer interrupt in ISR! Hardware automatically prevents re-entry.
     * TMR9_InterruptDisable/Enable() caused ISR to stop firing after first call.
     * ═══════════════════════════════════════════════════════════════════════ */

    /* Step 1: Check if ALL axes are idle (no motion in progress) */
    bool is_busy = MultiAxis_IsBusy();

    /* DEBUG: Disable ISR entry debug - floods UART at 100Hz preventing command RX!
     * The constant stream of debug output blocks serial receive, causing:
     *   - Commands not parsed
     *   - No "ok" responses sent
     *   - Motion never executes
     * Re-enable ONLY when debugging specific ISR timing issues. */
    // UGS_Printf("[TMR9-ISR] Entry: busy=%d block_discarded=%d\r\n",
    //            is_busy ? 1 : 0, block_discarded ? 1 : 0);

    if (!is_busy)
    {
        /* Step 1a: If previous block still active, discard it now (motion complete)
         *
         * CRITICAL FIX (October 19, 2025): Only discard after motion completes!
         * Old bug: Discarded immediately after starting → multiple moves executed simultaneously
         * New logic: Discard only when machine becomes idle */
        if (!block_discarded)
        {
#ifdef DEBUG_MOTION_BUFFER
            UGS_Print("[TMR9] Discarding previous block...\r\n");
#endif

            /* CRITICAL FIX (October 19, 2025): Update absolute machine position!
             * The step counters (axis_state[].step_count) track progress within move (0 → total_steps).
             * We need to update machine_position[] with the delta from this completed move.
             * This ensures MultiAxis_GetStepCount() returns ABSOLUTE position for GRBL feedback! */
            MultiAxis_UpdatePosition(last_move_steps);

            GRBLPlanner_DiscardCurrentBlock();
            block_discarded = true;
#ifdef DEBUG_MOTION_BUFFER
            UGS_Print("[TMR9] Block completed and discarded\r\n");
#endif
        }

        /* Step 2: Get next planned block from GRBL planner (Phase 1 - NEW!)
         *
         * GRBL INTEGRATION (October 19, 2025):
         *   - Changed from MotionBuffer_HasData()/GetNext() to GRBLPlanner_GetCurrentBlock()
         *   - GRBL planner ring buffer now feeds motion execution
         *   - Block contains: target position (steps), entry/exit velocities, acceleration
         *
         * Returns: Pointer to grbl_plan_block_t, or NULL if buffer empty */
        grbl_plan_block_t *grbl_block = GRBLPlanner_GetCurrentBlock();

        if (grbl_block != NULL)
        {
            /* Step 3: Convert GRBL block to coordinated move format
             *
             * CRITICAL (October 19, 2025): GRBL stores motion differently than our system!
             *   - GRBL: steps[] = unsigned delta (e.g., 6400 for 5mm move)
             *           direction_bits = bit mask (bit N set = axis N moves negative)
             *   - Our system: steps[] = signed delta (negative = backward, positive = forward)
             *
             * Must convert: Check direction bit, apply sign to step count.
             *
             * Example: Z-axis moving from 5mm to 0mm:
             *   GRBL: steps[AXIS_Z] = 6400, direction_bits |= Z_DIRECTION_BIT
             *   Our:  steps[AXIS_Z] = -6400 (negative for backward motion)
             */
            int32_t steps[NUM_AXES];

#ifdef DEBUG_MOTION_BUFFER
            /* DEBUG: Show what GRBL planner calculated */
            UGS_Printf("[GRBL-BLOCK] Raw: X=%lu Y=%lu Z=%lu A=%lu dir_bits=0x%02X\r\n",
                       grbl_block->steps[AXIS_X],
                       grbl_block->steps[AXIS_Y],
                       grbl_block->steps[AXIS_Z],
                       grbl_block->steps[AXIS_A],
                       grbl_block->direction_bits);
#endif

            for (uint8_t axis = 0; axis < NUM_AXES; axis++)
            {
                /* Get unsigned step count from GRBL */
                uint32_t abs_steps = grbl_block->steps[axis];

                /* Check if this axis moves in negative direction */
                uint8_t dir_mask = (1U << axis); /* Bit 0=X, 1=Y, 2=Z, 3=A */
                bool is_negative = (grbl_block->direction_bits & dir_mask) != 0;

                /* Apply sign based on direction */
                if (is_negative)
                {
                    steps[axis] = -(int32_t)abs_steps; /* Negative motion */
                }
                else
                {
                    steps[axis] = (int32_t)abs_steps; /* Positive motion */
                }
            }

            /* Step 4: Filter zero-step blocks (no actual motion)
             * These can occur when moving to current position:
             *   Example: G0 X0 Y0 when already at origin (0,0)
             * Skip execution to avoid clogging the pipeline */
            bool has_steps = (steps[AXIS_X] != 0) ||
                             (steps[AXIS_Y] != 0) ||
                             (steps[AXIS_Z] != 0) ||
                             (steps[AXIS_A] != 0);

            if (has_steps)
            {
                /* Step 5: Save step deltas for position update when move completes
                 * CRITICAL FIX (October 19, 2025): Must save BEFORE starting motion!
                 * When motion completes, we call MultiAxis_UpdatePosition(last_move_steps)
                 * to update the absolute machine position tracker. */
                for (uint8_t axis = 0; axis < NUM_AXES; axis++)
                {
                    last_move_steps[axis] = steps[axis];
                }

                /* Step 6: Start coordinated move with pre-calculated S-curve
                 * This function returns immediately (non-blocking):
                 *   - Sets up axis states with S-curve timing
                 *   - Configures OCR hardware for pulse generation
                 *   - Starts timers for all active axes
                 * TMR1 @ 1kHz handles actual motion execution */
                MultiAxis_ExecuteCoordinatedMove(steps);

                /* Mark block as active (will be discarded when motion completes) */
                block_discarded = false;

#ifdef DEBUG_MOTION_BUFFER
                UGS_Printf("[TMR9] Started: X=%ld Y=%ld Z=%ld A=%ld\r\n",
                           steps[AXIS_X],
                           steps[AXIS_Y],
                           steps[AXIS_Z],
                           steps[AXIS_A]);
#endif
            }
            else
            {
                /* Zero-step block - discard immediately (no motion to wait for) */
                GRBLPlanner_DiscardCurrentBlock();
                block_discarded = true;

#ifdef DEBUG_MOTION_BUFFER
                UGS_Printf("[TMR9] Filtered zero-step block\r\n");
#endif
            }
        }
        /* else: GRBL planner empty - machine will remain idle */
    }
    /* else: Machine busy - wait for current move to complete */

    /* ═══════════════════════════════════════════════════════════════════════
     * CRITICAL: Re-enable TMR9 interrupt before exit (use Harmony getter/setter)
     * Clear any pending interrupt flags automatically handled by hardware
     * ═══════════════════════════════════════════════════════════════════════ */
    //  TMR9_InterruptEnable();
}

// *****************************************************************************
// Section: Public Interface Functions
// *****************************************************************************

/*! \brief Initialize motion manager and register TMR9 callback
 *
 *  Registers MotionManager_TMR9_ISR() as the TMR9 callback.
 *  TMR9 configured in MCC for 10ms period, Priority 1 (lowest).
 *
 *  CRITICAL FIX (October 19, 2025): Switched from CoreTimer to TMR9!
 *  - PIC32MZ CoreTimer has CPU-coupled behavior (unreliable disable/enable)
 *  - TMR9 is standard peripheral timer (clean interrupt handling)
 *  - No manual interrupt management needed (hardware handles it)
 *
 *  Call this from MultiAxis_Initialize() after all motion subsystems
 *  have been initialized.
 *
 *  \return None
 *
 *  MISRA Rule 8.7: External linkage required (called from multiaxis_control.c)
 */
void MotionManager_Initialize(void)
{
    /* Register TMR9 callback for motion buffer feeding (10ms, GRBL-style)
     * Priority 1 (lowest) ensures:
     *   - TMR1 S-curve updates not interrupted (higher priority)
     *   - OCR step generation not interrupted (higher priority)
     *   - Real-time commands processed in main loop (runs faster than 10ms)
     *
     * TMR9 hardware automatically:
     *   - Clears interrupt flag after ISR
     *   - Prevents re-entry until ISR completes
     *   - Maintains precise 10ms timing regardless of ISR execution time
     */
    TMR9_CallbackRegister(MotionManager_TMR9_ISR, 0);

    /* Start TMR9 - begins automatic motion buffer feeding */
    TMR9_Start();

    /* Block discard flag already initialized to true (see static variable) */
}
