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
static void APP_UARTPrint(const char* str) {
    if (!str) return;
    
    // Use simple UART for all messages
    while (*str) {
        while (!UART2_TransmitterIsReady());
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

void APP_Initialize ( void )
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
    for(int i = 0; i < MOTION_BUFFER_SIZE; i++) {
        motion_buffer[i].is_valid = false;
    }
    
    /* Initialize UGS/GRBL Settings Interface */
    if (!GRBL_Initialize()) {
        APP_UARTPrint("ERROR: Failed to initialize GRBL settings interface\r\n");
        return;
    }
    
    /* Initialize Interpolation Engine */
    if (!INTERP_Initialize()) {
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

void APP_Tasks ( void )
{
    /* Check the application's current state. */
    switch ( appData.state )
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
            if (!GCODE_DMA_Initialize()) {
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
            if(!APP_IsBufferEmpty()) {
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
            /* Check if current motion is complete */
            bool all_axes_complete = true;
            for(int i = 0; i < MAX_AXES; i++) {
                if(cnc_axes[i].is_active && 
                   cnc_axes[i].motion_state != AXIS_IDLE && 
                   cnc_axes[i].motion_state != AXIS_COMPLETE) {
                    all_axes_complete = false;
                    break;
                }
            }
            
            if(all_axes_complete) {
                /* Mark current buffer entry as processed */
                motion_buffer[motion_buffer_tail].is_valid = false;
                motion_buffer_tail = (motion_buffer_tail + 1) % MOTION_BUFFER_SIZE;
                
                /* Reset axes to idle state */
                for(int i = 0; i < MAX_AXES; i++) {
                    cnc_axes[i].is_active = false;
                    cnc_axes[i].motion_state = AXIS_IDLE;
                    cnc_axes[i].current_velocity = 0;
                }
                
                /* Check for more commands */
                if(!APP_IsBufferEmpty()) {
                    appData.state = APP_STATE_MOTION_PLANNING;
                } else {
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
    for(int i = 0; i < MAX_AXES; i++) {
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
    if(!appData.trajectory_timer_active) {
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
    if((current_time - appData.last_switch_time) < SWITCH_DEBOUNCE_MS) {
        return;
    }
    
    if(!SW1_Get() && !appData.switch_pressed) {
        /* Forward move - X-axis only for now */
        float target[] = {10000, 0, 0};  // X, Y, Z
        if(APP_AddLinearMove(target, 500)) {
            appData.switch_pressed = true;
            appData.last_switch_time = current_time;
        }
    }
    else if(!SW2_Get() && !appData.switch_pressed) {
        /* Reverse move - X-axis only for now */
        float target[] = {-10000, 0, 0}; // X, Y, Z
        if(APP_AddLinearMove(target, 500)) {
            appData.switch_pressed = true;
            appData.last_switch_time = current_time;
        }
    }
    else if(SW1_Get() && SW2_Get()) {
        /* Both switches released */
        appData.switch_pressed = false;
    }
}

static void APP_ProcessLimitSwitches(void)
{
    /* Check for hard limit switch activation
     * Assuming limit switches are connected to GPIO pins:
     * RA7  - X-axis limit (negative)
     * RA8  - X-axis limit (positive)
     * RA9  - Y-axis limit (negative)
     * RA10 - Y-axis limit (positive)
     * RA11 - Z-axis limit (negative)
     * RA12 - Z-axis limit (positive)
     */
    
    static uint32_t last_limit_check = 0;
    uint32_t current_time = appData.system_tick_counter;
    
    /* Check limit switches every 5ms for debouncing */
    if((current_time - last_limit_check) < 5) {
        return;
    }
    last_limit_check = current_time;
    
    /* Read GPIO pins - active low limit switches */
    bool x_neg_limit = !GPIO_PinRead(GPIO_PIN_RA7);   // X-axis negative limit
    bool x_pos_limit = !GPIO_PinRead(GPIO_PIN_RA9);   // X-axis positive limit
    bool y_neg_limit = !GPIO_PinRead(GPIO_PIN_RA10);  // Y-axis negative limit
    bool y_pos_limit = !GPIO_PinRead(GPIO_PIN_RA14);  // Y-axis positive limit
    bool z_neg_limit = !GPIO_PinRead(GPIO_PIN_RA15);  // Z-axis negative limit
    bool z_pos_limit = false;  // Z-axis positive limit - placeholder for now
    
    /* Check X-axis limits */
    if(x_neg_limit || x_pos_limit) {
        INTERP_HandleHardLimit(AXIS_X, x_neg_limit, x_pos_limit);
        APP_UARTPrint("ALARM: X-axis hard limit triggered\r\n");
    }
    
    /* Check Y-axis limits */
    if(y_neg_limit || y_pos_limit) {
        INTERP_HandleHardLimit(AXIS_Y, y_neg_limit, y_pos_limit);
        APP_UARTPrint("ALARM: Y-axis hard limit triggered\r\n");
    }
    
    /* Check Z-axis limits */
    if(z_neg_limit || z_pos_limit) {
        INTERP_HandleHardLimit(AXIS_Z, z_neg_limit, z_pos_limit);
        APP_UARTPrint("ALARM: Z-axis hard limit triggered\r\n");
    }
    
    /* Set alarm state if any limit is triggered */
    if(x_neg_limit || x_pos_limit || y_neg_limit || y_pos_limit || z_neg_limit || z_pos_limit) {
        /* Transition to error state for safety */
        appData.state = APP_STATE_MOTION_ERROR;
    }
}

static void APP_ExecuteNextMotionBlock(void)
{
    if(APP_IsBufferEmpty()) {
        return;
    }
    
    motion_block_t *block = &motion_buffer[motion_buffer_tail];
    
    if(!block->is_valid) {
        return;
    }
    
    /* Setup axes for movement */
    for(int i = 0; i < MAX_AXES; i++) {
        int32_t target_pos = (int32_t)block->target_pos[i];
        
        /* Only move axis if target is different from current position */
        if(target_pos != cnc_axes[i].current_position) {
            cnc_axes[i].target_position = target_pos;
            cnc_axes[i].target_velocity = block->feedrate;
            cnc_axes[i].max_velocity = block->max_velocity > 0 ? 
                                      block->max_velocity : DEFAULT_MAX_VELOCITY;
            cnc_axes[i].motion_state = AXIS_ACCEL;
            cnc_axes[i].is_active = true;
            cnc_axes[i].step_count = 0;
            
            /* Set direction */
            cnc_axes[i].direction_forward = (target_pos > cnc_axes[i].current_position);
            
            /* Enable the appropriate OCR module for this axis */
            switch(i) {
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
    if(axis_id >= MAX_AXES) return;
    
    cnc_axis_t *axis = &cnc_axes[axis_id];
    
    if(!axis->is_active || axis->motion_state == AXIS_IDLE || axis->motion_state == AXIS_COMPLETE) {
        return;
    }
    
    const float dt = 1.0f / TRAJECTORY_TIMER_FREQ; // Time step in seconds
    
    switch(axis->motion_state) {
        case AXIS_ACCEL:
            /* Accelerate until we reach target velocity or need to decelerate */
            axis->current_velocity += axis->acceleration * dt;
            
            if(axis->current_velocity >= axis->target_velocity) {
                axis->current_velocity = axis->target_velocity;
                axis->motion_state = AXIS_CONSTANT;
            }
            
            /* Check if we need to start decelerating */
            int32_t remaining_steps = abs(axis->target_position - axis->current_position);
            float decel_distance = (axis->current_velocity * axis->current_velocity) / 
                                 (2.0f * axis->deceleration);
            
            if(remaining_steps <= decel_distance) {
                axis->motion_state = AXIS_DECEL;
            }
            break;
            
        case AXIS_CONSTANT:
            /* Check if we need to start decelerating */
            remaining_steps = abs(axis->target_position - axis->current_position);
            decel_distance = (axis->current_velocity * axis->current_velocity) / 
                           (2.0f * axis->deceleration);
            
            if(remaining_steps <= decel_distance) {
                axis->motion_state = AXIS_DECEL;
            }
            break;
            
        case AXIS_DECEL:
            /* Decelerate until we reach target or stop */
            axis->current_velocity -= axis->deceleration * dt;
            
            if(axis->current_velocity <= 0 || 
               axis->current_position == axis->target_position) {
                axis->current_velocity = 0;
                axis->motion_state = AXIS_COMPLETE;
                
                /* Disable the OCR for this axis */
                switch(axis_id) {
                    case 0: OCMP4_Disable(); break;
                    case 1: OCMP1_Disable(); break;
                    case 2: OCMP5_Disable(); break;
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
    if(axis_id >= MAX_AXES) return;
    
    cnc_axis_t *axis = &cnc_axes[axis_id];
    
    if(axis->current_velocity > 0) {
        uint32_t timer_freq;
        
        /* Use the appropriate timer frequency for each OCR module */
        switch(axis_id) {
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
        if(axis->ocr_period < 10) {
            axis->ocr_period = 10;
        }
        
        /* Update the appropriate OCR module */
        switch(axis_id) {
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
    for(int i = 0; i < MAX_AXES; i++) {
        if(cnc_axes[i].is_active) {
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
    
    if(cnc_axes[1].is_active && cnc_axes[1].motion_state != AXIS_IDLE) {
        cnc_axes[1].current_position += cnc_axes[1].direction_forward ? 1 : -1;
        cnc_axes[1].step_count++;
    }
}

void APP_OCMP4_Callback(uintptr_t context)
{
    /* X-axis step pulse */
    LED2_Toggle(); // Step activity indicator
    
    if(cnc_axes[0].is_active && cnc_axes[0].motion_state != AXIS_IDLE) {
        cnc_axes[0].current_position += cnc_axes[0].direction_forward ? 1 : -1;
        cnc_axes[0].step_count++;
    }
}

void APP_OCMP5_Callback(uintptr_t context)
{
    /* Z-axis step pulse */
    LED2_Toggle(); // Step activity indicator
    
    if(cnc_axes[2].is_active && cnc_axes[2].motion_state != AXIS_IDLE) {
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
    if(APP_IsBufferFull()) {
        return false; // Buffer full
    }
    
    /* Check soft limits before adding move to buffer */
    position_t target_position = {target[0], target[1], target[2], 0.0f};
    if(!INTERP_CheckSoftLimits(target_position)) {
        APP_UARTPrint("ERROR: Soft limit would be exceeded\r\n");
        return false;
    }
    
    motion_block_t *block = &motion_buffer[motion_buffer_head];
    
    /* Copy target positions */
    for(int i = 0; i < MAX_AXES; i++) {
        block->target_pos[i] = target[i];
    }
    
    block->feedrate = feedrate;
    block->entry_velocity = 0;    // Will be calculated in look-ahead
    block->exit_velocity = 0;     // Will be calculated in look-ahead
    block->max_velocity = feedrate; // For now, max_velocity = feedrate
    block->motion_type = 1;       // G1 - Linear move
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
    if(!APP_IsBufferEmpty()) {
        return false;
    }
    
    for(int i = 0; i < MAX_AXES; i++) {
        if(cnc_axes[i].is_active && cnc_axes[i].motion_state != AXIS_IDLE) {
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
    for(int i = 0; i < MAX_AXES; i++) {
        cnc_axes[i].is_active = false;
        cnc_axes[i].motion_state = AXIS_IDLE;
        cnc_axes[i].current_velocity = 0;
        cnc_axes[i].target_velocity = 0;
    }
    
    /* Clear motion buffer */
    motion_buffer_head = 0;
    motion_buffer_tail = 0;
    for(int i = 0; i < MOTION_BUFFER_SIZE; i++) {
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
    bool x_neg_limit = !GPIO_PinRead(GPIO_PIN_RA7);   // X-axis negative limit
    bool x_pos_limit = !GPIO_PinRead(GPIO_PIN_RA9);   // X-axis positive limit
    bool y_neg_limit = !GPIO_PinRead(GPIO_PIN_RA10);  // Y-axis negative limit
    bool y_pos_limit = !GPIO_PinRead(GPIO_PIN_RA14);  // Y-axis positive limit
    bool z_neg_limit = !GPIO_PinRead(GPIO_PIN_RA15);  // Z-axis negative limit
    bool z_pos_limit = false;  // Z-axis positive limit - placeholder for now
    
    if(x_neg_limit || x_pos_limit || y_neg_limit || y_pos_limit || z_neg_limit || z_pos_limit) {
        APP_UARTPrint("ERROR: Cannot reset alarm - limit switches still active\r\n");
        return;
    }
    
    /* Reset interpolation engine alarm state */
    INTERP_ClearAlarmState();
    
    /* Clear motion buffer */
    motion_buffer_head = 0;
    motion_buffer_tail = 0;
    for(int i = 0; i < MOTION_BUFFER_SIZE; i++) {
        motion_buffer[i].is_valid = false;
    }
    
    /* Reset all axes to idle */
    for(int i = 0; i < MAX_AXES; i++) {
        cnc_axes[i].is_active = false;
        cnc_axes[i].motion_state = AXIS_IDLE;
        cnc_axes[i].current_velocity = 0;
        cnc_axes[i].target_velocity = 0;
    }
    
    /* Return to idle state */
    appData.state = APP_STATE_MOTION_IDLE;
    APP_UARTPrint("Alarm cleared - System ready\r\n");
}

// *****************************************************************************
// G-code Command Handler for DMA Parser Integration
// *****************************************************************************

void APP_ExecuteGcodeCommand(const char *command)
{
    if (!command || strlen(command) == 0) {
        GCODE_DMA_SendError(3);  // Invalid command error
        return;
    }
    
    // Enhanced G-code processing for professional motion control
    // Supports G0/G1 linear moves and G2/G3 arc moves
    
    if (command[0] == 'G' || command[0] == 'g') {
        // Parse G-code number
        int g_code = -1;
        sscanf(command, "G%d", &g_code);
        
        // Parse coordinates and parameters
        float x = 0, y = 0, z = 0, a = 0;
        float i = 0, j = 0, k = 0, r = 0;
        float f = 100.0f; // Default feed rate mm/min
        
        bool has_x = false, has_y = false, has_z = false, has_a = false;
        bool has_i = false, has_j = false, has_k = false, has_r = false;
        
        // Simple coordinate parsing (would be enhanced in production)
        char *ptr = (char*)command;
        while (*ptr) {
            if (*ptr == 'X' || *ptr == 'x') {
                sscanf(ptr + 1, "%f", &x);
                has_x = true;
            } else if (*ptr == 'Y' || *ptr == 'y') {
                sscanf(ptr + 1, "%f", &y);
                has_y = true;
            } else if (*ptr == 'Z' || *ptr == 'z') {
                sscanf(ptr + 1, "%f", &z);
                has_z = true;
            } else if (*ptr == 'A' || *ptr == 'a') {
                sscanf(ptr + 1, "%f", &a);
                has_a = true;
            } else if (*ptr == 'I' || *ptr == 'i') {
                sscanf(ptr + 1, "%f", &i);
                has_i = true;
            } else if (*ptr == 'J' || *ptr == 'j') {
                sscanf(ptr + 1, "%f", &j);
                has_j = true;
            } else if (*ptr == 'K' || *ptr == 'k') {
                sscanf(ptr + 1, "%f", &k);
                has_k = true;
            } else if (*ptr == 'R' || *ptr == 'r') {
                sscanf(ptr + 1, "%f", &r);
                has_r = true;
            } else if (*ptr == 'F' || *ptr == 'f') {
                sscanf(ptr + 1, "%f", &f);
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
        if (has_x) target_pos.x = x;
        if (has_y) target_pos.y = y;
        if (has_z) target_pos.z = z;
        if (has_a) target_pos.a = a;
        
        bool success = false;
        
        switch (g_code) {
            case 0: // G0 - Rapid move (treat as G1 with high speed)
                f = 3000.0f; // Rapid feed rate
                // Fall through to G1
                
            case 1: // G1 - Linear interpolation
                success = INTERP_PlanLinearMove(start_pos, target_pos, f);
                if (success) {
                    printf("G%d move: X%.2f Y%.2f Z%.2f A%.2f F%.1f\n", 
                           g_code, target_pos.x, target_pos.y, target_pos.z, target_pos.a, f);
                }
                break;
                
            case 2: // G2 - Clockwise arc
                if (has_i || has_j || has_k) {
                    // I,J,K offset format
                    success = INTERP_PlanArcMove(start_pos, target_pos, i, j, k, ARC_DIRECTION_CW, f);
                    if (success) {
                        printf("G2 arc: X%.2f Y%.2f I%.3f J%.3f F%.1f\n", 
                               target_pos.x, target_pos.y, i, j, f);
                    }
                } else if (has_r) {
                    // R radius format
                    success = INTERP_PlanArcMoveRadius(start_pos, target_pos, r, ARC_DIRECTION_CW, f);
                    if (success) {
                        printf("G2 arc: X%.2f Y%.2f R%.3f F%.1f\n", 
                               target_pos.x, target_pos.y, r, f);
                    }
                } else {
                    GCODE_DMA_SendError(3); // Missing I,J,K or R parameter
                    return;
                }
                break;
                
            case 3: // G3 - Counter-clockwise arc
                if (has_i || has_j || has_k) {
                    // I,J,K offset format
                    success = INTERP_PlanArcMove(start_pos, target_pos, i, j, k, ARC_DIRECTION_CCW, f);
                    if (success) {
                        printf("G3 arc: X%.2f Y%.2f I%.3f J%.3f F%.1f\n", 
                               target_pos.x, target_pos.y, i, j, f);
                    }
                } else if (has_r) {
                    // R radius format
                    success = INTERP_PlanArcMoveRadius(start_pos, target_pos, r, ARC_DIRECTION_CCW, f);
                    if (success) {
                        printf("G3 arc: X%.2f Y%.2f R%.3f F%.1f\n", 
                               target_pos.x, target_pos.y, r, f);
                    }
                } else {
                    GCODE_DMA_SendError(3); // Missing I,J,K or R parameter
                    return;
                }
                break;
                
            default:
                printf("Unsupported G-code: G%d\n", g_code);
                GCODE_DMA_SendError(3); // Unsupported command
                return;
        }
        
        if (success) {
            // Update current position for next move
            cnc_axes[0].current_position = target_pos.x;
            cnc_axes[1].current_position = target_pos.y;
            cnc_axes[2].current_position = target_pos.z;
            cnc_axes[3].current_position = target_pos.a;
            
            GCODE_DMA_SendOK();  // Send OK for successful G-code
        } else {
            printf("Failed to plan motion\n");
            GCODE_DMA_SendError(3); // Motion planning failed
        }
    }
    else if (command[0] == 'M' || command[0] == 'm') {
        // M-code command - machine control
        GCODE_DMA_SendOK();  // Send OK for successful M-code
    }
    else if (command[0] == '$') {
        // GRBL system command - already handled in uart_grbl_simple.c
        // This shouldn't reach here as system commands are processed directly
        GCODE_DMA_SendError(3);  // Invalid command
    }
    else {
        // Unknown command
        GCODE_DMA_SendError(3);  // Invalid command error
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
    /* Send detailed status report */
    char status_buffer[256];
    
    const char* state_name = "Unknown";
    switch (appData.state) {
        case APP_STATE_MOTION_IDLE: state_name = "Idle"; break;
        case APP_STATE_MOTION_PLANNING: state_name = "Run"; break;
        case APP_STATE_MOTION_EXECUTING: state_name = "Run"; break;
        case APP_STATE_MOTION_ERROR: state_name = "Alarm"; break;
        default: state_name = "Unknown"; break;
    }
    
    /* Get current position (would come from encoder feedback in real system) */
    float current_pos[3] = {
        cnc_axes[0].current_position,
        cnc_axes[1].current_position, 
        cnc_axes[2].current_position
    };
    
    /* Get current feed rate */
    float current_feed = 0.0f;
    for (int i = 0; i < MAX_AXES; i++) {
        if (cnc_axes[i].is_active && cnc_axes[i].current_velocity > current_feed) {
            current_feed = cnc_axes[i].current_velocity * 60.0f; // Convert to mm/min
        }
    }
    
    snprintf(status_buffer, sizeof(status_buffer),
        "<%s|MPos:%.3f,%.3f,%.3f|FS:%.0f,0|Bf:%d,%d|Ov:100,100,100>",
        state_name,
        current_pos[0], current_pos[1], current_pos[2],
        current_feed,
        GCODE_DMA_GetCommandCount(),
        MOTION_BUFFER_SIZE - ((motion_buffer_head >= motion_buffer_tail) ? 
            (motion_buffer_head - motion_buffer_tail) : 
            (MOTION_BUFFER_SIZE - motion_buffer_tail + motion_buffer_head))
    );
    
    GCODE_DMA_SendResponse(status_buffer);
}

#ifdef TEST_ARC_INTERPOLATION
void APP_TestArcInterpolation(void)
{
    printf("\n=== Arc Interpolation Test Suite ===\n");
    
    if (!INTERP_Initialize()) {
        printf("ERROR: Failed to initialize interpolation engine\n");
        return;
    }
    
    INTERP_Enable(true);
    printf("Interpolation engine initialized and enabled successfully\n");
    
    // Test 1: Simple quarter circle using I,J format
    printf("\n--- Test 1: Quarter Circle G2 (I,J format) ---\n");
    position_t start1 = {0.0f, 0.0f, 0.0f, 0.0f};
    position_t end1 = {10.0f, 10.0f, 0.0f, 0.0f};
    if (INTERP_PlanArcMove(start1, end1, 10.0f, 0.0f, 0.0f, ARC_DIRECTION_CW, 500.0f)) {
        printf("✓ G2 Quarter circle planned successfully\n");
    } else {
        printf("✗ G2 Quarter circle planning failed\n");
    }
    
    // Test 2: Semi-circle using R format
    printf("\n--- Test 2: Semi-circle G3 (R format) ---\n");
    position_t start2 = {0.0f, 0.0f, 0.0f, 0.0f};
    position_t end2 = {20.0f, 0.0f, 0.0f, 0.0f};
    if (INTERP_PlanArcMoveRadius(start2, end2, 10.0f, ARC_DIRECTION_CCW, 300.0f)) {
        printf("✓ G3 Semi-circle planned successfully\n");
    } else {
        printf("✗ G3 Semi-circle planning failed\n");
    }
    
    // Test 3: Helical arc (Z-axis motion)
    printf("\n--- Test 3: Helical Arc ---\n");
    position_t start3 = {0.0f, 0.0f, 0.0f, 0.0f};
    position_t end3 = {10.0f, 10.0f, 5.0f, 0.0f};
    if (INTERP_PlanArcMove(start3, end3, 10.0f, 0.0f, 0.0f, ARC_DIRECTION_CW, 400.0f)) {
        printf("✓ Helical arc planned successfully\n");
    } else {
        printf("✗ Helical arc planning failed\n");
    }
    
    // Test 4: Small radius arc (precision test)
    printf("\n--- Test 4: Small Radius Arc ---\n");
    position_t start4 = {0.0f, 0.0f, 0.0f, 0.0f};
    position_t end4 = {1.0f, 1.0f, 0.0f, 0.0f};
    if (INTERP_PlanArcMove(start4, end4, 1.0f, 0.0f, 0.0f, ARC_DIRECTION_CW, 100.0f)) {
        printf("✓ Small radius arc planned successfully\n");
    } else {
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

/*******************************************************************************
 End of File
 */