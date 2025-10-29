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








#endif // MOTION_DRIVER_H
// *****************************************************************************