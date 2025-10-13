/*******************************************************************************
  Motion Planner Header File

  Company:
    Microchip Technology Inc.

  File Name:
    motion_planner.h

  Summary:
    This header file provides the prototypes and definitions for motion planning
    functionality with look-ahead optimization.

  Description:
    This file declares the functions used for motion planning including:
    - 16-buffer look-ahead processing
    - Velocity optimization and junction analysis
    - Motion block processing and execution
    - Trajectory calculation and smoothing
*******************************************************************************/

#ifndef MOTION_PLANNER_H
#define MOTION_PLANNER_H

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************
#include <stdint.h>
#include <stdbool.h>
#include "app.h"           // For motion_block_t definition
#include "motion_buffer.h" // For buffer management

// *****************************************************************************
// *****************************************************************************
// Section: Constants
// *****************************************************************************
// *****************************************************************************

// Motion planning constants
#define MAX_JUNCTION_SPEED 500.0f    // Maximum speed through junctions
#define MIN_JUNCTION_SPEED 10.0f     // Minimum speed through junctions
#define ACCELERATION_DEFAULT 1000.0f // Default acceleration (mm/s²)
#define JUNCTION_DEVIATION 0.1f      // Maximum junction deviation (mm)

// *****************************************************************************
// *****************************************************************************
// Section: Data Types
// *****************************************************************************
// *****************************************************************************

// Motion execution state (using unique names to avoid conflicts)
typedef enum
{
  PLANNER_STATE_IDLE,
  PLANNER_STATE_PLANNING,
  PLANNER_STATE_EXECUTING,
  PLANNER_STATE_ERROR
} motion_execution_state_t;

// Motion planner statistics
typedef struct
{
  uint32_t blocks_processed;
  uint32_t blocks_optimized;
  float average_velocity;
  float peak_velocity;
  uint32_t execution_time_ms;
} motion_planner_stats_t;

// *****************************************************************************
// *****************************************************************************
// Section: Interface Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void MotionPlanner_Initialize(void)

  Summary:
    Initializes the motion planner system.

  Description:
    This function initializes the motion planner with default parameters
    and resets all state variables.

  Parameters:
    None

  Returns:
    None
*******************************************************************************/
void MotionPlanner_Initialize(void);

/*******************************************************************************
  Function:
    void MotionPlanner_ProcessBuffer(void)

  Summary:
    Processes the motion buffer for execution.

  Description:
    This function takes the next motion block from the buffer, calculates
    motion parameters, optimizes velocity profile, and starts execution.
    This is the main processing function called periodically.

  Parameters:
    None

  Returns:
    None
*******************************************************************************/
void MotionPlanner_ProcessBuffer(void);

/*******************************************************************************
  Function:
    void MotionPlanner_CalculateDistance(motion_block_t *block)

  Summary:
    Calculates the total distance for a motion block.

  Description:
    This function calculates the Euclidean distance for the motion block
    and estimates the execution duration based on feedrate.

  Parameters:
    block - Pointer to motion block to calculate

  Returns:
    None
*******************************************************************************/
void MotionPlanner_CalculateDistance(motion_block_t *block);

/*******************************************************************************
  Function:
    void MotionPlanner_OptimizeVelocityProfile(motion_block_t *block)

  Summary:
    Optimizes the velocity profile for a motion block using look-ahead.

  Description:
    This function analyzes the current block and upcoming blocks to optimize
    entry and exit velocities for smooth motion. Implements junction analysis
    and acceleration planning.

  Parameters:
    block - Pointer to motion block to optimize

  Returns:
    None
*******************************************************************************/
void MotionPlanner_OptimizeVelocityProfile(motion_block_t *block);

/*******************************************************************************
  Function:
    void MotionPlanner_ExecuteBlock(motion_block_t *block)

  Summary:
    Starts execution of a motion block.

  Description:
    This function initiates the execution of a motion block by updating
    the target position and starting the step generation hardware.

  Parameters:
    block - Pointer to motion block to execute

  Returns:
    None
*******************************************************************************/
void MotionPlanner_ExecuteBlock(motion_block_t *block);

/*******************************************************************************
  Function:
    float MotionPlanner_CalculateJunctionVelocity(motion_block_t *block1, motion_block_t *block2)

  Summary:
    Calculates the safe junction velocity between two blocks.

  Description:
    This function analyzes the angle and parameters of two consecutive motion
    blocks to determine the maximum safe velocity through the junction.

  Parameters:
    block1 - Pointer to first motion block
    block2 - Pointer to second motion block

  Returns:
    Maximum safe junction velocity
*******************************************************************************/
float MotionPlanner_CalculateJunctionVelocity(motion_block_t *block1, motion_block_t *block2);

