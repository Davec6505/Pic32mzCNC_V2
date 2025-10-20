/*******************************************************************************
  Motion Manager Implementation (Phase 2 - GRBL Stepper Integration)

  Company:
    Microchip Technology Inc.

  File Name:
    motion_manager.c

  Summary:
    GRBL segment prep using TMR9 @ 100Hz (Phase 2).

  Description:
    This module implements GRBL's st_prep_buffer() pattern using segment-based
    execution. TMR9 ISR runs at 100Hz (10ms intervals) and prepares 2mm segments
    from planner blocks, feeding them to OCR hardware for execution.

    PHASE 2 CHANGES (October 19, 2025):
      - Replaced Phase 1 time-based S-curve interpolation
      - Now calls GRBLStepper_PrepSegment() to break blocks into segments
      - Segments fed to OCR hardware via new stepper module
      - Same TMR9 @ 100Hz pattern (just different work being done)

    Dave's Understanding:
      - Planner calculates velocities/junctions ONCE per block (strategic)
      - THIS module prepares segments continuously (tactical)
      - OCR hardware executes segments automatically (zero CPU)

    TMR9 Configuration (MCC):
      - Prescaler: 1:256
      - Period: ~19531 (10ms @ 50MHz PBCLK)
      - Priority: 1 (lowest - background task)
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
#include "motion/grbl_stepper.h"      // GRBLStepper_PrepSegment() (NEW - Phase 2)
#include "motion/grbl_planner.h"      // GRBLPlanner (Phase 1)
#include "motion/multiaxis_control.h" // MultiAxis (Phase 1 - will be replaced by OCR)
#include "ugs_interface.h"            // UGS_Printf() for debug output

// *****************************************************************************
// Section: Local Variables
// *****************************************************************************

/*! \brief Statistics for segment prep monitoring
 *
 * Phase 2: Track segment preparation for debugging/tuning.
 */
static uint32_t prep_calls = 0;
static uint32_t prep_success = 0;

// *****************************************************************************
// Section: TMR9 ISR - Segment Preparation
// *****************************************************************************

/*! \brief TMR9 ISR - Segment preparation (Phase 2 GRBL pattern)
 *
 *  Called every 10ms by TMR9 interrupt at Priority 1 (lowest).
 *
 *  PHASE 2 CHANGES (October 19, 2025):
 *    - Replaced direct motion execution with segment preparation
 *    - Calls GRBLStepper_PrepSegment() to break blocks into 2mm chunks
 *    - Segment buffer feeds OCR hardware continuously
 *
 *  Dave's Understanding:
 *    - This is the "cruise control" that adjusts velocity smoothly
 *    - Gets blocks from planner (strategic data: velocities, junctions)
 *    - Breaks into segments (tactical data: steps, timing)
 *    - OCR hardware executes segments (zero CPU)
 *
 *  Dave's Understanding:
 *    - This is the "cruise control" that adjusts velocity smoothly
 *    - Gets blocks from planner (strategic data: velocities, junctions)
 *    - Breaks into segments (tactical data: steps, timing)
 *    - OCR hardware executes segments (zero CPU)
 *
 *  Timing:
 *    - 10ms interval (100 Hz segment prep rate)
 *    - Typical execution: <500µs per segment prep
 *    - Fills segment buffer (6 segments) before hardware exhausts it
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
   * PHASE 2: Segment Preparation (GRBL st_prep_buffer() pattern)
   *
   * This ISR prepares segments from planner blocks continuously.
   * The segment buffer (6 segments) acts as a "sliding window" through
   * the current block, feeding OCR hardware with pre-calculated data.
   *
   * Dave's Understanding:
   *   - Fill segment buffer with up to 6 segments
   *   - Each segment: 2mm distance, calculated velocity, Bresenham steps
   *   - OCR hardware executes segments automatically
   *   - When segment completes, prepare next one
   * ═══════════════════════════════════════════════════════════════════════ */

  prep_calls++;

  /* Try to prepare segments until buffer full or no blocks */
  uint8_t segments_prepped = 0;
  while (segments_prepped < 3)
  { // Prep up to 3 segments per ISR call
    bool success = GRBLStepper_PrepSegment();
    if (!success)
    {
      break; // Buffer full or no blocks available
    }
    segments_prepped++;
    prep_success++;
  }

  /* PHASE 2B: Segment execution start (REMOVED October 20, 2025)
   *
   * CRITICAL ARCHITECTURAL FIX:
   *   - Execution start moved to main loop (main.c Stage 3)
   *   - ISR should ONLY prepare segments, NOT start hardware execution
   *   - Starting OCR/TMR hardware from ISR caused race conditions
   *   - Main loop now polls: if (idle && segments available) → start execution
   *
   * Dave's insight: "segment methods should be in the main loop outside of any isr"
   *
   * Why this matters:
   *   - Hardware configuration not safe in ISR context
   *   - Avoids ISR-to-ISR timing conflicts
   *   - Clean separation: ISR prepares, main loop executes, OCR fires pulses
   *
   * See main.c lines 683-700 for execution start logic.
   */

#ifdef DEBUG_MOTION_MANAGER
  if (segments_prepped > 0)
  {
    UGS_Printf("[TMR9] Prepped %u segments (buffer: %u/6)\r\n",
               segments_prepped,
               GRBLStepper_GetBufferCount());
  }
#endif
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
  /* Phase 2: Initialize GRBL stepper module (segment buffer) */
  GRBLStepper_Initialize();

  /* Register TMR9 callback for segment preparation (10ms, 100Hz)
   * Priority 1 (lowest) ensures:
   *   - Real-time tasks not interrupted (step generation, position tracking)
   *   - Segment prep runs in background
   *   - Fills segment buffer before hardware exhausts it
   *
   * Dave's Understanding:
   *   - This is the "cruise control" timer
   *   - Prepares 2mm segments continuously
   *   - Feeds OCR hardware with pre-calculated data
   */
  TMR9_CallbackRegister(MotionManager_TMR9_ISR, 0);

  /* Start TMR9 - begins automatic segment preparation */
  TMR9_Start();

  /* Reset statistics */
  prep_calls = 0;
  prep_success = 0;
}
