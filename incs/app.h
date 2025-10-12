/*******************************************************************************
  MPLAB Harmony Application Header File

  Company:
    Microchip Technology Inc.

  File Name:
    app.h

  Summary:
    This header file provides prototypes and definitions for the application.

  Description:
    This header file provides function prototypes and data type definitions for
    the application.  Some of these are required by the system (such as the
    "APP_Initialize" and "APP_Tasks" prototypes) and some of them are only used
    internally by the application (such as the "APP_STATES" definition).  Both
    are defined here for convenience.
*******************************************************************************/

#ifndef _APP_H
#define _APP_H

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include "definitions.h"

// Public function prototypes
void APP_UARTPrint_blocking(const char *str);
void APP_UARTPrint(const char *str);
void APP_UARTWrite_nonblocking(const char *str); // New non-blocking write
bool APP_AddLinearMove(float *target, float feedrate);
bool APP_AddRapidMove(float *target);
bool APP_IsMotionComplete(void);
void APP_EmergencyStop(void);
void APP_EmergencyReset(void);
void APP_AlarmReset(void);
void APP_StartHomingCycle(void);
void APP_SetPickAndPlaceMode(bool enable);
bool APP_IsPickAndPlaceMode(void);

// Temporarily commented out for UART debug
// #include "speed_control.h"
// #include "motion_profile.h"

// *****************************************************************************
// *****************************************************************************
// Section: Configuration Defines
// *****************************************************************************
// *****************************************************************************

// Enable arc interpolation testing - comment out to disable
#define TEST_ARC_INTERPOLATION

// *****************************************************************************
// Limit Switch Pin Assignments
// *****************************************************************************
/*
 * Limit switch GPIO pins are defined in plib_gpio.h
 * Pin assignments for this CNC controller (active low):
 * GPIO_PIN_RA7  - X-axis negative limit
 * GPIO_PIN_RA9  - X-axis positive limit
 * GPIO_PIN_RA10 - Y-axis negative limit
 * GPIO_PIN_RA14 - Y-axis positive limit
 * GPIO_PIN_RA15 - Z-axis negative limit
 * (Z-axis positive limit requires additional GPIO configuration)
 */

// *****************************************************************************
// *****************************************************************************
// Section: Type Definitions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Application states

  Summary:
    Application states enumeration

  Description:
    This enumeration defines the valid application states.  These states
    determine the behavior of the application at various times.
*/

typedef enum
{
  /* Application's state machine's initial state. */
  APP_STATE_INIT = 0,
  APP_STATE_SERVICE_TASKS,
  APP_STATE_GCODE_INIT,
  APP_STATE_MOTION_INIT,
  APP_STATE_MOTION_IDLE,
  APP_STATE_MOTION_PLANNING,
  APP_STATE_MOTION_EXECUTING,
  APP_STATE_MOTION_ERROR,
} APP_STATES;

// *****************************************************************************
/* CNC Axis Motion States

  Summary:
    Motion states for each CNC axis

  Description:
    This enumeration defines the motion states for trajectory planning
*/

typedef enum
{
  AXIS_IDLE,
  AXIS_ACCEL,
  AXIS_CONSTANT,
  AXIS_DECEL,
  AXIS_COMPLETE
} axis_state_t;

// *****************************************************************************
/* CNC Axis Data Structure

  Summary:
    Contains all data for a single axis motion control

  Description:
    This structure holds position, velocity, and acceleration data for
    real-time trajectory calculation and stepper motor control
*/

typedef struct
{
  // Position tracking
  int32_t current_position; // Current position in steps
  int32_t target_position;  // Target position in steps

  // Velocity profile (steps/sec)
  float current_velocity; // Current velocity
  float target_velocity;  // Target velocity for this move
  float max_velocity;     // Maximum allowed velocity

  // Acceleration parameters (steps/sec²)
  float acceleration; // Acceleration rate
  float deceleration; // Deceleration rate

  // State management
  axis_state_t motion_state; // Current motion state
  bool direction_forward;    // Direction flag
  bool is_active;            // Axis is currently moving

  // Hardware control
  uint16_t ocr_period; // Current OCR period value
  uint32_t step_count; // Steps completed in current move

  // Look-ahead support
  float junction_velocity; // Velocity at path junctions
  bool buffer_active;      // Motion buffer is active

} cnc_axis_t;

// *****************************************************************************
/* Motion Planning Buffer

  Summary:
    Buffer for look-ahead motion planning

  Description:
    This structure holds motion commands for advanced trajectory planning
    with look-ahead capability for smooth path execution
*/

