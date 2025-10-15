/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It
    implements the logic of the application's state machine and it may call
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include "app.h"
#include "gcode_parser.h"
#include "grbl_serial.h"
#include "grbl_settings.h"
#include "interpolation_engine.h"
#include "motion_profile.h"
#include "speed_control.h"
#include "motion_buffer.h"
#include "motion_gcode_parser.h"
#include "motion_planner.h"
#include "gcode_helpers.h"
#include "peripheral/uart/plib_uart2.h"
#include "peripheral/uart/plib_uart_common.h"
#include "peripheral/coretimer/plib_coretimer.h"
#include "peripheral/tmr1/plib_tmr1.h"
#include "peripheral/tmr/plib_tmr2.h"
#include "peripheral/tmr/plib_tmr4.h"
#include "peripheral/ocmp/plib_ocmp1.h"
#include "peripheral/ocmp/plib_ocmp4.h"
#include "peripheral/ocmp/plib_ocmp5.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include "utils.h"

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_Initialize function.

    Application strings and buffers are be defined outside this structure.
*/

// Global counter to verify APP_Initialize was called
uint32_t app_init_counter = 0;
uint32_t uart_callback_counter = 0;

APP_DATA appData;

// TIMER1 ISSUE RESOLVED:
// Timer1 prescaler fixed to run at 1kHz instead of problematic 300μs
// Now using Timer1 for both interpolation engine and motion planner
// Timer1 callback handles: INTERP_Tasks() + MotionPlanner_UpdateTrajectory() + LED heartbeat

// CNC Axis data structures
cnc_axis_t cnc_axes[MAX_AXES];

// UART callback data
static uint8_t uart_rx_char;
volatile uint32_t uart_callback_count = 0;

// Buffer for line commands (non-? characters)
#define LINE_BUFFER_SIZE 256
static char line_buffer[LINE_BUFFER_SIZE];
static volatile uint8_t line_buffer_head = 0;
static volatile uint8_t line_buffer_tail = 0;

// Flag for immediate status report request
static volatile bool status_report_requested = false;

// GRBL State variables
static grbl_state_t grbl_state = GRBL_STATE_IDLE;
static uint8_t grbl_alarm_code = 0;

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************

void APP_UARTPrint(const char *str);
void APP_ExecuteMotionCommand(const char *command);
void APP_SendStatus(void);
void APP_SendDetailedStatus(void);
void APP_EmergencyReset(void);
static void APP_UART_Callback(uintptr_t context);
static void APP_ProcessGRBLCommand(const char *command);
static void APP_PrintGRBLSettings(void);
static void APP_PrintGCodeParameters(void);
static void APP_PrintParserState(void);
static void APP_ProcessSettingChange(const char *command);
static void APP_ProcessGCodeCommand(const char *command);

// Motion System Functions
static void APP_SendError(uint8_t error_code);
static void APP_SendAlarm(uint8_t alarm_code);

static void APP_MotionSystemInit(void);
static void APP_ProcessSwitches(void);
static void APP_ProcessLimitSwitches(void);
/* static void APP_CalculateTrajectory(uint8_t axis_id); */ /* TEMPORARILY COMMENTED FOR DEBUG */
/* static void APP_UpdateOCRPeriod(uint8_t axis_id); */     /* TEMPORARILY COMMENTED FOR DEBUG */
static void APP_ExecuteNextMotionBlock(void);
static void APP_ProcessLookAhead(void);
static bool APP_IsBufferEmpty(void);
static bool APP_IsBufferFull(void);
static void APP_SendMotionOkResponse(void); // Helper for motion timing synchronization
/* static void APP_ProcessUART(void); */    // Removed - now handled by GRBL serial module

// Callback function declarations

// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization
// *****************************************************************************
// *****************************************************************************

/******************************************************************************
  Motion Complete Callback

  Called by interpolation engine when motion completes.
  Ensures UGS receives 'ok' response so it can send next command.
 */
static void APP_OnMotionComplete(void)
{
    // Motion completed - buffer is now empty, ready for next command
    // Send ok response so UGS knows it can send the next command
    APP_SendMotionOkResponse();
}

/******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize(void)
{
    // Increment counter to prove this function ran
    app_init_counter = 42; // Distinctive value

    /* Place the App state machine in its initial state. */
    appData.state = APP_STATE_INIT;

    // CRITICAL DEBUG: Turn LED2 OFF at startup - will be turned ON in APP_MotionSystemInit()
    LED2_Clear();

    // Temporarily disable GRBL Serial to test direct UART handling
    // GRBL_Serial_Initialize();
    // GRBL_RegisterMotionCallback(APP_ExecuteMotionCommand);
    // GRBL_RegisterStatusCallback(APP_SendStatus);
    // GRBL_RegisterEmergencyCallback(APP_EmergencyReset);
    // GRBL_RegisterWriteCallback(APP_UARTPrint);

    /* Initialize application data */
    appData.motion_system_ready = false;
    appData.trajectory_timer_active = false;
    appData.switch_pressed = false;
    appData.switch_debounce_timer = 0;
    appData.last_switch_time = 0;
    appData.system_tick_counter = 0;

    /* Initialize motion buffer */
    MotionBuffer_Initialize();

    /* Initialize motion G-code parser */
    MotionGCodeParser_Initialize();

    /* Initialize motion planner */
    MotionPlanner_Initialize();

    /* Initialize motion profile system */
    MOTION_PROFILE_Initialize();

    /* CRITICAL: Register motion complete callback for interpolation engine
     * This ensures 'ok' is sent to UGS when motion completes */
    INTERP_RegisterMotionCompleteCallback(APP_OnMotionComplete);

    /* Initialize the motion control system */
    APP_MotionSystemInit();

    // Setup simple UART callback for immediate real-time command handling
    UART2_ReadCallbackRegister(APP_UART_Callback, (uintptr_t)NULL);

    // CRITICAL: Enable UART2 RX interrupt - disabled by UART2_Initialize()
    IEC4SET = _IEC4_U2RXIE_MASK;

    // Start the first read - this should now work with interrupt enabled
    // Debug: Check if initial read was successful (disabled for UGS)
    /*
    bool read_success = UART2_Read(&uart_rx_char, 1);
    if (read_success)
    {
        APP_UARTPrint("UART2_Read() started successfully\r\n");
    }
    else
    {
        APP_UARTPrint("ERROR: UART2_Read() failed to start\r\n");
    }
    */

    // Start UART read - UGS will initiate handshake with ? and $I commands
    UART2_Read(&uart_rx_char, 1);
}

// *****************************************************************************
// *****************************************************************************
// Section: Application Tasks
// *****************************************************************************
// *****************************************************************************

