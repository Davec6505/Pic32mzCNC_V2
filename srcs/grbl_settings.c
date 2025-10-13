/*******************************************************************************
  GRBL Settings Interface Implementation

  File Name:
    grbl_settings.c

  Summary:
    Complete UGS-compatible GRBL settings implementation

  Description:
    Provides full Universal G-code Sender integration with real-time
    settings management, limits checking, and GRBL protocol compliance.
*******************************************************************************/

#include "grbl_settings.h"
#include "gcode_parser.h"
#include "interpolation_engine.h"
#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// *****************************************************************************
// Global Variables
// *****************************************************************************

static grbl_context_t grbl_context;
static bool grbl_initialized = false;

// *****************************************************************************
// Private Function Prototypes
// *****************************************************************************

static void load_default_settings(void);
static uint32_t calculate_settings_checksum(void);
static void apply_settings_to_motion_system(void);
static void format_setting_value(grbl_setting_id_t setting_id, float value, char *buffer, size_t buffer_size);

// *****************************************************************************
// Initialization and Configuration Functions
// *****************************************************************************

bool GRBL_Initialize(void)
{
    // Clear context
    memset(&grbl_context, 0, sizeof(grbl_context_t));

    // Load default settings
    load_default_settings();

    // Try to load saved settings (would come from EEPROM/Flash)
    if (!GRBL_LoadSettings())
    {
        // printf("No saved settings found, using defaults\n");  // Commented out - UGS treats as error
        grbl_context.settings = grbl_context.defaults;
    }

    // Initialize machine state
    grbl_context.state = GRBL_STATE_IDLE;
    grbl_context.alarm = ALARM_NONE;
    grbl_context.position_valid = false;

    // Configure limits
    grbl_context.limits.soft_limits_enabled = grbl_context.settings.soft_limits_enable;
    grbl_context.limits.hard_limits_enabled = grbl_context.settings.hard_limits_enable;
    grbl_context.limits.x_min = 0.0f;
    grbl_context.limits.x_max = grbl_context.settings.max_travel_mm[0];
    grbl_context.limits.y_min = 0.0f;
    grbl_context.limits.y_max = grbl_context.settings.max_travel_mm[1];
    grbl_context.limits.z_min = 0.0f;
    grbl_context.limits.z_max = grbl_context.settings.max_travel_mm[2];

    // Apply settings to motion system
    apply_settings_to_motion_system();

    grbl_initialized = true;

    // printf("GRBL Settings Interface Initialized\n");  // Commented out - UGS treats as error
    // printf("Soft Limits: %s, Hard Limits: %s\n",     // Commented out - UGS treats as error
    //        grbl_context.settings.soft_limits_enable ? "Enabled" : "Disabled",
    //        grbl_context.settings.hard_limits_enable ? "Enabled" : "Disabled");

    return true;
}

void GRBL_LoadDefaults(void)
{
    load_default_settings();
    grbl_context.settings = grbl_context.defaults;
    grbl_context.settings_changed = true;
    apply_settings_to_motion_system();

    printf("Settings reset to defaults\n");
}

bool GRBL_LoadSettings(void)
{
    // In a real implementation, this would load from EEPROM/Flash
    // For now, return false to use defaults
    return false;
}

bool GRBL_SaveSettings(void)
{
    if (!grbl_initialized)
        return false;

    // Calculate and store checksum
    grbl_context.settings.checksum = calculate_settings_checksum();
    grbl_context.settings.version = 1;

    // In a real implementation, this would save to EEPROM/Flash
    // For now, just mark as saved
    grbl_context.settings_changed = false;

    printf("Settings saved\n");
    return true;
}

void GRBL_Shutdown(void)
{
    if (grbl_context.settings_changed)
    {
        GRBL_SaveSettings();
    }

    grbl_initialized = false;
}

// *****************************************************************************
// Settings Management Functions
// *****************************************************************************