typedef struct
{
  float target_pos[3];  // Target coordinates [X, Y, Z]
  float feedrate;       // Requested feedrate (steps/sec)
  float entry_velocity; // Calculated entry velocity
  float exit_velocity;  // Calculated exit velocity
  float max_velocity;   // Maximum velocity for this segment
  float distance;       // Total move distance
  float duration;       // Estimated move duration (seconds)
  uint8_t motion_type;  // Motion type (G0, G1, etc.)
  bool is_valid;        // Buffer entry is valid
} motion_block_t;

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    Application strings and buffers are be defined outside this structure.
 */

typedef struct
{
  /* The application's current state */
  APP_STATES state;

  /* Motion control system status */
  bool motion_system_ready;
  bool trajectory_timer_active;
  uint8_t limit_switch_state;
  uint32_t last_switch_time;
  uint32_t last_heartbeat_time;
  uint32_t last_motion_time;

  /* User interface */
  bool switch_pressed;
  uint32_t switch_debounce_timer;

  /* System timing */
  uint32_t system_tick_counter;

  /* UART receive buffer */
  char uart_rx_buffer[256];
  uint16_t uart_rx_buffer_pos;

} APP_DATA;

// *****************************************************************************
// *****************************************************************************
// Section: Application Constants
// *****************************************************************************
// *****************************************************************************

#define MAX_AXES 3                   // X, Y, Z axes
#define MOTION_BUFFER_SIZE 16        // Look-ahead buffer size (16-command look-ahead)
#define SWITCH_DEBOUNCE_MS 100       // Switch debounce time
#define TRAJECTORY_TIMER_FREQ 1000   // 1kHz trajectory calculation
#define DEFAULT_ACCELERATION 500.0f  // Default acceleration (steps/sec²)
#define DEFAULT_MAX_VELOCITY 1000.0f // Default max velocity (steps/sec)

// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Function Prototypes
// *****************************************************************************
// *****************************************************************************

/* Timer callback prototypes */
void APP_TrajectoryTimerCallback_CoreTimer(uint32_t status, uintptr_t context);
void APP_CoreTimerCallback(uint32_t status, uintptr_t context);

/* OCR callback prototypes */
void APP_OCMP1_Callback(uintptr_t context);
void APP_OCMP4_Callback(uintptr_t context);
void APP_OCMP5_Callback(uintptr_t context);

// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Summary:
     MPLAB Harmony application initialization routine.

  Description:
    This function initializes the Harmony application.  It places the
    application in its initial state and prepares it to run so that its
    APP_Tasks function can be called.

  Precondition:
    All other system initialization routines should be called before calling
    this routine (in "SYS_Initialize").

  Parameters:
    None.

  Returns:
    None.

  Example:
    <code>
    APP_Initialize();
    </code>

  Remarks:
    This routine must be called from the SYS_Initialize function.
*/

void APP_Initialize(void);

/*******************************************************************************
  Function:
    void APP_Tasks ( void )

  Summary:
    MPLAB Harmony Demo application tasks function

  Description:
    This routine is the Harmony Demo application's tasks function.  It
    defines the application's state machine and core logic.

  Precondition:
    The system and application initialization ("APP_Initialize") should be
    called before calling this.

  Parameters:
    None.

  Returns:
    None.

  Example:
    <code>
    APP_Tasks();
    </code>

  Remarks:
    This routine must be called from SYS_Tasks() routine.
 */

void APP_Tasks(void);

// *****************************************************************************
// *****************************************************************************
// Section: Motion Control Function Prototypes
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    bool APP_AddLinearMove(float *target, float feedrate)

  Summary:
    Add a linear move to the motion planning buffer

  Description:
    This function adds a linear move command to the motion planning buffer
    for look-ahead trajectory optimization.

  Parameters:
    target    - Array of target positions [X, Y, Z] in steps
    feedrate  - Desired feedrate in steps/sec

  Returns:
    true if move was added successfully, false if buffer is full

  Remarks:
    This function is non-blocking and adds moves to a buffer for execution
*/

bool APP_AddLinearMove(float *target, float feedrate);

/*******************************************************************************
  Function:
    bool APP_AddRapidMove(float *target)

  Summary:
    Add a rapid move (G0) to the motion planning buffer

  Description:
    This function adds a rapid positioning move at maximum velocity

  Parameters:
    target    - Array of target positions [X, Y, Z] in steps

  Returns:
    true if move was added successfully, false if buffer is full
*/

bool APP_AddRapidMove(float *target);