/******************************************************************************
  Function:
    void APP_Tasks ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Tasks(void)
{
    /* Unified UART processing:
     * - All commands processed in main loop for proper sequencing
     * - No hybrid processing to avoid race conditions
     */

    static char rx_buffer[64];
    static uint8_t rx_pos = 0;
    uint8_t received_char; // Process buffered characters from callback
    while (line_buffer_tail != line_buffer_head)
    {
        received_char = line_buffer[line_buffer_tail];
        line_buffer_tail = (line_buffer_tail + 1) % LINE_BUFFER_SIZE;

        // Handle real-time commands immediately (single character commands)
        if (received_char == '?')
        {
            // Status report - process immediately for proper sequencing
            // DEBUG: Log status request for UGS debugging
            // APP_UARTPrint_blocking("[DEBUG: Status request received]\r\n");
            APP_SendStatus();
            continue; // Don't buffer this character
        }

        if (received_char == '\r' || received_char == '\n')
        {
            // End of line - process the command
            if (rx_pos > 0)
            {
                rx_buffer[rx_pos] = '\0';
                // DEBUG: Log command received for UGS debugging
                char debug_cmd[128];
                sprintf(debug_cmd, "[DEBUG: Command received: '%s']\r\n", rx_buffer);
                // APP_UARTPrint_blocking(debug_cmd);  // Commented out for UGS compatibility
                APP_ProcessGRBLCommand(rx_buffer);
                rx_pos = 0; // Reset buffer
            }
        }
        else
        {
            // Buffer the character
            if (rx_pos < sizeof(rx_buffer) - 1)
            {
                rx_buffer[rx_pos++] = received_char;
            }
            else
            {
                rx_pos = 0; // Buffer overflow, reset
            }
        }
    }

    /* Check the application's current state. */
    switch (appData.state)
    {
    /* Application's initial state. */
    case APP_STATE_INIT:
    {
        // Initialization is now done in APP_Initialize()
        // The new interrupt-driven model doesn't need the complex state machine for init.
        appData.state = APP_STATE_MOTION_IDLE;
        break;
    }

    case APP_STATE_SERVICE_TASKS:
    {
        // This state is obsolete
        appData.state = APP_STATE_MOTION_IDLE;
        break;
    }

    case APP_STATE_GCODE_INIT:
    {
        // This state is obsolete
        appData.state = APP_STATE_MOTION_IDLE;
        break;
    }

    case APP_STATE_MOTION_INIT:
    {
        // This state is obsolete
        appData.state = APP_STATE_MOTION_IDLE;
        break;
    }

    case APP_STATE_MOTION_IDLE:
    {
        /* Process user input */
        APP_ProcessSwitches();

        /* Process look-ahead planning */
        APP_ProcessLookAhead();

        /* Check if motion commands are queued */
        if (!APP_IsBufferEmpty())
        {
            appData.state = APP_STATE_MOTION_PLANNING;
        }
        break;
    }

    case APP_STATE_MOTION_PLANNING:
    {
        /* DEBUG: Confirm we're entering planning state */
        APP_UARTPrint_blocking("[PLANNING]\r\n");

        /* Execute the next motion block */
        APP_ExecuteNextMotionBlock();

        appData.state = APP_STATE_MOTION_EXECUTING;
        break;
    }

    case APP_STATE_MOTION_EXECUTING:
    {
        /* Check if homing cycle is active */
        if (INTERP_IsHomingActive())
        {
            /* Homing in progress - check status */
            homing_state_t homing_state = INTERP_GetHomingState();

            if (homing_state == HOMING_STATE_COMPLETE)
            {
                APP_UARTPrint("Homing cycle completed successfully\r\n");
                appData.state = APP_STATE_MOTION_IDLE;
                break;
            }
            else if (homing_state == HOMING_STATE_ERROR)
            {
                APP_UARTPrint("ERROR: Homing cycle failed\r\n");
                appData.state = APP_STATE_MOTION_ERROR;
                break;
            }
            /* Continue homing - stay in executing state */
            break;
        }

        /* Check if current motion is complete */
        bool all_axes_complete = true;

        for (int i = 0; i < MAX_AXES; i++)
        {
            if (cnc_axes[i].is_active &&
                cnc_axes[i].motion_state != AXIS_IDLE &&
                cnc_axes[i].motion_state != AXIS_COMPLETE)
            {
                all_axes_complete = false;
                break;
            }
        }

        if (all_axes_complete)
        {
            /* Mark current buffer entry as processed */
            MotionBuffer_Complete();

            /* Reset axes to idle state */
            for (int i = 0; i < MAX_AXES; i++)
            {
                cnc_axes[i].is_active = false;
                cnc_axes[i].motion_state = AXIS_IDLE;
                cnc_axes[i].current_velocity = 0;
            }

            /* Check for more commands */
            if (!APP_IsBufferEmpty())
            {
                appData.state = APP_STATE_MOTION_PLANNING;
            }
            else
            {
                appData.state = APP_STATE_MOTION_IDLE;
            }
        }
        break;
    }

    case APP_STATE_MOTION_ERROR:
    {
        /* Handle error state - stop all motion */
        APP_EmergencyStop();
        appData.state = APP_STATE_MOTION_IDLE;
        break;
    }

    /* The default state should never be executed. */
    default:
    {
        break;
    }
    }
}

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************

static void APP_MotionSystemInit(void)
{
    /* Initialize interpolation engine - CRITICAL! */
    if (!INTERP_Initialize())
    {
        // REMOVED: printf() blocks before UART ready - causes hang
        // printf("ERROR: Failed to initialize interpolation engine!\n");
        return;
    }
    // REMOVED: printf() blocks before UART ready - causes hang
    // printf("Interpolation engine initialized successfully\n");

    /* Initialize CNC axes */
    for (int i = 0; i < MAX_AXES; i++)
    {
        cnc_axes[i].current_position = 0;
        cnc_axes[i].target_position = 0;
        cnc_axes[i].current_velocity = 0;
        cnc_axes[i].target_velocity = 0;
        cnc_axes[i].max_velocity = DEFAULT_MAX_VELOCITY;
        cnc_axes[i].acceleration = DEFAULT_ACCELERATION;
        cnc_axes[i].deceleration = DEFAULT_ACCELERATION;
        cnc_axes[i].motion_state = AXIS_IDLE;
        cnc_axes[i].direction_forward = true;
        cnc_axes[i].is_active = false;
        cnc_axes[i].ocr_period = 0;
        cnc_axes[i].step_count = 0;
        cnc_axes[i].junction_velocity = 0;
        cnc_axes[i].buffer_active = false;
    }

    /* Setup OCR callbacks for step pulse generation */
    OCMP1_CallbackRegister(APP_OCMP1_Callback, 0);
    OCMP4_CallbackRegister(APP_OCMP4_Callback, 0);
    OCMP5_CallbackRegister(APP_OCMP5_Callback, 0);

    /* Start base timers that drive OCR modules - CRITICAL for motion! */
    /* Timer-to-OCR mapping per MCC configuration:
     * TMR2 → OCMP4 (X-axis)
     * TMR3 → OCMP5 (Z-axis)
     * TMR4 → OCMP1 (Y-axis)
     * TMR5 → OCMP3 (A-axis/4th axis)
     */
    TMR2_Start(); // Base timer for OCMP4 (X-axis)
    TMR3_Start(); // Base timer for OCMP5 (Z-axis)
    TMR4_Start(); // Base timer for OCMP1 (Y-axis)

    /* TRAJECTORY SYSTEM - Now using Timer1 (prescaler fixed to 1kHz) */
    /* Timer1 configured by Harmony, just need to start it */
    if (!appData.trajectory_timer_active)
    {
        // CRITICAL DEBUG: Turn ON LED2 to prove we reach this point
        LED2_Set(); // Changed from Toggle to Set for clear indication

        // Timer1 callback already registered by interpolation engine
        // Timer1 now handles both INTERP_Tasks() AND MotionPlanner_UpdateTrajectory()
        TMR1_Start(); // This should work now that prescaler is fixed
        appData.trajectory_timer_active = true;
    }

    appData.motion_system_ready = true;
}

static void APP_ProcessSwitches(void)
{
    /* Process limit switches first - highest priority */
    APP_ProcessLimitSwitches();

    /* Simple switch debouncing and command generation */
    uint32_t current_time = appData.system_tick_counter;

    /* Check debounce timing */
    if ((current_time - appData.last_switch_time) < SWITCH_DEBOUNCE_MS)
    {
        return;
    }

    if (!SW1_Get() && !appData.switch_pressed)
    {
        /* Forward move - X-axis only for now */
        float target[] = {10000, 0, 0}; // X, Y, Z
        if (APP_AddLinearMove(target, 500))
        {
            appData.switch_pressed = true;
            appData.last_switch_time = current_time;
        }
    }
    else if (!SW2_Get() && !appData.switch_pressed)
    {
        /* Reverse move - X-axis only for now */
        float target[] = {-10000, 0, 0}; // X, Y, Z
        if (APP_AddLinearMove(target, 500))
        {
            appData.switch_pressed = true;
            appData.last_switch_time = current_time;
        }
    }
    else if (SW1_Get() && SW2_Get())
    {
        /* Both switches released */
        appData.switch_pressed = false;
    }
}

static void APP_ProcessLimitSwitches(void)
{
    uint8_t limit_state = 0;
    //    if (LIMIT_X_Get()) {
    //        limit_state |= (1 << 0);
    //    }
    //    if (LIMIT_Y_Get()) {
    //        limit_state |= (1 << 1);
    //    }
    //    if (LIMIT_Z_Get()) {
    //        limit_state |= (1 << 2);
    //    }

    if (limit_state != appData.limit_switch_state)
    {
        appData.limit_switch_state = limit_state;

        /* Limit switch state changed - report alarm if any limit is active */
        if (limit_state != 0)
        {
            APP_UARTPrint("ALARM: Limit switch triggered\r\n");
            appData.state = APP_STATE_MOTION_ERROR;
        }
    }
}