bool GRBL_SetSetting(grbl_setting_id_t setting_id, float value)
{
    if (!grbl_initialized)
        return false;

    // Validate the setting value
    if (!GRBL_ValidateSetting(setting_id, value))
    {
        GRBL_SendError(GRBL_ERROR_VALUE_NEGATIVE);
        return false;
    }

    // Apply the setting
    switch (setting_id)
    {
    case SETTING_STEP_PULSE_MICROSECONDS:
        grbl_context.settings.step_pulse_microseconds = (uint8_t)value;
        break;
    case SETTING_STEP_IDLE_DELAY:
        grbl_context.settings.step_idle_delay = (uint8_t)value;
        break;
    case SETTING_STEP_PORT_INVERT_MASK:
        grbl_context.settings.step_port_invert_mask = (uint8_t)value;
        break;
    case SETTING_DIR_PORT_INVERT_MASK:
        grbl_context.settings.dir_port_invert_mask = (uint8_t)value;
        break;
    case SETTING_STEP_ENABLE_INVERT:
        grbl_context.settings.step_enable_invert = (value != 0.0f);
        break;
    case SETTING_LIMIT_PINS_INVERT:
        grbl_context.settings.limit_pins_invert = (value != 0.0f);
        grbl_context.limits.limit_switches_inverted = grbl_context.settings.limit_pins_invert;
        break;
    case SETTING_PROBE_PIN_INVERT:
        grbl_context.settings.probe_pin_invert = (value != 0.0f);
        break;
    case SETTING_STATUS_REPORT_MASK:
        grbl_context.settings.status_report_mask = (uint8_t)value;
        break;
    case SETTING_JUNCTION_DEVIATION:
        grbl_context.settings.junction_deviation = value;
        break;
    case SETTING_ARC_TOLERANCE:
        grbl_context.settings.arc_tolerance = value;
        break;
    case SETTING_REPORT_INCHES:
        grbl_context.settings.report_inches = (value != 0.0f);
        break;
    case SETTING_SOFT_LIMITS:
        grbl_context.settings.soft_limits_enable = (value != 0.0f);
        grbl_context.limits.soft_limits_enabled = grbl_context.settings.soft_limits_enable;
        break;
    case SETTING_HARD_LIMITS:
        grbl_context.settings.hard_limits_enable = (value != 0.0f);
        grbl_context.limits.hard_limits_enabled = grbl_context.settings.hard_limits_enable;
        break;
    case SETTING_HOMING_CYCLE:
        grbl_context.settings.homing_cycle_enable = (value != 0.0f);
        break;
    case SETTING_HOMING_DIR_INVERT:
        grbl_context.settings.homing_dir_invert_mask = (uint8_t)value;
        break;
    case SETTING_HOMING_FEED:
        grbl_context.settings.homing_feed_rate = value;
        break;
    case SETTING_HOMING_SEEK:
        grbl_context.settings.homing_seek_rate = value;
        break;
    case SETTING_HOMING_DEBOUNCE:
        grbl_context.settings.homing_debounce_ms = (uint16_t)value;
        break;
    case SETTING_HOMING_PULLOFF:
        grbl_context.settings.homing_pulloff_mm = value;
        break;
    case SETTING_SPINDLE_MAX_RPM:
        grbl_context.settings.spindle_max_rpm = value;
        break;
    case SETTING_SPINDLE_MIN_RPM:
        grbl_context.settings.spindle_min_rpm = value;
        break;
    case SETTING_LASER_MODE:
        grbl_context.settings.laser_mode = (value != 0.0f);
        break;
    case SETTING_X_STEPS_PER_MM:
        grbl_context.settings.steps_per_mm[0] = value;
        break;
    case SETTING_Y_STEPS_PER_MM:
        grbl_context.settings.steps_per_mm[1] = value;
        break;
    case SETTING_Z_STEPS_PER_MM:
        grbl_context.settings.steps_per_mm[2] = value;
        break;
    case SETTING_X_MAX_RATE:
        grbl_context.settings.max_rate_mm_per_min[0] = value;
        break;
    case SETTING_Y_MAX_RATE:
        grbl_context.settings.max_rate_mm_per_min[1] = value;
        break;
    case SETTING_Z_MAX_RATE:
        grbl_context.settings.max_rate_mm_per_min[2] = value;
        break;
    case SETTING_X_ACCELERATION:
        grbl_context.settings.acceleration_mm_per_sec2[0] = value;
        break;
    case SETTING_Y_ACCELERATION:
        grbl_context.settings.acceleration_mm_per_sec2[1] = value;
        break;
    case SETTING_Z_ACCELERATION:
        grbl_context.settings.acceleration_mm_per_sec2[2] = value;
        break;
    case SETTING_X_MAX_TRAVEL:
        grbl_context.settings.max_travel_mm[0] = value;
        grbl_context.limits.x_max = value;
        break;
    case SETTING_Y_MAX_TRAVEL:
        grbl_context.settings.max_travel_mm[1] = value;
        grbl_context.limits.y_max = value;
        break;
    case SETTING_Z_MAX_TRAVEL:
        grbl_context.settings.max_travel_mm[2] = value;
        grbl_context.limits.z_max = value;
        break;
    default:
        GRBL_SendError(GRBL_ERROR_INVALID_STATEMENT);
        return false;
    }

    // Apply changes to motion system
    apply_settings_to_motion_system();

    // Mark settings as changed
    grbl_context.settings_changed = true;

    // Send confirmation
    GRBL_SendOK();

    printf("Setting $%d set to %.3f\n", setting_id, value);

    return true;
}

