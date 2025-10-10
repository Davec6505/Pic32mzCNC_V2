/*******************************************************************************
  Motion Profile and Step Resolution Header File

  Company:
    Microchip Technology Inc.

  File Name:
    motion_profile.h

  Summary:
    This header file provides step resolution, microstepping, and mechanical
    scaling calculations for CNC motion control.

  Description:
    This file contains functions and definitions for handling:
    - Microstepping resolution (full, half, 1/4, 1/8, 1/16, 1/32)
    - Mechanical scaling (belts, screws, ball screws)
    - Unit conversions (mm, inches, degrees)
    - Speed profile calculations with proper resolution
*******************************************************************************/

#ifndef MOTION_PROFILE_H
#define MOTION_PROFILE_H

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>

// *****************************************************************************
// *****************************************************************************
// Section: Constants and Enumerations
// *****************************************************************************
// *****************************************************************************

/* Microstepping Resolutions */
typedef enum {
    MICROSTEP_FULL = 1,     // Full step
    MICROSTEP_HALF = 2,     // Half step
    MICROSTEP_QUARTER = 4,  // 1/4 step
    MICROSTEP_EIGHTH = 8,   // 1/8 step
    MICROSTEP_16TH = 16,    // 1/16 step
    MICROSTEP_32ND = 32     // 1/32 step
} microstep_resolution_t;

/* Drive Type Definitions */
typedef enum {
    DRIVE_DIRECT,           // Direct motor connection
    DRIVE_BELT,             // Belt drive system
    DRIVE_LEADSCREW,        // Lead screw
    DRIVE_BALLSCREW,        // Ball screw
    DRIVE_RACK_PINION       // Rack and pinion
} drive_type_t;

/* Unit System */
typedef enum {
    UNITS_METRIC,           // mm, mm/min
    UNITS_IMPERIAL,         // inches, inches/min
    UNITS_DEGREES           // degrees, degrees/min (rotary axes)
} unit_system_t;

// *****************************************************************************
// *****************************************************************************
// Section: Type Definitions
// *****************************************************************************
// *****************************************************************************

/* Axis Mechanical Configuration */
typedef struct {
    // Motor specifications
    uint16_t motor_steps_per_rev;       // Steps per revolution (200, 400, etc.)
    microstep_resolution_t microsteps;  // Microstepping setting
    
    // Drive system parameters
    drive_type_t drive_type;            // Type of drive system
    float drive_ratio;                  // Drive ratio (belt ratio, gear ratio)
    float pitch;                        // Lead screw pitch (mm/rev) or belt pitch
    float pulley_diameter;              // Belt pulley diameter (mm)
    uint16_t pulley_teeth;              // Belt pulley teeth count
    
    // Mechanical limits
    float max_velocity_units;           // Max velocity in user units/min
    float max_acceleration_units;       // Max acceleration in user units/min²
    float max_jerk_units;               // Max jerk in user units/min³
    
    // Unit system
    unit_system_t units;                // Unit system for this axis
    
    // Calculated values (computed from above)
    uint32_t steps_per_unit;            // Steps per mm/inch/degree
    float max_velocity_steps;           // Max velocity in steps/sec
    float max_acceleration_steps;       // Max acceleration in steps/sec²
    
} axis_mechanical_config_t;

/* Motion Profile Parameters */
typedef struct {
    float target_velocity_units;        // Target velocity in user units/min
    float acceleration_units;           // Acceleration in user units/min²
    float deceleration_units;           // Deceleration in user units/min²
    float jerk_limit_units;             // Jerk limit in user units/min³
    
    // Calculated step values
    float target_velocity_steps;        // Target velocity in steps/sec
    float acceleration_steps;           // Acceleration in steps/sec²
    float deceleration_steps;           // Deceleration in steps/sec²
    
    // Profile timing
    float accel_time;                   // Time to reach target velocity
    float decel_time;                   // Time to decelerate to stop
    uint32_t accel_steps;               // Steps during acceleration
    uint32_t decel_steps;               // Steps during deceleration
    
} motion_profile_t;

// *****************************************************************************
// *****************************************************************************
// Section: Interface Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void MOTION_PROFILE_Initialize(void)

  Summary:
    Initialize the motion profile system

  Description:
    This function initializes the motion profile calculation system
*/
void MOTION_PROFILE_Initialize(void);

/*******************************************************************************
  Function:
    bool MOTION_PROFILE_ConfigureAxis(uint8_t axis_id, axis_mechanical_config_t *config)

  Summary:
    Configure mechanical parameters for an axis

  Description:
    This function configures the mechanical parameters for a specific axis
    and calculates the derived values (steps_per_unit, etc.)

  Parameters:
    axis_id - Axis identifier (0=X, 1=Y, 2=Z, etc.)
    config  - Pointer to mechanical configuration structure

  Returns:
    true if configuration successful, false if error
*/
bool MOTION_PROFILE_ConfigureAxis(uint8_t axis_id, axis_mechanical_config_t *config);

/*******************************************************************************
  Function:
    uint32_t MOTION_PROFILE_UnitsToSteps(uint8_t axis_id, float units)

  Summary:
    Convert user units to steps

  Description:
    Converts distance in user units (mm, inches, degrees) to motor steps

  Parameters:
    axis_id - Axis identifier
    units   - Distance in user units

  Returns:
    Distance in motor steps
*/
uint32_t MOTION_PROFILE_UnitsToSteps(uint8_t axis_id, float units);

