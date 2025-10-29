#ifndef MOTION_DRIVER_H
#define MOTION_DRIVER_H


#include "motion_types.h"
#include "config/default/peripheral/gpio/plib_gpio.h"
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


// ******************************************************************************
// Drive Control Pin Macros (Dynamic Function Pointer Lookup)
// *****************************************************************************/
// Wrapper function prototypes for enable pins
void enx_set_wrapper(void);
void enx_clear_wrapper(void);
bool enx_get_wrapper(void);
void eny_set_wrapper(void);
void eny_clear_wrapper(void);
bool eny_get_wrapper(void);
void enz_set_wrapper(void);
void enz_clear_wrapper(void);
bool enz_get_wrapper(void);
#ifdef ENABLE_AXIS_A
void ena_set_wrapper(void);
void ena_clear_wrapper(void);
bool ena_get_wrapper(void);
#endif

extern void (*const en_set_funcs[NUM_AXES])(void);
extern void (*const en_clear_funcs[NUM_AXES])(void);
extern bool (*const en_get_funcs[NUM_AXES])(void);


// *****************************************************************************
// Direction pin control Pin Macros (Dynamic Function Pointer Lookup)
// *****************************************************************************/
// *****************************************************************************/
// Wrapper function prototypes for direction pins
void dirx_set_wrapper(void);
void dirx_clear_wrapper(void);
void diry_set_wrapper(void);
void diry_clear_wrapper(void);
void dirz_set_wrapper(void);
void dirz_clear_wrapper(void);
#ifdef ENABLE_AXIS_A
void dira_set_wrapper(void);
void dira_clear_wrapper(void);
#endif


extern void (*const dir_set_funcs[NUM_AXES])(void);
extern void (*const dir_clear_funcs[NUM_AXES])(void);
extern volatile bool driver_enabled[NUM_AXES];
extern volatile bool axis_was_dominant_last_isr[NUM_AXES];






#endif // MOTION_DRIVER_H
// *****************************************************************************