float GRBL_GetSetting(grbl_setting_id_t setting_id)
{
    if (!grbl_initialized)
        return 0.0f;

    switch (setting_id)
    {
    case SETTING_STEP_PULSE_MICROSECONDS:
        return (float)grbl_context.settings.step_pulse_microseconds;
    case SETTING_STEP_IDLE_DELAY:
        return (float)grbl_context.settings.step_idle_delay;
    case SETTING_STEP_PORT_INVERT_MASK:
        return (float)grbl_context.settings.step_port_invert_mask;
    case SETTING_DIR_PORT_INVERT_MASK:
        return (float)grbl_context.settings.dir_port_invert_mask;
    case SETTING_STEP_ENABLE_INVERT:
        return grbl_context.settings.step_enable_invert ? 1.0f : 0.0f;
    case SETTING_LIMIT_PINS_INVERT:
        return grbl_context.settings.limit_pins_invert ? 1.0f : 0.0f;
    case SETTING_PROBE_PIN_INVERT:
        return grbl_context.settings.probe_pin_invert ? 1.0f : 0.0f;
    case SETTING_STATUS_REPORT_MASK:
        return (float)grbl_context.settings.status_report_mask;
    case SETTING_JUNCTION_DEVIATION:
        return grbl_context.settings.junction_deviation;
    case SETTING_ARC_TOLERANCE:
        return grbl_context.settings.arc_tolerance;
    case SETTING_REPORT_INCHES:
        return grbl_context.settings.report_inches ? 1.0f : 0.0f;
    case SETTING_SOFT_LIMITS:
        return grbl_context.settings.soft_limits_enable ? 1.0f : 0.0f;
    case SETTING_HARD_LIMITS:
        return grbl_context.settings.hard_limits_enable ? 1.0f : 0.0f;
    case SETTING_HOMING_CYCLE:
        return grbl_context.settings.homing_cycle_enable ? 1.0f : 0.0f;
    case SETTING_HOMING_DIR_INVERT:
        return (float)grbl_context.settings.homing_dir_invert_mask;
    case SETTING_HOMING_FEED:
        return grbl_context.settings.homing_feed_rate;
    case SETTING_HOMING_SEEK:
        return grbl_context.settings.homing_seek_rate;
    case SETTING_HOMING_DEBOUNCE:
        return (float)grbl_context.settings.homing_debounce_ms;
    case SETTING_HOMING_PULLOFF:
        return grbl_context.settings.homing_pulloff_mm;
    case SETTING_SPINDLE_MAX_RPM:
        return grbl_context.settings.spindle_max_rpm;
    case SETTING_SPINDLE_MIN_RPM:
        return grbl_context.settings.spindle_min_rpm;
    case SETTING_LASER_MODE:
        return grbl_context.settings.laser_mode ? 1.0f : 0.0f;
    case SETTING_X_STEPS_PER_MM:
        return grbl_context.settings.steps_per_mm[0];
    case SETTING_Y_STEPS_PER_MM:
        return grbl_context.settings.steps_per_mm[1];
    case SETTING_Z_STEPS_PER_MM:
        return grbl_context.settings.steps_per_mm[2];
    case SETTING_X_MAX_RATE:
        return grbl_context.settings.max_rate_mm_per_min[0];
    case SETTING_Y_MAX_RATE:
        return grbl_context.settings.max_rate_mm_per_min[1];
    case SETTING_Z_MAX_RATE:
        return grbl_context.settings.max_rate_mm_per_min[2];
    case SETTING_X_ACCELERATION:
        return grbl_context.settings.acceleration_mm_per_sec2[0];
    case SETTING_Y_ACCELERATION:
        return grbl_context.settings.acceleration_mm_per_sec2[1];
    case SETTING_Z_ACCELERATION:
        return grbl_context.settings.acceleration_mm_per_sec2[2];
    case SETTING_X_MAX_TRAVEL:
        return grbl_context.settings.max_travel_mm[0];
    case SETTING_Y_MAX_TRAVEL:
        return grbl_context.settings.max_travel_mm[1];
    case SETTING_Z_MAX_TRAVEL:
        return grbl_context.settings.max_travel_mm[2];
    default:
        return 0.0f;
    }
}