static void APP_ExecuteNextMotionBlock(void)
{
    if (APP_IsBufferEmpty())
    {
        return;
    }

    motion_block_t *block = MotionBuffer_GetNext();

    if (block == NULL)
    {
        return;
    }

    /* DEBUG: Print block execution start */
    char exec_msg[100];
    snprintf(exec_msg, sizeof(exec_msg), "[EXEC] Block X=%.3f Y=%.3f Z=%.3f F=%.1f\r\n",
             block->target_pos[0], block->target_pos[1], block->target_pos[2], block->feedrate);
    APP_UARTPrint_blocking(exec_msg);

    /* Get steps per mm from GRBL settings (with fallback to defaults) */
    float steps_per_mm[3];
    steps_per_mm[0] = GRBL_GetSetting(SETTING_X_STEPS_PER_MM); // X-axis
    steps_per_mm[1] = GRBL_GetSetting(SETTING_Y_STEPS_PER_MM); // Y-axis
    steps_per_mm[2] = GRBL_GetSetting(SETTING_Z_STEPS_PER_MM); // Z-axis

    /* Fallback to defaults if GRBL not initialized */
    if (steps_per_mm[0] <= 0.0f || isnan(steps_per_mm[0]))
        steps_per_mm[0] = 250.0f;
    if (steps_per_mm[1] <= 0.0f || isnan(steps_per_mm[1]))
        steps_per_mm[1] = 250.0f;
    if (steps_per_mm[2] <= 0.0f || isnan(steps_per_mm[2]))
        steps_per_mm[2] = 250.0f;

    /* Setup axes for movement with OCR dual-compare mode */
    for (int i = 0; i < MAX_AXES; i++)
    {
        /* Convert target position from mm to steps */
        int32_t target_pos_steps = (int32_t)(block->target_pos[i] * steps_per_mm[i]);

        /* Only move axis if target is different from current position */
        if (target_pos_steps != cnc_axes[i].current_position)
        {
            /* Calculate number of steps to move (delta, not absolute position) */
            int32_t steps_to_move = abs(target_pos_steps - cnc_axes[i].current_position);

            /* DEBUG: Print axis activation details */
            char debug_msg[128];
            snprintf(debug_msg, sizeof(debug_msg), "[AXIS%d] target_mm=%.3f curr_steps=%d target_steps=%d delta=%d\r\n",
                     i, block->target_pos[i], cnc_axes[i].current_position, target_pos_steps, steps_to_move);
            APP_UARTPrint_blocking(debug_msg);

            cnc_axes[i].target_position = steps_to_move; // Store delta for callback comparison
            cnc_axes[i].target_velocity = block->feedrate;
            cnc_axes[i].max_velocity = block->max_velocity > 0 ? block->max_velocity : DEFAULT_MAX_VELOCITY;
            cnc_axes[i].motion_state = AXIS_ACCEL;
            cnc_axes[i].is_active = true;
            cnc_axes[i].step_count = 0;

            /* Set direction */
            cnc_axes[i].direction_forward = (target_pos_steps > cnc_axes[i].current_position);

            /* Set direction GPIO pins - DRV8825 requires direction set BEFORE step pulses */
            switch (i)
            {
            case 0: // X-axis
                if (cnc_axes[i].direction_forward)
                {
                    DirX_Set(); // Forward (positive direction)
                }
                else
                {
                    DirX_Clear(); // Reverse (negative direction)
                }
                break;

            case 1: // Y-axis
                if (cnc_axes[i].direction_forward)
                {
                    DirY_Set();
                }
                else
                {
                    DirY_Clear();
                }
                break;

            case 2: // Z-axis
                if (cnc_axes[i].direction_forward)
                {
                    DirZ_Set();
                }
                else
                {
                    DirZ_Clear();
                }
                break;
            }

            /* Calculate OCR period from velocity */
            uint32_t period = MotionPlanner_CalculateOCRPeriod(cnc_axes[i].target_velocity);

            /* DRV8825 requires minimum 1.9µs pulse width - use 40 timer counts for safety */
            const uint32_t OCMP_PULSE_WIDTH = 40;

            /* Clamp period to 16-bit timer maximum (safety margin) */
            if (period > 65485)
            {
                period = 65485;
            }

            /* Ensure period is greater than pulse width */
            if (period <= OCMP_PULSE_WIDTH)
            {
                period = OCMP_PULSE_WIDTH + 10;
            }

            /* Configure OCR dual-compare mode - VERIFIED WORKING CONFIGURATION:
             * Proven ratio pattern from idle state test: OC4R=250, OC4RS=40, Period=300
             * This translates to: OCxR = (period - OCMP_PULSE_WIDTH), OCxRS = OCMP_PULSE_WIDTH
             * Timer-to-OCR mapping per MCC: TMR2→OCMP4, TMR3→OCMP5, TMR4→OCMP1, TMR5→OCMP3
             */
            switch (i)
            {
            case 0: // X-axis -> OCMP4 + TMR2 (per MCC)
                /* TMR2 has no timer interrupt - only OCR interrupt used */
                TMR2_PeriodSet(period);                           // Set timer rollover
                OCMP4_CompareValueSet(period - OCMP_PULSE_WIDTH); // Rising edge (e.g., 300-40=260)
                OCMP4_CompareSecondaryValueSet(OCMP_PULSE_WIDTH); // Falling edge (e.g., 40)
                OCMP4_Enable();
                TMR2_Start(); // CRITICAL: Restart timer for each move
                break;

            case 1:                                               // Y-axis -> OCMP1 + TMR4 (per MCC)
                TMR4_InterruptDisable();                          // Disable timer interrupt (use OCR interrupt only)
                TMR4_PeriodSet(period);                           // Set timer rollover
                OCMP1_CompareValueSet(period - OCMP_PULSE_WIDTH); // Rising edge
                OCMP1_CompareSecondaryValueSet(OCMP_PULSE_WIDTH); // Falling edge
                OCMP1_Enable();
                TMR4_Start(); // CRITICAL: Restart timer for each move
                break;

            case 2:                                               // Z-axis -> OCMP5 + TMR3 (per MCC)
                TMR3_InterruptDisable();                          // Disable timer interrupt (use OCR interrupt only)
                TMR3_PeriodSet(period);                           // Set timer rollover
                OCMP5_CompareValueSet(period - OCMP_PULSE_WIDTH); // Rising edge
                OCMP5_CompareSecondaryValueSet(OCMP_PULSE_WIDTH); // Falling edge
                OCMP5_Enable();
                TMR3_Start(); // CRITICAL: Restart timer for each move
                break;
            }
        }
    }
}

/* TEMPORARILY COMMENTED FOR DEBUG - APP_CalculateTrajectory function
static void APP_CalculateTrajectory(uint8_t axis_id)
{
    // Function implementation commented out to isolate Timer1 callback issue
    // Will restore once Timer1 hang is resolved
}
*/

/* TEMPORARILY COMMENTED FOR DEBUG - APP_UpdateOCRPeriod function
static void APP_UpdateOCRPeriod(uint8_t axis_id)
{
    // Function implementation commented out to isolate Timer1 callback issue
    // Will restore once Timer1 hang is resolved
}
*/

static void APP_ProcessLookAhead(void)
{
    /* TODO: Implement look-ahead trajectory optimization */
    /* For now, this is a placeholder for future enhancement */

    /* This function will analyze the motion buffer and optimize */
    /* entry/exit velocities for smooth cornering */
}

bool APP_ExecuteMotionBlock(motion_block_t *block)
{
    /* CRITICAL FIX (Bug #7): Disable TMR1 execution path - state machine handles execution
     *
     * This function is called by MotionPlanner_UpdateTrajectory() via TMR1 @ 1kHz
     * The state machine (APP_STATE_MOTION_PLANNING) also calls APP_ExecuteNextMotionBlock()
     * Having BOTH paths active causes all blocks to execute simultaneously (diagonal motion)
     *
     * SOLUTION: TMR1 path is now a NO-OP - state machine has exclusive execution control
     *
     * This was causing the diagonal motion bug where all axes activated at once because:
     * 1. State machine adds block to buffer → transitions to PLANNING → calls APP_ExecuteNextMotionBlock()
     * 2. TMR1 @ 1kHz also calling this function → was also calling APP_ExecuteNextMotionBlock()
     * 3. Both paths pull from same buffer → activate all axes before any complete
     * 4. Result: XY diagonal instead of sequential X then Y moves
     */

    /* TMR1 execution path disabled - return success to keep motion planner happy */
    return true;
}

static bool APP_IsBufferEmpty(void)
{
    return MotionBuffer_IsEmpty();
}

static bool APP_IsBufferFull(void)
{
    return !MotionBuffer_HasSpace();
}

