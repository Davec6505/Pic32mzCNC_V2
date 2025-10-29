#ifndef MULTIAXIS_DRIVER_H
#define MULTIAXIS_DRIVER_H

// Function prototypes for multi-axis driver control


 void (*const en_set_funcs[NUM_AXES])(void);

 void (*const en_clear_funcs[NUM_AXES])(void);

bool (*const en_get_funcs[NUM_AXES])(void);

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

#endif // MULTIAXIS_DRIVER_H