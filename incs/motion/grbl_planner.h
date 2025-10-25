/*******************************************************************************
  GRBL Planner API (Ported from gnea/grbl v1.1f)

  Company:
    Microchip Technology Inc. (PIC32MZ hardware adaptation)

  Original Authors:
    Sungeun K. Jeon for Gnea Research LLC
    Simen Svale Skogsrud

  File Name:
    grbl_planner.h

  Summary:
    Public API for GRBL motion planner with look-ahead optimization.

  Description:
    This header defines the public interface for GRBL's proven trapezoidal
    velocity planner with junction deviation cornering algorithm.

    Ported from GRBL v1.1f (gnea/grbl) with minimal modifications:
      - Adapted for PIC32MZ/XC32 compiler
      - Changed N_AXIS from 3 to 4 (added A-axis for rotary)
      - Integrated with our motion_math.h settings system
      - MISRA C:2012 compliant

    CRITICAL DESIGN PRINCIPLE:
      Planner algorithms preserved EXACTLY from GRBL for proven reliability.
      Only hardware/settings interfaces modified.

  Architecture:
    - Ring buffer of plan_block_t (16 blocks)
    - Forward/reverse passes for velocity optimization
    - Junction deviation algorithm (no jerk limit needed)
    - Single source of truth for position tracking

  MISRA C:2012 Compliance:
    - Rule 8.7: Static functions where external linkage not required
    - Rule 8.9: Single responsibility (motion planning only)
    - Rule 17.7: All return values checked by caller
    - Rule 21.3: No dynamic allocation (static ring buffer)

  Integration Points:
    - gcode_parser.c: Calls GRBLPlanner_BufferLine()
    - motion_math.c: Provides settings ($100-$133)
    - Phase 2: grbl_stepper.c will consume blocks via GetCurrentBlock()

*******************************************************************************/

#ifndef GRBL_PLANNER_H
#define GRBL_PLANNER_H

// *****************************************************************************
// Section: Included Files
// *****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include "motion_types.h" // Single source of truth for axis/position types

// *****************************************************************************
// Section: GRBL Configuration Constants
// *****************************************************************************

/*! \brief Planner ring buffer size (must be power of 2)
 *  GRBL default: 16 blocks (provides ~40-50ms of motion at typical feedrates)
 *  Flow control (Oct 25, 2025): Buffer full check prevents overflow
 *  - main.c only sends "ok" when buffer has space
 *  - UGS waits if "ok" not received, preventing command loss
 *  Larger buffer = smoother motion but more RAM usage
 *  Smaller buffer = less RAM but may starve during complex curves
 */
#define BLOCK_BUFFER_SIZE 16

/*! \brief Minimum junction speed (mm/min)
 *  GRBL default: 0.0 for exact path mode (G61.1)
 *  Non-zero values allow machine to "round" sharp corners slightly
 */
#define MINIMUM_JUNCTION_SPEED 0.0f

/*! \brief Minimum feed rate (mm/min)
 *  GRBL default: 1.0 mm/min
 *  Prevents floating-point round-off errors, ensures motion always completes
 */
#define MINIMUM_FEED_RATE 1.0f

/*! \brief Large value for "infinite" junction speed
 *  Used when blocks are collinear (180° or 0° junction angle)
 */
#define SOME_LARGE_VALUE 1.0E+38f

// *****************************************************************************
// Section: Planner Condition Flags (GRBL-Compatible)
// *****************************************************************************

/*! \brief Block condition flags (from GRBL planner.h)
 *  These flags denote running conditions and control overrides.
 *  Set in grbl_plan_line_data_t.condition before calling BufferLine().
 */
#define PL_COND_FLAG_RAPID_MOTION (1U << 0)     // G0 rapid positioning
#define PL_COND_FLAG_SYSTEM_MOTION (1U << 1)    // Homing/parking (bypass planner state)
#define PL_COND_FLAG_NO_FEED_OVERRIDE (1U << 2) // Ignore feedrate override (rapids, homing)
#define PL_COND_FLAG_INVERSE_TIME (1U << 3)     // G93 inverse time mode
#define PL_COND_FLAG_SPINDLE_CW (1U << 4)       // M3 spindle clockwise
#define PL_COND_FLAG_SPINDLE_CCW (1U << 5)      // M4 spindle counter-clockwise
#define PL_COND_FLAG_COOLANT_FLOOD (1U << 6)    // M8 flood coolant
#define PL_COND_FLAG_COOLANT_MIST (1U << 7)     // M7 mist coolant