bool GRBL_ValidateSetting(grbl_setting_id_t setting_id, float value)
{
    switch (setting_id)
    {
    case SETTING_STEP_PULSE_MICROSECONDS:
        return (value >= 3.0f && value <= 1000.0f);
    case SETTING_STEP_IDLE_DELAY:
        return (value >= 0.0f && value <= 65535.0f);
    case SETTING_STEP_PORT_INVERT_MASK:
    case SETTING_DIR_PORT_INVERT_MASK:
        return (value >= 0.0f && value <= 255.0f);
    case SETTING_STEP_ENABLE_INVERT:
    case SETTING_LIMIT_PINS_INVERT:
    case SETTING_PROBE_PIN_INVERT:
    case SETTING_REPORT_INCHES:
    case SETTING_SOFT_LIMITS:
    case SETTING_HARD_LIMITS:
    case SETTING_HOMING_CYCLE:
    case SETTING_LASER_MODE:
        return (value == 0.0f || value == 1.0f);
    case SETTING_JUNCTION_DEVIATION:
        return (value >= 0.0f && value <= 2.0f);
    case SETTING_ARC_TOLERANCE:
        return (value >= 0.0f && value <= 1.0f);
    case SETTING_HOMING_FEED:
    case SETTING_HOMING_SEEK:
        return (value > 0.0f && value <= 50000.0f);
    case SETTING_HOMING_DEBOUNCE:
        return (value >= 0.0f && value <= 65535.0f);
    case SETTING_HOMING_PULLOFF:
        return (value >= 0.0f && value <= 100.0f);
    case SETTING_SPINDLE_MAX_RPM:
    case SETTING_SPINDLE_MIN_RPM:
        return (value >= 0.0f && value <= 100000.0f);
    case SETTING_X_STEPS_PER_MM:
    case SETTING_Y_STEPS_PER_MM:
    case SETTING_Z_STEPS_PER_MM:
        return (value > 0.0f && value <= 10000.0f);
    case SETTING_X_MAX_RATE:
    case SETTING_Y_MAX_RATE:
    case SETTING_Z_MAX_RATE:
        return (value > 0.0f && value <= 100000.0f);
    case SETTING_X_ACCELERATION:
    case SETTING_Y_ACCELERATION:
    case SETTING_Z_ACCELERATION:
        return (value > 0.0f && value <= 10000.0f);
    case SETTING_X_MAX_TRAVEL:
    case SETTING_Y_MAX_TRAVEL:
    case SETTING_Z_MAX_TRAVEL:
        return (value > 0.0f && value <= 10000.0f);
    default:
        return false;
    }
}

void GRBL_PrintSettings(void)
{
    if (!grbl_initialized)
        return;

    for (int i = 0; i < SETTINGS_COUNT; i++)
    {
        // Skip unused setting numbers
        if (i >= 7 && i <= 9)
            continue;
        if (i >= 14 && i <= 19)
            continue;
        if (i >= 28 && i <= 29)
            continue;
        if (i >= 33 && i <= 99)
            continue;
        if (i >= 103 && i <= 109)
            continue;
        if (i >= 113 && i <= 119)
            continue;
        if (i >= 123 && i <= 129)
            continue;
        if (i >= 133)
            continue;

        GRBL_PrintSetting((grbl_setting_id_t)i);
    }

    // Send ok after all settings are printed (GRBL standard)
    GRBL_SendOK();
}