/*******************************************************************************
  Function:
    bool APP_IsMotionComplete(void)

  Summary:
    Check if all motion is complete

  Description:
    This function checks if all axes have completed their current moves

  Returns:
    true if all motion is complete, false if any axis is still moving
*/

bool APP_IsMotionComplete(void);

/*******************************************************************************
  Function:
    void APP_EmergencyStop(void)

  Summary:
    Emergency stop all motion

  Description:
    This function immediately stops all motion and clears the motion buffer
*/

void APP_EmergencyStop(void);

/*******************************************************************************
  Function:
    void APP_AlarmReset(void)

  Summary:
    Reset alarm state after limit switch trigger

  Description:
    This function clears the alarm state after verifying that all limit
    switches are released. Used to recover from hard limit triggers.
*/

void APP_AlarmReset(void);

/*******************************************************************************
  Function:
    void APP_StartHomingCycle(void)

  Summary:
    Start homing cycle for all configured axes

  Description:
    This function initiates a GRBL-compatible homing cycle for X, Y, and Z axes.
    The cycle includes seek phase (fast move to limit), locate phase (slow precise
    positioning), and pulloff phase (move away from limit switch).
*/

void APP_StartHomingCycle(void);

/*******************************************************************************
  Function:
    void APP_ExecuteGcodeCommand(gcode_parsed_line_t *command)

  Summary:
    Execute a parsed G-code command

  Description:
    This function is called by the DMA G-code parser when a complete
    command has been received and parsed. It processes G and M codes
    and executes the appropriate motion commands.

  Parameters:
    command - Pointer to parsed G-code command structure

  Returns:
    None (responses sent via DMA parser)
*/

void APP_ExecuteMotionCommand(const char *command);

/*******************************************************************************
  Function:
    bool APP_ExecuteMotionBlock(motion_block_t *block)

  Summary:
    Execute a motion block with actual OCR hardware

  Description:
    This function is called by the motion planner to execute a motion block
    using the actual OCR hardware modules. It starts the step pulse generation
    and position feedback for real motion execution.

  Parameters:
    block - Pointer to the motion block to execute

  Returns:
    true if motion block was successfully started, false otherwise
*/

bool APP_ExecuteMotionBlock(motion_block_t *block);

/*******************************************************************************
  Function:
    void APP_HandleEmergencyStop(void)

  Summary:
    Handle emergency stop from G-code parser

  Description:
    This function is called when an emergency stop command (Ctrl-X)
    is received via the DMA G-code parser.
*/

void APP_HandleEmergencyStop(void);

// *****************************************************************************
// *****************************************************************************
// Section: Hardware Layer Getter/Setter Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    int32_t APP_GetAxisCurrentPosition(uint8_t axis)

  Summary:
    Gets the current position for specified axis from hardware layer.

  Description:
    Returns the current position in steps for the specified axis.
    Provides clean interface to access hardware axis position data.

  Parameters:
    axis - Axis index (0=X, 1=Y, 2=Z)

  Returns:
    Current position in steps, or 0 if axis invalid
*******************************************************************************/
int32_t APP_GetAxisCurrentPosition(uint8_t axis);

/*******************************************************************************
  Function:
    void APP_SetAxisCurrentPosition(uint8_t axis, int32_t position)

  Summary:
    Sets the current position for specified axis in hardware layer.

  Description:
    Updates the current position in steps for the specified axis.
    Used for coordinate system updates and homing operations.

  Parameters:
    axis - Axis index (0=X, 1=Y, 2=Z)
    position - Position in steps

  Returns:
    None
*******************************************************************************/
void APP_SetAxisCurrentPosition(uint8_t axis, int32_t position);

/*******************************************************************************
  Function:
    bool APP_GetAxisActiveState(uint8_t axis)

  Summary:
    Gets the active state for specified axis.

  Description:
    Returns true if the specified axis is currently active in hardware.
    Provides clean interface to check hardware motion state.

  Parameters:
    axis - Axis index (0=X, 1=Y, 2=Z)

  Returns:
    true if axis is active, false otherwise
*******************************************************************************/
bool APP_GetAxisActiveState(uint8_t axis);

/*******************************************************************************
  Function:
    void APP_SetAxisActiveState(uint8_t axis, bool active)

  Summary:
    Sets the active state for specified axis.

  Description:
    Updates the active state for the specified axis in hardware layer.
    Controls OCR module activation and motion execution.

  Parameters:
    axis - Axis index (0=X, 1=Y, 2=Z)
    active - true to activate axis, false to deactivate

  Returns:
    None
*******************************************************************************/
void APP_SetAxisActiveState(uint8_t axis, bool active);

