/*******************************************************************************
  Motion Manager Header File

  Company:
    Microchip Technology Inc.

  File Name:
    motion_manager.h

  Summary:
    Motion buffer feeding coordination for GRBL-style continuous motion.

  Description:
    This module implements GRBL's auto-start pattern using CoreTimer @ 10ms.
    It automatically feeds motion blocks from the ring buffer to the multi-axis
    controller when the machine is idle, ensuring continuous motion without
    gaps between moves.

    Architecture:
      CoreTimer ISR (10ms) → MotionManager_FeedBuffer()
                           ↓
                  Check MultiAxis_IsBusy()
                           ↓
                  MotionBuffer_GetNext()
                           ↓
            MultiAxis_ExecuteCoordinatedMove()

    MISRA C:2012 Compliance:
      - Rule 8.7: Functions declared static where possible
      - Rule 8.9: Single responsibility per module
      - Rule 10.1: Explicit type conversions
      - Rule 17.7: Return values always checked

    Separation of Concerns:
      - motion_buffer.c: Ring buffer management
      - multiaxis_control.c: S-curve motion execution  
      - motion_manager.c: Buffer feeding coordination (this module)
*******************************************************************************/

#ifndef MOTION_MANAGER_H
#define MOTION_MANAGER_H

// *****************************************************************************
// Section: Included Files
// *****************************************************************************

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// *****************************************************************************
// Section: Public Interface Functions
// *****************************************************************************

/*! \brief Initialize motion manager and register CoreTimer callback
 *
 *  Registers the CoreTimer ISR for automatic motion buffer feeding.
 *  CoreTimer runs at 10ms intervals (100 Hz) at Priority 1 (lowest).
 *
 *  Call from MultiAxis_Initialize() after all motion subsystems initialized.
 *
 *  \return None
 *
 *  MISRA Rule 8.7: External linkage required (called from multiaxis_control.c)
 */
void MotionManager_Initialize(void);

/*! \brief CoreTimer ISR - Automatic motion buffer feeding (GRBL-style)
 *
 *  Called every 10ms by CoreTimer interrupt at Priority 1 (lowest).
 *  Implements GRBL's st_prep_buffer() pattern:
 *    1. Check if ALL axes idle (no motion in progress)
 *    2. If idle AND motion buffer has blocks: dequeue and start move
 *    3. This guarantees continuous motion without gaps
 *
 *  ISR Safety Pattern:
 *    - Disables CoreTimer interrupt on entry (prevents re-entrancy)
 *    - Executes motion feed logic (atomic, non-blocking)
 *    - Clears pending interrupt flags
 *    - Re-enables CoreTimer interrupt before exit
 *
 *  This prevents CPU stall from ISR nesting while maintaining guaranteed
 *  10ms minimum interval between motion starts.
 *
 *  \param status CoreTimer status (unused)
 *  \param context User context (unused)
 *
 *  \return None
 *
 *  MISRA Rule 8.2: Function prototype matches implementation
 *  MISRA Rule 2.7: Unused parameters documented with (void) cast
 */
void MotionManager_CoreTimerISR(uint32_t status, uintptr_t context);

#ifdef __cplusplus
}
#endif

#endif /* MOTION_MANAGER_H */
