/*******************************************************************************
  GRBL Settings Interface for Universal G-code Sender (UGS) Compatibility

  File Name:
    grbl_settings.h

  Summary:
    Complete GRBL settings implementation for UGS integration

  Description:
    Provides full UGS-compatible settings interface including:
    - $$ commands for viewing/setting machine parameters
    - Software and hardware limit management
    - Real-time setting updates
    - EEPROM-style persistent storage
    - Complete GRBL protocol compliance
*******************************************************************************/

#ifndef GRBL_SETTINGS_H
#define GRBL_SETTINGS_H

#include <stdint.h>
#include <stdbool.h>
#include <float.h>

// *****************************************************************************
// GRBL Settings Numbers (Standard GRBL Protocol)
// *****************************************************************************

typedef enum {
    SETTING_STEP_PULSE_MICROSECONDS = 0,       // $0 - Step pulse time
    SETTING_STEP_IDLE_DELAY = 1,                // $1 - Step idle delay
    SETTING_STEP_PORT_INVERT_MASK = 2,          // $2 - Step port invert
    SETTING_DIR_PORT_INVERT_MASK = 3,           // $3 - Direction port invert
    SETTING_STEP_ENABLE_INVERT = 4,             // $4 - Step enable invert
    SETTING_LIMIT_PINS_INVERT = 5,              // $5 - Limit pins invert
    SETTING_PROBE_PIN_INVERT = 6,               // $6 - Probe pin invert
    SETTING_STATUS_REPORT_MASK = 10,            // $10 - Status report options
    SETTING_JUNCTION_DEVIATION = 11,            // $11 - Junction deviation
    SETTING_ARC_TOLERANCE = 12,                 // $12 - Arc tolerance
    SETTING_REPORT_INCHES = 13,                 // $13 - Report in inches
    SETTING_SOFT_LIMITS = 20,                   // $20 - Soft limits enable
    SETTING_HARD_LIMITS = 21,                   // $21 - Hard limits enable
    SETTING_HOMING_CYCLE = 22,                  // $22 - Homing cycle enable
    SETTING_HOMING_DIR_INVERT = 23,             // $23 - Homing direction invert
    SETTING_HOMING_FEED = 24,                   // $24 - Homing feed rate
    SETTING_HOMING_SEEK = 25,                   // $25 - Homing seek rate
    SETTING_HOMING_DEBOUNCE = 26,               // $26 - Homing debounce
    SETTING_HOMING_PULLOFF = 27,                // $27 - Homing pull-off
    SETTING_SPINDLE_MAX_RPM = 30,               // $30 - Maximum spindle speed
    SETTING_SPINDLE_MIN_RPM = 31,               // $31 - Minimum spindle speed
    SETTING_LASER_MODE = 32,                    // $32 - Laser mode enable
    
    // Axis-specific settings (X, Y, Z)
    SETTING_X_STEPS_PER_MM = 100,               // $100 - X steps per mm
    SETTING_Y_STEPS_PER_MM = 101,               // $101 - Y steps per mm
    SETTING_Z_STEPS_PER_MM = 102,               // $102 - Z steps per mm
    SETTING_X_MAX_RATE = 110,                   // $110 - X max rate mm/min
    SETTING_Y_MAX_RATE = 111,                   // $111 - Y max rate mm/min
    SETTING_Z_MAX_RATE = 112,                   // $112 - Z max rate mm/min
    SETTING_X_ACCELERATION = 120,               // $120 - X acceleration mm/sec²
    SETTING_Y_ACCELERATION = 121,               // $121 - Y acceleration mm/sec²
    SETTING_Z_ACCELERATION = 122,               // $122 - Z acceleration mm/sec²
    SETTING_X_MAX_TRAVEL = 130,                 // $130 - X max travel mm
    SETTING_Y_MAX_TRAVEL = 131,                 // $131 - Y max travel mm
    SETTING_Z_MAX_TRAVEL = 132,                 // $132 - Z max travel mm
    
    SETTINGS_COUNT                              // Total number of settings
} grbl_setting_id_t;

// *****************************************************************************
// Machine State and Limits
// *****************************************************************************

typedef enum {
    GRBL_STATE_IDLE = 0,
    GRBL_STATE_RUN,
    GRBL_STATE_HOLD,
    GRBL_STATE_JOG,
    GRBL_STATE_ALARM,
    GRBL_STATE_DOOR,
    GRBL_STATE_CHECK,
    GRBL_STATE_HOME,
    GRBL_STATE_SLEEP
} grbl_state_t;

typedef enum {
    ALARM_NONE = 0,
    ALARM_HARD_LIMIT = 1,
    ALARM_SOFT_LIMIT = 2,
    ALARM_ABORT_CYCLE = 3,
    ALARM_PROBE_FAIL_INITIAL = 4,
    ALARM_PROBE_FAIL_CONTACT = 5,
    ALARM_HOMING_FAIL_RESET = 6,
    ALARM_HOMING_FAIL_DOOR = 7,
    ALARM_HOMING_FAIL_PULLOFF = 8,
    ALARM_HOMING_FAIL_APPROACH = 9
} grbl_alarm_t;

