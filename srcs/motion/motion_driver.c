

#include <assert.h>
#include "motion/motion_driver.h"


// Hardware configuration table
const axis_hardware_t axis_hw[NUM_AXES] = {
    // AXIS_X: OCMP5 + TMR3 (CORRECTED - per hardware wiring)
    {
        .OCMP_Enable = OCMP5_Enable,
        .OCMP_Disable = OCMP5_Disable,
        .OCMP_CompareValueSet = OCMP5_CompareValueSet,
        .OCMP_CompareSecondaryValueSet = OCMP5_CompareSecondaryValueSet,
        .OCMP_CallbackRegister = OCMP5_CallbackRegister,
        .TMR_Start = TMR3_Start,
        .TMR_Stop = TMR3_Stop,
        .TMR_PeriodSet = TMR3_PeriodSet},
    // AXIS_Y: OCMP1 + TMR4 (CORRECT - per hardware wiring)
    {
        .OCMP_Enable = OCMP1_Enable,
        .OCMP_Disable = OCMP1_Disable,
        .OCMP_CompareValueSet = OCMP1_CompareValueSet,
        .OCMP_CompareSecondaryValueSet = OCMP1_CompareSecondaryValueSet,
        .OCMP_CallbackRegister = OCMP1_CallbackRegister,
        .TMR_Start = TMR4_Start,
        .TMR_Stop = TMR4_Stop,
        .TMR_PeriodSet = TMR4_PeriodSet},
    // AXIS_Z: OCMP4 + TMR2 (CORRECTED - per hardware wiring)
    {
        .OCMP_Enable = OCMP4_Enable,
        .OCMP_Disable = OCMP4_Disable,
        .OCMP_CompareValueSet = OCMP4_CompareValueSet,
        .OCMP_CompareSecondaryValueSet = OCMP4_CompareSecondaryValueSet,
        .OCMP_CallbackRegister = OCMP4_CallbackRegister,
        .TMR_Start = TMR2_Start,
        .TMR_Stop = TMR2_Stop,
        .TMR_PeriodSet = TMR2_PeriodSet},
    // AXIS_A: OCMP3 + TMR5 (CORRECT - per hardware wiring)
    {
        .OCMP_Enable = OCMP3_Enable,
        .OCMP_Disable = OCMP3_Disable,
        .OCMP_CompareValueSet = OCMP3_CompareValueSet,
        .OCMP_CompareSecondaryValueSet = OCMP3_CompareSecondaryValueSet,
        .OCMP_CallbackRegister = OCMP3_CallbackRegister,
        .TMR_Start = TMR5_Start,
        .TMR_Stop = TMR5_Stop,
        .TMR_PeriodSet = TMR5_PeriodSet}};
#include "motion/motion_driver.h"
#include "motion/motion_math.h"
#include "definitions.h"

#include <math.h>



// ******************************************************************************
// Drive Control Pin Macros (Dynamic Function Pointer Lookup)
// *****************************************************************************/
// Wrapper functions for direction pin macros (macros can't be used as function pointers)
// Direction pin wrappers
void dirx_set_wrapper(void) { DirX_Set(); }
void dirx_clear_wrapper(void) { DirX_Clear(); }
void diry_set_wrapper(void) { DirY_Set(); }
void diry_clear_wrapper(void) { DirY_Clear(); }
void dirz_set_wrapper(void) { DirZ_Set(); }
void dirz_clear_wrapper(void) { DirZ_Clear(); }
#ifdef ENABLE_AXIS_A
void dira_set_wrapper(void) { DirA_Set(); }
void dira_clear_wrapper(void) { DirA_Clear(); }
#endif

// Direction set function pointer array
void (*const dir_set_funcs[NUM_AXES])(void) = {
    dirx_set_wrapper, // AXIS_X
    diry_set_wrapper, // AXIS_Y
    dirz_set_wrapper, // AXIS_Z
#ifdef ENABLE_AXIS_A
    dira_set_wrapper, // AXIS_A
#endif
};

// Direction clear function pointer array
void (*const dir_clear_funcs[NUM_AXES])(void) = {
    dirx_clear_wrapper, // AXIS_X
    diry_clear_wrapper, // AXIS_Y
    dirz_clear_wrapper, // AXIS_Z
#ifdef ENABLE_AXIS_A
    dira_clear_wrapper, // AXIS_A
#endif
};
void enx_set_wrapper(void) { EnX_Set(); }
void enx_clear_wrapper(void) { EnX_Clear(); }
bool enx_get_wrapper(void) { return EnX_Get(); }
void eny_set_wrapper(void) { EnY_Set(); }
void eny_clear_wrapper(void) { EnY_Clear(); }
bool eny_get_wrapper(void) { return EnY_Get(); }
void enz_set_wrapper(void) { EnZ_Set(); }
void enz_clear_wrapper(void) { EnZ_Clear(); }
bool enz_get_wrapper(void) { return EnZ_Get(); }
#ifdef ENABLE_AXIS_A
void ena_set_wrapper(void) { EnA_Set(); }
void ena_clear_wrapper(void) { EnA_Clear(); }
bool ena_get_wrapper(void) { return EnA_Get(); }
#endif

