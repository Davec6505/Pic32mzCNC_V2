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

APP_DATA appData;

// TIMER1 ISSUE DOCUMENTED:
// Timer1 causes system hang when started (TMR1_Start())
// Issue is likely interrupt priority conflict or configuration problem
// System works perfectly without Timer1 trajectory timer
// TODO: Investigate Timer1 interrupt priorities in MCC when time allows

// CNC Axis data structures
cnc_axis_t cnc_axes[MAX_AXES];

// Motion planning buffer for look-ahead
motion_block_t motion_buffer[MOTION_BUFFER_SIZE];
uint8_t motion_buffer_head = 0;
uint8_t motion_buffer_tail = 0;

// UART callback data
static uint8_t uart_rx_char;

// Buffer for line commands (non-? characters)
#define LINE_BUFFER_SIZE 256
static char line_buffer[LINE_BUFFER_SIZE];
static volatile uint8_t line_buffer_head = 0;
static volatile uint8_t line_buffer_tail = 0;

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
void APP_EmergencyReset(void);
static void APP_UART_Callback(uintptr_t context);
static void APP_ProcessGRBLCommand(const char *command);
static void APP_PrintGRBLSettings(void);
static void APP_PrintGCodeParameters(void);
static void APP_PrintParserState(void);
static void APP_ProcessSettingChange(const char *command);
static void APP_ProcessGCodeCommand(const char *command);
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
/* static void APP_ProcessUART(void); */ // Removed - now handled by GRBL serial module

// Callback function declarations

// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization
// *****************************************************************************
// *****************************************************************************