typedef struct {
    float x_min, x_max;                         // Software limits
    float y_min, y_max;
    float z_min, z_max;
    bool soft_limits_enabled;
    bool hard_limits_enabled;
    bool limit_switches_inverted;
} machine_limits_t;

typedef struct {
    bool x_limit_triggered;                     // Hardware limit states
    bool y_limit_triggered;
    bool z_limit_triggered;
    bool probe_triggered;
    bool door_open;
    bool reset_triggered;
} hardware_status_t;

// *****************************************************************************
// Settings Storage Structure
// *****************************************************************************

typedef struct {
    // Stepper settings
    uint8_t step_pulse_microseconds;            // $0
    uint8_t step_idle_delay;                    // $1
    uint8_t step_port_invert_mask;              // $2
    uint8_t dir_port_invert_mask;               // $3
    bool step_enable_invert;                    // $4
    bool limit_pins_invert;                     // $5
    bool probe_pin_invert;                      // $6
    
    // Reporting and control
    uint8_t status_report_mask;                 // $10
    float junction_deviation;                   // $11
    float arc_tolerance;                        // $12
    bool report_inches;                         // $13
    
    // Limits and safety
    bool soft_limits_enable;                    // $20
    bool hard_limits_enable;                    // $21
    bool homing_cycle_enable;                   // $22
    uint8_t homing_dir_invert_mask;             // $23
    float homing_feed_rate;                     // $24
    float homing_seek_rate;                     // $25
    uint16_t homing_debounce_ms;                // $26
    float homing_pulloff_mm;                    // $27
    
    // Spindle settings
    float spindle_max_rpm;                      // $30
    float spindle_min_rpm;                      // $31
    bool laser_mode;                            // $32
    
    // Axis parameters
    float steps_per_mm[3];                      // $100-102
    float max_rate_mm_per_min[3];               // $110-112
    float acceleration_mm_per_sec2[3];          // $120-122
    float max_travel_mm[3];                     // $130-132
    
    // Validation and versioning
    uint32_t checksum;                          // Settings validation
    uint16_t version;                           // Settings version
} grbl_settings_t;

// *****************************************************************************
// Global Settings Context
// *****************************************************************************

typedef struct {
    grbl_settings_t settings;                   // Current settings
    grbl_settings_t defaults;                   // Factory defaults
    grbl_state_t state;                         // Current machine state
    grbl_alarm_t alarm;                         // Current alarm state
    machine_limits_t limits;                    // Limit configuration
    hardware_status_t hardware;                 // Hardware status
    
    // Runtime state
    float current_position[3];                  // X, Y, Z positions
    float work_coordinate_offset[3];            // G54 work coordinates
    float machine_position[3];                  // Machine coordinates
    float current_feed_rate;                    // Current feed rate
    float current_spindle_speed;                // Current spindle RPM
    
    // Status tracking
    uint32_t line_number;                       // Current G-code line
    bool settings_changed;                      // Flag for EEPROM save
    bool position_valid;                        // Position known flag
} grbl_context_t;

// *****************************************************************************
// Function Prototypes
// *****************************************************************************

// Initialization and Configuration
bool GRBL_Initialize(void);
void GRBL_LoadDefaults(void);
bool GRBL_LoadSettings(void);
bool GRBL_SaveSettings(void);
void GRBL_Shutdown(void);

// Settings Management
bool GRBL_SetSetting(grbl_setting_id_t setting_id, float value);
float GRBL_GetSetting(grbl_setting_id_t setting_id);
bool GRBL_ValidateSetting(grbl_setting_id_t setting_id, float value);
void GRBL_PrintSettings(void);
void GRBL_PrintSetting(grbl_setting_id_t setting_id);

// Command Processing
bool GRBL_ProcessSystemCommand(const char *command);
void GRBL_SendOK(void);
void GRBL_SendError(uint8_t error_code);
void GRBL_SendAlarm(grbl_alarm_t alarm);
void GRBL_SendStatusReport(void);

// State Management
void GRBL_SetState(grbl_state_t new_state);
grbl_state_t GRBL_GetState(void);
void GRBL_SetAlarm(grbl_alarm_t alarm);
grbl_alarm_t GRBL_GetAlarm(void);

// Limit and Safety Functions
bool GRBL_CheckSoftLimits(float x, float y, float z);
bool GRBL_CheckHardLimits(void);
void GRBL_TriggerAlarm(grbl_alarm_t alarm);
void GRBL_ClearAlarm(void);
void GRBL_EmergencyStop(void);