// *****************************************************************************
// Section: Return Codes
// *****************************************************************************

typedef enum {
    PLAN_OK = 1,          // Block successfully added to buffer
    PLAN_BUFFER_FULL = 0, // Buffer full - temporary, RETRY after waiting
    PLAN_EMPTY_BLOCK = -1 // Zero-length block - permanent, DO NOT RETRY
} plan_status_t;

// *****************************************************************************
// Section: Type Definitions (GRBL-Compatible)
// *****************************************************************************

/*! \brief GRBL planner block structure
 *
 *  This is the core data structure for GRBL's motion planner.
 *  Each block represents one linear move with pre-calculated:
 *    - Step counts (Bresenham algorithm inputs)
 *    - Velocity profile parameters (entry/exit speeds, acceleration)
 *    - Junction velocity limits (from look-ahead optimization)
 *
 *  GRBL Original: plan_block_t in planner.h lines 47-83
 *  Modifications: N_AXIS changed from 3 to 4
 *
 *  MISRA Rule 8.9: Defined in header (external linkage required)
 */
typedef struct
{
    // ═════════════════════════════════════════════════════════════════════
    // Bresenham Algorithm Fields (used by stepper execution)
    // ═════════════════════════════════════════════════════════════════════

    /*! Step count along each axis (absolute difference from start to end) */
    uint32_t steps[NUM_AXES];

    /*! Maximum step count in any single axis (determines Bresenham iterations) */
    uint32_t step_event_count;

    /*! Direction bit mask (bit N set = axis N moves negative) */
    uint8_t direction_bits;

    // ═════════════════════════════════════════════════════════════════════
    // Motion Planner Fields (computed by look-ahead algorithm)
    // ═════════════════════════════════════════════════════════════════════

    /*! Planned entry speed at block junction (mm/min)² - squared for efficiency */
    float entry_speed_sqr;

    /*! Maximum allowable entry speed based on junction geometry (mm/min)² */
    float max_entry_speed_sqr;

    /*! Axis-limited acceleration for this block direction (mm/min²) */
    float acceleration;

    /*! Remaining distance to execute (mm) - updated by stepper during execution */
    float millimeters;

    // ═════════════════════════════════════════════════════════════════════
    // Rate Limiting Fields (pre-computed from settings)
    // ═════════════════════════════════════════════════════════════════════

    /*! Junction speed limit from deviation algorithm (mm/min)² */
    float max_junction_speed_sqr;

    /*! Maximum rate for this direction (axis-limited) (mm/min) */
    float rapid_rate;

    /*! Programmed feed rate from G-code (mm/min) */
    float programmed_rate;

    // ═════════════════════════════════════════════════════════════════════
    // Block Condition Flags
    // ═════════════════════════════════════════════════════════════════════

    /*! Condition flags (PL_COND_FLAG_* bits) */
    uint8_t condition;

} grbl_plan_block_t;

/*! \brief Planner input data structure
 *
 *  Used to pass motion parameters to GRBLPlanner_BufferLine().
 *  Contains feed rate, spindle state, and condition flags.
 *
 *  GRBL Original: plan_line_data_t in planner.h lines 88-96
 *  Modifications: Removed line_number (we don't use line number reporting yet)
 *
 *  MISRA Rule 8.9: Defined in header (external linkage required)
 */
typedef struct
{
    /*! Commanded feed rate (mm/min) - ignored for rapid motions (G0) */
    float feed_rate;

    /*! Spindle speed (RPM) - for future spindle control */
    float spindle_speed;

    /*! Condition flags (PL_COND_FLAG_* bits) */
    uint8_t condition;

} grbl_plan_line_data_t;

// *****************************************************************************
// Section: Public API - Initialization
// *****************************************************************************

/*! \brief Initialize GRBL planner subsystem
 *
 *  Called once at system startup. Performs:
 *    1. Clears ring buffer (all blocks zeroed)
 *    2. Resets buffer pointers (head/tail/planned)
 *    3. Zeros position tracking
 *    4. Clears previous unit vector
 *
 *  GRBL Original: plan_reset() in planner.c line 199
 *
 *  \return None
 *
 *  Thread Safety: Main loop only (not ISR-safe)
 *  MISRA Rule 8.7: External linkage (called from main.c)
 */
