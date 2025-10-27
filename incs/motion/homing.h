/*******************************************************************************
  Homing Module - Header File

  Description:
    Implements GRBL-style homing cycle for PIC32MZ CNC Controller.
    Supports multi-axis homing with configurable sequences, speeds, and pulloff.

  Architecture:
    - State machine pattern (non-blocking for responsive main loop)
    - Integrates with existing limit switch handling (app.c)
    - Uses motion_math for settings ($23-$27)
    - Reuses MultiAxis_ExecuteCoordinatedMove() for motion
    
  Usage:
    // In main loop initialization:
    Homing_Initialize();
    
    // Trigger homing (G28 or $H command):
    Homing_ExecuteCycle(HOMING_CYCLE_ALL_AXES);
    
    // In main loop (non-blocking update):
    while (true) {
        if (Homing_IsActive()) {
            Homing_Update();  // Process one state machine step
        }
        // ... other main loop tasks
    }
*******************************************************************************/

#ifndef HOMING_H
#define HOMING_H

#include "motion_types.h"
#include <stdint.h>
#include <stdbool.h>

// *****************************************************************************
// Section: Type Definitions
// *****************************************************************************

/*! \brief Homing cycle state machine states */
typedef enum {
    HOMING_IDLE = 0,           /* No homing active */
    HOMING_INIT,               /* Initialize homing sequence */
    HOMING_APPROACH,           /* Fast search for limit switch */
    HOMING_BACKOFF,            /* Pull off limit after hit */
    HOMING_SLOW_APPROACH,      /* Slow precision approach */
    HOMING_PULLOFF_FINAL,      /* Final pulloff to clear switch */
    HOMING_COMPLETE,           /* Homing successful */
    HOMING_ERROR               /* Error state (aborted or failed) */
} homing_state_t;

/*! \brief Homing cycle axis mask (bitfield) */
typedef enum {
    HOMING_CYCLE_NONE = 0x00,
    HOMING_CYCLE_X = (1U << AXIS_X),   /* 0x01 */
    HOMING_CYCLE_Y = (1U << AXIS_Y),   /* 0x02 */
    HOMING_CYCLE_Z = (1U << AXIS_Z),   /* 0x04 */
    HOMING_CYCLE_A = (1U << AXIS_A),   /* 0x08 */
    HOMING_CYCLE_ALL_AXES = 0x0F       /* All axes */
} homing_cycle_mask_t;

/*! \brief Homing error codes */
typedef enum {
    HOMING_ERROR_NONE = 0,
    HOMING_ERROR_ABORTED,          /* User abort (soft reset) */
    HOMING_ERROR_TIMEOUT,          /* Limit switch not found */
    HOMING_ERROR_SWITCH_STUCK,     /* Switch still active after pulloff */
    HOMING_ERROR_INVALID_AXIS      /* Invalid axis in cycle mask */
} homing_error_code_t;

// *****************************************************************************
// Section: Public API
// *****************************************************************************

/*! \brief Initialize homing system
 *
 *  Must be called once at startup before using homing functions.
 *  Loads settings from motion_math ($23-$27).
 *
 *  \param get_limit_state Callback function to read limit switch states
 *                          Returns true if switch is triggered (closed/grounded)
 *
 *  Example callback:
 *    static bool GetLimitSwitchState(axis_id_t axis, bool positive_direction) {
 *        if (axis == AXIS_X && !positive_direction) {
 *            return !LIMIT_X_PIN_Get();  // Active LOW logic
 *        }
 *        return false;
 *    }
 */
void Homing_Initialize(bool (*get_limit_state)(axis_id_t axis, bool positive_direction));

/*! \brief Execute homing cycle for specified axes
 *
 *  Starts non-blocking state machine. Call Homing_Update() in main loop
 *  until Homing_IsActive() returns false.
 *
 *  \param axes Bitmask of axes to home (HOMING_CYCLE_X | HOMING_CYCLE_Y, etc.)
 *  \return true if cycle started, false if already active or invalid axes
 *
 *  Example:
 *    Homing_ExecuteCycle(HOMING_CYCLE_X | HOMING_CYCLE_Y);  // Home X and Y
 *    Homing_ExecuteCycle(HOMING_CYCLE_ALL_AXES);            // Home all axes
 */
bool Homing_ExecuteCycle(homing_cycle_mask_t axes);

/*! \brief Non-blocking state machine update
 *
 *  Must be called repeatedly in main loop while Homing_IsActive() is true.
 *  Advances state machine by one step per call.
 *
 *  \return Current homing state
 */
homing_state_t Homing_Update(void);

/*! \brief Check if homing cycle is active
 *
 *  \return true if state machine running, false if idle/complete/error
 */
bool Homing_IsActive(void);

/*! \brief Abort current homing cycle
 *
 *  Stops all motion and returns to idle state.
 *  Called by soft reset (Ctrl-X) handler.
 */
void Homing_Abort(void);

/*! \brief Set home position (G28.1 command)
 *
 *  Stores current machine position as predefined home (index 0).
 *  Does NOT move axes - just records position for later G28 command.
 */
void Homing_SetHomePosition(void);

/*! \brief Get current homing state
 *
 *  \return Current state machine state
 */
homing_state_t Homing_GetState(void);

/*! \brief Get last homing error code
 *
 *  \return Error code (HOMING_ERROR_NONE if no error)
 */
homing_error_code_t Homing_GetLastError(void);

/*! \brief Get homing settings from motion_math
 *
 *  Convenience wrappers for $23-$27 settings.
 *  
 *  \param axis Axis identifier
 *  \return Setting value in appropriate units
 */
float Homing_GetSeekRate(axis_id_t axis);    /* Fast search speed (mm/min) */
float Homing_GetFeedRate(axis_id_t axis);    /* Slow approach speed (mm/min) */
float Homing_GetPulloff(axis_id_t axis);     /* Backoff distance (mm) */
uint8_t Homing_GetCycleMask(void);           /* $23: Enabled axes bitmask */

/*! \brief Get limit switch inversion mask
 *
 *  \return $28 setting (bitmask of axes with inverted switches)
 */
uint8_t Homing_GetInvertMask(void);

#endif /* HOMING_H */

/*******************************************************************************
 End of File
*******************************************************************************/