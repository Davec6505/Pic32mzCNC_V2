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
// Temporarily commented out for UART debug
// #include "speed_control.h"
// #include "motion_profile.h"
#include "gcode_parser_dma.h"

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
    APP_STATE_INIT=0,
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

typedef enum {
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

typedef struct {
    // Position tracking
    int32_t current_position;      // Current position in steps
    int32_t target_position;       // Target position in steps
    
    // Velocity profile (steps/sec)
    float current_velocity;        // Current velocity
    float target_velocity;         // Target velocity for this move
    float max_velocity;            // Maximum allowed velocity
    
    // Acceleration parameters (steps/sec²)
    float acceleration;            // Acceleration rate
    float deceleration;            // Deceleration rate
    
    // State management
    axis_state_t motion_state;     // Current motion state
    bool direction_forward;        // Direction flag
    bool is_active;               // Axis is currently moving
    
    // Hardware control
    uint16_t ocr_period;          // Current OCR period value
    uint32_t step_count;          // Steps completed in current move
    
    // Look-ahead support
    float junction_velocity;       // Velocity at path junctions
    bool buffer_active;           // Motion buffer is active
    
} cnc_axis_t;

// *****************************************************************************
/* Motion Planning Buffer

  Summary:
    Buffer for look-ahead motion planning

  Description:
    This structure holds motion commands for advanced trajectory planning
    with look-ahead capability for smooth path execution
*/

typedef struct {
    float target_pos[3];          // Target coordinates [X, Y, Z]
    float feedrate;               // Requested feedrate (steps/sec)
    float entry_velocity;         // Calculated entry velocity
    float exit_velocity;          // Calculated exit velocity
    float max_velocity;           // Maximum velocity for this segment
    uint8_t motion_type;          // Motion type (G0, G1, etc.)
    bool is_valid;               // Buffer entry is valid
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
    
    /* User interface */
    bool switch_pressed;
    uint32_t switch_debounce_timer;
    uint32_t last_switch_time;
    
    /* System timing */
    uint32_t system_tick_counter;
    
} APP_DATA;

// *****************************************************************************
// *****************************************************************************
// Section: Application Constants
// *****************************************************************************
// *****************************************************************************

#define MAX_AXES 3                    // X, Y, Z axes
#define MOTION_BUFFER_SIZE 8          // Look-ahead buffer size
#define SWITCH_DEBOUNCE_MS 100        // Switch debounce time
#define TRAJECTORY_TIMER_FREQ 1000    // 1kHz trajectory calculation
#define DEFAULT_ACCELERATION 500.0f   // Default acceleration (steps/sec²)
#define DEFAULT_MAX_VELOCITY 1000.0f  // Default max velocity (steps/sec)

// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Function Prototypes
// *****************************************************************************
// *****************************************************************************

/* Timer callback prototypes */
void APP_TrajectoryTimerCallback(uint32_t status, uintptr_t context);
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

void APP_Initialize ( void );


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

void APP_Tasks( void );

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

void APP_ExecuteGcodeCommand(gcode_parsed_line_t *command);

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

// *****************************************************************************
// *****************************************************************************
// Section: Global Data
// *****************************************************************************
// *****************************************************************************

/* Application Data */
extern APP_DATA appData;

/* CNC Axis Data */
extern cnc_axis_t cnc_axes[MAX_AXES];

/* Motion Planning Buffer */
extern motion_block_t motion_buffer[MOTION_BUFFER_SIZE];
extern uint8_t motion_buffer_head;
extern uint8_t motion_buffer_tail;

//DOM-IGNORE-BEGIN
#ifdef __cplusplus
}
#endif
//DOM-IGNORE-END

#endif /* _APP_H */

/*******************************************************************************
 End of File
 */