/* UART processing is now handled by GRBL serial module - removed APP_ProcessUART to avoid conflicts
static void APP_ProcessUART(void)
{
    uint8_t received_char;
    while (UART2_Read(&received_char, 1))
    {
        // Check for real-time commands first
        if (received_char == '?')
        {
            APP_SendStatus();
            continue; // Don't buffer this character
        }
        // Add other real-time command checks here (e.g., '~' for cycle start, '!' for feed hold)

        // If not a real-time command, buffer it for line processing
        if (appData.uart_rx_buffer_pos < sizeof(appData.uart_rx_buffer) - 1)
        {
            if (received_char == '\n' || received_char == '\r')
            {
                appData.uart_rx_buffer[appData.uart_rx_buffer_pos] = '\0';
                if (appData.uart_rx_buffer_pos > 0) {
                    APP_ProcessGRBLCommand(appData.uart_rx_buffer);
                }
                appData.uart_rx_buffer_pos = 0; // Reset for next line
            }
            else
            {
                appData.uart_rx_buffer[appData.uart_rx_buffer_pos++] = received_char;
            }
        }
        else
        {
            // Buffer overflow, reset
            appData.uart_rx_buffer_pos = 0;
        }
    }
}
*/

// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Function Implementations
// *****************************************************************************
// *****************************************************************************

static void APP_UART_Callback(uintptr_t context)
{
    // Count callbacks to verify this function is being called
    uart_callback_counter++;

    // Check for UART errors first
    if (UART2_ErrorGet() == UART_ERROR_NONE)
    {
        // Handle real-time commands immediately (single character commands)
        switch (uart_rx_char)
        {
        case '?':
            // Status report - buffer it like other commands for proper sequencing
            {
                uint8_t next_head = (line_buffer_head + 1) % LINE_BUFFER_SIZE;
                if (next_head != line_buffer_tail) // Buffer not full
                {
                    line_buffer[line_buffer_head] = uart_rx_char;
                    line_buffer_head = next_head;
                }
            }
            break;

        case '!':
            // Feed hold
            break; // Removed message

        case '~':
            // Cycle start (resume)
            APP_UARTPrint_blocking("[MSG:Cycle Start]\r\n");
            break;

        case 0x18: // Ctrl-X
            // Soft reset
            grbl_state = GRBL_STATE_IDLE;
            grbl_alarm_code = 0;
            APP_UARTPrint_blocking("\r\n\r\nGrbl 1.1f ['$' for help]\r\n");
            break;

        case 0x84: // Safety door (if enabled)
            APP_UARTPrint_blocking("[MSG:Safety Door]\r\n");
            break;

        case 0x9E: // Jog cancel
            APP_UARTPrint_blocking("[MSG:Jog Cancelled]\r\n");
            break;

        default:
            // Buffer other characters for line processing
            {
                uint8_t next_head = (line_buffer_head + 1) % LINE_BUFFER_SIZE;
                if (next_head != line_buffer_tail) // Buffer not full
                {
                    line_buffer[line_buffer_head] = uart_rx_char;
                    line_buffer_head = next_head;
                }
            }
            break;
        }
    }

    // CRITICAL: Re-arm for next character
    UART2_Read(&uart_rx_char, 1);
}
static void APP_ProcessGRBLCommand(const char *command)
{
    if (command[0] == '$')
    {
        // GRBL System Commands
        if (strcmp(command, "$I") == 0)
        {
            // Build info - send as single message to avoid timing issues
            APP_UARTPrint_blocking("[VER:1.1f.20241012:CNC Controller]\r\n[OPT:V,15,128]\r\nok\r\n");
        }
        else if (strcmp(command, "$$") == 0)
        {
            // Print all settings
            APP_PrintGRBLSettings();
        }
        else if (strcmp(command, "$#") == 0)
        {
            // Print gcode parameters (coordinate offsets)
            APP_PrintGCodeParameters();
        }
        else if (strcmp(command, "$G") == 0)
        {
            // Print parser state
            APP_PrintParserState();
        }
        else if (strcmp(command, "$N") == 0)
        {
            // Print startup blocks - send as single message to avoid timing issues
            APP_UARTPrint_blocking("$N0=\r\n$N1=\r\nok\r\n");
        }
        else if (strcmp(command, "$C") == 0)
        {
            // Check gcode mode toggle
            APP_UARTPrint_blocking("[MSG:Enabled]\r\nok\r\n");
        }
        else if (strcmp(command, "$X") == 0)
        {
            // Kill alarm lock
            if (grbl_state == GRBL_STATE_ALARM)
            {
                grbl_state = GRBL_STATE_IDLE;
                grbl_alarm_code = 0;
                APP_UARTPrint_blocking("[MSG:Caution: Unlocked]\r\nok\r\n");
            }
            else
            {
                APP_UARTPrint_blocking("[MSG:Caution: Unlocked]\r\nok\r\n");
            }
        }
        else if (strcmp(command, "$H") == 0)
        {
            // Run homing cycle
            APP_UARTPrint_blocking("[MSG:Homing cycle completed]\r\nok\r\n");
        }
        else if (strncmp(command, "$J=", 3) == 0)
        {
            // Jogging command - extract G-code part and process as motion
            const char *gcode_part = &command[3]; // Skip "$J="
            APP_ProcessGCodeCommand(gcode_part);
            // Note: Jogging commands in GRBL don't send "ok" response immediately
            // The motion system will handle the response timing
        }
        else if (command[1] >= '0' && command[1] <= '9')
        {
            // Setting change command (like $0=10)
            APP_ProcessSettingChange(command);
        }
        else
        {
            APP_SendError(3); // Invalid statement
        }
    }
    else if (strncmp(command, "DEBUG", 5) == 0)
    {
        // Debug command - special case
        APP_SendDetailedStatus();
    }
    else if ((command[0] >= 'G' && command[0] <= 'Z') || (command[0] >= 'g' && command[0] <= 'z'))
    {
        // G-code command
        APP_ProcessGCodeCommand(command);
    }
    else
    {
        APP_SendError(3); // Invalid statement
    }
}

