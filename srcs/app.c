/*******************************************************************************
  Hardware Abstraction Layer for GRBL on PIC32MZ
  
  File: app.c
  
  Summary:
    Provides hardware interface for GRBL stepper control using OCR modules
    
  Description:
    This file contains the hardware-specific functions for the PIC32MZ CNC
    controller. It provides an interface between GRBL's stepper module and
    the PIC32MZ Output Compare (OCR) hardware for step pulse generation.
    
    Key Features:
    - OCR dual-compare continuous pulse mode for step generation
    - Hardware timers (TMR2/3/4/5) for OCR timebase
    - Direction control via GPIO pins
    - Limit switch interrupt handling
    - Position tracking via OCR callbacks
    
  Hardware Mapping:
    OCMP4 + TMR2 → X-axis step pulses
    OCMP5 + TMR3 → Z-axis step pulses  
    OCMP1 + TMR4 → Y-axis step pulses
    OCMP3 + TMR5 → A-axis step pulses (4th axis)
    
*******************************************************************************/

// *****************************************************************************
// Section: Included Files
// *****************************************************************************

#include "app.h"
#include "definitions.h"
#include "peripheral/uart/plib_uart2.h"
#include "peripheral/tmr1/plib_tmr1.h"
#include "peripheral/tmr/plib_tmr2.h"
#include "peripheral/tmr/plib_tmr3.h"
#include "peripheral/tmr/plib_tmr4.h"
#include "peripheral/tmr/plib_tmr5.h"
#include "peripheral/ocmp/plib_ocmp1.h"
#include "peripheral/ocmp/plib_ocmp3.h"
#include "peripheral/ocmp/plib_ocmp4.h"
#include "peripheral/ocmp/plib_ocmp5.h"
#include "peripheral/gpio/plib_gpio.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

// *****************************************************************************
// Section: Global Data
// *****************************************************************************

APP_DATA appData;

// CNC Axis hardware state
cnc_axis_t cnc_axes[MAX_AXES];

// OCR pulse width for DRV8825 (40 counts @ 1MHz = 40µs, well above 1.9µs minimum)
static const uint32_t OCR_PULSE_WIDTH = 40;

// Maximum OCR period (16-bit timer limit with safety margin)
static const uint32_t OCR_MAX_PERIOD = 65485;

// Steps per mm for each axis (from GRBL settings, default 250 for now)
static float steps_per_mm[MAX_AXES] = {250.0f, 250.0f, 250.0f, 250.0f};

// *****************************************************************************
// Section: OCR Callback Functions
// *****************************************************************************

// X-axis OCR callback - increments step count
void APP_OCMP4_Callback(uintptr_t context)
{
    if (cnc_axes[AXIS_X].direction_forward) {
        cnc_axes[AXIS_X].current_position++;
    } else {
        cnc_axes[AXIS_X].current_position--;
    }
    cnc_axes[AXIS_X].steps_executed++;
    
    // Check if target reached
    if (cnc_axes[AXIS_X].steps_executed >= cnc_axes[AXIS_X].steps_to_execute) {
        OCMP4_Disable();
        TMR2_Stop();
        cnc_axes[AXIS_X].is_moving = false;
    }
}

// Y-axis OCR callback
void APP_OCMP1_Callback(uintptr_t context)
{
    if (cnc_axes[AXIS_Y].direction_forward) {
        cnc_axes[AXIS_Y].current_position++;
    } else {
        cnc_axes[AXIS_Y].current_position--;
    }
    cnc_axes[AXIS_Y].steps_executed++;
    
    if (cnc_axes[AXIS_Y].steps_executed >= cnc_axes[AXIS_Y].steps_to_execute) {
        OCMP1_Disable();
        TMR4_Stop();
        cnc_axes[AXIS_Y].is_moving = false;
    }
}

// Z-axis OCR callback
void APP_OCMP5_Callback(uintptr_t context)
{
    if (cnc_axes[AXIS_Z].direction_forward) {
        cnc_axes[AXIS_Z].current_position++;
    } else {
        cnc_axes[AXIS_Z].current_position--;
    }
    cnc_axes[AXIS_Z].steps_executed++;
    
    if (cnc_axes[AXIS_Z].steps_executed >= cnc_axes[AXIS_Z].steps_to_execute) {
        OCMP5_Disable();
        TMR3_Stop();
        cnc_axes[AXIS_Z].is_moving = false;
    }
}