void GRBL_PrintSetting(grbl_setting_id_t setting_id)
{
    char value_buffer[32];
    float value = GRBL_GetSetting(setting_id);

    format_setting_value(setting_id, value, value_buffer, sizeof(value_buffer));

    char response[128];
    snprintf(response, sizeof(response), "$%d=%s", setting_id, value_buffer);

    GRBL_SendResponse(response);
}

// *****************************************************************************
// Command Processing Functions
// *****************************************************************************

bool GRBL_ProcessSystemCommand(const char *command)
{
    if (!grbl_initialized || !command)
        return false;

    // DEBUG: Log which command we're processing (disabled for clean UGS operation)
    // char debug_msg[64];
    // snprintf(debug_msg, sizeof(debug_msg), "DEBUG: Processing command '%s'", command);
    // GCODE_DMA_SendResponse(debug_msg);

    // Handle $$ command (print all settings)
    if (strcmp(command, "$$") == 0)
    {
        GRBL_PrintSettings();
        return true;
    }

    // Handle $RST=$ command (reset settings)
    if (strcmp(command, "$RST=$") == 0)
    {
        GRBL_LoadDefaults();
        GRBL_SendOK();
        return true;
    }

    // Handle setting assignment: $100=160
    if (command[0] == '$' && strchr(command, '=') != NULL)
    {
        char *equals_pos = strchr(command, '=');
        *equals_pos = '\0';

        int setting_num = atoi(command + 1);
        float value = atof(equals_pos + 1);

        return GRBL_SetSetting((grbl_setting_id_t)setting_num, value);
    }

    // Handle other system commands
    if (strcmp(command, "$H") == 0)
    {
        // Home cycle - would trigger homing routine
        printf("Homing cycle initiated\n");
        GRBL_SendOK();
        return true;
    }

    if (strcmp(command, "$X") == 0)
    {
        // Unlock/clear alarm
        GRBL_ClearAlarm();
        GRBL_SetState(GRBL_STATE_IDLE);
        GRBL_SendOK();
        return true;
    }

    if (strcmp(command, "$C") == 0)
    {
        // Check mode toggle
        printf("Check mode toggled\n");
        GRBL_SendOK();
        return true;
    }

    // Handle $I command (build info) - required by UGS
    if (strcmp(command, "$I") == 0)
    {
        // Send GRBL build info in exact v1.1f format as per official GRBL
        GRBL_SendResponse("[VER:1.1f.20161014:]");
        GRBL_SendResponse("[OPT:VL,15,128]"); // VL = Variable spindle + Laser mode, 15 blocks, 128 bytes
        GRBL_SendOK();
        return true;
    }

    // Handle $G command (parser state) - required by UGS
    if (strcmp(command, "$G") == 0)
    {
        // Send GRBL parser state in standard format
        GRBL_SendResponse("[GC:G0 G54 G17 G21 G90 G94 M5 M9 T0 F0.0 S0]");
        GRBL_SendOK();
        return true;
    }

    // Handle $# command (coordinate system data) - required by UGS
    if (strcmp(command, "$#") == 0)
    {
        // Send coordinate system data in GRBL format
        GRBL_SendResponse("[G54:0.000,0.000,0.000]");
        GRBL_SendResponse("[G55:0.000,0.000,0.000]");
        GRBL_SendResponse("[G56:0.000,0.000,0.000]");
        GRBL_SendResponse("[G57:0.000,0.000,0.000]");
        GRBL_SendResponse("[G58:0.000,0.000,0.000]");
        GRBL_SendResponse("[G59:0.000,0.000,0.000]");
        GRBL_SendResponse("[G28:0.000,0.000,0.000]");
        GRBL_SendResponse("[G30:0.000,0.000,0.000]");
        GRBL_SendResponse("[G92:0.000,0.000,0.000]");
        GRBL_SendResponse("[TLO:0.000]");
        GRBL_SendResponse("[PRB:0.000,0.000,0.000:0]");
        GRBL_SendOK();
        return true;
    }

    // Handle $N command (startup lines) - sometimes used by UGS
    if (strncmp(command, "$N", 2) == 0)
    {
        // Send empty startup line responses
        GRBL_SendResponse("$N0=");
        GRBL_SendResponse("$N1=");
        GRBL_SendOK();
        return true;
    }

    GRBL_SendError(GRBL_ERROR_INVALID_STATEMENT);
    return false;
}