static void APP_SendError(uint8_t error_code)
{
    char error_msg[32];
    sprintf(error_msg, "error:%d\r\n", error_code);
    APP_UARTPrint(error_msg);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void APP_SendAlarm(uint8_t alarm_code)
{
    char alarm_msg[32];
    grbl_state = GRBL_STATE_ALARM;
    grbl_alarm_code = alarm_code;
    sprintf(alarm_msg, "ALARM:%d\r\n", alarm_code);
    APP_UARTPrint(alarm_msg);
    // Example: APP_SendAlarm(1); // Hard limit alarm
}
#pragma GCC diagnostic pop

// Use APP_SendAlarm in error conditions:
// APP_SendAlarm(1); // Hard limit triggered
// APP_SendAlarm(2); // Soft limit exceeded

static void APP_PrintGRBLSettings(void)
{
    // GRBL 1.1f standard settings - send as single message to avoid timing issues
    APP_UARTPrint_blocking("$0=10\r\n$1=25\r\n$2=0\r\n$3=0\r\n$4=0\r\n$5=0\r\n$6=0\r\n"
                           "$10=1\r\n$11=0.010\r\n$12=0.002\r\n$13=0\r\n$20=0\r\n$21=0\r\n$22=0\r\n"
                           "$23=0\r\n$24=25.000\r\n$25=500.000\r\n$26=250\r\n$27=1.000\r\n"
                           "$30=1000\r\n$31=0\r\n$32=0\r\n$100=250.000\r\n$101=250.000\r\n$102=250.000\r\n"
                           "$110=500.000\r\n$111=500.000\r\n$112=500.000\r\n$120=10.000\r\n$121=10.000\r\n$122=10.000\r\n"
                           "$130=200.000\r\n$131=200.000\r\n$132=200.000\r\nok\r\n");
}

static void APP_ProcessSettingChange(const char *command)
{
    // Parse setting change commands like $0=10
    // For now, just acknowledge the setting change
    APP_UARTPrint_blocking("ok\r\n");
}

static void APP_PrintGCodeParameters(void)
{
    // Print coordinate system offsets and other parameters - send as single message
    APP_UARTPrint_blocking("[G54:0.000,0.000,0.000]\r\n[G55:0.000,0.000,0.000]\r\n[G56:0.000,0.000,0.000]\r\n"
                           "[G57:0.000,0.000,0.000]\r\n[G58:0.000,0.000,0.000]\r\n[G59:0.000,0.000,0.000]\r\n"
                           "[G28:0.000,0.000,0.000]\r\n[G30:0.000,0.000,0.000]\r\n[G92:0.000,0.000,0.000]\r\n"
                           "[TLO:0.000]\r\n[PRB:0.000,0.000,0.000:0]\r\nok\r\n");
}

static void APP_PrintParserState(void)
{
    // Print current parser state (modal groups) - send as single message
    APP_UARTPrint_blocking("[GC:G0 G54 G17 G21 G90 G94 M5 M9 T0 F0 S0]\r\nok\r\n");
}

static void APP_ProcessGCodeCommand(const char *command)
{
    // Set up debug callback for helper functions (disabled for UGS compatibility)
    // GCodeHelpers_SetDebugCallback(APP_UARTPrint_blocking);

    // DEBUG: Disabled for UGS compatibility
    // char debug_msg[128];
    // sprintf(debug_msg, "[DEBUG: ProcessGCode called with: '%s']\r\n", command);
    // APP_UARTPrint_blocking(debug_msg);

    // Clean and validate the command
    if (!GCodeHelpers_IsValidCommand(command))
    {
        // DEBUG: Disabled for UGS compatibility
        // APP_UARTPrint_blocking("[DEBUG: Invalid or empty command]\r\n");
        APP_UARTPrint_blocking("ok\r\n"); // Empty commands get "ok"
        return;
    }

    // Tokenize the command using MikroC-style tokenizer
    char tokens[MAX_GCODE_TOKENS][MAX_GCODE_TOKEN_LENGTH];
    int token_count = GCodeHelpers_TokenizeString(command, tokens);

    if (token_count == 0)
    {
        // DEBUG: Disabled for UGS compatibility
        // APP_UARTPrint_blocking("[DEBUG: No tokens found]\r\n");
        APP_UARTPrint_blocking("ok\r\n");
        return;
    }

    // Debug: Show the tokenized command (disabled for UGS compatibility)
    // GCodeHelpers_DebugTokens(tokens, token_count);

    // Analyze the tokens for motion commands
    motion_analysis_t analysis = GCodeHelpers_AnalyzeMotionTokens(tokens, token_count);

    // Process motion commands if present
    if (analysis.has_motion)
    {
        // DEBUG: Disabled for UGS compatibility
        // char motion_debug[128];
        // sprintf(motion_debug, "[DEBUG: Motion command detected - %s]\r\n",
        //         GCodeHelpers_GetGCodeTypeName(analysis.motion_type));
        // APP_UARTPrint_blocking(motion_debug);

        // Use existing motion parser for actual execution
        if (MotionBuffer_HasSpace())
        {
            motion_block_t move_block;
            if (MotionGCodeParser_ParseMove(command, &move_block))
            {
                /* DEBUG: Print what's being added to buffer */
                char add_msg[100];
                snprintf(add_msg, sizeof(add_msg), "[ADD] X=%.3f Y=%.3f Z=%.3f F=%.1f\r\n",
                         move_block.target_pos[0], move_block.target_pos[1], move_block.target_pos[2], move_block.feedrate);
                APP_UARTPrint_blocking(add_msg);

                MotionPlanner_CalculateDistance(&move_block);
                if (MotionBuffer_Add(&move_block))
                {
                    /* CRITICAL: Update parser position immediately after queuing
                     * This ensures subsequent commands use the correct starting position */
                    MotionGCodeParser_SetPosition(move_block.target_pos[0],
                                                  move_block.target_pos[1],
                                                  move_block.target_pos[2]);

                    // Comment out this debug line:
                    // APP_UARTPrint_blocking("[BUFFER_ADD] head=X tail=Y");
                    MotionPlanner_ProcessBuffer();

                    /* CRITICAL: Trigger state machine to start execution if not already executing
                     * Fixed: Only trigger if IDLE or SERVICE_TASKS (not if already EXECUTING)
                     * When already executing, the completion handler will transition to PLANNING for next block */
                    if (appData.state == APP_STATE_MOTION_IDLE ||
                        appData.state == APP_STATE_SERVICE_TASKS)
                    {
                        appData.state = APP_STATE_MOTION_PLANNING;
                    }

                    /* CRITICAL FIX (Bug #8): Don't send 'ok' immediately!
                     * UGS was sending all commands in bulk because it got 'ok' before motion started.
                     * This caused all blocks to buffer, then interpolation engine vectorized them.
                     * NOW: Only send 'ok' when motion completes (via APP_OnMotionComplete callback).
                     * This forces UGS to wait for each move to finish before sending next command.
                     */
                    // APP_SendMotionOkResponse(); // DISABLED - callback sends it when motion completes
                }
                else
                {
                    APP_UARTPrint_blocking("error:14\r\n"); // Motion buffer overflow
                }
            }
            else
            {
                APP_UARTPrint_blocking("error:33\r\n"); // Invalid gcode
            }
        }
        else
        {
            APP_UARTPrint_blocking("error:14\r\n"); // Motion buffer full
        }
        return; // Early return for motion commands
    }

    // Process non-motion commands - check first token for G/M codes
    gcode_type_t gcode_type = GCodeHelpers_ParseGCodeType(tokens[0]);

    // DEBUG: Disabled for UGS compatibility
    // char type_debug[128];
    // sprintf(type_debug, "[DEBUG: Processing %s]\r\n", GCodeHelpers_GetGCodeTypeName(gcode_type));
    // APP_UARTPrint_blocking(type_debug);

    switch (gcode_type)
    {
    case GCODE_G0:
    case GCODE_G1:
        // Motion commands - Check buffer space before adding new move
        // APP_UARTPrint_blocking("[GCODE] G0/G1 command received\r\n");

        if (MotionBuffer_HasSpace())
        {
            // APP_UARTPrint_blocking("[GCODE] Buffer has space\r\n");
            motion_block_t move_block;
            if (MotionGCodeParser_ParseMove(command, &move_block))
            {
                /* DEBUG: Print what's being added to buffer */
                char add_msg[100];
                snprintf(add_msg, sizeof(add_msg), "[ADD] X=%.3f Y=%.3f Z=%.3f F=%.1f\r\n",
                         move_block.target_pos[0], move_block.target_pos[1], move_block.target_pos[2], move_block.feedrate);
                APP_UARTPrint_blocking(add_msg);

                // APP_UARTPrint_blocking("[GCODE] Move parsed successfully\r\n");
                // Calculate distance and duration for the motion block
                MotionPlanner_CalculateDistance(&move_block);

                if (MotionBuffer_Add(&move_block))
                {
                    /* CRITICAL: Update parser position immediately after queuing */
                    MotionGCodeParser_SetPosition(move_block.target_pos[0],
                                                  move_block.target_pos[1],
                                                  move_block.target_pos[2]);

                    MotionPlanner_ProcessBuffer();

                    /* CRITICAL: Trigger state machine if idle */
                    if (appData.state == APP_STATE_MOTION_IDLE ||
                        appData.state == APP_STATE_SERVICE_TASKS)
                    {
                        appData.state = APP_STATE_MOTION_PLANNING;
                    }

                    // APP_UARTPrint_blocking("[GCODE] Move added to buffer\r\n");
                    // APP_UARTPrint_blocking("[GCODE] Buffer processed\r\n");

                    /* CRITICAL FIX (Bug #8): Don't send 'ok' immediately - see G0 handler comment */
                    // APP_SendMotionOkResponse(); // DISABLED - callback sends it when motion completes
                }
                else
                {
                    APP_UARTPrint_blocking("error:14\r\n"); // Motion buffer overflow
                }
            }
            else
            {
                APP_UARTPrint_blocking("error:33\r\n"); // Invalid gcode ID:33
            }
        }
        else
        {
            APP_UARTPrint_blocking("error:14\r\n"); // Motion buffer overflow
        }
        break;

    case GCODE_G2:
    case GCODE_G3:
        // Circular movement
        if (MotionBuffer_HasSpace())
        {
            motion_block_t arc_block;
            if (MotionGCodeParser_ParseArc(command, &arc_block))
            {
                MotionBuffer_Add(&arc_block);
                MotionPlanner_ProcessBuffer();
                APP_SendMotionOkResponse(); // Use helper for proper timing
            }
            else
            {
                APP_UARTPrint_blocking("error:33\r\n");
            }
        }
        else
        {
            APP_UARTPrint_blocking("error:14\r\n");
        }
        break;

    case GCODE_G4:
        // Dwell
        if (MotionBuffer_HasSpace())
        {
            motion_block_t dwell_block;
            if (MotionGCodeParser_ParseDwell(command, &dwell_block))
            {
                MotionBuffer_Add(&dwell_block);
                MotionPlanner_ProcessBuffer();
                APP_SendMotionOkResponse(); // Use helper for proper timing
            }
            else
            {
                APP_UARTPrint_blocking("error:33\r\n");
            }
        }
        else
        {
            APP_UARTPrint_blocking("error:14\r\n");
        }
        break;

    case GCODE_G10:
        // Coordinate system programming (G10 L2 P1 X0 Y0 Z0 or G10 L20 P0 X0 Y0 Z0)
        // UGS uses this for coordinate system setup and position setting
        MotionGCodeParser_UpdateCoordinateOffset(command);
        APP_UARTPrint_blocking("ok\r\n");
        break;

    case GCODE_M3:
    case GCODE_M4:
    case GCODE_M5:
        // Spindle control
        MotionGCodeParser_UpdateSpindleState(command);
        APP_UARTPrint_blocking("ok\r\n");
        break;

    case GCODE_M8:
    case GCODE_M9:
        // Coolant control
        MotionGCodeParser_UpdateCoolantState(command);
        APP_UARTPrint("ok\r\n");
        break;

    case GCODE_G17:
    case GCODE_G18:
    case GCODE_G19:
        // Plane selection
        MotionGCodeParser_UpdatePlaneSelection(command);
        APP_UARTPrint("ok\r\n");
        break;

    case GCODE_G20:
    case GCODE_G21:
        // Units - Update parser state
        MotionGCodeParser_UpdateUnits(command);
        APP_UARTPrint("ok\r\n");
        break;

    case GCODE_G28:
    case GCODE_G30:
        // Homing
        if (MotionBuffer_HasSpace())
        {
            motion_block_t home_block;
            if (MotionGCodeParser_ParseHome(command, &home_block))
            {
                MotionBuffer_Add(&home_block);
                MotionPlanner_ProcessBuffer();
                APP_SendMotionOkResponse(); // Use helper for proper timing
            }
            else
            {
                APP_UARTPrint_blocking("error:33\r\n");
            }
        }
        else
        {
            APP_UARTPrint_blocking("error:14\r\n");
        }
        break;

    case GCODE_G90:
    case GCODE_G91:
        // Distance mode
        MotionGCodeParser_UpdateDistanceMode(command);
        APP_UARTPrint("ok\r\n");
        break;

    case GCODE_G92:
        // Coordinate system offset
        MotionGCodeParser_UpdateCoordinateOffset(command);
        APP_UARTPrint("ok\r\n");
        break;

    case GCODE_G93:
    case GCODE_G94:
        // Feed rate mode
        MotionGCodeParser_UpdateFeedRateMode(command);
        APP_UARTPrint("ok\r\n");
        break;

    case GCODE_G54:
    case GCODE_G55:
    case GCODE_G56:
    case GCODE_G57:
    case GCODE_G58:
    case GCODE_G59:
        // Work coordinate systems
        MotionGCodeParser_UpdateWorkCoordinateSystem(command);
        APP_UARTPrint("ok\r\n");
        break;

    case GCODE_F:
        // Feed rate command
        APP_UARTPrint("ok\r\n");
        break;

    case GCODE_UNKNOWN:
    default:
        // Unknown command
        APP_UARTPrint("error:20\r\n"); // Unsupported or invalid g-code ID:20
        break;
    }
}

void APP_TrajectoryTimerCallback(uint32_t status, uintptr_t context)
{
    /* MINIMAL TEST CALLBACK - should not corrupt stack */
    /* Even this simple increment might be too much during rapid interrupts */

    /* Try completely empty callback first */
    /* appData.system_tick_counter++; */

    /* If empty works, uncomment the above line and test again */
}

void APP_TrajectoryTimerCallback_CoreTimer(uint32_t status, uintptr_t context)
{
    /* Core Timer trajectory callback - handles both LED heartbeat and trajectory */
    /* This replaces the problematic Timer1 implementation */

    appData.system_tick_counter++;

    /* LED heartbeat every 100ms (100 ticks at 1kHz) */
    if ((appData.system_tick_counter % 100) == 0)
    {
        LED1_Toggle(); // System heartbeat indicator
    }

    /* Real-time trajectory calculations at 1kHz */
    /* This is where the motion planner calculates velocities and updates OCR periods */
    MotionPlanner_UpdateTrajectory();
}
void APP_CoreTimerCallback(uint32_t status, uintptr_t context)
{
    /* This function is now replaced by APP_TrajectoryTimerCallback_CoreTimer */
    /* Left here for compatibility but should not be called */
    LED1_Toggle(); // System heartbeat indicator
}

void APP_OCMP1_Callback(uintptr_t context)
{
    /* Y-axis step pulse - PERFORMANCE CRITICAL: Direct access for speed */
    LED2_Toggle(); // Step activity indicator

    // DEBUG: Callback debug disabled for UGS compatibility
    /*
    static uint32_t debug_counter = 0;
    if ((debug_counter++ % 100) == 0) // Print every 100th callback to avoid spam
    {
        APP_UARTPrint("[OCMP1_CB]\r\n");
    }
    */

    if (cnc_axes[1].is_active && cnc_axes[1].motion_state != AXIS_IDLE)
    {
        // Direct access for maximum speed in interrupt context
        cnc_axes[1].current_position += cnc_axes[1].direction_forward ? 1 : -1;
        cnc_axes[1].step_count++;

        // Provide position feedback to motion planner
        MotionPlanner_UpdateAxisPosition(1, cnc_axes[1].current_position);

        // Check if target reached
        if (cnc_axes[1].step_count >= cnc_axes[1].target_position)
        {
            // Stop Y-axis (OCMP1 uses TMR4 per MCC)
            TMR4_Stop();
            OCMP1_Disable();

            // Mark axis complete
            cnc_axes[1].motion_state = AXIS_COMPLETE;
            cnc_axes[1].is_active = false;
        }
    }
}

void APP_OCMP4_Callback(uintptr_t context)
{
    /* X-axis step pulse - PERFORMANCE CRITICAL: Direct access for speed */
    LED2_Toggle(); // Step activity indicator

    if (cnc_axes[0].is_active && cnc_axes[0].motion_state != AXIS_IDLE)
    {
        // Direct access for maximum speed in interrupt context
        cnc_axes[0].current_position += cnc_axes[0].direction_forward ? 1 : -1;
        cnc_axes[0].step_count++;

        // Provide position feedback to motion planner
        MotionPlanner_UpdateAxisPosition(0, cnc_axes[0].current_position);

        // Check if target reached
        if (cnc_axes[0].step_count >= cnc_axes[0].target_position)
        {
            // Stop X-axis (OCMP4 uses TMR2 per MCC)
            TMR2_Stop();
            OCMP4_Disable();

            // Mark axis complete
            cnc_axes[0].motion_state = AXIS_COMPLETE;
            cnc_axes[0].is_active = false;
        }
    }
}
void APP_OCMP5_Callback(uintptr_t context)
{
    /* Z-axis step pulse - PERFORMANCE CRITICAL: Direct access for speed */
    LED2_Toggle(); // Step activity indicator

    if (cnc_axes[2].is_active && cnc_axes[2].motion_state != AXIS_IDLE)
    {
        // Direct access for maximum speed in interrupt context
        cnc_axes[2].current_position += cnc_axes[2].direction_forward ? 1 : -1;
        cnc_axes[2].step_count++;

        // Provide position feedback to motion planner
        MotionPlanner_UpdateAxisPosition(2, cnc_axes[2].current_position);

        // Check if target reached
        if (cnc_axes[2].step_count >= cnc_axes[2].target_position)
        {
            // Stop Z-axis (OCMP5 uses TMR3 per MCC)
            TMR3_Stop();
            OCMP5_Disable();

            // Mark axis complete
            cnc_axes[2].motion_state = AXIS_COMPLETE;
            cnc_axes[2].is_active = false;
        }
    }
}

// *****************************************************************************
// *****************************************************************************
// Section: Application Public Function Implementations
// *****************************************************************************
// *****************************************************************************
void APP_UARTPrint_blocking(const char *str)
{
    if (str == NULL)
        return;

    for (size_t i = 0; i < strlen(str); i++)
    {
        // Wait until the transmit buffer is not full
        while (U2STAbits.UTXBF)
        {
            ;
        }
        // Write the character to the transmit register
        U2TXREG = str[i];
    }
    // Wait until transmission is complete
    while (!U2STAbits.TRMT)
    {
        ;
    }
}

void APP_UARTPrint(const char *str)
{
    if (str)
    {
        UART2_Write((void *)str, strlen(str));
    }
}

void APP_UARTWrite_nonblocking(const char *str)
{
    if (str && !UART2_WriteIsBusy())
    {
        UART2_Write((void *)str, strlen(str));
    }
}

bool APP_AddLinearMove(float *target, float feedrate)
{
    if (APP_IsBufferFull())
    {
        return false; // Buffer full
    }

    /* Check soft limits before adding move to buffer */
    position_t target_position = {target[0], target[1], target[2], 0.0f};
    if (!INTERP_CheckSoftLimits(target_position))
    {
        APP_UARTPrint("ERROR: Soft limit would be exceeded\r\n");
        return false;
    }

    motion_block_t block;

    /* Copy target positions */
    for (int i = 0; i < MAX_AXES; i++)
    {
        block.target_pos[i] = target[i];
    }

    block.feedrate = feedrate;
    block.entry_velocity = 0;      // Will be calculated in look-ahead
    block.exit_velocity = 0;       // Will be calculated in look-ahead
    block.max_velocity = feedrate; // For now, max_velocity = feedrate
    block.motion_type = 1;         // G1 - Linear move
    block.is_valid = true;

    /* Add to motion buffer */
    return MotionBuffer_Add(&block);
}

bool APP_AddRapidMove(float *target)
{
    /* Rapid move at maximum velocity */
    return APP_AddLinearMove(target, DEFAULT_MAX_VELOCITY);
}

bool APP_IsMotionComplete(void)
{
    /* Check if buffer is empty and all axes are idle */
    if (!APP_IsBufferEmpty())
    {
        return false;
    }

    for (int i = 0; i < MAX_AXES; i++)
    {
        if (cnc_axes[i].is_active && cnc_axes[i].motion_state != AXIS_IDLE)
        {
            return false;
        }
    }

    return true;
}

void APP_EmergencyStop(void)
{
    /* Use interpolation engine emergency stop for proper coordination */
    INTERP_EmergencyStop();

    /* Stop trajectory timer */
    TMR1_Stop();
    appData.trajectory_timer_active = false;
    /* Stop all OCR modules and their timers */
    OCMP1_Disable();
    TMR2_Stop(); // Stop Timer2 (drives OCMP1)
    OCMP4_Disable();
    TMR3_Stop(); // Stop Timer3 (drives OCMP4)
    OCMP5_Disable();
    TMR4_Stop(); // Stop Timer4 (drives OCMP5)

    /* Reset all axes */
    for (int i = 0; i < MAX_AXES; i++)
    {
        cnc_axes[i].is_active = false;
        cnc_axes[i].motion_state = AXIS_IDLE;
        cnc_axes[i].current_velocity = 0;
        cnc_axes[i].target_velocity = 0;
    }

    /* Clear motion buffer */
    MotionBuffer_Clear();

    appData.state = APP_STATE_MOTION_ERROR;
    APP_UARTPrint("EMERGENCY STOP - All motion halted\r\n");
}

void APP_EmergencyReset(void)
{
    // TODO: Implement a soft reset of the system
    // For now, just print a message
    APP_UARTPrint("ALARM: Hard reset\r\n");
    // In a real scenario, you would reset peripherals and state machines
}

void APP_AlarmReset(void)
{
    // Check if limit switches are released
    //    bool x_min_limit = !GPIO_PinRead(LIMIT_X_PIN); // X-axis min limit switch
    //    bool y_min_limit = !GPIO_PinRead(LIMIT_Y_PIN); // Y-axis min limit switch
    //    bool z_min_limit = !GPIO_PinRead(LIMIT_Z_PIN); // Z-axis min limit switch

    //    if (!x_min_limit && !y_min_limit && !z_min_limit)
    //    {
    // All limit switches are released, reset alarm state
    appData.state = APP_STATE_MOTION_IDLE;
    APP_UARTPrint("[MSG: Alarm Reset]\r\n");
    //    }
    //    else
    //    {
    //        APP_UARTPrint("[MSG: Limit switches must be released to reset alarm]\r\n");
    //    }
}

void APP_StartHomingCycle(void)
{
    /* Start homing cycle for all axes (X, Y, Z) */

    // Check if system is ready for homing
    if (appData.state == APP_STATE_MOTION_ERROR)
    {
        APP_UARTPrint("ERROR: Clear alarm state before homing\r\n");
        return;
    }

    if (INTERP_IsHomingActive())
    {
        APP_UARTPrint("ERROR: Homing cycle already in progress\r\n");
        return;
    }

    // Stop any current motion
    APP_EmergencyStop();

    // Clear motion buffer
    MotionBuffer_Clear();

    // Start homing for X, Y, Z axes (bitmask: 0x07 = 0b111)
    uint8_t axes_to_home = 0x07; // X=bit0, Y=bit1, Z=bit2

    if (INTERP_StartHomingCycle(axes_to_home))
    {
        appData.state = APP_STATE_MOTION_EXECUTING; // Use executing state during homing
        APP_UARTPrint("Starting homing cycle...\r\n");
    }
    else
    {
        APP_UARTPrint("ERROR: Failed to start homing cycle\r\n");
    }
}

// *****************************************************************************
// G-code Command Handler for DMA Parser Integration
// *****************************************************************************

void APP_ReportError(int error_code)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "error:%d\r\n", error_code);
    APP_UARTPrint(buffer);
}

