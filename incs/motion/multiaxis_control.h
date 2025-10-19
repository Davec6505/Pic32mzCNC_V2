#ifndef _MULTIAXIS_CONTROL_H
#define _MULTIAXIS_CONTROL_H

#include "motion_types.h" // Centralized type definitions

// *****************************************************************************
// Public Multi-Axis API
// *****************************************************************************

/*! \brief Initialize multi-axis control subsystem
 *
 *  Initializes all axes (X, Y, Z) with their respective hardware modules.
 *  Must be called before any motion commands.
 */
void MultiAxis_Initialize(void);

/*! \brief Move single axis with S-curve profile
 *
 *  Executes jerk-limited motion on a single axis.
 *
 *  \param axis Axis to move (AXIS_X, AXIS_Y, or AXIS_Z)
 *  \param steps Number of steps to move (absolute value)
 *  \param forward true = forward (CW), false = reverse (CCW)
 */
void MultiAxis_MoveSingleAxis(axis_id_t axis, int32_t steps, bool forward);

/*! \brief Execute time-synchronized coordinated multi-axis motion
 *
 *  MISRA C compliant coordinated motion controller that ensures all axes finish
 *  simultaneously with correct distances traveled. Uses time-based interpolation
 *  where the dominant axis (longest distance) determines total move time.
 *
 *  \param steps Array of steps for each axis [X, Y, Z, A]
 *               Positive = forward, Negative = reverse, 0 = no motion
 *               MISRA Rule 17.4: Array bounds checked internally
 *
 *  \return void (implicit success - motion queued)
 *          Invalid parameters result in no motion (defensive programming)
 *
 *  Time-Synchronized Algorithm:
 *  ─────────────────────────────
 *  1. Find dominant axis: axis_d = argmax(|steps[axis]|)
 *  2. Calculate S-curve for dominant axis → determines total_time
 *  3. For each subordinate axis:
 *       velocity_scale[axis] = steps[axis] / steps[axis_d]
 *       velocity[axis] = velocity_dominant × velocity_scale[axis]
 *  4. All axes share identical segment times (t1-t7) from dominant axis
 *  5. Result: All axes complete motion at exactly total_time
 *
 *  Why This Matters:
 *  ────────────────
 *  Without time synchronization, each axis calculates independent timing:
 *    - X-axis (4000 steps) → completes in 3.0s
 *    - Y-axis (2000 steps) → completes in 2.0s ❌ FINISHES EARLY!
 *    - Result: Curved path instead of straight line
 *
 *  With time synchronization (this function):
 *    - X-axis is dominant (4000 > 2000) → sets total_time = 3.0s
 *    - Y-axis scales velocity by 0.5 (2000/4000) → also completes in 3.0s ✓
 *    - Result: Perfect straight line motion
 *
 *  MISRA C Compliance:
 *  ──────────────────
 *  - Rule 8.7: Internal helper functions declared static
 *  - Rule 17.4: Array indexing validated against NUM_AXES
 *  - Rule 10.1: All type conversions explicit with bounds checking
 *  - Rule 14.4: Single return point per function (defensive early return)
 *
 *  Example Usage:
 *  ─────────────
 *    // Move X=50mm, Y=25mm (80 steps/mm, GT2 belt drive)
 *    int32_t move[NUM_AXES] = {4000, 2000, 0, 0};
 *    MultiAxis_ExecuteCoordinatedMove(move);
 *
 *    // X is dominant: 4000 steps @ 16.7mm/s → 3.0s total time
 *    // Y subordinate: 2000 steps @ 8.3mm/s → 3.0s total time (scaled velocity)
 *    // Both axes arrive at target simultaneously with correct distances
 *
 *  Thread Safety:
 *  ─────────────
 *  Call only from main loop or task context (not from ISR).
 *  Internal state uses volatile variables updated by TMR1 ISR @ 1kHz.
 */
void MultiAxis_ExecuteCoordinatedMove(int32_t steps[NUM_AXES]);