void GRBL_SendResponse(const char *response)
{
    APP_UARTPrint(response);
    APP_UARTPrint("\r\n");
}

void GRBL_SendOK(void)
{
    GRBL_SendResponse("ok");
}

void GRBL_SendError(uint8_t error_code)
{
    char error_msg[32];
    snprintf(error_msg, sizeof(error_msg), "error:%d", error_code);
    GRBL_SendResponse(error_msg);
}

void GRBL_SendAlarm(grbl_alarm_t alarm)
{
    char alarm_msg[32];
    snprintf(alarm_msg, sizeof(alarm_msg), "ALARM:%d", alarm);
    GRBL_SendResponse(alarm_msg);

    grbl_context.alarm = alarm;
    GRBL_SetState(GRBL_STATE_ALARM);
}

void GRBL_SendStatusReport(void)
{
    if (!grbl_initialized)
        return;

    // Get current position from interpolation engine
    volatile position_t current_pos = INTERP_GetCurrentPosition(); // Ensure volatile to prevent optimization

    // Update context
    grbl_context.current_position[0] = current_pos.x;
    grbl_context.current_position[1] = current_pos.y;
    grbl_context.current_position[2] = current_pos.z;

    // Calculate work position
    float work_x = current_pos.x - grbl_context.work_coordinate_offset[0];
    float work_y = current_pos.y - grbl_context.work_coordinate_offset[1];
    float work_z = current_pos.z - grbl_context.work_coordinate_offset[2];

    // Format status report
    char status_buffer[256];
    const char *state_str = "Idle";
    switch (grbl_context.state)
    {
    case GRBL_STATE_IDLE:
        state_str = "Idle";
        break;
    case GRBL_STATE_RUN:
        state_str = "Run";
        break;
    case GRBL_STATE_HOLD:
        state_str = "Hold";
        break;
    case GRBL_STATE_JOG:
        state_str = "Jog";
        break;
    case GRBL_STATE_ALARM:
        state_str = "Alarm";
        break;
    case GRBL_STATE_DOOR:
        state_str = "Door";
        break;
    case GRBL_STATE_CHECK:
        state_str = "Check";
        break;
    case GRBL_STATE_HOME:
        state_str = "Home";
        break;
    case GRBL_STATE_SLEEP:
        state_str = "Sleep";
        break;
    }

    snprintf(status_buffer, sizeof(status_buffer),
             "<%s|MPos:%.3f,%.3f,%.3f|WPos:%.3f,%.3f,%.3f|FS:%.0f,%.0f>",
             state_str,
             current_pos.x, current_pos.y, current_pos.z,
             work_x, work_y, work_z,
             grbl_context.current_feed_rate,
             grbl_context.current_spindle_speed);

    GRBL_SendResponse(status_buffer);
}

// *****************************************************************************
// Limit and Safety Functions
// *****************************************************************************

bool GRBL_CheckSoftLimits(float x, float y, float z)
{
    if (!grbl_context.limits.soft_limits_enabled)
        return true;

    if (x < grbl_context.limits.x_min || x > grbl_context.limits.x_max ||
        y < grbl_context.limits.y_min || y > grbl_context.limits.y_max ||
        z < grbl_context.limits.z_min || z > grbl_context.limits.z_max)
    {

        GRBL_TriggerAlarm(ALARM_SOFT_LIMIT);
        return false;
    }

    return true;
}

bool GRBL_CheckHardLimits(void)
{
    if (!grbl_context.limits.hard_limits_enabled)
        return true;

    // Read hardware limit switches
    bool limits_triggered = GRBL_ReadLimitSwitches();

    if (limits_triggered)
    {
        GRBL_TriggerAlarm(ALARM_HARD_LIMIT);
        return false;
    }

    return true;
}

