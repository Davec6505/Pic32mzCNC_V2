/*******************************************************************************
  Hardware Abstraction Layer for GRBL on PIC32MZ
  
  File: app.h
  
  Summary:
    Hardware interface definitions for GRBL stepper control
    
  Description:
    Provides function prototypes and data structures for hardware control
    of the PIC32MZ CNC controller using OCR pulse generation.
    
*******************************************************************************/

#ifndef _APP_H
#define _APP_H

// *****************************************************************************
// Section: Included Files
// *****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include "configuration.h"
#include "definitions.h"

// Provide C++ Compatibility
#ifdef __cplusplus
extern "C" {
#endif

// *****************************************************************************
// Section: Type Definitions
// *****************************************************************************

// Application states
typedef enum
{
    APP_STATE_INIT=0,
    APP_STATE_SERVICE_TASKS,
} APP_STATES;

// Application data structure
typedef struct
{
    APP_STATES state;
} APP_DATA;

// Axis indices
#define AXIS_X  0
#define AXIS_Y  1
#define AXIS_Z  2
#define AXIS_A  3  // 4th axis
#define MAX_AXES 4

// CNC axis hardware state
typedef struct {
    volatile int32_t current_position;   // Current position in steps
    volatile int32_t target_position;    // Target position in steps
    volatile uint32_t steps_to_execute;  // Number of steps for current move
    volatile uint32_t steps_executed;    // Steps completed so far
    volatile bool direction_forward;      // Direction: true=forward, false=reverse
    volatile bool is_moving;             // Motion in progress flag
} cnc_axis_t;

// *****************************************************************************
// Section: Application Callback Routines
// *****************************************************************************

// OCR callbacks (called from interrupt context)
void APP_OCMP4_Callback(uintptr_t context);  // X-axis
void APP_OCMP1_Callback(uintptr_t context);  // Y-axis
void APP_OCMP5_Callback(uintptr_t context);  // Z-axis
void APP_OCMP3_Callback(uintptr_t context);  // A-axis

// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************

void APP_Initialize(void);
void APP_Tasks(void);

// *****************************************************************************
// Section: OCR Hardware Control Functions
// *****************************************************************************

/**
 * Calculate OCR period from step rate
 * @param step_rate Steps per second
 * @return Timer period value
 */
uint32_t APP_CalculateOCRPeriod(float step_rate);

/**
 * Start step pulse generation for an axis
 * @param axis Axis index (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 * @param steps Number of steps to execute
 * @param step_rate Steps per second
 * @param direction true = forward, false = reverse
 */
void APP_StartAxisMotion(uint8_t axis, uint32_t steps, float step_rate, bool direction);

/**
 * Stop step pulse generation for an axis
 * @param axis Axis index
 */
void APP_StopAxisMotion(uint8_t axis);

/**
 * Emergency stop - disable all axes immediately
 */
void APP_EmergencyStop(void);

// *****************************************************************************
// Section: Axis State Getters/Setters
// *****************************************************************************

int32_t APP_GetAxisCurrentPosition(uint8_t axis);
void APP_SetAxisCurrentPosition(uint8_t axis, int32_t position);
int32_t APP_GetAxisTargetPosition(uint8_t axis);
void APP_SetAxisTargetPosition(uint8_t axis, int32_t position);
bool APP_IsAxisMoving(uint8_t axis);

float APP_GetAxisStepsPerMM(uint8_t axis);
void APP_SetAxisStepsPerMM(uint8_t axis, float steps_mm);

// *****************************************************************************
// Section: Limit Switch Functions
// *****************************************************************************

/**
 * Check limit switches and stop axes if triggered
 * Called periodically from main loop
 */
void APP_ProcessLimitSwitches(void);

// *****************************************************************************
// Section: Global Data
// *****************************************************************************

extern APP_DATA appData;
extern cnc_axis_t cnc_axes[MAX_AXES];

// Provide C++ Compatibility
#ifdef __cplusplus
}
#endif

#endif /* _APP_H */