/*! \brief Check if any axis is moving
 *
 *  \return true if any axis is in motion, false if all idle
 */
bool MultiAxis_IsBusy(void);

/*! \brief Check if specific axis is moving
 *
 *  \param axis Axis to check
 *  \return true if axis is moving, false if idle
 */
bool MultiAxis_IsAxisBusy(axis_id_t axis);

/*! \brief Emergency stop all axes
 *
 *  Immediately stops all axis motion.
 */
void MultiAxis_StopAll(void);

/*! \brief Get absolute machine position for an axis
 *
 *  CRITICAL FIX (October 19, 2025): Returns absolute position from power-on/homing,
 *  NOT progress within current move!
 *
 *  \param axis Axis to query (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 *  \return Absolute machine position in steps (unsigned cast of signed value)
 */
uint32_t MultiAxis_GetStepCount(axis_id_t axis);

/*! \brief Update absolute machine position after move completion
 *
 *  CRITICAL FIX (October 19, 2025): Call when motion completes!
 *
 *  Adds the move delta to the absolute machine position tracker.
 *  Must be called from MotionManager_TMR9_ISR() after discarding block.
 *
 *  \param steps Array of step deltas [X, Y, Z, A] (signed: negative = backward)
 */
void MultiAxis_UpdatePosition(const int32_t steps[NUM_AXES]);

// *****************************************************************************
// Dynamic Direction Control
// *****************************************************************************

/*! \brief Set direction pin for specified axis
 *
 *  Uses function pointer lookup for dynamic axis-to-pin mapping.
 *  Called internally or from G-code layer.
 *
 *  \param axis Axis to set direction for (AXIS_X, AXIS_Y, AXIS_Z)
 */
void MultiAxis_SetDirection(axis_id_t axis);

/*! \brief Clear direction pin for specified axis
 *
 *  Uses function pointer lookup for dynamic axis-to-pin mapping.
 *  Called internally or from G-code layer.
 *
 *  \param axis Axis to clear direction for (AXIS_X, AXIS_Y, AXIS_Z)
 */
void MultiAxis_ClearDirection(axis_id_t axis);

// *****************************************************************************
// Stepper Driver Enable Control
// *****************************************************************************

/*! \brief Enable stepper driver for specified axis
 *
 *  Activates the DRV8825 driver by setting ENABLE pin LOW (active-low).
 *  Driver must be enabled before motion begins.
 *
 *  \param axis Axis to enable (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 */
void MultiAxis_EnableDriver(axis_id_t axis);

/*! \brief Disable stepper driver for specified axis
 *
 *  Deactivates the DRV8825 driver by setting ENABLE pin HIGH (active-low).
 *  Motor enters high-impedance state (no holding torque).
 *
 *  \param axis Axis to disable (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 */
void MultiAxis_DisableDriver(axis_id_t axis);

/*! \brief Check if stepper driver is enabled for specified axis
 *
 *  Returns the software-tracked enable state (fast, no GPIO read).
 *
 *  \param axis Axis to query (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 *  \return true if driver is enabled, false if disabled (LOGICAL STATE)
 */
bool MultiAxis_IsDriverEnabled(axis_id_t axis);

/*! \brief Read enable pin GPIO state for specified axis
 *
 *  Reads the actual hardware pin state (for debugging/verification).
 *
 *  ⚠️ Returns RAW electrical state, not logical enabled/disabled!
 *  DRV8825 ENABLE is ACTIVE LOW:
 *    - Returns false (LOW) when driver is ENABLED  ✅
 *    - Returns true (HIGH) when driver is DISABLED ❌
 *
 *  For logical state, use MultiAxis_IsDriverEnabled() instead.
 *
 *  \param axis Axis to query (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 *  \return Raw GPIO pin state (true = HIGH, false = LOW)
 */
bool MultiAxis_ReadEnablePin(axis_id_t axis);

#endif // _MULTIAXIS_CONTROL_H