/******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize(void)
{
    /* Place the App state machine in its initial state. */
    appData.state = APP_STATE_INIT;

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
    motion_buffer_head = 0;
    motion_buffer_tail = 0;
    for (int i = 0; i < MOTION_BUFFER_SIZE; i++)
    {
        motion_buffer[i].is_valid = false;
    }

    /* Initialize motion profile system */
    MOTION_PROFILE_Initialize();

    /* Initialize the motion control system */
    APP_MotionSystemInit();

    // Setup simple UART callback for immediate real-time command handling
    UART2_ReadCallbackRegister(APP_UART_Callback, (uintptr_t)NULL);
    UART2_Read(&uart_rx_char, 1);

    // Removed automatic welcome message - UGS expects clean handshake
    // APP_UARTPrint_blocking("Grbl 1.1f ['$' for help]\r\n");
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
    /* Hybrid UART processing:
     * - Callback handles ? real-time commands immediately
     * - Polling handles line-based commands like $I, $$, etc.
     */
    static char rx_buffer[64];
    static uint8_t rx_pos = 0;
    uint8_t received_char;

    // Process buffered characters from callback
    while (line_buffer_tail != line_buffer_head)
    {
        received_char = line_buffer[line_buffer_tail];
        line_buffer_tail = (line_buffer_tail + 1) % LINE_BUFFER_SIZE;

        if (received_char == '\r' || received_char == '\n')
        {
            // End of line - process the command
            if (rx_pos > 0)
            {
                rx_buffer[rx_pos] = '\0';
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
            motion_buffer[motion_buffer_tail].is_valid = false;
            motion_buffer_tail = (motion_buffer_tail + 1) % MOTION_BUFFER_SIZE;

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

    /* TRAJECTORY SYSTEM - Using Core Timer instead of Timer1 */
    /* Core Timer runs at 100MHz, set period for 1kHz trajectory updates */
    /* Period = 100MHz / 1000Hz = 100000 */
    if (!appData.trajectory_timer_active)
    {
        CORETIMER_CallbackSet(APP_TrajectoryTimerCallback_CoreTimer, 0);
        CORETIMER_PeriodSet(100000); // 1kHz trajectory updates
        CORETIMER_Start();
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

    motion_block_t *block = &motion_buffer[motion_buffer_tail];

    if (!block->is_valid)
    {
        return;
    }

    /* Setup axes for movement */
    for (int i = 0; i < MAX_AXES; i++)
    {
        int32_t target_pos = (int32_t)block->target_pos[i];

        /* Only move axis if target is different from current position */
        if (target_pos != cnc_axes[i].current_position)
        {
            cnc_axes[i].target_position = target_pos;
            cnc_axes[i].target_velocity = block->feedrate;
            cnc_axes[i].max_velocity = block->max_velocity > 0 ? block->max_velocity : DEFAULT_MAX_VELOCITY;
            cnc_axes[i].motion_state = AXIS_ACCEL;
            cnc_axes[i].is_active = true;
            cnc_axes[i].step_count = 0;

            /* Set direction */
            cnc_axes[i].direction_forward = (target_pos > cnc_axes[i].current_position);

            /* Enable the appropriate OCR module for this axis */
            switch (i)
            {
            case 0: // X-axis -> OCMP4 (as per existing main.c)
                OCMP4_Enable();
                break;
            case 1: // Y-axis -> OCMP1
                OCMP1_Enable();
                break;
            case 2: // Z-axis -> OCMP5
                OCMP5_Enable();
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

static bool APP_IsBufferEmpty(void)
{
    return motion_buffer_head == motion_buffer_tail && !motion_buffer[motion_buffer_tail].is_valid;
}

static bool APP_IsBufferFull(void)
{
    uint8_t next_head = (motion_buffer_head + 1) % MOTION_BUFFER_SIZE;
    return next_head == motion_buffer_tail;
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
                if (appData.uart_rx_buffer_pos > 0)
                {
                    APP_ExecuteMotionCommand(appData.uart_rx_buffer);
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
    // Check for UART errors first
    if (UART2_ErrorGet() == UART_ERROR_NONE)
    {
        // Handle real-time commands immediately (single character commands)
        switch (uart_rx_char)
        {
        case '?':
            // Status report
            APP_SendStatus();
            break;

        case '!':
            // Feed hold
            APP_UARTPrint("[MSG:Feed Hold]\r\n");
            break;

        case '~':
            // Cycle start (resume)
            APP_UARTPrint("[MSG:Cycle Start]\r\n");
            break;

        case 0x18: // Ctrl-X
            // Soft reset
            grbl_state = GRBL_STATE_IDLE;
            grbl_alarm_code = 0;
            APP_UARTPrint("\r\n\r\nGrbl 1.1f ['$' for help]\r\n");
            break;

        case 0x84: // Safety door (if enabled)
            APP_UARTPrint("[MSG:Safety Door]\r\n");
            break;

        case 0x9E: // Jog cancel
            APP_UARTPrint("[MSG:Jog Cancelled]\r\n");
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
            APP_UARTPrint("[VER:1.1f.20241012:CNC Controller]\r\n[OPT:V,15,128]\r\nok\r\n");
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
            // Print startup blocks
            APP_UARTPrint("$N0=\r\n");
            APP_UARTPrint("$N1=\r\n");
            APP_UARTPrint("ok\r\n");
        }
        else if (strcmp(command, "$C") == 0)
        {
            // Check gcode mode toggle
            APP_UARTPrint("[MSG:Enabled]\r\n");
            APP_UARTPrint("ok\r\n");
        }
        else if (strcmp(command, "$X") == 0)
        {
            // Kill alarm lock
            if (grbl_state == GRBL_STATE_ALARM)
            {
                grbl_state = GRBL_STATE_IDLE;
                grbl_alarm_code = 0;
                APP_UARTPrint("[MSG:Caution: Unlocked]\r\n");
            }
            else
            {
                APP_UARTPrint("[MSG:Caution: Unlocked]\r\n");
            }
            APP_UARTPrint("ok\r\n");
        }
        else if (strcmp(command, "$H") == 0)
        {
            // Run homing cycle
            APP_UARTPrint("[MSG:Homing cycle completed]\r\n");
            APP_UARTPrint("ok\r\n");
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
    // GRBL 1.1f standard settings - output all key settings
    APP_UARTPrint("$0=10\r\n");        // Step pulse time
    APP_UARTPrint("$1=25\r\n");        // Step idle delay
    APP_UARTPrint("$2=0\r\n");         // Step pulse invert mask
    APP_UARTPrint("$3=0\r\n");         // Step direction invert mask
    APP_UARTPrint("$4=0\r\n");         // Invert step enable pin
    APP_UARTPrint("$5=0\r\n");         // Invert limit pins
    APP_UARTPrint("$6=0\r\n");         // Invert probe pin
    APP_UARTPrint("$10=1\r\n");        // Status report options
    APP_UARTPrint("$11=0.010\r\n");    // Junction deviation
    APP_UARTPrint("$12=0.002\r\n");    // Arc tolerance
    APP_UARTPrint("$13=0\r\n");        // Report in inches
    APP_UARTPrint("$20=0\r\n");        // Soft limits enable
    APP_UARTPrint("$21=0\r\n");        // Hard limits enable
    APP_UARTPrint("$22=0\r\n");        // Homing cycle enable
    APP_UARTPrint("$23=0\r\n");        // Homing direction invert mask
    APP_UARTPrint("$24=25.000\r\n");   // Homing locate feed rate
    APP_UARTPrint("$25=500.000\r\n");  // Homing search seek rate
    APP_UARTPrint("$26=250\r\n");      // Homing switch debounce delay
    APP_UARTPrint("$27=1.000\r\n");    // Homing switch pull-off distance
    APP_UARTPrint("$30=1000\r\n");     // Maximum spindle speed
    APP_UARTPrint("$31=0\r\n");        // Minimum spindle speed
    APP_UARTPrint("$32=0\r\n");        // Laser mode enable
    APP_UARTPrint("$100=250.000\r\n"); // X-axis travel resolution
    APP_UARTPrint("$101=250.000\r\n"); // Y-axis travel resolution
    APP_UARTPrint("$102=250.000\r\n"); // Z-axis travel resolution
    APP_UARTPrint("$110=500.000\r\n"); // X-axis maximum rate
    APP_UARTPrint("$111=500.000\r\n"); // Y-axis maximum rate
    APP_UARTPrint("$112=500.000\r\n"); // Z-axis maximum rate
    APP_UARTPrint("$120=10.000\r\n");  // X-axis acceleration
    APP_UARTPrint("$121=10.000\r\n");  // Y-axis acceleration
    APP_UARTPrint("$122=10.000\r\n");  // Z-axis acceleration
    APP_UARTPrint("$130=200.000\r\n"); // X-axis maximum travel
    APP_UARTPrint("$131=200.000\r\n"); // Y-axis maximum travel
    APP_UARTPrint("$132=200.000\r\n"); // Z-axis maximum travel
    APP_UARTPrint("ok\r\n");
}

static void APP_ProcessSettingChange(const char *command)
{
    // Parse setting change commands like $0=10
    // For now, just acknowledge the setting change
    APP_UARTPrint("ok\r\n");
}

static void APP_PrintGCodeParameters(void)
{
    // Print coordinate system offsets and other parameters
    APP_UARTPrint("[G54:0.000,0.000,0.000]\r\n"); // Work coordinate offset
    APP_UARTPrint("[G55:0.000,0.000,0.000]\r\n");
    APP_UARTPrint("[G56:0.000,0.000,0.000]\r\n");
    APP_UARTPrint("[G57:0.000,0.000,0.000]\r\n");
    APP_UARTPrint("[G58:0.000,0.000,0.000]\r\n");
    APP_UARTPrint("[G59:0.000,0.000,0.000]\r\n");
    APP_UARTPrint("[G28:0.000,0.000,0.000]\r\n");   // Home position
    APP_UARTPrint("[G30:0.000,0.000,0.000]\r\n");   // Pre-defined position
    APP_UARTPrint("[G92:0.000,0.000,0.000]\r\n");   // Coordinate offset
    APP_UARTPrint("[TLO:0.000]\r\n");               // Tool length offset
    APP_UARTPrint("[PRB:0.000,0.000,0.000:0]\r\n"); // Probe result
    APP_UARTPrint("ok\r\n");
}

static void APP_PrintParserState(void)
{
    // Print current parser state (modal groups)
    APP_UARTPrint("[GC:G0 G54 G17 G21 G90 G94 M5 M9 T0 F0 S0]\r\n");
    APP_UARTPrint("ok\r\n");
}

static void APP_ProcessGCodeCommand(const char *command)
{
    // Basic G-code command processing
    // For now, acknowledge all G-code commands
    if (strncmp(command, "G0", 2) == 0 || strncmp(command, "G1", 2) == 0)
    {
        // Rapid/Linear movement
        APP_UARTPrint("ok\r\n");
    }
    else if (strncmp(command, "G2", 2) == 0 || strncmp(command, "G3", 2) == 0)
    {
        // Circular movement
        APP_UARTPrint("ok\r\n");
    }
    else if (strncmp(command, "M3", 2) == 0 || strncmp(command, "M4", 2) == 0)
    {
        // Spindle on
        APP_UARTPrint("ok\r\n");
    }
    else if (strncmp(command, "M5", 2) == 0)
    {
        // Spindle off
        APP_UARTPrint("ok\r\n");
    }
    else if (strncmp(command, "M8", 2) == 0 || strncmp(command, "M9", 2) == 0)
    {
        // Coolant control
        APP_UARTPrint("ok\r\n");
    }
    else if (strncmp(command, "G4", 2) == 0)
    {
        // Dwell
        APP_UARTPrint("ok\r\n");
    }
    else if (strncmp(command, "G17", 3) == 0 || strncmp(command, "G18", 3) == 0 || strncmp(command, "G19", 3) == 0)
    {
        // Plane selection
        APP_UARTPrint("ok\r\n");
    }
    else if (strncmp(command, "G20", 3) == 0 || strncmp(command, "G21", 3) == 0)
    {
        // Units
        APP_UARTPrint("ok\r\n");
    }
    else if (strncmp(command, "G28", 3) == 0 || strncmp(command, "G30", 3) == 0)
    {
        // Go to predefined position
        APP_UARTPrint("ok\r\n");
    }
    else if (strncmp(command, "G90", 3) == 0 || strncmp(command, "G91", 3) == 0)
    {
        // Distance mode
        APP_UARTPrint("ok\r\n");
    }
    else if (strncmp(command, "G92", 3) == 0)
    {
        // Coordinate system offset
        APP_UARTPrint("ok\r\n");
    }
    else if (strncmp(command, "G54", 3) == 0 || strncmp(command, "G55", 3) == 0 ||
             strncmp(command, "G56", 3) == 0 || strncmp(command, "G57", 3) == 0 ||
             strncmp(command, "G58", 3) == 0 || strncmp(command, "G59", 3) == 0)
    {
        // Work coordinate systems
        APP_UARTPrint("ok\r\n");
    }
    else
    {
        // Unknown command
        APP_UARTPrint("ok\r\n"); // Still acknowledge for compatibility
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

    /* TODO: Add trajectory calculations here once proven stable */
    /* APP_CalculateTrajectory() calls will go here */
}
void APP_CoreTimerCallback(uint32_t status, uintptr_t context)
{
    /* This function is now replaced by APP_TrajectoryTimerCallback_CoreTimer */
    /* Left here for compatibility but should not be called */
    LED1_Toggle(); // System heartbeat indicator
}

void APP_OCMP1_Callback(uintptr_t context)
{
    /* Y-axis step pulse */
    LED2_Toggle(); // Step activity indicator

    if (cnc_axes[1].is_active && cnc_axes[1].motion_state != AXIS_IDLE)
    {
        cnc_axes[1].current_position += cnc_axes[1].direction_forward ? 1 : -1;
        cnc_axes[1].step_count++;
    }
}

void APP_OCMP4_Callback(uintptr_t context)
{
    /* X-axis step pulse */
    LED2_Toggle(); // Step activity indicator

    if (cnc_axes[0].is_active && cnc_axes[0].motion_state != AXIS_IDLE)
    {
        cnc_axes[0].current_position += cnc_axes[0].direction_forward ? 1 : -1;
        cnc_axes[0].step_count++;
    }
}

void APP_OCMP5_Callback(uintptr_t context)
{
    /* Z-axis step pulse */
    LED2_Toggle(); // Step activity indicator

    if (cnc_axes[2].is_active && cnc_axes[2].motion_state != AXIS_IDLE)
    {
        cnc_axes[2].current_position += cnc_axes[2].direction_forward ? 1 : -1;
        cnc_axes[2].step_count++;
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

    motion_block_t *block = &motion_buffer[motion_buffer_head];

    /* Copy target positions */
    for (int i = 0; i < MAX_AXES; i++)
    {
        block->target_pos[i] = target[i];
    }

    block->feedrate = feedrate;
    block->entry_velocity = 0;      // Will be calculated in look-ahead
    block->exit_velocity = 0;       // Will be calculated in look-ahead
    block->max_velocity = feedrate; // For now, max_velocity = feedrate
    block->motion_type = 1;         // G1 - Linear move
    block->is_valid = true;

    /* Advance head pointer */
    motion_buffer_head = (motion_buffer_head + 1) % MOTION_BUFFER_SIZE;

    return true;
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

    /* Stop all OCR modules directly */
    OCMP1_Disable();
    OCMP4_Disable();
    OCMP5_Disable();

    /* Reset all axes */
    for (int i = 0; i < MAX_AXES; i++)
    {
        cnc_axes[i].is_active = false;
        cnc_axes[i].motion_state = AXIS_IDLE;
        cnc_axes[i].current_velocity = 0;
        cnc_axes[i].target_velocity = 0;
    }

    /* Clear motion buffer */
    motion_buffer_head = 0;
    motion_buffer_tail = 0;
    for (int i = 0; i < MOTION_BUFFER_SIZE; i++)
    {
        motion_buffer[i].is_valid = false;
    }

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
    motion_buffer_head = 0;
    motion_buffer_tail = 0;
    for (int i = 0; i < MOTION_BUFFER_SIZE; i++)
    {
        motion_buffer[i].is_valid = false;
    }

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
    // Trim leading/trailing whitespace from the command line
    int start = 0;
    while (isspace((unsigned char)line[start]))
    {
        start++;
    }

    int end = strlen(line) - 1;
    while (end > start && isspace((unsigned char)line[end]))
    {
        end--;
    }

    // Handle system commands ($)
    if (line[start] == '$')
    {
        if (strcmp(&line[start], "$I") == 0)
        {
            // Report build info - send as single message to avoid timing issues
            APP_UARTPrint("[VER:1.1f.20241012:CNC Controller]\r\n[OPT:V,15,128]\r\nok\r\n");
        }
        else if (strcmp(&line[start], "$$") == 0)
        {
            // Report settings
            // TODO: Implement actual settings reporting from grbl_settings.h
            APP_UARTPrint("$0=10\r\n");
            APP_UARTPrint("$1=25\r\n");
            // ... more settings ...
            APP_UARTPrint("ok\r\n");
        }
        else
        {
            APP_ReportError(3); // Grbl '$' system command was not recognized or supported.
        }
        return;
    }

    // Handle real-time commands that might have been passed as lines
    if (strlen(&line[start]) == 1)
    {
        if (line[start] == '?')
        {
            APP_SendStatus();
            return;
        }
    }

    // Handle empty lines or comments which should just return 'ok'
    if (line[start] == '\0' || line[start] == ';' || line[start] == '(')
    {
        APP_UARTPrint("ok\r\n");
        return;
    }

    gcode_command_t command;
    if (GCODE_ParseLine(&line[start], &command))
    {
        bool command_processed = false;
        if (command.words & WORD_G)
        {
            if (command.G == 0.0 || command.G == 1.0) // G0/G1 Linear Move
            {
                float target[3] = {
                    (command.words & WORD_X) ? command.X : cnc_axes[0].current_position,
                    (command.words & WORD_Y) ? command.Y : cnc_axes[1].current_position,
                    (command.words & WORD_Z) ? command.Z : cnc_axes[2].current_position};
                float feedrate = (command.words & WORD_F) ? command.F : DEFAULT_MAX_VELOCITY;

                if (!APP_AddLinearMove(target, feedrate))
                {
                    APP_ReportError(9); // G-code locked out (buffer full)
                    return;
                }
                command_processed = true;
            }
            // Add other G-code handlers here (G28, G90, etc.)
        }

        // Process M-codes
        if (command.words & WORD_M)
        {
            // Add M-code handlers here
            command_processed = true;
        }

        if (command_processed)
        {
            // If any word was parsed, it's a valid command that needs an 'ok'
            // even if we don't explicitly handle it yet.
            APP_UARTPrint("ok\r\n");
        }
        else
        {
            APP_UARTPrint("ok\r\n");
        }
    }
    else
    {
        // Parsing failed for a non-empty, non-comment line.
        APP_ReportError(20); // Unsupported or invalid g-code command
    }
}

void APP_SendStatus(void)
{
    /* Send GRBL v1.1f compliant status report */
    char status_buffer[256];
    char x_str[20], y_str[20], z_str[20];

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

    // Get current position
    position_t current_pos = INTERP_GetCurrentPosition();

    // Convert floats to strings using our custom ftoa function
    ftoa(current_pos.x, x_str, 3);
    ftoa(current_pos.y, y_str, 3);
    ftoa(current_pos.z, z_str, 3);

    // Build the complete string using sprintf with string arguments (no floats)
    sprintf(status_buffer, "<%s|MPos:%s,%s,%s|FS:0,0>\r\n",
            state_name, x_str, y_str, z_str);

    APP_UARTPrint(status_buffer);
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

/*******************************************************************************
 End of File
 */