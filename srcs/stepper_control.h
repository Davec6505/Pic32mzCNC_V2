#ifndef _STEPPER_CONTROL_H
#define _STEPPER_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

// *****************************************************************************
// Motion Control Constants
// *****************************************************************************

// Default motion parameters
#define DEFAULT_ACCEL 80000 // Acceleration in 0.01*rad/sec^2
#define DEFAULT_DECEL 80000 // Deceleration in 0.01*rad/sec^2
#define DEFAULT_SPEED 10000 // Max speed in 0.01*rad/sec
#define DEFAULT_STEPS 5000  // Default steps per move

// *****************************************************************************
// Motion State Enumerations
// *****************************************************************************

typedef enum
{
    STOP = 0,
    ACCEL = 1,
    DECEL = 2,
    RUN = 3
} run_state_t;

typedef enum
{
    CW = 0,
    CCW = 1
} direction_t;

// *****************************************************************************
// Public API
// *****************************************************************************

/*! \brief Initialize stepper control subsystem
 *  Registers OCR and TMR1 callbacks, starts 1kHz control loop
 */
void StepperControl_Initialize(void);

/*! \brief Move stepper motor with S-curve acceleration
 *  \param steps Number of steps to move (absolute value)
 *  \param forward true = forward (CW), false = reverse (CCW)
 */
void StepperControl_MoveSteps(int32_t steps, bool forward);

/*! \brief Check if motion is in progress
 *  \return true if motor is moving, false if idle
 */
bool StepperControl_IsBusy(void);

/*! \brief Set motion profile parameters
 *  \param accel Acceleration in 0.01*rad/sec^2
 *  \param decel Deceleration in 0.01*rad/sec^2
 *  \param speed Max speed in 0.01*rad/sec
 */
void StepperControl_SetProfile(uint32_t accel, uint32_t decel, uint32_t speed);

/*! \brief Emergency stop - immediately halt motion
 */
void StepperControl_Stop(void);

/*! \brief Get current step count
 *  \return Number of steps executed in current move
 */
uint32_t StepperControl_GetStepCount(void);

#endif /* _STEPPER_CONTROL_H */