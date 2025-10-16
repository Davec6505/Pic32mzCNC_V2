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

/*! \brief Move multiple axes with coordinated S-curve profiles
 *
 *  Executes synchronized jerk-limited motion across multiple axes.
 *  All axes use the same time-based S-curve profile for coordinated motion.
 *
 *  \param steps Array of steps for each axis [X, Y, Z]
 *               Positive = forward, Negative = reverse, 0 = no motion
 *
 *  Example:
 *    int32_t move[3] = {5000, 3000, 0};  // X=5000 fwd, Y=3000 fwd, Z=idle
 *    MultiAxis_MoveCoordinated(move);
 */
void MultiAxis_MoveCoordinated(int32_t steps[NUM_AXES]);

/*! \brief Execute time-synchronized coordinated move
 *
 *  Proper coordinated motion where the dominant axis (longest distance)
 *  determines the total move time, and all other axes scale their velocities
 *  proportionally to finish simultaneously. This ensures accurate multi-axis
 *  motion with correct distances traveled.
 *
 *  \param steps Array of steps for each axis [X, Y, Z, A]
 *               Positive = forward, Negative = reverse, 0 = no motion
 *
 *  Algorithm:
 *    1. Find dominant axis (max absolute steps)
 *    2. Calculate S-curve profile for dominant axis → determines total_time
 *    3. Scale velocities: v_axis = (distance_axis / total_time)
 *    4. All axes share same segment times (t1-t7) but scaled velocities
 *    5. Result: All axes finish simultaneously with correct distances
 *
 *  Example:
 *    int32_t move[4] = {4000, 2000, 0, 0};  // X=50mm, Y=25mm (80 steps/mm)
 *    MultiAxis_ExecuteCoordinatedMove(move);
 *    // X is dominant, takes 3.0s. Y uses 0.5x velocity, also finishes at 3.0s
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

/*! \brief Get current step count for an axis
 *
 *  \param axis Axis to query
 *  \return Current step count
 */
uint32_t MultiAxis_GetStepCount(axis_id_t axis);

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