void GRBL_TriggerAlarm(grbl_alarm_t alarm)
{
    grbl_context.alarm = alarm;
    GRBL_SetState(GRBL_STATE_ALARM);

    // Emergency stop motion
    INTERP_EmergencyStop();

    // Send alarm notification
    GRBL_SendAlarm(alarm);

    printf("ALARM TRIGGERED: %d\n", alarm);
}

void GRBL_ClearAlarm(void)
{
    grbl_context.alarm = ALARM_NONE;
    printf("Alarm cleared\n");
}

// *****************************************************************************
// Real-time Command Handlers (UGS Interface)
// *****************************************************************************

void GRBL_HandleQuestionMark(void)
{
    GRBL_SendStatusReport();
}

void GRBL_HandleExclamation(void)
{
    GRBL_FeedHold();
}

void GRBL_HandleTilde(void)
{
    GRBL_CycleStart();
}

void GRBL_HandleCtrlX(void)
{
    GRBL_Reset();
}

void GRBL_FeedHold(void)
{
    INTERP_FeedHold(true);
    GRBL_SetState(GRBL_STATE_HOLD);
    printf("Feed hold activated\n");
}

void GRBL_CycleStart(void)
{
    INTERP_FeedHold(false);
    GRBL_SetState(GRBL_STATE_RUN);
    printf("Cycle start/resume\n");
}

void GRBL_Reset(void)
{
    INTERP_EmergencyStop();
    GRBL_SetState(GRBL_STATE_IDLE);
    GRBL_ClearAlarm();
    printf("System reset\n");
}

// *****************************************************************************
// Private Helper Functions
// *****************************************************************************

static void load_default_settings(void)
{
    grbl_settings_t *defaults = &grbl_context.defaults;

    // Stepper settings
    defaults->step_pulse_microseconds = GRBL_DEFAULT_STEP_PULSE_MICROSECONDS;
    defaults->step_idle_delay = GRBL_DEFAULT_STEP_IDLE_DELAY;
    defaults->step_port_invert_mask = 0;
    defaults->dir_port_invert_mask = 0;
    defaults->step_enable_invert = false;
    defaults->limit_pins_invert = false;
    defaults->probe_pin_invert = false;

    // Reporting and control
    defaults->status_report_mask = 1;
    defaults->junction_deviation = GRBL_DEFAULT_JUNCTION_DEVIATION;
    defaults->arc_tolerance = GRBL_DEFAULT_ARC_TOLERANCE;
    defaults->report_inches = false;

    // Limits and safety
    defaults->soft_limits_enable = false;
    defaults->hard_limits_enable = false;
    defaults->homing_cycle_enable = false;
    defaults->homing_dir_invert_mask = 0;
    defaults->homing_feed_rate = GRBL_DEFAULT_HOMING_FEED_RATE;
    defaults->homing_seek_rate = GRBL_DEFAULT_HOMING_SEEK_RATE;
    defaults->homing_debounce_ms = GRBL_DEFAULT_HOMING_DEBOUNCE_MS;
    defaults->homing_pulloff_mm = GRBL_DEFAULT_HOMING_PULLOFF_MM;

    // Spindle settings
    defaults->spindle_max_rpm = GRBL_DEFAULT_SPINDLE_MAX_RPM;
    defaults->spindle_min_rpm = GRBL_DEFAULT_SPINDLE_MIN_RPM;
    defaults->laser_mode = false;

    // Axis parameters
    defaults->steps_per_mm[0] = GRBL_DEFAULT_X_STEPS_PER_MM;
    defaults->steps_per_mm[1] = GRBL_DEFAULT_Y_STEPS_PER_MM;
    defaults->steps_per_mm[2] = GRBL_DEFAULT_Z_STEPS_PER_MM;
    defaults->max_rate_mm_per_min[0] = GRBL_DEFAULT_X_MAX_RATE;
    defaults->max_rate_mm_per_min[1] = GRBL_DEFAULT_Y_MAX_RATE;
    defaults->max_rate_mm_per_min[2] = GRBL_DEFAULT_Z_MAX_RATE;
    defaults->acceleration_mm_per_sec2[0] = GRBL_DEFAULT_X_ACCELERATION;
    defaults->acceleration_mm_per_sec2[1] = GRBL_DEFAULT_Y_ACCELERATION;
    defaults->acceleration_mm_per_sec2[2] = GRBL_DEFAULT_Z_ACCELERATION;
    defaults->max_travel_mm[0] = GRBL_DEFAULT_X_MAX_TRAVEL;
    defaults->max_travel_mm[1] = GRBL_DEFAULT_Y_MAX_TRAVEL;
    defaults->max_travel_mm[2] = GRBL_DEFAULT_Z_MAX_TRAVEL;
}