void GRBLPlanner_Initialize(void);

/*! \brief Reset planner to safe state (preserve position)
 *
 *  Called during soft reset (Ctrl-X) or feed hold recovery.
 *  Clears all blocks from buffer but preserves current position tracking.
 *
 *  GRBL Original: plan_reset_buffer() in planner.c line 205
 *
 *  \return None
 *
 *  Thread Safety: Main loop only
 */
void GRBLPlanner_Reset(void);

// *****************************************************************************
// Section: Public API - Motion Planning
// *****************************************************************************

/*! \brief Add linear motion to planner buffer
 *
 *  This is the ONLY entry point for motion commands. Performs:
 *    1. Converts mm to steps using motion_math settings
 *    2. Calculates unit vector and block distance
 *    3. Computes axis-limited acceleration/velocity
 *    4. Calculates junction velocity (look-ahead)
 *    5. Triggers full buffer recalculation (forward/reverse passes)
 *
 *  CRITICAL: Target must be in ABSOLUTE MACHINE COORDINATES (mm).
 *  G-code parser handles work coordinate offsets (G54-G59, G92).
 *
 *  GRBL Original: plan_buffer_line() in planner.c line 311
 *  Modifications: Uses motion_math for settings instead of global 'settings'
 *
 *  \param target[N_AXIS] Target position in mm (absolute machine coords)
 *  \param pl_data Feed rate, spindle speed, condition flags
 *
 *  \return PLAN_OK if block added successfully
 *  \return PLAN_BUFFER_FULL if buffer is full (temporary - retry after waiting)
 *  \return PLAN_EMPTY_BLOCK if zero-length move (permanent - do not retry)
 *
 *  Thread Safety: Main loop only (modifies ring buffer state)
 *  Performance: ~300-500µs typical, up to 2ms for full recalculation
 *
 *  MISRA Rule 17.7: Caller must check return value
 */
plan_status_t GRBLPlanner_BufferLine(float *target, grbl_plan_line_data_t *pl_data);

/*! \brief Get next block for execution
 *
 *  Returns pointer to block at buffer tail (oldest block in buffer).
 *  Does NOT remove block - call DiscardCurrentBlock() after execution complete.
 *
 *  GRBL Original: plan_get_current_block() in planner.c line 233
 *
 *  \return Pointer to block at tail, or NULL if buffer empty
 *
 *  Thread Safety: ISR-safe (read-only access to tail pointer)
 *
 *  Usage Pattern:
 *    grbl_plan_block_t *block = GRBLPlanner_GetCurrentBlock();
 *    if (block != NULL) {
 *        // Execute block...
 *        GRBLPlanner_DiscardCurrentBlock();
 *    }
 */
grbl_plan_block_t *GRBLPlanner_GetCurrentBlock(void);

/*! \brief Discard completed block from buffer
 *
 *  Advances buffer tail pointer, making space for new blocks.
 *  Call this ONLY after block execution is complete.
 *
 *  GRBL Original: plan_discard_current_block() in planner.c line 215
 *
 *  \return None
 *
 *  Thread Safety: Main loop only (modifies tail pointer)
 */
void GRBLPlanner_DiscardCurrentBlock(void);

// *****************************************************************************
// Section: Public API - Buffer Status (GRBL Flow Control)
// *****************************************************************************

/*! \brief Check if planner buffer is full
 *
 *  Used for GRBL flow control protocol:
 *    - If buffer full: DON'T send "ok" to UGS
 *    - If buffer has space: Send "ok" to request next command
 *
 *  GRBL Original: plan_check_full_buffer() in planner.c line 250
 *
 *  \return true if buffer cannot accept more blocks
 *  \return false if at least one slot available
 *
 *  Thread Safety: ISR-safe (read-only access to pointers)
 */
bool GRBLPlanner_IsBufferFull(void);

/*! \brief Get number of available buffer slots
 *
 *  Returns number of free slots (0 to BLOCK_BUFFER_SIZE-1).
 *  Useful for advanced flow control or status reporting.
 *
 *  GRBL Original: plan_get_block_buffer_available() in planner.c line 496
 *
 *  \return Number of free slots (0 = full, 15 = empty)
 *
 *  Thread Safety: ISR-safe
 */
uint8_t GRBLPlanner_GetBufferAvailable(void);

