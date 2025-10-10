/*******************************************************************************
  Motion Profile and Step Resolution Implementation File

  Company:
    Microchip Technology Inc.

  File Name:
    motion_profile.c

  Summary:
    This file contains the implementation of motion profile calculations
    including step resolution, microstepping, and mechanical scaling.

  Description:
    This file implements functions for handling:
    - Microstepping resolution calculations
    - Mechanical scaling for different drive systems
    - Unit conversions and profile calculations
    - CNC-grade motion planning with proper step resolution
*******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include "motion_profile.h"

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

/* Axis mechanical configurations */
axis_mechanical_config_t axis_configs[3]; // X, Y, Z configurations

/* System initialization flag */
static bool motion_profile_initialized = false;

// *****************************************************************************
// *****************************************************************************
// Section: Local Functions
// *****************************************************************************
// *****************************************************************************

static void calculate_derived_values(uint8_t axis_id);
static float calculate_steps_per_unit(axis_mechanical_config_t *config);

// *****************************************************************************
// *****************************************************************************
// Section: Interface Function Implementations
// *****************************************************************************
// *****************************************************************************

void MOTION_PROFILE_Initialize(void)
{
    /* Initialize all axis configurations to safe defaults */
    for(int i = 0; i < 3; i++) {
        axis_configs[i].motor_steps_per_rev = NEMA17_200_STEPS;
        axis_configs[i].microsteps = MICROSTEP_16TH;
        axis_configs[i].drive_type = DRIVE_LEADSCREW;
        axis_configs[i].drive_ratio = 1.0f;
        axis_configs[i].pitch = LEADSCREW_2MM;
        axis_configs[i].pulley_diameter = 0.0f;
        axis_configs[i].pulley_teeth = 0;
        axis_configs[i].max_velocity_units = 1000.0f;     // 1000 mm/min
        axis_configs[i].max_acceleration_units = 500.0f;  // 500 mm/min²
        axis_configs[i].max_jerk_units = 1000.0f;         // 1000 mm/min³
        axis_configs[i].units = UNITS_METRIC;
        
        /* Calculate derived values */
        calculate_derived_values(i);
    }
    
    motion_profile_initialized = true;
}

bool MOTION_PROFILE_ConfigureAxis(uint8_t axis_id, axis_mechanical_config_t *config)
{
    if(axis_id >= 3 || config == NULL) {
        return false;
    }
    
    /* Copy configuration */
    axis_configs[axis_id] = *config;
    
    /* Calculate derived values */
    calculate_derived_values(axis_id);
    
    return true;
}

uint32_t MOTION_PROFILE_UnitsToSteps(uint8_t axis_id, float units)
{
    if(axis_id >= 3) return 0;
    
    return (uint32_t)(fabsf(units) * axis_configs[axis_id].steps_per_unit);
}

float MOTION_PROFILE_StepsToUnits(uint8_t axis_id, uint32_t steps)
{
    if(axis_id >= 3) return 0.0f;
    
    return (float)steps / axis_configs[axis_id].steps_per_unit;
}

float MOTION_PROFILE_VelocityUnitsToSteps(uint8_t axis_id, float velocity_units_per_min)
{
    if(axis_id >= 3) return 0.0f;
    
    /* Convert units/min to steps/sec */
    return (velocity_units_per_min * axis_configs[axis_id].steps_per_unit) / SECONDS_PER_MINUTE;
}

float MOTION_PROFILE_AccelerationUnitsToSteps(uint8_t axis_id, float accel_units_per_min2)
{
    if(axis_id >= 3) return 0.0f;
    
    /* Convert units/min² to steps/sec² */
    return (accel_units_per_min2 * axis_configs[axis_id].steps_per_unit) / (SECONDS_PER_MINUTE * SECONDS_PER_MINUTE);
}