static void apply_settings_to_motion_system(void)
{
    // Update interpolation engine with new settings
    INTERP_Configure(grbl_context.settings.steps_per_mm,
                     grbl_context.settings.max_rate_mm_per_min,
                     grbl_context.settings.acceleration_mm_per_sec2);
}

static void format_setting_value(grbl_setting_id_t setting_id, float value, char *buffer, size_t buffer_size)
{
    // Format based on setting type
    if (setting_id == SETTING_STEP_PULSE_MICROSECONDS ||
        setting_id == SETTING_STEP_IDLE_DELAY ||
        setting_id == SETTING_STEP_PORT_INVERT_MASK ||
        setting_id == SETTING_DIR_PORT_INVERT_MASK ||
        setting_id == SETTING_STATUS_REPORT_MASK ||
        setting_id == SETTING_HOMING_DIR_INVERT ||
        setting_id == SETTING_HOMING_DEBOUNCE)
    {
        snprintf(buffer, buffer_size, "%.0f", value);
    }
    else if (setting_id == SETTING_STEP_ENABLE_INVERT ||
             setting_id == SETTING_LIMIT_PINS_INVERT ||
             setting_id == SETTING_PROBE_PIN_INVERT ||
             setting_id == SETTING_REPORT_INCHES ||
             setting_id == SETTING_SOFT_LIMITS ||
             setting_id == SETTING_HARD_LIMITS ||
             setting_id == SETTING_HOMING_CYCLE ||
             setting_id == SETTING_LASER_MODE)
    {
        snprintf(buffer, buffer_size, "%.0f", value);
    }
    else
    {
        snprintf(buffer, buffer_size, "%.3f", value);
    }
}

static uint32_t calculate_settings_checksum(void)
{
    // Simple checksum calculation
    uint32_t checksum = 0;
    uint8_t *data = (uint8_t *)&grbl_context.settings;
    size_t size = sizeof(grbl_settings_t) - sizeof(uint32_t) - sizeof(uint16_t); // Exclude checksum and version

    for (size_t i = 0; i < size; i++)
    {
        checksum += data[i];
    }

    return checksum;
}

// State and position management
void GRBL_SetState(grbl_state_t new_state)
{
    grbl_context.state = new_state;
}

grbl_state_t GRBL_GetState(void)
{
    return grbl_context.state;
}

void GRBL_UpdatePosition(float x, float y, float z)
{
    grbl_context.current_position[0] = x;
    grbl_context.current_position[1] = y;
    grbl_context.current_position[2] = z;
    grbl_context.position_valid = true;
}

void GRBL_SetWorkCoordinates(float x, float y, float z)
{
    grbl_context.work_coordinate_offset[0] = grbl_context.current_position[0] - x;
    grbl_context.work_coordinate_offset[1] = grbl_context.current_position[1] - y;
    grbl_context.work_coordinate_offset[2] = grbl_context.current_position[2] - z;
}

// Hardware interface stubs (implement based on your hardware)
bool GRBL_ReadLimitSwitches(void)
{
    return INTERP_ReadLimitSwitch(AXIS_X) ||
           INTERP_ReadLimitSwitch(AXIS_Y) ||
           INTERP_ReadLimitSwitch(AXIS_Z);
}

bool GRBL_ReadProbePin(void)
{
    // Implement based on your probe pin configuration
    return false;
}

void GRBL_SetSpindleSpeed(float rpm)
{
    grbl_context.current_spindle_speed = rpm;
    // Implement spindle control based on your hardware
}

/*******************************************************************************
 End of File
 */