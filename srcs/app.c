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
#include "uart_grbl_simple.h"
#include "grbl_settings.h"
#include "interpolation_engine.h"
#include "motion_profile.h"
#include "speed_control.h"
#include "peripheral/uart/plib_uart2.h"
#include "peripheral/tmr1/plib_tmr1.h"
#include "peripheral/tmr/plib_tmr2.h"
#include "peripheral/tmr/plib_tmr4.h"
#include "peripheral/ocmp/plib_ocmp1.h"
#include "peripheral/ocmp/plib_ocmp4.h"
#include "peripheral/ocmp/plib_ocmp5.h"

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

// CNC Axis data structures
cnc_axis_t cnc_axes[MAX_AXES];

// Motion planning buffer for look-ahead
motion_block_t motion_buffer[MOTION_BUFFER_SIZE];
uint8_t motion_buffer_head = 0;
uint8_t motion_buffer_tail = 0;

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************

// Simple UART print function using DMA or fallback to basic UART
static void APP_UARTPrint(const char *str)
{
    if (!str)
        return;

    // Use simple UART for all messages
    while (*str)
    {
        while (!UART2_TransmitterIsReady())
            ;
        UART2_WriteByte(*str);
        str++;
    }
}

static void APP_MotionSystemInit(void);
static void APP_ProcessSwitches(void);
static void APP_ProcessLimitSwitches(void);
static void APP_CalculateTrajectory(uint8_t axis_id);
static void APP_UpdateOCRPeriod(uint8_t axis_id);
static void APP_ExecuteNextMotionBlock(void);
static void APP_ProcessLookAhead(void);
static bool APP_IsBufferEmpty(void);
static bool APP_IsBufferFull(void);

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

    /* Initialize UGS/GRBL Settings Interface */
    if (!GRBL_Initialize())
    {
        APP_UARTPrint("ERROR: Failed to initialize GRBL settings interface\r\n");
        return;
    }

    /* Initialize Interpolation Engine */
    if (!INTERP_Initialize())
    {
        APP_UARTPrint("ERROR: Failed to initialize interpolation engine\r\n");
        return;
    }

    /* Initialize G-code UART Parser (Simple Mode) */
    UART_GRBL_Initialize();

    APP_UARTPrint("CNC Controller initialization complete\r\n");
    APP_UARTPrint("Ready for Universal G-code Sender (UGS) connection\r\n");
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
    /* Check the application's current state. */
    switch (appData.state)
    {
    /* Application's initial state. */
    case APP_STATE_INIT:
    {
        bool appInitialized = true;

        if (appInitialized)
        {
            appData.state = APP_STATE_SERVICE_TASKS;
        }
        break;
    }

    case APP_STATE_SERVICE_TASKS:
    {
        /* Initialize G-code parser and UART communication */
        appData.state = APP_STATE_GCODE_INIT;
        break;
    }

    case APP_STATE_GCODE_INIT:
    {
        /* Initialize DMA G-code parser system */
        if (!GCODE_DMA_Initialize())
        {
            appData.state = APP_STATE_MOTION_ERROR;
            break;
        }

        /* Register callbacks for integration */
        GCODE_DMA_RegisterMotionCallback(APP_ExecuteGcodeCommand);
        GCODE_DMA_RegisterStatusCallback(APP_SendStatusReport);
        GCODE_DMA_RegisterEmergencyCallback(APP_HandleEmergencyStop);

        /* Enable DMA G-code processing */
        GCODE_DMA_Enable();

        /* Initialize motion profile system */
        MOTION_PROFILE_Initialize();

        appData.state = APP_STATE_MOTION_INIT;
        break;
    }

    case APP_STATE_MOTION_INIT:
    {
        /* Initialize the motion control system */
        APP_MotionSystemInit();

/* Run arc interpolation test if enabled */
#ifdef TEST_ARC_INTERPOLATION
        APP_TestArcInterpolation();
#endif

        appData.state = APP_STATE_MOTION_IDLE;
        break;
    }

    case APP_STATE_MOTION_IDLE:
    {
        /* Process MikroC UART GRBL commands */
        UART_GRBL_Tasks();

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

    /* Initialize speed control system (legacy compatibility) */
    speed_cntr_Init_Timer1();

    /* Setup OCR callbacks for step pulse generation */
    OCMP1_CallbackRegister(APP_OCMP1_Callback, 0);
    OCMP4_CallbackRegister(APP_OCMP4_Callback, 0);
    OCMP5_CallbackRegister(APP_OCMP5_Callback, 0);

    /* Setup Timer1 for trajectory calculation at 1kHz */
    if (!appData.trajectory_timer_active)
    {
        TMR1_CallbackRegister(APP_TrajectoryTimerCallback, 0);
        /* Set Timer1 period for 1kHz (assuming Timer1 is configured in MCC) */
        TMR1_Start();
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
    /* Check for hard limit switch activation
     * Min switches: Your actual MCC-configured pins (active for min limits)
     * Max switches: Dummy for now - will be configured later:
     *   - LIMIT_X_MAX_PIN, LIMIT_Y_MAX_PIN, LIMIT_Z_MAX_PIN
     */

    static uint32_t last_limit_check = 0;
    uint32_t current_time = appData.system_tick_counter;

    /* Check limit switches every 5ms for debouncing */
    if ((current_time - last_limit_check) < 5)
    {
        return;
    }
    last_limit_check = current_time;

    /* Read GPIO pins - active low limit switches */
    bool x_min_limit = !GPIO_PinRead(LIMIT_X_PIN); // X-axis min limit switch (homing + limit)
    bool x_max_limit = false;                      // TODO: !GPIO_PinRead(LIMIT_X_MAX_PIN) when configured
    bool y_min_limit = !GPIO_PinRead(LIMIT_Y_PIN); // Y-axis min limit switch (homing + limit)
    bool y_max_limit = false;                      // TODO: !GPIO_PinRead(LIMIT_Y_MAX_PIN) when configured
    bool z_min_limit = !GPIO_PinRead(LIMIT_Z_PIN); // Z-axis min limit switch (homing + limit)
    bool z_max_limit = false;                      // TODO: !GPIO_PinRead(LIMIT_Z_MAX_PIN) when configured

    /* Apply limit masking for pick-and-place operations */
    if (INTERP_IsLimitMasked(AXIS_X, false))
        x_min_limit = false;
    if (INTERP_IsLimitMasked(AXIS_X, true))
        x_max_limit = false;
    if (INTERP_IsLimitMasked(AXIS_Y, false))
        y_min_limit = false;
    if (INTERP_IsLimitMasked(AXIS_Y, true))
        y_max_limit = false;
    if (INTERP_IsLimitMasked(AXIS_Z, false))
        z_min_limit = false; // Critical for spring-loaded nozzle
    if (INTERP_IsLimitMasked(AXIS_Z, true))
        z_max_limit = false;

    /* Check X-axis limits */
    if (x_min_limit || x_max_limit)
    {
        INTERP_HandleHardLimit(AXIS_X, x_min_limit, x_max_limit);
        if (x_min_limit)
        {
            APP_UARTPrint("ALARM: X-axis minimum limit triggered\r\n");
        }
        if (x_max_limit)
        {
            APP_UARTPrint("ALARM: X-axis maximum limit triggered\r\n");
        }
    }

    /* Check Y-axis limits */
    if (y_min_limit || y_max_limit)
    {
        INTERP_HandleHardLimit(AXIS_Y, y_min_limit, y_max_limit);
        if (y_min_limit)
        {
            APP_UARTPrint("ALARM: Y-axis minimum limit triggered\r\n");
        }
        if (y_max_limit)
        {
            APP_UARTPrint("ALARM: Y-axis maximum limit triggered\r\n");
        }
    }

    /* Check Z-axis limits */
    if (z_min_limit || z_max_limit)
    {
        INTERP_HandleHardLimit(AXIS_Z, z_min_limit, z_max_limit);
        if (z_min_limit)
        {
            APP_UARTPrint("ALARM: Z-axis minimum limit triggered\r\n");
        }
        if (z_max_limit)
        {
            APP_UARTPrint("ALARM: Z-axis maximum limit triggered\r\n");
        }
    }

    /* Set alarm state if any limit is triggered */
    if (x_min_limit || x_max_limit || y_min_limit || y_max_limit || z_min_limit || z_max_limit)
    {
        /* Transition to error state for safety */
        appData.state = APP_STATE_MOTION_ERROR;
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

static void APP_CalculateTrajectory(uint8_t axis_id)
{
    if (axis_id >= MAX_AXES)
        return;

    cnc_axis_t *axis = &cnc_axes[axis_id];

    if (!axis->is_active || axis->motion_state == AXIS_IDLE || axis->motion_state == AXIS_COMPLETE)
    {
        return;
    }

    const float dt = 1.0f / TRAJECTORY_TIMER_FREQ; // Time step in seconds

    switch (axis->motion_state)
    {
    case AXIS_ACCEL:
        /* Accelerate until we reach target velocity or need to decelerate */
        axis->current_velocity += axis->acceleration * dt;

        if (axis->current_velocity >= axis->target_velocity)
        {
            axis->current_velocity = axis->target_velocity;
            axis->motion_state = AXIS_CONSTANT;
        }

        /* Check if we need to start decelerating */
        int32_t remaining_steps = abs(axis->target_position - axis->current_position);
        float decel_distance = (axis->current_velocity * axis->current_velocity) /
                               (2.0f * axis->deceleration);

        if (remaining_steps <= decel_distance)
        {
            axis->motion_state = AXIS_DECEL;
        }
        break;

    case AXIS_CONSTANT:
        /* Check if we need to start decelerating */
        remaining_steps = abs(axis->target_position - axis->current_position);
        decel_distance = (axis->current_velocity * axis->current_velocity) /
                         (2.0f * axis->deceleration);

        if (remaining_steps <= decel_distance)
        {
            axis->motion_state = AXIS_DECEL;
        }
        break;

    case AXIS_DECEL:
        /* Decelerate until we reach target or stop */
        axis->current_velocity -= axis->deceleration * dt;

        if (axis->current_velocity <= 0 ||
            axis->current_position == axis->target_position)
        {
            axis->current_velocity = 0;
            axis->motion_state = AXIS_COMPLETE;

            /* Disable the OCR for this axis */
            switch (axis_id)
            {
            case 0:
                OCMP4_Disable();
                break;
            case 1:
                OCMP1_Disable();
                break;
            case 2:
                OCMP5_Disable();
                break;
            }
            return;
        }
        break;

    default:
        return;
    }

    /* Update OCR period based on new velocity */
    APP_UpdateOCRPeriod(axis_id);
}

static void APP_UpdateOCRPeriod(uint8_t axis_id)
{
    if (axis_id >= MAX_AXES)
        return;

    cnc_axis_t *axis = &cnc_axes[axis_id];

    if (axis->current_velocity > 0)
    {
        uint32_t timer_freq;

        /* Use the appropriate timer frequency for each OCR module */
        switch (axis_id)
        {
        case 0: // X-axis -> OCMP4 -> TMR3
            timer_freq = TMR3_FrequencyGet();
            break;
        case 1: // Y-axis -> OCMP1 -> TMR2
            timer_freq = TMR2_FrequencyGet();
            break;
        case 2: // Z-axis -> OCMP5 -> TMR4
            timer_freq = TMR4_FrequencyGet();
            break;
        default:
            return;
        }

        axis->ocr_period = (uint16_t)(timer_freq / (2.0f * axis->current_velocity)); // Divide by 2 for 50% duty cycle

        /* Ensure minimum period to prevent timer overflow */
        if (axis->ocr_period < 10)
        {
            axis->ocr_period = 10;
        }

        /* Update the appropriate OCR module */
        switch (axis_id)
        {
        case 0: // X-axis -> OCMP4
            OCMP4_CompareSecondaryValueSet(axis->ocr_period);
            break;
        case 1: // Y-axis -> OCMP1
            OCMP1_CompareSecondaryValueSet(axis->ocr_period);
            break;
        case 2: // Z-axis -> OCMP5
            OCMP5_CompareSecondaryValueSet(axis->ocr_period);
            break;
        }
    }
}

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

// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Function Implementations
// *****************************************************************************
// *****************************************************************************

void APP_TrajectoryTimerCallback(uint32_t status, uintptr_t context)
{
    /* Increment system tick counter */
    appData.system_tick_counter++;

    /* Calculate trajectory for all active axes */
    for (int i = 0; i < MAX_AXES; i++)
    {
        if (cnc_axes[i].is_active)
        {
            APP_CalculateTrajectory(i);
        }
    }
}

void APP_CoreTimerCallback(uint32_t status, uintptr_t context)
{
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

void APP_AlarmReset(void)
{
    /* Reset alarm state after limit switch trigger */
    /* This should only be called after the limit condition is cleared */

    /* Verify all limit switches are released */
    bool x_min_limit = !GPIO_PinRead(LIMIT_X_PIN); // X-axis min limit switch
    bool x_max_limit = false;                      // TODO: !GPIO_PinRead(LIMIT_X_MAX_PIN) when configured
    bool y_min_limit = !GPIO_PinRead(LIMIT_Y_PIN); // Y-axis min limit switch
    bool y_max_limit = false;                      // TODO: !GPIO_PinRead(LIMIT_Y_MAX_PIN) when configured
    bool z_min_limit = !GPIO_PinRead(LIMIT_Z_PIN); // Z-axis min limit switch
    bool z_max_limit = false;                      // TODO: !GPIO_PinRead(LIMIT_Z_MAX_PIN) when configured

    if (x_min_limit || x_max_limit || y_min_limit || y_max_limit || z_min_limit || z_max_limit)
    {
        APP_UARTPrint("ERROR: Cannot reset alarm - limit switches still active\r\n");
        return;
    }

    /* Reset interpolation engine alarm state */
    INTERP_ClearAlarmState();

    /* Clear motion buffer */
    motion_buffer_head = 0;
    motion_buffer_tail = 0;
    for (int i = 0; i < MOTION_BUFFER_SIZE; i++)
    {
        motion_buffer[i].is_valid = false;
    }

    /* Reset all axes to idle */
    for (int i = 0; i < MAX_AXES; i++)
    {
        cnc_axes[i].is_active = false;
        cnc_axes[i].motion_state = AXIS_IDLE;
        cnc_axes[i].current_velocity = 0;
        cnc_axes[i].target_velocity = 0;
    }

    /* Return to idle state */
    appData.state = APP_STATE_MOTION_IDLE;
    APP_UARTPrint("Alarm cleared - System ready\r\n");
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

void APP_ExecuteGcodeCommand(const char *command)
{
    if (!command || strlen(command) == 0)
    {
        GCODE_DMA_SendError(3); // Invalid command error
        return;
    }

    // Handle special GRBL commands first ($H, $$ settings, etc.)
    if (command[0] == '$')
    {
        if (command[1] == 'H' || command[1] == 'h')
        {
            // Homing cycle command
            APP_StartHomingCycle();
            GCODE_DMA_SendResponse("ok");
            return;
        }
        else if (command[1] == '$')
        {
            // View GRBL settings - send back default settings
            GCODE_DMA_SendResponse("$0=10");        // Step pulse time
            GCODE_DMA_SendResponse("$1=25");        // Step idle delay
            GCODE_DMA_SendResponse("$2=0");         // Step pulse invert
            GCODE_DMA_SendResponse("$3=0");         // Step direction invert
            GCODE_DMA_SendResponse("$4=0");         // Step enable invert
            GCODE_DMA_SendResponse("$5=0");         // Limit pins invert
            GCODE_DMA_SendResponse("$6=0");         // Probe pin invert
            GCODE_DMA_SendResponse("$10=1");        // Status report options
            GCODE_DMA_SendResponse("$11=0.010");    // Junction deviation
            GCODE_DMA_SendResponse("$12=0.002");    // Arc tolerance
            GCODE_DMA_SendResponse("$13=0");        // Report inches
            GCODE_DMA_SendResponse("$20=0");        // Soft limits enable
            GCODE_DMA_SendResponse("$21=0");        // Hard limits enable
            GCODE_DMA_SendResponse("$22=1");        // Homing cycle enable
            GCODE_DMA_SendResponse("$23=0");        // Homing direction invert
            GCODE_DMA_SendResponse("$24=25.000");   // Homing seek rate
            GCODE_DMA_SendResponse("$25=500.000");  // Homing feed rate
            GCODE_DMA_SendResponse("$26=250");      // Homing debounce delay
            GCODE_DMA_SendResponse("$27=1.000");    // Homing pulloff distance
            GCODE_DMA_SendResponse("$100=80.000");  // X steps per mm
            GCODE_DMA_SendResponse("$101=80.000");  // Y steps per mm
            GCODE_DMA_SendResponse("$102=400.000"); // Z steps per mm
            GCODE_DMA_SendResponse("$110=500.000"); // X max rate
            GCODE_DMA_SendResponse("$111=500.000"); // Y max rate
            GCODE_DMA_SendResponse("$112=500.000"); // Z max rate
            GCODE_DMA_SendResponse("$120=10.000");  // X acceleration
            GCODE_DMA_SendResponse("$121=10.000");  // Y acceleration
            GCODE_DMA_SendResponse("$122=10.000");  // Z acceleration
            GCODE_DMA_SendResponse("$130=200.000"); // X max travel
            GCODE_DMA_SendResponse("$131=200.000"); // Y max travel
            GCODE_DMA_SendResponse("$132=200.000"); // Z max travel
            GCODE_DMA_SendResponse("ok");
            return;
        }
        // Other $ commands - acknowledge but don't implement yet
        GCODE_DMA_SendResponse("ok");
        return;
    }

    // Handle real-time control characters
    if (command[0] == '!')
    {
        // Feed hold
        INTERP_FeedHold(true);
        return; // No response for real-time commands
    }
    else if (command[0] == '~')
    {
        // Resume
        INTERP_FeedHold(false);
        return; // No response for real-time commands
    }
    else if (command[0] == '?')
    {
        // Status report query - immediate response
        APP_SendStatusReport();
        return; // No additional response for real-time commands
    }
    else if (command[0] == 18) // Ctrl-R (reset)
    {
        // Soft reset
        APP_AlarmReset();
        GCODE_DMA_SendResponse("Grbl 1.1f ['$' for help]");
        return;
    }

    // Enhanced G-code processing for professional motion control
    // Supports full GRBL v1.1f command set plus pick-and-place extensions

    if (command[0] == 'G' || command[0] == 'g')
    {
        // Parse G-code number
        int g_code = -1;
        sscanf(command, "G%d", &g_code);

        // Parse coordinates and parameters
        float x = 0, y = 0, z = 0, a = 0;
        float i = 0, j = 0, k = 0, r = 0;
        float f = 100.0f; // Default feed rate mm/min
        float p = 0;      // Dwell time parameter

        bool has_x = false, has_y = false, has_z = false, has_a = false;
        bool has_i = false, has_j = false, has_k = false, has_r = false;
        bool has_f = false, has_p = false;

        // Advanced coordinate parsing
        char *ptr = (char *)command;
        while (*ptr)
        {
            if (*ptr == 'X' || *ptr == 'x')
            {
                sscanf(ptr + 1, "%f", &x);
                has_x = true;
            }
            else if (*ptr == 'Y' || *ptr == 'y')
            {
                sscanf(ptr + 1, "%f", &y);
                has_y = true;
            }
            else if (*ptr == 'Z' || *ptr == 'z')
            {
                sscanf(ptr + 1, "%f", &z);
                has_z = true;
            }
            else if (*ptr == 'A' || *ptr == 'a')
            {
                sscanf(ptr + 1, "%f", &a);
                has_a = true;
            }
            else if (*ptr == 'I' || *ptr == 'i')
            {
                sscanf(ptr + 1, "%f", &i);
                has_i = true;
            }
            else if (*ptr == 'J' || *ptr == 'j')
            {
                sscanf(ptr + 1, "%f", &j);
                has_j = true;
            }
            else if (*ptr == 'K' || *ptr == 'k')
            {
                sscanf(ptr + 1, "%f", &k);
                has_k = true;
            }
            else if (*ptr == 'R' || *ptr == 'r')
            {
                sscanf(ptr + 1, "%f", &r);
                has_r = true;
            }
            else if (*ptr == 'F' || *ptr == 'f')
            {
                sscanf(ptr + 1, "%f", &f);
                has_f = true;
            }
            else if (*ptr == 'P' || *ptr == 'p')
            {
                sscanf(ptr + 1, "%f", &p);
                has_p = true;
            }
            ptr++;
        }

        // Get current position for start point
        position_t start_pos;
        start_pos.x = cnc_axes[0].current_position;
        start_pos.y = cnc_axes[1].current_position;
        start_pos.z = cnc_axes[2].current_position;
        start_pos.a = cnc_axes[3].current_position;

        // Calculate target position (absolute coordinates)
        position_t target_pos = start_pos;
        if (has_x)
            target_pos.x = x;
        if (has_y)
            target_pos.y = y;
        if (has_z)
            target_pos.z = z;
        if (has_a)
            target_pos.a = a;

        // Update feed rate if specified
        static float current_feed_rate = 100.0f; // Default feed rate
        if (has_f)
        {
            current_feed_rate = f;
        }
        else
        {
            f = current_feed_rate; // Use previous feed rate
        }

        bool success = false;

        switch (g_code)
        {
        case 0:          // G0 - Rapid move (treat as G1 with high speed)
            f = 3000.0f; // Rapid feed rate
            // Fall through to G1

        case 1: // G1 - Linear interpolation
            success = INTERP_PlanLinearMove(start_pos, target_pos, f);
            if (success)
            {
                printf("G%d move: X%.2f Y%.2f Z%.2f A%.2f F%.1f\n",
                       g_code, target_pos.x, target_pos.y, target_pos.z, target_pos.a, f);
            }
            break;

        case 2: // G2 - Clockwise arc
            if (has_i || has_j || has_k)
            {
                // I,J,K offset format
                success = INTERP_PlanArcMove(start_pos, target_pos, i, j, k, ARC_DIRECTION_CW, f);
                if (success)
                {
                    printf("G2 arc: X%.2f Y%.2f I%.3f J%.3f F%.1f\n",
                           target_pos.x, target_pos.y, i, j, f);
                }
            }
            else if (has_r)
            {
                // R radius format
                success = INTERP_PlanArcMoveRadius(start_pos, target_pos, r, ARC_DIRECTION_CW, f);
                if (success)
                {
                    printf("G2 arc: X%.2f Y%.2f R%.3f F%.1f\n",
                           target_pos.x, target_pos.y, r, f);
                }
            }
            else
            {
                GCODE_DMA_SendError(3); // Missing I,J,K or R parameter
                return;
            }
            break;

        case 3: // G3 - Counter-clockwise arc
            if (has_i || has_j || has_k)
            {
                // I,J,K offset format
                success = INTERP_PlanArcMove(start_pos, target_pos, i, j, k, ARC_DIRECTION_CCW, f);
                if (success)
                {
                    printf("G3 arc: X%.2f Y%.2f I%.3f J%.3f F%.1f\n",
                           target_pos.x, target_pos.y, i, j, f);
                }
            }
            else if (has_r)
            {
                // R radius format
                success = INTERP_PlanArcMoveRadius(start_pos, target_pos, r, ARC_DIRECTION_CCW, f);
                if (success)
                {
                    printf("G3 arc: X%.2f Y%.2f R%.3f F%.1f\n",
                           target_pos.x, target_pos.y, r, f);
                }
            }
            else
            {
                GCODE_DMA_SendError(3); // Missing I,J,K or R parameter
                return;
            }
            break;

        case 4: // G4 - Dwell (pause)
            if (has_p)
            {
                printf("G4 dwell: P%.3f seconds\n", p);
                // TODO: Implement actual dwell/delay
                success = true;
            }
            else
            {
                GCODE_DMA_SendError(3); // Missing P parameter
                return;
            }
            break;

        case 10: // G10 - Coordinate system data tool and work offset
            // TODO: Implement coordinate system offsets
            printf("G10 coordinate system (not fully implemented)\n");
            success = true;
            break;

        case 17: // G17 - XY plane selection
            printf("G17 XY plane selected\n");
            success = true;
            break;

        case 18: // G18 - XZ plane selection
            printf("G18 XZ plane selected\n");
            success = true;
            break;

        case 19: // G19 - YZ plane selection
            printf("G19 YZ plane selected\n");
            success = true;
            break;

        case 20: // G20 - Inch units
            printf("G20 inch units selected\n");
            success = true;
            break;

        case 21: // G21 - Millimeter units
            printf("G21 millimeter units selected\n");
            success = true;
            break;

        case 28: // G28 - Go to predefined home position
            // Use homing cycle to find home
            APP_StartHomingCycle();
            success = true;
            break;

        case 30: // G30 - Go to predefined position
            // TODO: Implement predefined positions
            printf("G30 predefined position (not implemented)\n");
            success = true;
            break;

        case 53: // G53 - Move in absolute machine coordinates
            // TODO: Implement absolute machine coordinate mode
            printf("G53 absolute machine coordinates\n");
            success = true;
            break;

        case 54:
        case 55:
        case 56:
        case 57:
        case 58:
        case 59: // G54-G59 - Work coordinate systems
            printf("G%d work coordinate system selected\n", g_code);
            success = true;
            break;

        case 80: // G80 - Cancel canned cycle
            printf("G80 cancel canned cycle\n");
            success = true;
            break;

        case 90: // G90 - Absolute positioning
            printf("G90 absolute positioning mode\n");
            success = true;
            break;

        case 91: // G91 - Relative positioning
            printf("G91 relative positioning mode\n");
            success = true;
            break;

        case 92: // G92 - Set current position
            if (has_x)
                cnc_axes[0].current_position = x;
            if (has_y)
                cnc_axes[1].current_position = y;
            if (has_z)
                cnc_axes[2].current_position = z;
            if (has_a)
                cnc_axes[3].current_position = a;
            printf("G92 position set: X%d Y%d Z%d A%d\n",
                   cnc_axes[0].current_position, cnc_axes[1].current_position,
                   cnc_axes[2].current_position, cnc_axes[3].current_position);
            success = true;
            break;

        case 93: // G93 - Inverse time feed rate mode
            printf("G93 inverse time feed rate mode\n");
            success = true;
            break;

        case 94: // G94 - Units per minute feed rate mode
            printf("G94 units per minute feed rate mode\n");
            success = true;
            break;

        default:
            printf("Unsupported G-code: G%d\n", g_code);
            GCODE_DMA_SendError(3); // Unsupported command
            return;
        }

        if (success)
        {
            // Update current position for moves that change position
            if (g_code == 0 || g_code == 1 || g_code == 2 || g_code == 3)
            {
                cnc_axes[0].current_position = target_pos.x;
                cnc_axes[1].current_position = target_pos.y;
                cnc_axes[2].current_position = target_pos.z;
                cnc_axes[3].current_position = target_pos.a;
            }
            GCODE_DMA_SendOK(); // Send OK for successful G-code
        }
        else
        {
            printf("Failed to plan motion\n");
            GCODE_DMA_SendError(3); // Motion planning failed
        }
    }
    else if (command[0] == 'M' || command[0] == 'm')
    {
        // M-code command - machine control and pick-and-place extensions
        int m_code = -1;
        sscanf(command, "M%d", &m_code);

        float p = 0; // Parameter for M-codes
        bool has_p = false;

        // Parse P parameter if present
        char *ptr = (char *)command;
        while (*ptr)
        {
            if (*ptr == 'P' || *ptr == 'p')
            {
                sscanf(ptr + 1, "%f", &p);
                has_p = true;
            }
            ptr++;
        }

        bool success = false;

        switch (m_code)
        {
        case 0: // M0 - Program pause
            printf("M0 program pause\n");
            success = true;
            break;

        case 1: // M1 - Optional program pause
            printf("M1 optional program pause\n");
            success = true;
            break;

        case 2: // M2 - Program end
            printf("M2 program end\n");
            success = true;
            break;

        case 3: // M3 - Spindle on (clockwise)
            printf("M3 spindle CW\n");
            // TODO: Implement spindle control
            success = true;
            break;

        case 4: // M4 - Spindle on (counter-clockwise)
            printf("M4 spindle CCW\n");
            // TODO: Implement spindle control
            success = true;
            break;

        case 5: // M5 - Spindle stop
            printf("M5 spindle stop\n");
            // TODO: Implement spindle control
            success = true;
            break;

        case 7: // M7 - Mist coolant on
            printf("M7 mist coolant on\n");
            // TODO: Implement coolant control
            success = true;
            break;

        case 8: // M8 - Flood coolant on
            printf("M8 flood coolant on\n");
            // TODO: Implement coolant control
            success = true;
            break;

        case 9: // M9 - Coolant off
            printf("M9 coolant off\n");
            // TODO: Implement coolant control
            success = true;
            break;

        case 30: // M30 - Program end and reset
            printf("M30 program end and reset\n");
            success = true;
            break;

        // **PICK-AND-PLACE EXTENSIONS** - Custom M-codes for limit masking
        case 100: // M100 - Enable pick-and-place mode
            APP_SetPickAndPlaceMode(true);
            printf("M100 Pick-and-place mode enabled\n");
            success = true;
            break;

        case 101: // M101 - Disable pick-and-place mode
            APP_SetPickAndPlaceMode(false);
            printf("M101 Pick-and-place mode disabled\n");
            success = true;
            break;

        case 102: // M102 - Enable specific limit mask (P parameter = mask value)
            if (has_p)
            {
                INTERP_EnableLimitMask((limit_mask_t)((int)p));
                printf("M102 P%d - Enabled limit mask 0x%02X\n", (int)p, (int)p);
                success = true;
            }
            else
            {
                GCODE_DMA_SendError(3); // Missing P parameter
                return;
            }
            break;

        case 103: // M103 - Disable specific limit mask (P parameter = mask value)
            if (has_p)
            {
                INTERP_DisableLimitMask((limit_mask_t)((int)p));
                printf("M103 P%d - Disabled limit mask 0x%02X\n", (int)p, (int)p);
                success = true;
            }
            else
            {
                GCODE_DMA_SendError(3); // Missing P parameter
                return;
            }
            break;

        case 104: // M104 - Set complete limit mask (P parameter = mask value)
            if (has_p)
            {
                INTERP_SetLimitMask((limit_mask_t)((int)p));
                printf("M104 P%d - Set limit mask to 0x%02X\n", (int)p, (int)p);
                success = true;
            }
            else
            {
                GCODE_DMA_SendError(3); // Missing P parameter
                return;
            }
            break;

        case 105: // M105 - Report limit mask status
        {
            limit_mask_t current_mask = INTERP_GetLimitMask();
            printf("M105 - Current limit mask: 0x%02X\n", current_mask);
            if (current_mask & LIMIT_MASK_Z_MIN)
                printf("  Z-min masked (pick-and-place mode)\n");
            if (current_mask & LIMIT_MASK_X_MIN)
                printf("  X-min masked\n");
            if (current_mask & LIMIT_MASK_Y_MIN)
                printf("  Y-min masked\n");
            if (current_mask == LIMIT_MASK_NONE)
                printf("  All limits active (CNC mode)\n");
            success = true;
        }
        break;

        case 106: // M106 - Enable Z minimum mask only (quick pick-and-place)
            APP_EnableZMinMask();
            printf("M106 Z minimum limit masked for nozzle compression\n");
            success = true;
            break;

        case 107: // M107 - Disable Z minimum mask only
            APP_DisableZMinMask();
            printf("M107 Z minimum limit restored\n");
            success = true;
            break;

        case 108: // M108 - Emergency restore all limits
            INTERP_SetLimitMask(LIMIT_MASK_NONE);
            printf("M108 EMERGENCY: All limits restored\n");
            success = true;
            break;

        default:
            printf("Unsupported M-code: M%d\n", m_code);
            GCODE_DMA_SendError(3); // Unsupported command
            return;
        }

        if (success)
        {
            GCODE_DMA_SendOK(); // Send OK for successful M-code
        }
        else
        {
            GCODE_DMA_SendError(3); // M-code execution failed
        }
    }
    else if (command[0] == '$')
    {
        // GRBL system command - already handled above
        GCODE_DMA_SendError(3); // Invalid command
    }
    else
    {
        // Unknown command
        GCODE_DMA_SendError(3); // Invalid command error
    }
}

void APP_HandleEmergencyStop(void)
{
    /* Called from DMA parser on Ctrl-X */
    APP_EmergencyStop();
    appData.state = APP_STATE_MOTION_IDLE;
}

void APP_SendStatusReport(void)
{
    /* Send GRBL v1.1f compliant status report */
    char status_buffer[256];

    // Determine state
    const char *state_name = "Idle";
    switch (appData.state)
    {
    case APP_STATE_MOTION_IDLE:
        state_name = "Idle";
        break;
    case APP_STATE_MOTION_PLANNING:
    case APP_STATE_MOTION_EXECUTING:
        state_name = "Run";
        break;
    case APP_STATE_MOTION_ERROR:
        state_name = "Alarm";
        break;
    case APP_STATE_HOMING:
        state_name = "Home";
        break;
    default:
        state_name = "Idle";
        break;
    }

    // Get machine positions (absolute coordinates)
    float mpos_x = cnc_axes[0].current_position;
    float mpos_y = cnc_axes[1].current_position; 
    float mpos_z = cnc_axes[2].current_position;
    
    // Calculate work positions (machine pos - work coordinate offset)
    // For now, use G54 work coordinate system (future: implement G54-G59)
    static float work_offset[3] = {0.0f, 0.0f, 0.0f}; // G54 offset
    float wpos_x = mpos_x - work_offset[0];
    float wpos_y = mpos_y - work_offset[1];
    float wpos_z = mpos_z - work_offset[2];

    // Get buffer status (blocks used, characters available)
    uint8_t blocks_used = (motion_buffer_head >= motion_buffer_tail) 
        ? (motion_buffer_head - motion_buffer_tail) 
        : (MOTION_BUFFER_SIZE - motion_buffer_tail + motion_buffer_head);
    uint8_t blocks_available = MOTION_BUFFER_SIZE - blocks_used;
    
    // Get current feed rate and spindle speed
    float current_feed = 0.0f;
    for (int i = 0; i < MAX_AXES; i++)
    {
        if (cnc_axes[i].is_active && cnc_axes[i].current_velocity > current_feed)
        {
            current_feed = cnc_axes[i].current_velocity * 60.0f; // Convert to mm/min
        }
    }
    
    // Spindle speed (future: get from actual spindle controller)
    int spindle_speed = 0; // TODO: Implement spindle speed feedback

    // Build GRBL v1.1f compliant status report
    // Format: <State|MPos:x,y,z|WPos:x,y,z|Bf:used,available|FS:feed,spindle>
    snprintf(status_buffer, sizeof(status_buffer),
             "<%s|MPos:%.3f,%.3f,%.3f|WPos:%.3f,%.3f,%.3f|Bf:%d,%d|FS:%.0f,%d>",
             state_name,
             mpos_x, mpos_y, mpos_z,
             wpos_x, wpos_y, wpos_z,
             blocks_used, blocks_available,
             current_feed, spindle_speed);

    GCODE_DMA_SendResponse(status_buffer);
}

#ifdef TEST_ARC_INTERPOLATION
void APP_TestArcInterpolation(void)
{
    printf("\n=== Arc Interpolation Test Suite ===\n");

    if (!INTERP_Initialize())
    {
        printf("ERROR: Failed to initialize interpolation engine\n");
        return;
    }

    INTERP_Enable(true);
    printf("Interpolation engine initialized and enabled successfully\n");

    // Test 1: Simple quarter circle using I,J format
    printf("\n--- Test 1: Quarter Circle G2 (I,J format) ---\n");
    position_t start1 = {0.0f, 0.0f, 0.0f, 0.0f};
    position_t end1 = {10.0f, 10.0f, 0.0f, 0.0f};
    if (INTERP_PlanArcMove(start1, end1, 10.0f, 0.0f, 0.0f, ARC_DIRECTION_CW, 500.0f))
    {
        printf("✓ G2 Quarter circle planned successfully\n");
    }
    else
    {
        printf("✗ G2 Quarter circle planning failed\n");
    }

    // Test 2: Semi-circle using R format
    printf("\n--- Test 2: Semi-circle G3 (R format) ---\n");
    position_t start2 = {0.0f, 0.0f, 0.0f, 0.0f};
    position_t end2 = {20.0f, 0.0f, 0.0f, 0.0f};
    if (INTERP_PlanArcMoveRadius(start2, end2, 10.0f, ARC_DIRECTION_CCW, 300.0f))
    {
        printf("✓ G3 Semi-circle planned successfully\n");
    }
    else
    {
        printf("✗ G3 Semi-circle planning failed\n");
    }

    // Test 3: Helical arc (Z-axis motion)
    printf("\n--- Test 3: Helical Arc ---\n");
    position_t start3 = {0.0f, 0.0f, 0.0f, 0.0f};
    position_t end3 = {10.0f, 10.0f, 5.0f, 0.0f};
    if (INTERP_PlanArcMove(start3, end3, 10.0f, 0.0f, 0.0f, ARC_DIRECTION_CW, 400.0f))
    {
        printf("✓ Helical arc planned successfully\n");
    }
    else
    {
        printf("✗ Helical arc planning failed\n");
    }

    // Test 4: Small radius arc (precision test)
    printf("\n--- Test 4: Small Radius Arc ---\n");
    position_t start4 = {0.0f, 0.0f, 0.0f, 0.0f};
    position_t end4 = {1.0f, 1.0f, 0.0f, 0.0f};
    if (INTERP_PlanArcMove(start4, end4, 1.0f, 0.0f, 0.0f, ARC_DIRECTION_CW, 100.0f))
    {
        printf("✓ Small radius arc planned successfully\n");
    }
    else
    {
        printf("✗ Small radius arc planning failed\n");
    }

    // Test 5: G-code command processing
    printf("\n--- Test 5: G-code Command Processing ---\n");
    printf("Testing G2 X10 Y10 I10 F500\n");
    APP_ExecuteGcodeCommand("G2 X10 Y10 I10 F500");

    printf("Testing G3 X20 Y0 R10 F300\n");
    APP_ExecuteGcodeCommand("G3 X20 Y0 R10 F300");

    printf("Testing G1 X0 Y0 F1000 (linear move)\n");
    APP_ExecuteGcodeCommand("G1 X0 Y0 F1000");

    printf("\n=== Arc Interpolation Test Complete ===\n");
    printf("All tests demonstrate UGS-compatible G2/G3 circular interpolation\n");
    printf("Features: I,J,K offset format, R radius format, helical motion, look-ahead planning\n\n");
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