bool MOTION_PROFILE_CalculateProfile(uint8_t axis_id, float distance_units, 
                                   float feedrate_units_per_min, motion_profile_t *profile)
{
    if(axis_id >= 3 || profile == NULL) {
        return false;
    }
    
    axis_mechanical_config_t *config = &axis_configs[axis_id];
    
    /* Validate inputs */
    if(!MOTION_PROFILE_ValidateVelocity(axis_id, feedrate_units_per_min)) {
        feedrate_units_per_min = config->max_velocity_units; // Clamp to maximum
    }
    
    /* Fill in profile parameters */
    profile->target_velocity_units = feedrate_units_per_min;
    profile->acceleration_units = config->max_acceleration_units;
    profile->deceleration_units = config->max_acceleration_units; // Use same for decel
    profile->jerk_limit_units = config->max_jerk_units;
    
    /* Convert to step values */
    profile->target_velocity_steps = MOTION_PROFILE_VelocityUnitsToSteps(axis_id, feedrate_units_per_min);
    profile->acceleration_steps = MOTION_PROFILE_AccelerationUnitsToSteps(axis_id, profile->acceleration_units);
    profile->deceleration_steps = MOTION_PROFILE_AccelerationUnitsToSteps(axis_id, profile->deceleration_units);
    
    /* Calculate timing */
    profile->accel_time = profile->target_velocity_steps / profile->acceleration_steps;
    profile->decel_time = profile->target_velocity_steps / profile->deceleration_steps;
    
    /* Calculate step counts */
    profile->accel_steps = (uint32_t)(0.5f * profile->acceleration_steps * profile->accel_time * profile->accel_time);
    profile->decel_steps = (uint32_t)(0.5f * profile->deceleration_steps * profile->decel_time * profile->decel_time);
    
    /* Validate that we don't exceed the total distance */
    uint32_t total_steps = MOTION_PROFILE_UnitsToSteps(axis_id, fabsf(distance_units));
    
    if((profile->accel_steps + profile->decel_steps) > total_steps) {
        /* Triangular profile - no constant velocity phase */
        float max_reachable_velocity = sqrtf(profile->acceleration_steps * fabsf(distance_units) * config->steps_per_unit);
        profile->target_velocity_steps = max_reachable_velocity;
        profile->accel_time = max_reachable_velocity / profile->acceleration_steps;
        profile->decel_time = max_reachable_velocity / profile->deceleration_steps;
        profile->accel_steps = total_steps / 2;
        profile->decel_steps = total_steps / 2;
    }
    
    return true;
}

uint32_t MOTION_PROFILE_GetStepsPerUnit(uint8_t axis_id)
{
    if(axis_id >= 3) return 0;
    
    return axis_configs[axis_id].steps_per_unit;
}

bool MOTION_PROFILE_ValidateVelocity(uint8_t axis_id, float velocity_units_per_min)
{
    if(axis_id >= 3) return false;
    
    return (fabsf(velocity_units_per_min) <= axis_configs[axis_id].max_velocity_units);
}

bool MOTION_PROFILE_ValidateAcceleration(uint8_t axis_id, float accel_units_per_min2)
{
    if(axis_id >= 3) return false;
    
    return (fabsf(accel_units_per_min2) <= axis_configs[axis_id].max_acceleration_units);
}

// *****************************************************************************
// *****************************************************************************
// Section: Local Function Implementations
// *****************************************************************************
// *****************************************************************************

static void calculate_derived_values(uint8_t axis_id)
{
    if(axis_id >= 3) return;
    
    axis_mechanical_config_t *config = &axis_configs[axis_id];
    
    /* Calculate steps per unit */
    config->steps_per_unit = calculate_steps_per_unit(config);
    
    /* Calculate maximum velocity and acceleration in steps */
    config->max_velocity_steps = MOTION_PROFILE_VelocityUnitsToSteps(axis_id, config->max_velocity_units);
    config->max_acceleration_steps = MOTION_PROFILE_AccelerationUnitsToSteps(axis_id, config->max_acceleration_units);
}

static float calculate_steps_per_unit(axis_mechanical_config_t *config)
{
    float steps_per_rev = (float)config->motor_steps_per_rev * (float)config->microsteps;
    float units_per_rev = 0.0f;
    
    switch(config->drive_type) {
        case DRIVE_DIRECT:
            /* Direct connection - calculate based on unit system */
            if(config->units == UNITS_DEGREES) {
                units_per_rev = 360.0f; // Degrees per revolution
            } else {
                units_per_rev = 1.0f; // Assume 1 unit per revolution for direct drive
            }
            break;
            
        case DRIVE_BELT:
            /* Belt drive - calculate from pulley diameter or teeth */
            if(config->pulley_teeth > 0) {
                units_per_rev = (float)config->pulley_teeth * config->pitch; // GT2 belt calculation
            } else {
                units_per_rev = M_PI * config->pulley_diameter; // Circumference
            }
            units_per_rev /= config->drive_ratio; // Apply drive ratio
            break;
            
        case DRIVE_LEADSCREW:
        case DRIVE_BALLSCREW:
            /* Lead screw or ball screw */
            units_per_rev = config->pitch / config->drive_ratio;
            break;
            
        case DRIVE_RACK_PINION:
            /* Rack and pinion - use pulley diameter as pinion diameter */
            units_per_rev = (M_PI * config->pulley_diameter) / config->drive_ratio;
            break;
            
        default:
            units_per_rev = 1.0f; // Default fallback
            break;
    }
    
    /* Handle unit conversions */
    if(config->units == UNITS_IMPERIAL && 
       (config->drive_type != DRIVE_DIRECT && config->drive_type != DRIVE_RACK_PINION)) {
        /* Convert mm to inches if needed */
        units_per_rev /= MM_PER_INCH;
    }
    
    return steps_per_rev / units_per_rev;
}

/*******************************************************************************
 End of File
 */