// Position Management
void GRBL_UpdatePosition(float x, float y, float z);
void GRBL_SetWorkCoordinates(float x, float y, float z);
void GRBL_GetMachinePosition(float *x, float *y, float *z);
void GRBL_GetWorkPosition(float *x, float *y, float *z);

// Hardware Interface
bool GRBL_ReadLimitSwitches(void);
bool GRBL_ReadProbePin(void);
bool GRBL_ReadDoorPin(void);
void GRBL_SetSpindleSpeed(float rpm);
void GRBL_SetSpindleDirection(bool clockwise);
void GRBL_SpindleEnable(bool enable);

// Real-time Commands
void GRBL_FeedHold(void);
void GRBL_CycleStart(void);
void GRBL_Reset(void);
void GRBL_SafetyDoor(void);
void GRBL_JogCancel(void);

// UGS Interface Functions
void GRBL_HandleDollarCommand(const char *command);
void GRBL_HandleQuestionMark(void);
void GRBL_HandleExclamation(void);
void GRBL_HandleTilde(void);
void GRBL_HandleCtrlX(void);

// *****************************************************************************
// Default Settings Values
// *****************************************************************************

#define GRBL_DEFAULT_STEP_PULSE_MICROSECONDS    10
#define GRBL_DEFAULT_STEP_IDLE_DELAY            25
#define GRBL_DEFAULT_JUNCTION_DEVIATION         0.02f
#define GRBL_DEFAULT_ARC_TOLERANCE              0.002f
#define GRBL_DEFAULT_HOMING_FEED_RATE           25.0f
#define GRBL_DEFAULT_HOMING_SEEK_RATE           500.0f
#define GRBL_DEFAULT_HOMING_DEBOUNCE_MS         250
#define GRBL_DEFAULT_HOMING_PULLOFF_MM          1.0f
#define GRBL_DEFAULT_SPINDLE_MAX_RPM            1000.0f
#define GRBL_DEFAULT_SPINDLE_MIN_RPM            0.0f

// Default axis parameters (adjust for your machine)
#define GRBL_DEFAULT_X_STEPS_PER_MM             160.0f
#define GRBL_DEFAULT_Y_STEPS_PER_MM             160.0f
#define GRBL_DEFAULT_Z_STEPS_PER_MM             160.0f
#define GRBL_DEFAULT_X_MAX_RATE                 1500.0f
#define GRBL_DEFAULT_Y_MAX_RATE                 1500.0f
#define GRBL_DEFAULT_Z_MAX_RATE                 500.0f
#define GRBL_DEFAULT_X_ACCELERATION             100.0f
#define GRBL_DEFAULT_Y_ACCELERATION             100.0f
#define GRBL_DEFAULT_Z_ACCELERATION             50.0f
#define GRBL_DEFAULT_X_MAX_TRAVEL               200.0f
#define GRBL_DEFAULT_Y_MAX_TRAVEL               200.0f
#define GRBL_DEFAULT_Z_MAX_TRAVEL               200.0f

// *****************************************************************************
// Error Codes (GRBL Compatible)
// *****************************************************************************

#define GRBL_ERROR_OK                           0
#define GRBL_ERROR_EXPECTED_COMMAND_LETTER      1
#define GRBL_ERROR_BAD_NUMBER_FORMAT            2
#define GRBL_ERROR_INVALID_STATEMENT            3
#define GRBL_ERROR_VALUE_NEGATIVE               4
#define GRBL_ERROR_SETTING_DISABLED             5
#define GRBL_ERROR_SETTING_STEP_PULSE_MIN       6
#define GRBL_ERROR_SETTING_READ_FAIL            7
#define GRBL_ERROR_IDLE_ERROR                   8
#define GRBL_ERROR_SYSTEM_GC_LOCK               9
#define GRBL_ERROR_SOFT_LIMIT                   10

// *****************************************************************************
// Usage Examples for UGS Integration
// *****************************************************************************

/*
 * UGS Commands Supported:
 * 
 * Settings Management:
 *   $$              - View all settings
 *   $100=160        - Set X steps/mm to 160
 *   $110=1500       - Set X max rate to 1500 mm/min
 *   $RST=$          - Reset settings to defaults
 * 
 * Real-time Commands:
 *   ?               - Status report
 *   !               - Feed hold
 *   ~               - Cycle start/resume
 *   Ctrl+X          - Reset/abort
 * 
 * System Commands:
 *   $H              - Home cycle
 *   $X              - Unlock (clear alarm)
 *   $C              - Check mode toggle
 *   $SLP            - Sleep mode
 * 
 * Example UGS workflow:
 *   1. Connect to CNC controller
 *   2. Send $$ to view current settings
 *   3. Configure machine: $100=200, $110=2000, etc.
 *   4. Enable soft limits: $20=1
 *   5. Set work envelope: $130=300, $131=200
 *   6. Home machine: $H
 *   7. Load G-code and run
 */

#endif /* GRBL_SETTINGS_H */

/*******************************************************************************
 End of File
 */