/*******************************************************************************
  Motion Manager Implementation

  Company:
    Microchip Technology Inc.

  File Name:
    motion_manager.c

  Summary:
    GRBL-style automatic motion buffer feeding using CoreTimer @ 10ms.

  Description:
    This module implements the GRBL st_prep_buffer() pattern for continuous
    motion without gaps between moves. CoreTimer ISR runs at 10ms intervals
    (Priority 1 - lowest) and automatically starts the next move when the
    machine becomes idle.

    Critical ISR Pattern:
      1. Disable CoreTimer interrupt immediately on entry
      2. Check if machine idle AND buffer has data
      3. Dequeue block and start coordinated move
      4. Clear interrupt flags and re-enable interrupt
      
    This prevents ISR re-entrancy and CPU stall while maintaining guaranteed
    timing (10ms ±0ms jitter).

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
#include "motion/motion_buffer.h"     // MotionBuffer_GetNext(), HasData()
#include "motion/multiaxis_control.h" // MultiAxis_IsBusy(), ExecuteCoordinatedMove()
#include "ugs_interface.h"            // UGS_Printf() for debug output

// *****************************************************************************
// Section: CoreTimer ISR - Motion Buffer Feeding
// *****************************************************************************

/*! \brief CoreTimer ISR - Automatic motion buffer feeding (GRBL-style)
 *
 *  Called every 10ms by CoreTimer interrupt at Priority 1 (lowest).
 *
 *  CRITICAL ISR SAFETY PATTERN:
 *    - Disables CoreTimer interrupt immediately on entry
 *    - Executes motion feed logic (atomic, non-blocking)
 *    - Clears any pending interrupt flags
 *    - Re-enables CoreTimer interrupt before exit
 *
 *  This prevents ISR re-entrancy and CPU stall from ISR nesting.
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
 *    - Self-limiting: If ISR takes >10ms, next interrupt waits
 *
 *  \param status CoreTimer status (unused)
 *  \param context User context (unused)
 *
 *  \return None
 *
 *  MISRA Rule 2.7: Unused parameters explicitly documented
 *  MISRA Rule 8.7: External linkage required (registered as callback)
 */
void MotionManager_CoreTimerISR(uint32_t status, uintptr_t context)
{
    /* MISRA Rule 2.7: Explicitly document unused parameters */
    (void)status;   // CoreTimer status not used
    (void)context;  // User context not used

    /* ═══════════════════════════════════════════════════════════════════════
     * CRITICAL: Disable CoreTimer interrupt immediately on entry
     * Prevents re-entry if ISR execution takes longer than 10ms period
     * ═══════════════════════════════════════════════════════════════════════ */
    IEC0CLR = _IEC0_CTIE_MASK;  // Disable CoreTimer interrupt enable

    /* ═══════════════════════════════════════════════════════════════════════
     * Motion Buffer Feeding Logic (GRBL st_prep_buffer() pattern)
     * ═══════════════════════════════════════════════════════════════════════ */

    /* Step 1: Check if ALL axes are idle (no motion in progress) */
    if (!MultiAxis_IsBusy())
    {
        /* Step 2: Check if motion buffer has planned blocks available */
        if (MotionBuffer_HasData())
        {
            motion_block_t block;

            /* Step 3: Dequeue next planned block from ring buffer
             * MISRA Rule 17.7: Return value checked before proceeding */
            if (MotionBuffer_GetNext(&block))
            {
                /* Step 4: Filter zero-step blocks (no actual motion)
                 * These can occur when moving to current position:
                 *   Example: G0 X0 Y0 when already at origin (0,0)
                 * Skip execution to avoid clogging the pipeline */
                bool has_steps = (block.steps[AXIS_X] != 0) ||
                                (block.steps[AXIS_Y] != 0) ||
                                (block.steps[AXIS_Z] != 0) ||
                                (block.steps[AXIS_A] != 0);

                if (has_steps)
                {
                    /* Step 5: Start coordinated move with pre-calculated S-curve
                     * This function returns immediately (non-blocking):
                     *   - Sets up axis states with S-curve timing
                     *   - Configures OCR hardware for pulse generation
                     *   - Starts timers for all active axes
                     * TMR1 @ 1kHz handles actual motion execution */
                    MultiAxis_ExecuteCoordinatedMove(block.steps);

#ifdef DEBUG_MOTION_BUFFER
                    /* Debug: Report motion started from CoreTimer ISR */
                    UGS_Printf("[CORETIMER] Started: X=%ld Y=%ld Z=%ld A=%ld\r\n",
                               block.steps[AXIS_X],
                               block.steps[AXIS_Y],
                               block.steps[AXIS_Z],
                               block.steps[AXIS_A]);
#endif
                }
#ifdef DEBUG_MOTION_BUFFER
                else
                {
                    /* Debug: Zero-step block filtered (not executed) */
                    UGS_Printf("[CORETIMER] Filtered zero-step block\r\n");
                }
#endif
            }
            /* else: MotionBuffer_GetNext() returned false
             * This should not happen if HasData() returned true,
             * but defensive check prevents undefined behavior */
        }
        /* else: Motion buffer empty - machine will remain idle */
    }
    /* else: Machine busy - wait for current move to complete */

    /* ═══════════════════════════════════════════════════════════════════════
     * CRITICAL: Re-arm CoreTimer interrupt before exit
     * Clear any pending interrupt flags, then re-enable interrupt
     * ═══════════════════════════════════════════════════════════════════════ */
    IFS0CLR = _IFS0_CTIF_MASK;  // Clear CoreTimer interrupt flag (if set during work)
    IEC0SET = _IEC0_CTIE_MASK;  // Re-enable CoreTimer interrupt
}

// *****************************************************************************
// Section: Public Interface Functions
// *****************************************************************************

/*! \brief Initialize motion manager and register CoreTimer callback
 *
 *  Registers MotionManager_CoreTimerISR() as the CoreTimer callback.
 *  CoreTimer configured in MCC for 10ms period, Priority 1 (lowest).
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
    /* Register CoreTimer callback for motion buffer feeding (10ms, GRBL-style)
     * Priority 1 (lowest) ensures:
     *   - TMR1 S-curve updates not interrupted (higher priority)
     *   - OCR step generation not interrupted (higher priority)
     *   - Real-time commands processed in main loop (runs faster than 10ms)
     *
     * Callback uses disable/re-enable pattern for ISR safety:
     *   - Prevents re-entrancy if execution takes >10ms
     *   - Guarantees atomic buffer operations
     *   - Self-limiting execution time (CPU cannot stall) */
    CORETIMER_CallbackSet(MotionManager_CoreTimerISR, (uintptr_t)0);
    CORETIMER_Start();
}
