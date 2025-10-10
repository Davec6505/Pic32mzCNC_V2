/*******************************************************************************
  CNC Machine Configuration Example

  File Name:
    cnc_config_example.h

  Summary:
    Example configurations for common CNC machine setups

  Description:
    This file shows how to configure the motion profile system for
    different types of CNC machines with various drive systems.
*******************************************************************************/

#ifndef CNC_CONFIG_EXAMPLE_H
#define CNC_CONFIG_EXAMPLE_H

#include "motion_profile.h"

// *****************************************************************************
// Example 1: Belt-driven CNC Router
// - NEMA23 motors, 200 steps/rev
// - 1/16 microstepping 
// - GT2 timing belt with 20-tooth pulleys
// - Metric units (mm)
// *****************************************************************************

static inline void configure_belt_driven_router(void)
{
    axis_mechanical_config_t x_config = {
        .motor_steps_per_rev = NEMA23_200_STEPS,
        .microsteps = MICROSTEP_16TH,
        .drive_type = DRIVE_BELT,
        .drive_ratio = 1.0f,                    // Direct drive (1:1)
        .pitch = GT2_BELT_PITCH,                // 2mm GT2 belt pitch
        .pulley_diameter = GT2_20T_DIAMETER,    // 20-tooth GT2 pulley
        .pulley_teeth = 20,                     // 20 teeth
        .max_velocity_units = 6000.0f,          // 6000 mm/min (100 mm/s)
        .max_acceleration_units = 1000.0f,      // 1000 mm/min²
        .max_jerk_units = 5000.0f,              // 5000 mm/min³
        .units = UNITS_METRIC
    };
    
    /* X and Y axes typically identical for gantry router */
    MOTION_PROFILE_ConfigureAxis(0, &x_config); // X-axis
    MOTION_PROFILE_ConfigureAxis(1, &x_config); // Y-axis
    
    /* Z-axis usually slower and more precise */
    axis_mechanical_config_t z_config = x_config;
    z_config.max_velocity_units = 1500.0f;      // Slower Z-axis
    z_config.max_acceleration_units = 500.0f;   // More conservative acceleration
    MOTION_PROFILE_ConfigureAxis(2, &z_config); // Z-axis
}

// *****************************************************************************
// Example 2: Lead Screw CNC Mill
// - NEMA17 motors, 200 steps/rev
// - 1/32 microstepping for precision
// - 2mm pitch lead screws
// - Metric units (mm)
// *****************************************************************************

static inline void configure_leadscrew_mill(void)
{
    axis_mechanical_config_t mill_config = {
        .motor_steps_per_rev = NEMA17_200_STEPS,
        .microsteps = MICROSTEP_32ND,           // High precision
        .drive_type = DRIVE_LEADSCREW,
        .drive_ratio = 1.0f,                    // Direct connection
        .pitch = LEADSCREW_2MM,                 // 2mm pitch lead screw
        .pulley_diameter = 0.0f,                // Not used for lead screw
        .pulley_teeth = 0,                      // Not used for lead screw
        .max_velocity_units = 1200.0f,          // 1200 mm/min (20 mm/s)
        .max_acceleration_units = 300.0f,       // Conservative for precision
        .max_jerk_units = 1000.0f,              // Smooth motion
        .units = UNITS_METRIC
    };
    
    MOTION_PROFILE_ConfigureAxis(0, &mill_config); // X-axis
    MOTION_PROFILE_ConfigureAxis(1, &mill_config); // Y-axis
    MOTION_PROFILE_ConfigureAxis(2, &mill_config); // Z-axis
}

// *****************************************************************************
// Example 3: Ball Screw CNC with Reduction
// - NEMA23 motors, 200 steps/rev
// - 1/16 microstepping
// - 5mm pitch ball screws
// - 2:1 gear reduction
// - Imperial units (inches)
// *****************************************************************************