// A-axis OCR callback (4th axis)
void APP_OCMP3_Callback(uintptr_t context)
{
    if (cnc_axes[AXIS_A].direction_forward) {
        cnc_axes[AXIS_A].current_position++;
    } else {
        cnc_axes[AXIS_A].current_position--;
    }
    cnc_axes[AXIS_A].steps_executed++;
    
    if (cnc_axes[AXIS_A].steps_executed >= cnc_axes[AXIS_A].steps_to_execute) {
        OCMP3_Disable();
        TMR5_Stop();
        cnc_axes[AXIS_A].is_moving = false;
    }
}

// *****************************************************************************
// Section: OCR Hardware Control Functions
// *****************************************************************************

/**
 * Calculate OCR period from step rate (steps/second)
 * 
 * @param step_rate Steps per second
 * @return Timer period value (clamped to valid range)
 */
uint32_t APP_CalculateOCRPeriod(float step_rate)
{
    if (step_rate <= 0.0f) {
        return OCR_MAX_PERIOD;
    }
    
    // Timer runs at 1MHz, period = 1000000 / step_rate
    uint32_t period = (uint32_t)(1000000.0f / step_rate);
    
    // Clamp to valid range
    if (period > OCR_MAX_PERIOD) {
        period = OCR_MAX_PERIOD;
    }
    
    // Ensure period > pulse width
    if (period <= OCR_PULSE_WIDTH) {
        period = OCR_PULSE_WIDTH + 10;
    }
    
    return period;
}

/**
 * Start step pulse generation for an axis
 * 
 * @param axis Axis index (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 * @param steps Number of steps to execute
 * @param step_rate Steps per second
 * @param direction true = forward, false = reverse
 */
void APP_StartAxisMotion(uint8_t axis, uint32_t steps, float step_rate, bool direction)
{
    if (axis >= MAX_AXES) return;
    
    // Calculate OCR period
    uint32_t period = APP_CalculateOCRPeriod(step_rate);
    
    // Update axis state
    cnc_axes[axis].steps_to_execute = steps;
    cnc_axes[axis].steps_executed = 0;
    cnc_axes[axis].direction_forward = direction;
    cnc_axes[axis].is_moving = true;
    
    // Configure axis-specific hardware
    switch (axis) {
        case AXIS_X:
            // Set direction pin BEFORE enabling step pulses
            if (direction) {
                DirX_Set();
            } else {
                DirX_Clear();
            }
            
            // Configure OCR dual-compare mode
            TMR2_PeriodSet(period);
            OCMP4_CompareValueSet(period - OCR_PULSE_WIDTH);      // Rising edge
            OCMP4_CompareSecondaryValueSet(OCR_PULSE_WIDTH);      // Falling edge
            
            // Enable and start
            OCMP4_Enable();
            TMR2_Start();
            break;
            
        case AXIS_Y:
            if (direction) {
                DirY_Set();
            } else {
                DirY_Clear();
            }
            
            TMR4_PeriodSet(period);
            OCMP1_CompareValueSet(period - OCR_PULSE_WIDTH);
            OCMP1_CompareSecondaryValueSet(OCR_PULSE_WIDTH);
            
            OCMP1_Enable();
            TMR4_Start();
            break;
            
        case AXIS_Z:
            if (direction) {
                DirZ_Set();
            } else {
                DirZ_Clear();
            }
            
            TMR3_PeriodSet(period);
            OCMP5_CompareValueSet(period - OCR_PULSE_WIDTH);
            OCMP5_CompareSecondaryValueSet(OCR_PULSE_WIDTH);
            
            OCMP5_Enable();
            TMR3_Start();
            break;
            
        case AXIS_A:
            if (direction) {
                DirA_Set();
            } else {
                DirA_Clear();
            }
            
            TMR5_PeriodSet(period);
            OCMP3_CompareValueSet(period - OCR_PULSE_WIDTH);
            OCMP3_CompareSecondaryValueSet(OCR_PULSE_WIDTH);
            
            OCMP3_Enable();
            TMR5_Start();
            break;
    }
}

/**
 * Stop step pulse generation for an axis
 */
void APP_StopAxisMotion(uint8_t axis)
{
    if (axis >= MAX_AXES) return;
    
    switch (axis) {
        case AXIS_X:
            OCMP4_Disable();
            TMR2_Stop();
            break;
        case AXIS_Y:
            OCMP1_Disable();
            TMR4_Stop();
            break;
        case AXIS_Z:
            OCMP5_Disable();
            TMR3_Stop();
            break;
        case AXIS_A:
            OCMP3_Disable();
            TMR5_Stop();
            break;
    }
    
    cnc_axes[axis].is_moving = false;
}