/*******************************************************************************
  Function:
    bool MotionPlanner_IsMotionComplete(void)

  Summary:
    Checks if all motion is complete.

  Description:
    This function checks if the motion buffer is empty and all axes are idle.

  Parameters:
    None

  Returns:
    true - All motion is complete
    false - Motion is still in progress
*******************************************************************************/
bool MotionPlanner_IsMotionComplete(void);

/*******************************************************************************
  Function:
    motion_execution_state_t MotionPlanner_GetState(void)

  Summary:
    Gets the current motion execution state.

  Description:
    This function returns the current state of the motion planner.

  Parameters:
    None

  Returns:
    Current motion execution state
*******************************************************************************/
motion_execution_state_t MotionPlanner_GetState(void);

/*******************************************************************************
  Function:
    motion_planner_stats_t MotionPlanner_GetStatistics(void)

  Summary:
    Gets motion planner performance statistics.

  Description:
    This function returns statistical information about motion planner
    performance including throughput and velocity data.

  Parameters:
    None

  Returns:
    Motion planner statistics structure
*******************************************************************************/
motion_planner_stats_t MotionPlanner_GetStatistics(void);

/*******************************************************************************
  Function:
    uint32_t MotionPlanner_CalculateOCRPeriod(float velocity_mm_min)

  Summary:
    Calculates OCR period for given velocity.

  Description:
    This function calculates the OCR compare period required to achieve
    the specified velocity in mm/min. Used by hardware layer to configure
    step pulse generation timing.

  Parameters:
    velocity_mm_min - Target velocity in millimeters per minute

  Returns:
    OCR period value (0xFFFFFFFF if velocity is too low)
*******************************************************************************/
uint32_t MotionPlanner_CalculateOCRPeriod(float velocity_mm_min);

// *****************************************************************************
// *****************************************************************************
// Section: Motion System Getter/Setter Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    float MotionPlanner_GetCurrentVelocity(uint8_t axis)

  Summary:
    Gets the current velocity for specified axis.

  Description:
    Returns the current velocity in mm/min for the specified axis.
    Provides clean interface to access axis velocity data.

  Parameters:
    axis - Axis index (0=X, 1=Y, 2=Z)

  Returns:
    Current velocity in mm/min, or 0.0 if axis invalid
*******************************************************************************/
float MotionPlanner_GetCurrentVelocity(uint8_t axis);

/*******************************************************************************
  Function:
    void MotionPlanner_SetCurrentVelocity(uint8_t axis, float velocity)

  Summary:
    Sets the current velocity for specified axis.

  Description:
    Updates the current velocity in mm/min for the specified axis.
    Used by motion execution system to track real-time velocities.

  Parameters:
    axis - Axis index (0=X, 1=Y, 2=Z)
    velocity - Velocity in mm/min

  Returns:
    None
*******************************************************************************/
void MotionPlanner_SetCurrentVelocity(uint8_t axis, float velocity);

/*******************************************************************************
  Function:
    int32_t MotionPlanner_GetAxisPosition(uint8_t axis)

  Summary:
    Gets the current position for specified axis.

  Description:
    Returns the current position in steps for the specified axis.
    Provides clean interface to access real-time position data.

  Parameters:
    axis - Axis index (0=X, 1=Y, 2=Z)

  Returns:
    Current position in steps, or 0 if axis invalid
*******************************************************************************/
int32_t MotionPlanner_GetAxisPosition(uint8_t axis);

/*******************************************************************************
  Function:
    void MotionPlanner_SetAxisPosition(uint8_t axis, int32_t position)

  Summary:
    Sets the current position for specified axis.

  Description:
    Updates the current position in steps for the specified axis.
    Used by hardware layer to provide position feedback.

  Parameters:
    axis - Axis index (0=X, 1=Y, 2=Z)
    position - Position in steps

  Returns:
    None
*******************************************************************************/
void MotionPlanner_SetAxisPosition(uint8_t axis, int32_t position);

