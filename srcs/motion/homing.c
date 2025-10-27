/*******************************************************************************
  Homing Module - Implementation

  Description:
    GRBL-style homing cycle implementation for PIC32MZ CNC Controller.
    
  State Machine Flow:
    IDLE → INIT → APPROACH → BACKOFF → SLOW_APPROACH → PULLOFF_FINAL → COMPLETE
    
  Integration Points:
    - motion_math.c: Homing settings ($23-$27)
    - multiaxis_control.c: Motion execution
    - app.c: Limit switch state (APP_GetLimitSwitchState)
    - gcode_parser.c: G28/G28.1 command handlers
    
  Safety:
    - Non-blocking state machine (responsive to soft reset)
    - Timeout protection (prevents runaway if switch not found)
    - Switch validation (ensures switch clears after pulloff)
*******************************************************************************/

#include "motion/homing.h"
#include "motion/motion_math.h"
#include "motion/multiaxis_control.h"
#include "ugs_interface.h"
#include "definitions.h"  /* For CORETIMER_CounterGet() */
#include <stddef.h>
#include <string.h>

// *****************************************************************************
// Section: Local Constants
// *****************************************************************************

/* Homing timeout (30 seconds @ 100MHz CORETIMER) */
#define HOMING_TIMEOUT_TICKS 3000000000UL

/* Homing search distance (mm) - max travel to find limit switch */
#define HOMING_SEARCH_DISTANCE 350.0f

/* Debounce delay for limit switches (ms) */
#define HOMING_DEBOUNCE_MS 50

// *****************************************************************************
// Section: Local Variables
// *****************************************************************************

/* Current homing state */
static volatile homing_state_t current_state = HOMING_IDLE;

/* Axes being homed (bitmask) */
static uint8_t active_axes_mask = 0U;

/* Current axis being homed (for sequential homing) */
static axis_id_t current_axis = AXIS_X;

/* Error tracking */
static homing_error_code_t last_error = HOMING_ERROR_NONE;

/* Timeout tracking */
static uint32_t state_start_tick = 0U;

/* Limit switch debounce */
static uint32_t debounce_start_tick = 0U;
static bool debounce_active = false;

/* Callback pointer for limit switch state (replaces APP_GetLimitSwitchState) */
static bool (*limit_switch_callback)(axis_id_t axis, bool positive_direction) = NULL;

// *****************************************************************************
// Section: Helper Functions (Static/Private)
// *****************************************************************************

/* Forward declarations */
static bool read_limit_switch(axis_id_t axis, bool positive_direction);
static bool is_limit_triggered(axis_id_t axis, bool positive_direction);
static bool advance_to_next_axis(void);
static void execute_homing_move(float distance_mm, float feedrate_mm_min);
static bool is_timeout_expired(void);
static void reset_timeout(void);

/*! \brief Check if timeout expired
 *
 *  \return true if current state exceeded timeout period
 */
static bool is_timeout_expired(void)
{
    uint32_t now = CORETIMER_CounterGet();
    uint32_t elapsed = now - state_start_tick;
    return (elapsed >= HOMING_TIMEOUT_TICKS);
}

/*! \brief Reset state timeout timer */
static void reset_timeout(void)
{
    state_start_tick = CORETIMER_CounterGet();
}

/*! \brief Check if limit switch triggered (with debounce)
 *
 *  \param axis Axis to check
 *  \param positive_direction true for max limit, false for min limit
 *  \return true if switch active AND debounced
 */
static bool is_limit_triggered(axis_id_t axis, bool positive_direction)
{
    if (limit_switch_callback == NULL) {
        return false;  /* No callback registered */
    }
    
    bool switch_active = read_limit_switch(axis, positive_direction);  /* Use inversion logic */
    
    if (switch_active) {
        if (!debounce_active) {
            /* Start debounce timer */
            debounce_start_tick = CORETIMER_CounterGet();
            debounce_active = true;
            return false;
        }
        
        /* Check if debounce period elapsed */
        uint32_t now = CORETIMER_CounterGet();
        uint32_t elapsed_us = (now - debounce_start_tick) / 100;  /* 100MHz / 100 = 1us per count */
        uint32_t debounce_us = HOMING_DEBOUNCE_MS * 1000U;
        
        if (elapsed_us >= debounce_us) {
            return true;  /* Switch confirmed active */
        }
    } else {
        /* Switch released - reset debounce */
        debounce_active = false;
    }
    
    return false;
}

/*! \brief Get next axis in homing sequence
 *
 *  Advances current_axis to next enabled axis in mask.
 *  
 *  \return true if more axes to home, false if sequence complete
 */
static bool advance_to_next_axis(void)
{
    /* Find next enabled axis after current */
    for (axis_id_t next = (axis_id_t)(current_axis + 1); next < NUM_AXES; next++) {
        if ((active_axes_mask & (1U << next)) != 0U) {
            current_axis = next;
            return true;
        }
    }
    
    return false;  /* No more axes in sequence */
}