/*! \brief Get number of blocks currently in buffer
 *
 *  Returns number of blocks waiting for execution.
 *  Used for status reporting and debugging.
 *
 *  GRBL Original: plan_get_block_buffer_count() in planner.c line 503
 *
 *  \return Number of blocks in buffer (0 = empty, 16 = full)
 *
 *  Thread Safety: ISR-safe
 */
uint8_t GRBLPlanner_GetBufferCount(void);

/*! \brief Get planning threshold (minimum blocks for look-ahead)
 *
 *  Returns the minimum number of blocks required in buffer before
 *  motion execution should start. This ensures proper look-ahead planning
 *  for junction velocity optimization.
 *
 *  October 25, 2025: Added for delayed execution start pattern
 *
 *  \return Planning threshold (typically 4 blocks for GRBL)
 *
 *  Thread Safety: ISR-safe (constant value)
 *
 *  Usage:
 *    if (GRBLPlanner_GetBufferCount() >= GRBLPlanner_GetPlanningThreshold()) {
 *      // Safe to start execution - buffer has look-ahead window
 *    }
 */
uint8_t GRBLPlanner_GetPlanningThreshold(void);

// *****************************************************************************
// Section: Public API - Position Tracking (Single Source of Truth)
// *****************************************************************************

/*! \brief Synchronize planner position to current machine position
 *
 *  Called after events that bypass the planner:
 *    - Homing cycle complete (machine at known position)
 *    - Soft reset (position may have changed during stop)
 *    - Work coordinate override (G92 command)
 *
 *  CRITICAL: Position must be in STEPS, not mm.
 *  Use motion_math to convert if needed.
 *
 *  GRBL Original: plan_sync_position() in planner.c line 473
 *
 *  \param sys_position[N_AXIS] Current machine position in steps
 *
 *  \return None
 *
 *  Thread Safety: Main loop only (modifies position tracking)
 */
void GRBLPlanner_SyncPosition(int32_t *sys_position);

/*! \brief Get current planner position
 *
 *  Returns the planned position (where planner thinks machine will be
 *  after all buffered moves complete).
 *
 *  Used for:
 *    - Status reports (? command)
 *    - Work coordinate calculations
 *    - Soft limit checking
 *
 *  GRBL Original: plan_get_planner_mpos() in planner.c line 542
 *  Modifications: Returns mm instead of steps for convenience
 *
 *  \param target[N_AXIS] Output buffer for position in mm
 *
 *  \return None (position written to target array)
 *
 *  Thread Safety: ISR-safe (read-only access)
 */
void GRBLPlanner_GetPosition(float *target);

// *****************************************************************************
// Section: Public API - Ring Buffer Indexing (for stepper integration)
// *****************************************************************************

/*! \brief Get next block index in ring buffer
 *
 *  Helper function for ring buffer arithmetic.
 *  Used by stepper module during segment preparation.
 *
 *  GRBL Original: plan_next_block_index() in planner.c line 43
 *
 *  \param block_index Current index (0 to BLOCK_BUFFER_SIZE-1)
 *  \return Next index with wraparound
 *
 *  Thread Safety: ISR-safe (pure function, no state modification)
 *  MISRA Rule 8.7: External linkage (called from grbl_stepper.c in Phase 2)
 */
uint8_t GRBLPlanner_NextBlockIndex(uint8_t block_index);

/*! \brief Get next block in buffer (for stepper look-ahead)
 *
 *  Phase 3 Addition (October 22, 2025): Enables continuous motion through junctions
 *
 *  Returns pointer to next block in buffer for exit velocity determination.
 *  In GRBL, exit speed of block[N] = entry speed of block[N+1].
 *  This function allows stepper to look ahead for smooth cornering.
 *
 *  Usage by stepper:
 *    grbl_plan_block_t *next = GRBLPlanner_GetNextBlock(current_block);
 *    float exit_speed = (next != NULL) ? sqrt(next->entry_speed_sqr) : 0.0f;
 *
 *  \param current_block Pointer to block currently being executed
 *  \return Pointer to next block, or NULL if current is last in buffer
 *
 *  Thread Safety: ISR-safe (read-only access to buffer pointers)
 *  MISRA Rule 8.7: External linkage (called from grbl_stepper.c)
 */
grbl_plan_block_t *GRBLPlanner_GetNextBlock(grbl_plan_block_t *current_block);

#endif // GRBL_PLANNER_H
