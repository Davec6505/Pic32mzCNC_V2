#ifndef _STEPPER_CONTROL_H
#define _STEPPER_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

// *****************************************************************************
// Motion Control Constants
// *****************************************************************************

// Default motion parameters (adjustable via API)
#define DEFAULT_ACCEL 10000 // Acceleration in steps/sec²
#define DEFAULT_DECEL 10000 // Deceleration in steps/sec²
#define DEFAULT_SPEED 5000  // Max speed in steps/sec
#define DEFAULT_STEPS 5000  // Default steps per move

// *****************************************************************************
// Public API
// *****************************************************************************

/*! \brief Initialize stepper control subsystem
 *
 *  Registers OCR and TMR1 callbacks, starts 1kHz control loop.
 *  Must be called before any motion commands.
 */
void StepperControl_Initialize(void);

/*! \brief Move stepper motor with S-curve acceleration profile
 *
 *  Executes a jerk-limited 7-segment S-curve motion profile:
 *  1. Jerk-in during acceleration
 *  2. Constant acceleration
 *  3. Jerk-out during acceleration
 *  4. Constant velocity (cruise)
 *  5. Jerk-in during deceleration
 *  6. Constant deceleration
 *  7. Jerk-out during deceleration
 *
 *  \param steps Number of steps to move (absolute value)
 *  \param forward true = forward (CW), false = reverse (CCW)
 */
void StepperControl_MoveSteps(int32_t steps, bool forward);

/*! \brief Check if motion is in progress
 *
 *  \return true if motor is moving, false if idle
 */
bool StepperControl_IsBusy(void);

/*! \brief Set motion profile parameters
 *
 *  Updates the S-curve profile limits. Changes take effect on next move.
 *  Jerk is automatically calculated as 5x acceleration for smooth motion.
 *
 *  \param accel Acceleration in steps/sec²
 *  \param decel Deceleration in steps/sec² (currently same as accel)
 *  \param speed Max velocity in steps/sec
 */
void StepperControl_SetProfile(uint32_t accel, uint32_t decel, uint32_t speed);

/*! \brief Emergency stop - immediately halt motion
 *
 *  Forces motion to SEGMENT_COMPLETE state. Hardware stops on next TMR1 cycle.
 */
void StepperControl_Stop(void);

/*! \brief Get current step count
 *
 *  Returns number of steps executed in current move.
 *  Valid only while StepperControl_IsBusy() returns true.
 *
 *  \return Number of steps completed
 */
uint32_t StepperControl_GetStepCount(void);

#endif /* _STEPPER_CONTROL_H */