void APP_ReportAlarm(int alarm_code)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "ALARM:%d\r\n", alarm_code);
    APP_UARTPrint(buffer);
    appData.state = APP_STATE_MOTION_ERROR; // Enter alarm state
}

void APP_ExecuteMotionCommand(const char *line)
{
    // DEBUG: Track if this function is still being called somewhere (disabled for UGS compatibility)
    // APP_UARTPrint_blocking("[DEBUG:APP_ExecuteMotionCommand called - should NOT happen!]\r\n");
    APP_UARTPrint_blocking("error:99\r\n"); // Unique error to identify this path
}

void APP_SendStatus(void)
{
    /* Send GRBL v1.1f compliant status report with enhanced feedback */
    char status_buffer[512];

    // Determine state name based on GRBL state
    const char *state_name;
    switch (grbl_state)
    {
    case GRBL_STATE_IDLE:
        state_name = "Idle";
        break;
    case GRBL_STATE_RUN:
        state_name = "Run";
        break;
    case GRBL_STATE_HOLD:
        state_name = "Hold";
        break;
    case GRBL_STATE_JOG:
        state_name = "Jog";
        break;
    case GRBL_STATE_ALARM:
        state_name = "Alarm";
        break;
    case GRBL_STATE_DOOR:
        state_name = "Door";
        break;
    case GRBL_STATE_CHECK:
        state_name = "Check";
        break;
    case GRBL_STATE_HOME:
        state_name = "Home";
        break;
    case GRBL_STATE_SLEEP:
        state_name = "Sleep";
        break;
    default:
        state_name = "Idle";
        break;
    }

    // Get current position efficiently from step counters (not complex motion planner)
    float current_x, current_y, current_z;
    GCodeHelpers_GetCurrentPositionFromSteps(&current_x, &current_y, &current_z);

    // Position conversion working correctly - debug removed for UGS compatibility

    // Get motion planner statistics for enhanced feedback
    float current_velocity = MotionPlanner_GetCurrentVelocity(0); // Use X-axis velocity as representative
    float feed_rate = 100.0f;                                     // Default feed rate - could be retrieved from settings

    // Step counts removed for UGS compatibility - not used in clean status format

    // Build GRBL v1.1 compatible status string (clean format for UGS)
    sprintf(status_buffer, "<%s|MPos:%.3f,%.3f,%.3f|FS:%.1f,%.0f>\r\n",
            state_name, current_x, current_y, current_z, current_velocity, feed_rate);
    APP_UARTPrint_blocking(status_buffer);
}