/*! \brief Execute homing move for current axis
 *
 *  \param distance_mm Distance to move (negative for min limit homing)
 *  \param feedrate_mm_min Feedrate for move
 */
static void execute_homing_move(float distance_mm, float feedrate_mm_min)
{
    /* Convert mm to steps */
    int32_t steps[NUM_AXES] = {0, 0, 0, 0};
    steps[current_axis] = MotionMath_MMToSteps(distance_mm, current_axis);
    
    /* Set feedrate in motion_math (used by multiaxis_control) */
    /* Note: This assumes multiaxis_control respects requested feedrate */
    /* TODO: Add feedrate parameter to MultiAxis_ExecuteCoordinatedMove() */
    
    /* Execute move */
    MultiAxis_ExecuteCoordinatedMove(steps);
}

/*! \brief Read limit switch state with configurable NO/NC logic
 *
 *  Reads raw GPIO pin state and applies inversion based on $28 setting.
 *  
 *  \param axis Axis to check
 *  \param positive_direction true for max limit, false for min limit
 *  \return true if switch triggered (considers NO/NC configuration)
 */
static bool read_limit_switch(axis_id_t axis, bool positive_direction)
{
    /* Get raw pin state from callback */
    bool raw_state = limit_switch_callback(axis, positive_direction);
    
    /* Apply inversion based on $28 setting (homing_invert_mask)
     * 
     * Logic:
     *   - Normally Open (NO):     Pin HIGH when triggered → raw_state is true when triggered
     *   - Normally Closed (NC):   Pin LOW when triggered → raw_state is false when triggered
     * 
     * If invert bit set for this axis:
     *   - Flip raw_state (NC switch: LOW=triggered becomes HIGH=triggered after inversion)
     * If invert bit clear:
     *   - Use raw_state as-is (NO switch: HIGH=triggered)
     */
    bool should_invert = (motion_settings.homing_invert_mask & (1U << axis)) != 0U;
    
    if (should_invert) {
        return !raw_state;  /* NC switch: Invert (LOW becomes triggered) */
    } else {
        return raw_state;   /* NO switch: Direct (HIGH is triggered) */
    }
}

// *****************************************************************************
// Section: Public API Implementation
// *****************************************************************************

void Homing_Initialize(bool (*get_limit_state)(axis_id_t, bool))
{
    limit_switch_callback = get_limit_state;
    current_state = HOMING_IDLE;
    active_axes_mask = 0U;
    current_axis = AXIS_X;
    last_error = HOMING_ERROR_NONE;
    debounce_active = false;
}

bool Homing_ExecuteCycle(homing_cycle_mask_t axes)
{
    /* Don't start if already active */
    if (current_state != HOMING_IDLE) {
        return false;
    }
    
    /* Validate axes mask */
    if (axes == HOMING_CYCLE_NONE || axes > HOMING_CYCLE_ALL_AXES) {
        last_error = HOMING_ERROR_INVALID_AXIS;
        return false;
    }
    
    /* Filter by enabled axes from $23 setting */
    uint8_t enabled_mask = Homing_GetCycleMask();
    active_axes_mask = (uint8_t)axes & enabled_mask;
    
    if (active_axes_mask == 0U) {
        /* No axes enabled for homing */
        UGS_Print(">> No axes enabled for homing ($23=0)\r\n");
        return false;
    }
    
    /* Find first axis in sequence */
    current_axis = AXIS_X;
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++) {
        if ((active_axes_mask & (1U << axis)) != 0U) {
            current_axis = axis;
            break;
        }
    }
    
    /* Initialize state machine */
    current_state = HOMING_INIT;
    last_error = HOMING_ERROR_NONE;
    reset_timeout();
    
    UGS_Print(">> Homing cycle started\r\n");
    return true;
}