/*******************************************************************************
  Function:
    float MOTION_PROFILE_StepsToUnits(uint8_t axis_id, uint32_t steps)

  Summary:
    Convert steps to user units

  Description:
    Converts motor steps to distance in user units (mm, inches, degrees)

  Parameters:
    axis_id - Axis identifier
    steps   - Distance in motor steps

  Returns:
    Distance in user units
*/
float MOTION_PROFILE_StepsToUnits(uint8_t axis_id, uint32_t steps);

/*******************************************************************************
  Function:
    float MOTION_PROFILE_VelocityUnitsToSteps(uint8_t axis_id, float velocity_units_per_min)

  Summary:
    Convert velocity from user units/min to steps/sec

  Parameters:
    axis_id              - Axis identifier
    velocity_units_per_min - Velocity in user units per minute

  Returns:
    Velocity in steps per second
*/
float MOTION_PROFILE_VelocityUnitsToSteps(uint8_t axis_id, float velocity_units_per_min);

/*******************************************************************************
  Function:
    float MOTION_PROFILE_AccelerationUnitsToSteps(uint8_t axis_id, float accel_units_per_min2)

  Summary:
    Convert acceleration from user units/min² to steps/sec²

  Parameters:
    axis_id               - Axis identifier
    accel_units_per_min2  - Acceleration in user units per minute²

  Returns:
    Acceleration in steps per second²
*/
float MOTION_PROFILE_AccelerationUnitsToSteps(uint8_t axis_id, float accel_units_per_min2);

/*******************************************************************************
  Function:
    bool MOTION_PROFILE_CalculateProfile(uint8_t axis_id, float distance_units, 
                                        float feedrate_units_per_min, motion_profile_t *profile)

  Summary:
    Calculate complete motion profile for a move

  Description:
    Calculates acceleration, constant velocity, and deceleration phases
    for a motion with proper step resolution and mechanical scaling

  Parameters:
    axis_id               - Axis identifier
    distance_units        - Move distance in user units
    feedrate_units_per_min - Desired feedrate in user units/min
    profile              - Pointer to motion profile structure to fill

  Returns:
    true if profile calculated successfully, false if error
*/
bool MOTION_PROFILE_CalculateProfile(uint8_t axis_id, float distance_units, 
                                   float feedrate_units_per_min, motion_profile_t *profile);

/*******************************************************************************
  Function:
    uint32_t MOTION_PROFILE_GetStepsPerUnit(uint8_t axis_id)

  Summary:
    Get the calculated steps per unit for an axis

  Parameters:
    axis_id - Axis identifier

  Returns:
    Steps per user unit (steps/mm, steps/inch, steps/degree)
*/
uint32_t MOTION_PROFILE_GetStepsPerUnit(uint8_t axis_id);

/*******************************************************************************
  Function:
    bool MOTION_PROFILE_ValidateVelocity(uint8_t axis_id, float velocity_units_per_min)

  Summary:
    Validate if requested velocity is within axis limits

  Parameters:
    axis_id               - Axis identifier
    velocity_units_per_min - Requested velocity in user units/min

  Returns:
    true if velocity is valid, false if exceeds limits
*/
bool MOTION_PROFILE_ValidateVelocity(uint8_t axis_id, float velocity_units_per_min);

/*******************************************************************************
  Function:
    bool MOTION_PROFILE_ValidateAcceleration(uint8_t axis_id, float accel_units_per_min2)

  Summary:
    Validate if requested acceleration is within axis limits

  Parameters:
    axis_id              - Axis identifier
    accel_units_per_min2 - Requested acceleration in user units/min²

  Returns:
    true if acceleration is valid, false if exceeds limits
*/
bool MOTION_PROFILE_ValidateAcceleration(uint8_t axis_id, float accel_units_per_min2);

// *****************************************************************************
// *****************************************************************************
// Section: Predefined Configurations
// *****************************************************************************
// *****************************************************************************

/* Common stepper motor configurations */
#define NEMA17_200_STEPS    200     // Standard NEMA17 200 steps/rev
#define NEMA17_400_STEPS    400     // High resolution NEMA17
#define NEMA23_200_STEPS    200     // Standard NEMA23 200 steps/rev

/* Common belt configurations (GT2 timing belt) */
#define GT2_BELT_PITCH      2.0f    // GT2 belt pitch in mm
#define GT2_20T_DIAMETER    12.732f // GT2 20-tooth pulley diameter in mm
#define GT2_36T_DIAMETER    22.918f // GT2 36-tooth pulley diameter in mm
#define GT2_60T_DIAMETER    38.197f // GT2 60-tooth pulley diameter in mm

/* Common lead screw pitches */
#define LEADSCREW_2MM       2.0f    // 2mm pitch lead screw
#define LEADSCREW_4MM       4.0f    // 4mm pitch lead screw
#define LEADSCREW_8MM       8.0f    // 8mm pitch lead screw
#define BALLSCREW_5MM       5.0f    // 5mm pitch ball screw
#define BALLSCREW_10MM      10.0f   // 10mm pitch ball screw

/* Unit conversion constants */
#define MM_PER_INCH         25.4f
#define SECONDS_PER_MINUTE  60.0f

// *****************************************************************************
// *****************************************************************************
// Section: Global Data
// *****************************************************************************
// *****************************************************************************

extern axis_mechanical_config_t axis_configs[3]; // X, Y, Z configurations

#endif /* MOTION_PROFILE_H */

/*******************************************************************************
 End of File
 */