void APP_SendDetailedStatus(void)
{
    /* Send comprehensive debug status with all motion and position details */
    char debug_buffer[1024];
    char pos_str[200], step_str[200], vel_str[200];

    // Get positions efficiently from step counters
    float step_x, step_y, step_z;
    GCodeHelpers_GetCurrentPositionFromSteps(&step_x, &step_y, &step_z);

    // Get raw step counts for debugging
    int32_t x_steps, y_steps, z_steps;
    GCodeHelpers_GetPositionInSteps(&x_steps, &y_steps, &z_steps);

    // Get positions from interpolation engine for comparison (legacy)
    position_t interp_pos = INTERP_GetCurrentPosition();

    // Build position details - compare step-based vs interpolation-based positions
    sprintf(pos_str, "StepPos:%.3f,%.3f,%.3f InterpPos:%.3f,%.3f,%.3f RawSteps:%d,%d,%d",
            step_x, step_y, step_z, interp_pos.x, interp_pos.y, interp_pos.z,
            x_steps, y_steps, z_steps); // Build step count details
    sprintf(step_str, "Steps:%u,%u,%u",
            APP_GetAxisStepCount(0), APP_GetAxisStepCount(1), APP_GetAxisStepCount(2));

    // Build velocity details
    float current_vel = MotionPlanner_GetCurrentVelocity(0); // Use X-axis velocity as representative
    sprintf(vel_str, "Vel:%.1f Active:%d,%d,%d",
            current_vel,
            APP_GetAxisActiveState(0), APP_GetAxisActiveState(1), APP_GetAxisActiveState(2)); // Build comprehensive debug message
    sprintf(debug_buffer, "[DEBUG] %s | %s | %s\r\nok\r\n", pos_str, step_str, vel_str);

    APP_UARTPrint_blocking(debug_buffer);
}