homing_state_t Homing_Update(void)
{
    /* Check timeout on all states except IDLE/COMPLETE/ERROR */
    if (current_state > HOMING_INIT && current_state < HOMING_COMPLETE) {
        if (is_timeout_expired()) {
            current_state = HOMING_ERROR;
            last_error = HOMING_ERROR_TIMEOUT;
            UGS_Print(">> Homing timeout - limit switch not found\r\n");
            MultiAxis_StopAll();
            return current_state;
        }
    }
    
    switch (current_state) {
        case HOMING_IDLE:
            /* Nothing to do */
            break;
            
        case HOMING_INIT:
            /* Start fast approach to limit switch */
            UGS_Printf(">> Homing axis %c (fast search)\r\n", 'X' + (char)current_axis);
            
            /* Move toward min limit at seek rate */
            float seek_distance = -HOMING_SEARCH_DISTANCE;  /* Negative = toward min limit */
            float seek_rate = Homing_GetSeekRate(current_axis);
            
            execute_homing_move(seek_distance, seek_rate);
            
            current_state = HOMING_APPROACH;
            reset_timeout();
            break;
            
        case HOMING_APPROACH:
            /* Check if limit switch triggered */
            if (is_limit_triggered(current_axis, false)) {  /* false = min limit */
                /* Stop motion immediately */
                MultiAxis_StopAll();
                
                UGS_Printf(">> Axis %c limit found\r\n", 'X' + (char)current_axis);
                
                /* Proceed to backoff */
                current_state = HOMING_BACKOFF;
                reset_timeout();
            }
            /* Motion continues until switch found or timeout */
            break;
            
        case HOMING_BACKOFF:
            /* Wait for motion to stop completely */
            if (!MultiAxis_IsBusy()) {
                /* Pull off limit switch */
                float pulloff = Homing_GetPulloff(current_axis);
                float feed_rate = Homing_GetFeedRate(current_axis);
                
                execute_homing_move(pulloff, feed_rate);  /* Positive = away from limit */
                
                current_state = HOMING_SLOW_APPROACH;
                reset_timeout();
            }
            break;
            
        case HOMING_SLOW_APPROACH:
            /* Wait for backoff to complete */
            if (!MultiAxis_IsBusy()) {
                /* Ensure switch cleared */
                if (is_limit_triggered(current_axis, false)) {
                    current_state = HOMING_ERROR;
                    last_error = HOMING_ERROR_SWITCH_STUCK;
                    UGS_Print(">> Error: Limit switch stuck\r\n");
                    break;
                }
                
                /* Start slow precision approach */
                float pulloff = Homing_GetPulloff(current_axis);
                float feed_rate = Homing_GetFeedRate(current_axis);
                
                execute_homing_move(-pulloff * 2.0f, feed_rate);  /* Move back to switch slowly */
                
                /* Monitor for switch in this state */
                reset_timeout();
            }
            
            /* Check if limit triggered during slow approach */
            if (is_limit_triggered(current_axis, false)) {
                MultiAxis_StopAll();
                
                /* This is the precise home position - zero coordinates */
                UGS_Printf(">> Axis %c homed precisely\r\n", 'X' + (char)current_axis);
                
                /* Zero machine position for this axis */
                /* TODO: Add function to set machine position in multiaxis_control */
                /* For now, just note that position should be zeroed here */
                
                current_state = HOMING_PULLOFF_FINAL;
                reset_timeout();
            }
            break;
            
        case HOMING_PULLOFF_FINAL:
            /* Wait for stop, then final pulloff */
            if (!MultiAxis_IsBusy()) {
                float pulloff = Homing_GetPulloff(current_axis);
                float feed_rate = Homing_GetFeedRate(current_axis);
                
                execute_homing_move(pulloff, feed_rate);
                
                /* Check if more axes to home */
                if (advance_to_next_axis()) {
                    current_state = HOMING_INIT;  /* Home next axis */
                } else {
                    current_state = HOMING_COMPLETE;  /* All done */
                }
                reset_timeout();
            }
            break;
            
        case HOMING_COMPLETE:
            /* Wait for final pulloff to complete */
            if (!MultiAxis_IsBusy()) {
                UGS_Print(">> Homing cycle complete\r\n");
                current_state = HOMING_IDLE;
            }
            break;
            
        case HOMING_ERROR:
            /* Stay in error state until abort called */
            break;
    }
    
    return current_state;
}

bool Homing_IsActive(void)
{
    return (current_state != HOMING_IDLE && current_state != HOMING_ERROR);
}

void Homing_Abort(void)
{
    if (current_state != HOMING_IDLE) {
        MultiAxis_StopAll();
        current_state = HOMING_ERROR;
        last_error = HOMING_ERROR_ABORTED;
        UGS_Print(">> Homing aborted\r\n");
    }
}

void Homing_SetHomePosition(void)
{
    /* Get current machine positions */
    float positions[NUM_AXES];
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++) {
        positions[axis] = MotionMath_GetMachinePosition(axis);
    }
    
    /* Store as predefined position (index 0 = G28 home) */
    MotionMath_SetPredefinedPosition(0, positions);
    
    UGS_Print(">> Home position stored (G28.1)\r\n");
}

homing_state_t Homing_GetState(void)
{
    return current_state;
}

homing_error_code_t Homing_GetLastError(void)
{
    return last_error;
}

float Homing_GetSeekRate(axis_id_t axis)
{
    /* Use homing_seek_rate from motion_settings */
    return motion_settings.homing_seek_rate;
}

float Homing_GetFeedRate(axis_id_t axis)
{
    /* Use homing_feed_rate from motion_settings */
    return motion_settings.homing_feed_rate;
}

float Homing_GetPulloff(axis_id_t axis)
{
    /* Use homing_pulloff from motion_settings */
    return motion_settings.homing_pulloff;
}

uint8_t Homing_GetCycleMask(void)
{
    /* Use homing_cycle_mask from motion_settings ($23) */
    return motion_settings.homing_cycle_mask;
}

/*******************************************************************************
 End of File
*******************************************************************************/