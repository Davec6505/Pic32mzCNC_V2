
#ifndef MOTION_DRIVER_H
#define MOTION_DRIVER_H


#include "motion_types.h"
#include "definitions.h"

#include <stdint.h>
#include <stdbool.h>


// CRITICAL FIX (Oct 20, 2025): Define NOP for bit-bang delays
// XC32 doesn't have __NOP(), use inline assembly instead
#define NOP() __asm__ __volatile__("nop")



// OCR/Timer assignments per PIC32MZ hardware
typedef struct
{
    void (*OCMP_Enable)(void);
    void (*OCMP_Disable)(void);
    void (*OCMP_CompareValueSet)(uint16_t);
    void (*OCMP_CompareSecondaryValueSet)(uint16_t);
    void (*OCMP_CallbackRegister)(void (*)(uintptr_t), uintptr_t);
    void (*TMR_Start)(void);
    void (*TMR_Stop)(void);
    void (*TMR_PeriodSet)(uint16_t);
} axis_hardware_t;



// axis_hardware_t array for all axes
extern const axis_hardware_t axis_hw[NUM_AXES];

extern volatile bool driver_enabled[NUM_AXES];
extern volatile bool axis_was_dominant_last_isr[NUM_AXES];

// ******************************************************************************
// Drive Control Pin Macros (Dynamic Function Pointer Lookup)
// *****************************************************************************/
// Wrapper function array prototypes for enable pins
extern void (*const en_set_funcs[NUM_AXES])(void);
extern void (*const en_clear_funcs[NUM_AXES])(void);
extern bool (*const en_get_funcs[NUM_AXES])(void);


// *****************************************************************************
// Direction pin control Pin Macros (Dynamic Function Pointer Lookup)
// *****************************************************************************/
extern void (*const dir_set_funcs[NUM_AXES])(void);
extern void (*const dir_clear_funcs[NUM_AXES])(void);


// *****************************************************************************
// Inline functions for compiler optimization
// *****************************************************************************/

/*! \brief Set direction for specified axis using dynamic lookup force inline
 * using this function call otherwise usage of Multiaxis_SetDirection() incurs
 * CPU overhead.
 *
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 *  MISRA Rule 1.3: Validate function pointer before call
 */
static inline void MotionDriver_SetDirection(axis_id_t axis, bool forward) __attribute__((always_inline));
static inline void MotionDriver_SetDirection(axis_id_t axis, bool forward)
{
    /* MISRA Rule 17.4: Bounds checking before array access */
    if ((axis < 0) || (axis >= NUM_AXES))
    {
        return;
    }
    /* MISRA Rule 1.3: Validate function pointer before call */
    if (forward)
    {
        if (dir_set_funcs[axis] != NULL)
        {
            dir_set_funcs[axis]();
        }
    }
    else
    {
        if (dir_clear_funcs[axis] != NULL)
        {
            dir_clear_funcs[axis]();
        }
    }
}


/*! \brief Set direction for specified axis using dynamic lookup
 *
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 *  MISRA Rule 1.3: Validate function pointer before call
 */
static inline void MotionDriver_ClearDirection(axis_id_t axis) __attribute__((always_inline));
static inline void MotionDriver_ClearDirection(axis_id_t axis)
{
    /* MISRA Rule 17.4: Bounds checking before array access */
    if ((axis < 0) || (axis >= NUM_AXES))
    {
        return;
    }
    /* MISRA Rule 1.3: Validate function pointer before call */
    if (dir_clear_funcs[axis] != NULL)
    {
        dir_clear_funcs[axis]();
    }
}


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
static inline void MotionDriver_EnableDriver(axis_id_t axis) __attribute__((always_inline));
static inline void MotionDriver_EnableDriver(axis_id_t axis)
{
    /* MISRA Rule 17.4: Bounds checking before array access */
    if ((axis < 0) || (axis >= NUM_AXES))
    {
        return;
    }
    /* MISRA Rule 1.3: Validate function pointer before call */
    if (en_clear_funcs[axis] == NULL)
    {
        return;
    }
    if (!driver_enabled[axis]) /* Only enable if currently disabled */
    {
        en_clear_funcs[axis](); /* ACTIVE LOW: Clear = Enable (pin goes LOW) */
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
static inline void MotionDriver_DisableDriver(axis_id_t axis) __attribute__((always_inline));
static inline void MotionDriver_DisableDriver(axis_id_t axis)
{
    /* MISRA Rule 17.4: Bounds checking before array access */
    if ((axis < 0) || (axis >= NUM_AXES))
    {
        return;
    }
    /* MISRA Rule 1.3: Validate function pointer before call */
    if (en_set_funcs[axis] == NULL)
    {
        return;
    }
    if (driver_enabled[axis]) /* Only disable if currently enabled */
    {
        en_set_funcs[axis](); /* ACTIVE LOW: Set = Disable (pin goes HIGH) */
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

static inline bool MotionDriver_IsDriverEnabled(axis_id_t axis) __attribute__((always_inline));
static inline bool MotionDriver_IsDriverEnabled(axis_id_t axis)
{
    /* MISRA Rule 17.4: Bounds checking before array access */
    if ((axis < 0) || (axis >= NUM_AXES))
    {
        /* Optionally: assert or error handler */
        return false;
    }
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

static inline bool MotionDriver_ReadEnablePin(axis_id_t axis) __attribute__((always_inline));
static inline bool MotionDriver_ReadEnablePin(axis_id_t axis)
{
    /* MISRA Rule 17.4: Bounds checking before array access */
    if ((axis < 0) || (axis >= NUM_AXES))
    {
        /* Optionally: assert or error handler */
        return false;
    }
    /* MISRA Rule 1.3: Validate function pointer before call */
    if (en_get_funcs[axis] == NULL)
    {
        return false;
    }
    return en_get_funcs[axis]();
}



#endif // MOTION_DRIVER_H
// *****************************************************************************