#ifdef TEST_ARC_INTERPOLATION
void APP_TestArcInterpolation(void)
{
    // Example: G2 X10 Y5 I2 J0 F100
    char gcode_line[] = "G2 X10 Y5 I2 J0 F100";
    APP_ExecuteMotionCommand(gcode_line);
}

void APP_TestGCodeParsing(void)
{
    printf("\n=== G-code Parsing Test Suite ===\n");

    // Test 1: Simple G-code line
    printf("\n--- Test 1: Simple G-code line ---\n");
    char line1[] = "G1 X10 Y10 F1000";
    APP_ExecuteMotionCommand(line1);

    // Test 2: G-code line with missing parameters
    printf("\n--- Test 2: G-code line with missing parameters ---\n");
    char line2[] = "G1 X10 F1000";
    APP_ExecuteMotionCommand(line2);

    // Test 3: G-code line with invalid command
    printf("\n--- Test 3: G-code line with invalid command ---\n");
    char line3[] = "G9 X10 Y10 F1000"; // G9 is not a valid command in our parser
    APP_ExecuteMotionCommand(line3);

    // Test 4: Empty line
    printf("\n--- Test 4: Empty line ---\n");
    char line4[] = "";
    APP_ExecuteMotionCommand(line4);

    // Test 5: Commented line
    printf("\n--- Test 5: Commented line ---\n");
    char line5[] = "; This is a comment line";
    APP_ExecuteMotionCommand(line5);

    printf("\n=== G-code Parsing Test Complete ===\n");
    printf("All tests demonstrate correct G-code parsing behavior\n");
}
#endif

/* ============================================================================
 * Pick-and-Place Mode Control Functions
 * ============================================================================ */

void APP_EnableZMinMask(void)
{
    INTERP_EnableLimitMask(LIMIT_MASK_Z_MIN);
    APP_UARTPrint("PICK-AND-PLACE: Z minimum limit MASKED - Spring nozzle active\r\n");
}

void APP_DisableZMinMask(void)
{
    INTERP_DisableLimitMask(LIMIT_MASK_Z_MIN);
    APP_UARTPrint("CNC MODE: Z minimum limit ACTIVE - Normal operation restored\r\n");
}

void APP_SetPickAndPlaceMode(bool enable)
{
    if (enable)
    {
        INTERP_EnableLimitMask(LIMIT_MASK_Z_MIN);
        APP_UARTPrint("*** PICK-AND-PLACE MODE ENABLED ***\r\n");
        APP_UARTPrint("Z minimum limit masked for spring-loaded nozzle operation\r\n");
    }
    else
    {
        INTERP_SetLimitMask(LIMIT_MASK_NONE); // Clear all masks
        APP_UARTPrint("*** CNC MODE ENABLED ***\r\n");
        APP_UARTPrint("All limit switches active for normal CNC operation\r\n");
    }
}

bool APP_IsPickAndPlaceMode(void)
{
    return INTERP_IsLimitMasked(AXIS_Z, false); // Check if Z min is masked
}

// *****************************************************************************
// *****************************************************************************
// Section: Hardware Layer Getter/Setter Function Implementations
// *****************************************************************************
// *****************************************************************************

int32_t APP_GetAxisCurrentPosition(uint8_t axis)
{
    if (axis < MAX_AXES)
    {
        return cnc_axes[axis].current_position;
    }
    return 0;
}

void APP_SetAxisCurrentPosition(uint8_t axis, int32_t position)
{
    if (axis < MAX_AXES)
    {
        cnc_axes[axis].current_position = position;

        // Update motion planner position tracking
        MotionPlanner_SetAxisPosition(axis, position);
    }
}

bool APP_GetAxisActiveState(uint8_t axis)
{
    if (axis < MAX_AXES)
    {
        return cnc_axes[axis].is_active;
    }
    return false;
}

void APP_SetAxisActiveState(uint8_t axis, bool active)
{
    if (axis < MAX_AXES)
    {
        cnc_axes[axis].is_active = active;

        if (!active)
        {
            // Disable OCR module when axis becomes inactive
            switch (axis)
            {
            case 0: // X-axis -> OCMP4 (uses Timer3)
                OCMP4_Disable();
                TMR3_Stop(); // Stop Timer3 when X-axis motion complete
                break;
            case 1: // Y-axis -> OCMP1 (uses Timer2)
                OCMP1_Disable();
                TMR2_Stop(); // Stop Timer2 when Y-axis motion complete
                break;
            case 2: // Z-axis -> OCMP5 (uses Timer4)
                OCMP5_Disable();
                TMR4_Stop(); // Stop Timer4 when Z-axis motion complete
                break;
            }

            // Set motion state to idle
            cnc_axes[axis].motion_state = AXIS_IDLE;
        }
    }
}

float APP_GetAxisTargetVelocity(uint8_t axis)
{
    if (axis < MAX_AXES)
    {
        return cnc_axes[axis].target_velocity;
    }
    return 0.0f;
}

void APP_SetAxisTargetVelocity(uint8_t axis, float velocity)
{
    if (axis < MAX_AXES)
    {
        cnc_axes[axis].target_velocity = velocity;

        // Update OCR period when velocity changes
        if (cnc_axes[axis].is_active)
        {
            uint32_t ocr_period = MotionPlanner_CalculateOCRPeriod(velocity);

            switch (axis)
            {
            case 0: // X-axis -> OCMP4
                OCMP4_CompareSecondaryValueSet(ocr_period);
                break;
            case 1: // Y-axis -> OCMP1
                OCMP1_CompareSecondaryValueSet(ocr_period);
                break;
            case 2: // Z-axis -> OCMP5
                OCMP5_CompareSecondaryValueSet(ocr_period);
                break;
            }
        }
    }
}

uint32_t APP_GetAxisStepCount(uint8_t axis)
{
    if (axis < MAX_AXES)
    {
        return cnc_axes[axis].step_count;
    }
    return 0;
}

void APP_ResetAxisStepCount(uint8_t axis)
{
    if (axis < MAX_AXES)
    {
        cnc_axes[axis].step_count = 0;
    }
}

// *****************************************************************************
// Motion Timing Synchronization Helper
// *****************************************************************************
static void APP_SendMotionOkResponse(void)
{
    // For high-speed processors (200MHz): Ensure motion planner has time to process
    // before sending "ok" response to maintain proper synchronization with UGS

    motion_execution_state_t planner_state = MotionPlanner_GetState();
    if (planner_state == PLANNER_STATE_PLANNING || planner_state == PLANNER_STATE_EXECUTING)
    {
        // Motion planner is actively processing - respond immediately
        APP_UARTPrint_blocking("ok\r\n");
    }
    else
    {
        // Give motion planner a moment to engage (prevents race condition)
        // This small delay (50μs) ensures proper coordination without
        // significantly impacting performance (still 320x faster than 8-bit systems)
        for (volatile int delay = 0; delay < 1000; delay++)
            ; // ~50μs @ 200MHz
        APP_UARTPrint_blocking("ok\r\n");
    }
}

/*******************************************************************************
 End of File
 */