/*******************************************************************************
  Function:
    float APP_GetAxisTargetVelocity(uint8_t axis)

  Summary:
    Gets the target velocity for specified axis.

  Description:
    Returns the target velocity in mm/min for the specified axis.
    Used for motion monitoring and debugging.

  Parameters:
    axis - Axis index (0=X, 1=Y, 2=Z)

  Returns:
    Target velocity in mm/min, or 0.0 if axis invalid
*******************************************************************************/
float APP_GetAxisTargetVelocity(uint8_t axis);

/*******************************************************************************
  Function:
    void APP_SetAxisTargetVelocity(uint8_t axis, float velocity)

  Summary:
    Sets the target velocity for specified axis.

  Description:
    Updates the target velocity in mm/min for the specified axis.
    Used by motion planning system to control axis speeds.

  Parameters:
    axis - Axis index (0=X, 1=Y, 2=Z)
    velocity - Target velocity in mm/min

  Returns:
    None
*******************************************************************************/
void APP_SetAxisTargetVelocity(uint8_t axis, float velocity);

/*******************************************************************************
  Function:
    uint32_t APP_GetAxisStepCount(uint8_t axis)

  Summary:
    Gets the step count for specified axis.

  Description:
    Returns the current step count for the specified axis.
    Used for motion progress monitoring and diagnostics.

  Parameters:
    axis - Axis index (0=X, 1=Y, 2=Z)

  Returns:
    Current step count, or 0 if axis invalid
*******************************************************************************/
uint32_t APP_GetAxisStepCount(uint8_t axis);

/*******************************************************************************
  Function:
    void APP_ResetAxisStepCount(uint8_t axis)

  Summary:
    Resets the step count for specified axis.

  Description:
    Clears the step count for the specified axis to zero.
    Used when starting new motion blocks or homing operations.

  Parameters:
    axis - Axis index (0=X, 1=Y, 2=Z)

  Returns:
    None
*******************************************************************************/
void APP_ResetAxisStepCount(uint8_t axis);

/*******************************************************************************
  Function:
    void APP_EnableZMinMask(void)

  Summary:
    Enable Z minimum limit masking for pick-and-place operations

  Description:
    Masks the Z minimum limit switch to allow the nozzle to move below
    the normal limit for component placement with spring-loaded mechanism.
    CRITICAL for pick-and-place operations.
*/

void APP_EnableZMinMask(void);

/*******************************************************************************
  Function:
    void APP_DisableZMinMask(void)

  Summary:
    Disable Z minimum limit masking to restore normal CNC operation

  Description:
    Re-enables the Z minimum limit switch for normal CNC operation.
    Should be called after pick-and-place operations are complete.
*/

void APP_DisableZMinMask(void);

/*******************************************************************************
  Function:
    void APP_SetPickAndPlaceMode(bool enable)

  Summary:
    Enable/disable pick-and-place mode with appropriate limit masking

  Description:
    Convenience function that sets up limit masking for pick-and-place
    operations. When enabled, masks Z minimum limit. When disabled,
    restores all limits for normal CNC operation.
*/

void APP_SetPickAndPlaceMode(bool enable);

/*******************************************************************************
  Function:
    bool APP_IsPickAndPlaceMode(void)

  Summary:
    Check if system is in pick-and-place mode

  Description:
    Returns true if Z minimum limit is currently masked for
    pick-and-place operations.
*/

bool APP_IsPickAndPlaceMode(void);

// *****************************************************************************
// *****************************************************************************
// Section: extern declarations
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_SendStatusReport(void)

  Summary:
    Send detailed status report

  Description:
    This function is called when a status query (?) is received
    via the DMA G-code parser. It sends a GRBL-compatible status report.
*/

void APP_SendStatusReport(void);

/*******************************************************************************
  Function:
    void APP_TestArcInterpolation(void)

  Summary:
    Test arc interpolation functionality

  Description:
    This function demonstrates and tests the G2/G3 arc interpolation
    capabilities including I,J,K offset format, R radius format,
    and helical motion. Only compiled when TEST_ARC_INTERPOLATION is defined.
*/

#ifdef TEST_ARC_INTERPOLATION
void APP_TestArcInterpolation(void);
#endif

// *****************************************************************************
// *****************************************************************************
// Section: Global Data
// *****************************************************************************
// *****************************************************************************

/* Application Data */
extern APP_DATA appData;

/* CNC Axis Data */
extern cnc_axis_t cnc_axes[MAX_AXES];

// DOM-IGNORE-BEGIN
#ifdef __cplusplus
}
#endif
// DOM-IGNORE-END

#endif /* _APP_H */

/*******************************************************************************
 End of File
 */