void (*const en_set_funcs[NUM_AXES])(void) = {
    enx_set_wrapper, // AXIS_X
    eny_set_wrapper, // AXIS_Y
    enz_set_wrapper, // AXIS_Z
#ifdef ENABLE_AXIS_A
    ena_set_wrapper, // AXIS_A
#endif
};

void (*const en_clear_funcs[NUM_AXES])(void) = {
    enx_clear_wrapper, // AXIS_X
    eny_clear_wrapper, // AXIS_Y
    enz_clear_wrapper, // AXIS_Z
#ifdef ENABLE_AXIS_A
    ena_clear_wrapper, // AXIS_A
#endif
};

bool (*const en_get_funcs[NUM_AXES])(void) = {
    enx_get_wrapper, // AXIS_X
    eny_get_wrapper, // AXIS_Y
    enz_get_wrapper, // AXIS_Z
#ifdef ENABLE_AXIS_A
    ena_get_wrapper, // AXIS_A
#endif
};

volatile bool driver_enabled[NUM_AXES] = {false, false, false, false};
volatile bool axis_was_dominant_last_isr[NUM_AXES] = {false, false, false, false};

// *****************************************************************************
// Dominant Axis Transition State Tracking (ISR-Safe)
// *****************************************************************************

/**
 * @brief Per-axis transition detection state
 * 
 * Tracks whether each axis was dominant in the previous ISR cycle.
 * Used to detect transitions between dominant/subordinate roles without
 * wasteful operations every ISR.
 * 
 * @note volatile for ISR safety (prevents compiler optimization)
 * @note Initialized to false in MultiAxis_Initialize()
 */
// (Removed duplicate definition; only extern in header)

/*! \brief Enable stepper driver for specified axis using dynamic lookup
 *
 *  DRV8825 ENABLE pin is ACTIVE LOW:
 *    - LOW = Driver enabled (motor powered, torque applied)
 *    - HIGH = Driver disabled (motor unpowered, high-Z state)
 *
 *  This function makes the ENABLE pin LOW to activate the driver.
 *
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 *  MISRA Rule 1.3: Validate function pointer before call
 */
void MultiAxis_EnableDriver(axis_id_t axis)
{
    assert(axis < NUM_AXES); // Development-time check

    if (axis >= NUM_AXES || en_clear_funcs[axis] == NULL)
    {
        return; // Production safety check
    }

    if (!driver_enabled[axis]) // Only enable if currently disabled
    {
        en_clear_funcs[axis](); // ACTIVE LOW: Clear = Enable (pin goes LOW)
        driver_enabled[axis] = true;
    }
}

/*! \brief Disable stepper driver for specified axis using dynamic lookup
 *
 *  DRV8825 ENABLE pin is ACTIVE LOW:
 *    - LOW = Driver enabled (motor powered, torque applied)
 *    - HIGH = Driver disabled (motor unpowered, high-Z state)
 *
 *  This function makes the ENABLE pin HIGH to deactivate the driver.
 *
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 *  MISRA Rule 1.3: Validate function pointer before call
 */
void MultiAxis_DisableDriver(axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES || en_set_funcs[axis] == NULL)
    {
        return;
    }

    if (driver_enabled[axis]) // Only disable if currently enabled
    {
        en_set_funcs[axis](); // ACTIVE LOW: Set = Disable (pin goes HIGH)
        driver_enabled[axis] = false;
    }
}

/*! \brief Check if stepper driver is enabled for specified axis
 *
 *  Returns the software-tracked enable state (fast, no GPIO read).
 *
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 *  \return true if driver is enabled, false if disabled (LOGICAL STATE)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 */
bool MultiAxis_IsDriverEnabled(axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES)
        return false;

    return driver_enabled[axis];
}

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
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 *  \return Raw GPIO pin state (true = HIGH, false = LOW)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 *  MISRA Rule 1.3: Validate function pointer before call
 */

bool MultiAxis_ReadEnablePin(axis_id_t axis)
{
    // Defensive programming: validate parameters
    assert(axis < NUM_AXES);            // Development-time check
    assert(en_get_funcs[axis] != NULL); // Verify function pointer valid

    if ((axis < NUM_AXES) && (en_get_funcs[axis] != NULL))
    {
        return en_get_funcs[axis]();
    }

    return false; // Default return value
}



/*! \brief Set direction for specified axis using dynamic lookup
 *
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 *  MISRA Rule 1.3: Validate function pointer before call
 */
void MultiAxis_SetDirection(axis_id_t axis)
{
    // Defensive programming: validate parameters
    assert(axis < NUM_AXES);             // Development-time check
    assert(dir_set_funcs[axis] != NULL); // Verify function pointer valid

    if ((axis < NUM_AXES) && (dir_set_funcs[axis] != NULL))
    {
        dir_set_funcs[axis]();
    }
}

/*! \brief Clear direction for specified axis using dynamic lookup
 *
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 *  MISRA Rule 1.3: Validate function pointer before call
 */
void MultiAxis_ClearDirection(axis_id_t axis)
{
    // Defensive programming: validate parameters
    assert(axis < NUM_AXES);               // Development-time check
    assert(dir_clear_funcs[axis] != NULL); // Verify function pointer valid

    if ((axis < NUM_AXES) && (dir_clear_funcs[axis] != NULL))
    {
        dir_clear_funcs[axis]();
    }
}