/*******************************************************************************
  Function:
    bool MotionPlanner_IsAxisActive(uint8_t axis)

  Summary:
    Checks if specified axis is currently active.

  Description:
    Returns true if the specified axis is currently executing motion.
    Used for motion state monitoring and coordination.

  Parameters:
    axis - Axis index (0=X, 1=Y, 2=Z)

  Returns:
    true if axis is active, false otherwise
*******************************************************************************/
bool MotionPlanner_IsAxisActive(uint8_t axis);

/*******************************************************************************
  Function:
    void MotionPlanner_SetAxisActive(uint8_t axis, bool active)

  Summary:
    Sets the active state for specified axis.

  Description:
    Updates the active state for the specified axis.
    Used by hardware layer to control motion execution.

  Parameters:
    axis - Axis index (0=X, 1=Y, 2=Z)
    active - true to activate axis, false to deactivate

  Returns:
    None
*******************************************************************************/
void MotionPlanner_SetAxisActive(uint8_t axis, bool active);

/*******************************************************************************
  Function:
    uint32_t MotionPlanner_GetAxisStepCount(uint8_t axis)

  Summary:
    Gets the step count for specified axis.

  Description:
    Returns the current step count for the specified axis.
    Used for motion progress monitoring and debugging.

  Parameters:
    axis - Axis index (0=X, 1=Y, 2=Z)

  Returns:
    Current step count, or 0 if axis invalid
*******************************************************************************/
uint32_t MotionPlanner_GetAxisStepCount(uint8_t axis);

/*******************************************************************************
  Function:
    void MotionPlanner_ResetAxisStepCount(uint8_t axis)

  Summary:
    Resets the step count for specified axis.

  Description:
    Clears the step count for the specified axis to zero.
    Used when starting new motion blocks.

  Parameters:
    axis - Axis index (0=X, 1=Y, 2=Z)

  Returns:
    None
*******************************************************************************/
void MotionPlanner_ResetAxisStepCount(uint8_t axis);

/*******************************************************************************
  Function:
    void MotionPlanner_EmergencyStop(void)

  Summary:
    Performs an emergency stop of all motion.

  Description:
    This function immediately stops all motion, clears the motion buffer,
    and resets the motion planner to a safe state.

  Parameters:
    None

  Returns:
    None
*******************************************************************************/
void MotionPlanner_EmergencyStop(void);

/*******************************************************************************
  Function:
    void MotionPlanner_SetAcceleration(float acceleration)

  Summary:
    Sets the default acceleration for motion planning.

  Description:
    This function sets the acceleration parameter used for velocity
    profile calculations.

  Parameters:
    acceleration - Acceleration value in mm/s²

  Returns:
    None
*******************************************************************************/
void MotionPlanner_SetAcceleration(float acceleration);

/*******************************************************************************
  Function:
    float MotionPlanner_GetAcceleration(void)

  Summary:
    Gets the current acceleration setting.

  Parameters:
    None

  Returns:
    Current acceleration value in mm/s²
*******************************************************************************/
float MotionPlanner_GetAcceleration(void);

/*******************************************************************************
  Function:
    void MotionPlanner_UpdateTrajectory(void)

  Summary:
    Performs real-time trajectory calculations for stepper motor control.

  Description:
    This function is called at 1kHz by the Core Timer interrupt to calculate
    current velocities, accelerations, and update OCR periods for smooth
    stepper motor motion. Handles acceleration/deceleration profiles and
    jerk limiting in real-time.

  Parameters:
    None

  Returns:
    None

  Remarks:
    - Called from Core Timer interrupt context (1kHz)
    - Updates OCR compare periods for step timing
    - Calculates real-time velocity profiles
*******************************************************************************/
void MotionPlanner_UpdateTrajectory(void);

/*******************************************************************************
  Function:
    void MotionPlanner_UpdateAxisPosition(uint8_t axis, int32_t position)

  Summary:
    Updates position feedback from OCR interrupts.

  Description:
    This function is called from OCR interrupt callbacks to provide position
    feedback to the motion planner. Enables closed-loop position monitoring
    and error detection.

  Parameters:
    axis - Axis index (0=X, 1=Y, 2=Z)
    position - Current position in steps

  Returns:
    None

  Remarks:
    - Called from OCR interrupt context
    - Provides position feedback for monitoring
    - Can be used for position error detection
*******************************************************************************/
void MotionPlanner_UpdateAxisPosition(uint8_t axis, int32_t position);
float MotionPlanner_GetCurrentVelocity(uint8_t axis);

#endif /* MOTION_PLANNER_H */

/*******************************************************************************
 End of File
 */