static inline void configure_ballscrew_imperial(void)
{
    axis_mechanical_config_t ballscrew_config = {
        .motor_steps_per_rev = NEMA23_200_STEPS,
        .microsteps = MICROSTEP_16TH,
        .drive_type = DRIVE_BALLSCREW,
        .drive_ratio = 2.0f,                    // 2:1 gear reduction
        .pitch = BALLSCREW_5MM,                 // 5mm pitch ball screw
        .pulley_diameter = 0.0f,                // Not used for ball screw
        .pulley_teeth = 0,                      // Not used for ball screw
        .max_velocity_units = 200.0f,           // 200 inches/min
        .max_acceleration_units = 50.0f,        // 50 inches/min²
        .max_jerk_units = 200.0f,               // 200 inches/min³
        .units = UNITS_IMPERIAL
    };
    
    MOTION_PROFILE_ConfigureAxis(0, &ballscrew_config); // X-axis
    MOTION_PROFILE_ConfigureAxis(1, &ballscrew_config); // Y-axis
    MOTION_PROFILE_ConfigureAxis(2, &ballscrew_config); // Z-axis
}

// *****************************************************************************
// Example 4: Rotary 4th Axis
// - NEMA17 motor, 200 steps/rev
// - 1/8 microstepping
// - 90:1 worm gear reduction
// - Degrees output
// *****************************************************************************

static inline void configure_rotary_4th_axis(void)
{
    axis_mechanical_config_t rotary_config = {
        .motor_steps_per_rev = NEMA17_200_STEPS,
        .microsteps = MICROSTEP_EIGHTH,
        .drive_type = DRIVE_DIRECT,             // Direct rotary connection
        .drive_ratio = 90.0f,                   // 90:1 worm gear
        .pitch = 0.0f,                          // Not used for rotary
        .pulley_diameter = 0.0f,                // Not used for rotary
        .pulley_teeth = 0,                      // Not used for rotary
        .max_velocity_units = 360.0f,           // 360 degrees/min (1 RPM)
        .max_acceleration_units = 180.0f,       // 180 degrees/min²
        .max_jerk_units = 720.0f,               // 720 degrees/min³
        .units = UNITS_DEGREES
    };
    
    /* Configure as A-axis (assuming you extend to 4+ axes) */
    // MOTION_PROFILE_ConfigureAxis(3, &rotary_config); // A-axis
}

// *****************************************************************************
// Calculation Examples and Verification
// *****************************************************************************

/*
 * Belt Drive Calculation Example:
 * - NEMA23: 200 steps/rev
 * - 1/16 microstepping: 200 × 16 = 3200 microsteps/rev
 * - GT2 20-tooth pulley: 20 × 2mm = 40mm per revolution
 * - Steps per mm: 3200 / 40 = 80 steps/mm
 * 
 * For 1mm move: 80 steps
 * For 100mm/min (1.67mm/s): 80 × 1.67 = 133.3 steps/sec
 */

/*
 * Lead Screw Calculation Example:
 * - NEMA17: 200 steps/rev  
 * - 1/32 microstepping: 200 × 32 = 6400 microsteps/rev
 * - 2mm pitch lead screw: 2mm per revolution
 * - Steps per mm: 6400 / 2 = 3200 steps/mm
 * 
 * For 0.1mm move: 320 steps (very precise!)
 * For 20mm/min (0.33mm/s): 3200 × 0.33 = 1067 steps/sec
 */

/*
 * Ball Screw with Reduction Calculation Example:
 * - NEMA23: 200 steps/rev
 * - 1/16 microstepping: 200 × 16 = 3200 microsteps/rev
 * - 5mm pitch ball screw: 5mm per screw revolution
 * - 2:1 gear reduction: Motor turns 2× for 1 screw revolution
 * - Effective pitch: 5mm / 2 = 2.5mm per motor revolution
 * - Steps per mm: 3200 / 2.5 = 1280 steps/mm
 * 
 * For 1 inch (25.4mm): 1280 × 25.4 = 32,512 steps
 * For 100 inches/min: Very high step rates - need fast timers!
 */

#endif /* CNC_CONFIG_EXAMPLE_H */

/*******************************************************************************
 End of File
 */