/**
 * Emergency stop - disable all axes immediately
 */
void APP_EmergencyStop(void)
{
    for (uint8_t axis = 0; axis < MAX_AXES; axis++) {
        APP_StopAxisMotion(axis);
    }
}

// *****************************************************************************
// Section: Axis State Getters/Setters (Thread-Safe)
// *****************************************************************************

int32_t APP_GetAxisCurrentPosition(uint8_t axis)
{
    if (axis >= MAX_AXES) return 0;
    return cnc_axes[axis].current_position;
}

void APP_SetAxisCurrentPosition(uint8_t axis, int32_t position)
{
    if (axis >= MAX_AXES) return;
    cnc_axes[axis].current_position = position;
}

int32_t APP_GetAxisTargetPosition(uint8_t axis)
{
    if (axis >= MAX_AXES) return 0;
    return cnc_axes[axis].target_position;
}

void APP_SetAxisTargetPosition(uint8_t axis, int32_t position)
{
    if (axis >= MAX_AXES) return;
    cnc_axes[axis].target_position = position;
}

bool APP_IsAxisMoving(uint8_t axis)
{
    if (axis >= MAX_AXES) return false;
    return cnc_axes[axis].is_moving;
}

float APP_GetAxisStepsPerMM(uint8_t axis)
{
    if (axis >= MAX_AXES) return 250.0f;
    return steps_per_mm[axis];
}

void APP_SetAxisStepsPerMM(uint8_t axis, float steps_mm)
{
    if (axis >= MAX_AXES) return;
    steps_per_mm[axis] = steps_mm;
}

// *****************************************************************************
// Section: Limit Switch Handling
// *****************************************************************************

void APP_ProcessLimitSwitches(void)
{
    // Check X-axis limits (active low)
    bool x_neg_limit = !GPIO_PinRead(GPIO_PIN_RA7);
    bool x_pos_limit = !GPIO_PinRead(GPIO_PIN_RA9);
    
    if (x_neg_limit || x_pos_limit) {
        APP_StopAxisMotion(AXIS_X);
        // GRBL will handle the alarm
    }
    
    // Check Y-axis limits
    bool y_neg_limit = !GPIO_PinRead(GPIO_PIN_RA10);
    bool y_pos_limit = !GPIO_PinRead(GPIO_PIN_RA14);
    
    if (y_neg_limit || y_pos_limit) {
        APP_StopAxisMotion(AXIS_Y);
    }
    
    // Check Z-axis limits
    bool z_neg_limit = !GPIO_PinRead(GPIO_PIN_RA15);
    
    if (z_neg_limit) {
        APP_StopAxisMotion(AXIS_Z);
    }
}

// *****************************************************************************
// Section: Initialization
// *****************************************************************************

void APP_Initialize(void)
{
    appData.state = APP_STATE_INIT;
    
    // Initialize axis data structures
    for (uint8_t i = 0; i < MAX_AXES; i++) {
        cnc_axes[i].current_position = 0;
        cnc_axes[i].target_position = 0;
        cnc_axes[i].steps_to_execute = 0;
        cnc_axes[i].steps_executed = 0;
        cnc_axes[i].direction_forward = true;
        cnc_axes[i].is_moving = false;
    }
    
    // Register OCR callbacks
    OCMP4_CallbackRegister(APP_OCMP4_Callback, (uintptr_t)NULL);
    OCMP1_CallbackRegister(APP_OCMP1_Callback, (uintptr_t)NULL);
    OCMP5_CallbackRegister(APP_OCMP5_Callback, (uintptr_t)NULL);
    OCMP3_CallbackRegister(APP_OCMP3_Callback, (uintptr_t)NULL);
    
    // Initialize limit switch GPIOs (already configured by MCC)
    // Pins are active low with internal pull-ups
    
    appData.state = APP_STATE_SERVICE_TASKS;
}

void APP_Tasks(void)
{
    switch (appData.state) {
        case APP_STATE_INIT:
            // Initialization done in APP_Initialize
            break;
            
        case APP_STATE_SERVICE_TASKS:
            // Check limit switches periodically
            APP_ProcessLimitSwitches();
            
            // Heartbeat LED (optional)
            static uint32_t led_counter = 0;
            if (++led_counter > 100000) {
                LED2_Toggle();
                led_counter = 0;
            }
            break;
            
        default:
            break;
    }
}
