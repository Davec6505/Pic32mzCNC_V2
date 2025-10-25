/**
 * @file motion_buffer.c
 * @brief Ring buffer implementation for motion planning with look-ahead
 *
 * This module manages a circular buffer of planned motion blocks and
 * implements look-ahead velocity optimization for smooth cornering.
 *
 * @date October 16, 2025
 */

/* DEBUG_MOTION_BUFFER controlled by Makefile (make all DEBUG=1) */

#include "motion/motion_buffer.h"
#include "motion/motion_math.h"
#include "motion/multiaxis_control.h" /* For MultiAxis_GetStepCount() */
#include "motion/grbl_planner.h"      /* CRITICAL (Oct 25, 2025): For GRBLPlanner_BufferLine() */
#include "ugs_interface.h"            /* For UGS_Printf() debug output */
#include <string.h>
#include <math.h>

//=============================================================================
// RING BUFFER STATE (MCC UART Pattern)
//=============================================================================

/**
 * @brief Ring buffer storage
 *
 * Circular buffer using MCC UART-style In/Out indices:
 * - wrInIndex (write head): Next write position (written by main, read by ISR)
 * - rdOutIndex (read tail): Next read position (written by main, read by ISR)
 * - Empty when wrInIndex == rdOutIndex
 * - Full when (wrInIndex + 1) % SIZE == rdOutIndex
 *
 * MISRA C Compliance:
 * - Rule 8.7: Static file scope variables
 * - Rule 8.4: Volatile for shared access between contexts
 */
static motion_block_t motion_buffer[MOTION_BUFFER_SIZE];

/* MISRA Rule 8.4: Volatile required - accessed from main loop and future ISR */
static volatile uint32_t wrInIndex = 0U;  /* Write index (head pointer) */
static volatile uint32_t rdOutIndex = 0U; /* Read index (tail pointer) */

static volatile buffer_state_t buffer_state = BUFFER_STATE_IDLE;
static volatile bool paused = false;

/**
 * @brief Planned position tracker (NOT actual machine position)
 *
 * This tracks where the machine WILL BE after all queued moves complete.
 * Updated when blocks are added to the buffer, NOT when they execute.
 * This prevents race conditions where multiple moves are planned simultaneously.
 *
 * Formula: planned_position += delta for each new block added
 */
static float planned_position_mm[NUM_AXES] = {0.0f};

/** @brief Flag to disable position updates during arc segment generation
 *
 * When true, MotionBuffer_Add() will NOT update planned_position_mm.
 * This prevents arc segment recursion from corrupting the arc geometry.
 *
 * @date October 22, 2025
 */
static bool disable_position_update = false;

//=============================================================================
// FORWARD DECLARATIONS (MISRA Rule 8.4)
//=============================================================================

static uint32_t next_write_index(void);
static void recalculate_trapezoids(void);
static bool convert_arc_to_segments(const parsed_move_t *arc_move);

//=============================================================================
// INITIALIZATION
//=============================================================================

/**
 * @brief Initialize motion buffer ring buffer
 *
 * Resets all indices and clears buffer memory.
 * MISRA Rule 10.3: Explicit initialization to zero.
 */
void MotionBuffer_Initialize(void)
{
    /* MISRA Rule 10.3: Explicit unsigned zero initialization */
    wrInIndex = 0U;
    rdOutIndex = 0U;
    buffer_state = BUFFER_STATE_IDLE;
    paused = false;

    /* Clear all buffer entries (MISRA Rule 21.6: memset acceptable for initialization) */
    (void)memset(motion_buffer, 0, sizeof(motion_buffer));

    /* Sync planned position with actual machine position */
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        planned_position_mm[axis] = MotionMath_GetMachinePosition(axis);
    }
}

//=============================================================================
// ARC CONVERSION (G2/G3 → Linear Segments)
//=============================================================================

/**
 * @brief Convert G2/G3 arc move into multiple linear G1 segments
 *
 * GRBL-style arc-to-segment conversion based on arc tolerance.
 * Breaks circular arc into many small linear moves that approximate the curve.
 *
 * Algorithm:
 * 1. Calculate arc center from I,J,K offsets
 * 2. Calculate start/end angles and angular travel
 * 3. Determine number of segments based on $12 arc_tolerance
 * 4. Generate linear segments using vector rotation
 * 5. Feed each segment to motion buffer as G1 move
 *
 * @param arc_move Parsed arc command (G2/G3 with I,J,K or R parameters)
 * @return true if arc converted successfully, false on error
 *
 * References:
 * - GRBL motion_control.c::mc_arc()
 * - GRBL config.h::$12 arc_tolerance (default 0.002mm)
 *
 * @date October 22, 2025
 */
static bool convert_arc_to_segments(const parsed_move_t *arc_move)
{
    /* Parameter validation */
    if (arc_move == NULL)
    {
        return false;
    }

    /* Determine plane selection (currently only XY plane supported) */
    /* TODO: Add G18 (XZ) and G19 (YZ) plane support */
    const axis_id_t axis_0 = AXIS_X;  /* First axis in plane */
    const axis_id_t axis_1 = AXIS_Y;  /* Second axis in plane */
    const axis_id_t axis_linear = AXIS_Z;  /* Linear axis (for helical arcs) */

    /* Get current position in mm (machine coordinates) */
    float position[NUM_AXES];
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        position[axis] = planned_position_mm[axis];
    }

    /* Calculate arc center (offset from current position) */
    float center[3];
    center[axis_0] = position[axis_0] + arc_move->arc_center_offset[AXIS_X];
    center[axis_1] = position[axis_1] + arc_move->arc_center_offset[AXIS_Y];
    center[axis_linear] = position[axis_linear];  /* No offset on linear axis */

    /* Radius vector from center to current position */
    float r_axis0 = -arc_move->arc_center_offset[AXIS_X];
    float r_axis1 = -arc_move->arc_center_offset[AXIS_Y];

    /* Radius vector from center to target position */
    float rt_axis0 = arc_move->target[axis_0] - center[axis_0];
    float rt_axis1 = arc_move->target[axis_1] - center[axis_1];

    /* Calculate angular travel using atan2 (CCW angle between vectors) */
    float angular_travel = atan2f(r_axis0 * rt_axis1 - r_axis1 * rt_axis0,
                                   r_axis0 * rt_axis0 + r_axis1 * rt_axis1);

    /* Adjust for clockwise (G2) vs counter-clockwise (G3) */
    bool is_clockwise = (arc_move->motion_mode == 2);  /* G2 = CW, G3 = CCW */
    if (is_clockwise)
    {
        if (angular_travel >= -1.0e-6f)  /* Near zero or positive */
        {
            angular_travel -= 2.0f * M_PI;
        }
    }
    else  /* Counter-clockwise */
    {
        if (angular_travel <= 1.0e-6f)  /* Near zero or negative */
        {
            angular_travel += 2.0f * M_PI;
        }
    }

    /* Calculate arc radius */
    float radius = sqrtf(r_axis0 * r_axis0 + r_axis1 * r_axis1);

    /* Calculate number of segments based on arc tolerance ($12)
     * GRBL formula: segments = floor(0.5 * angular_travel * radius /
     *                                 sqrt(arc_tolerance * (2*radius - arc_tolerance)))
     */
    float arc_tolerance = MotionMath_GetArcTolerance();
    uint16_t segments = (uint16_t)floorf(fabsf(0.5f * angular_travel * radius) /
                                         sqrtf(arc_tolerance * (2.0f * radius - arc_tolerance)));

    if (segments < 1U)
    {
        segments = 1U;  /* Minimum one segment */
    }

    /* Calculate segment parameters */
    float theta_per_segment = angular_travel / (float)segments;
    float linear_per_segment = (arc_move->target[axis_linear] - position[axis_linear]) / (float)segments;

    /* Small angle approximation for fast rotation (GRBL optimization)
     * cos(theta) ≈ 2 - theta²/2
     * sin(theta) ≈ theta - theta³/6
     */
    float cos_T = 2.0f - theta_per_segment * theta_per_segment;
    float sin_T = theta_per_segment * 0.16666667f * (cos_T + 4.0f);
    cos_T *= 0.5f;

    /* Arc correction frequency (GRBL uses 12 segments between corrections) */
    #define N_ARC_CORRECTION 12U
    uint16_t arc_correction_counter = 0;

    /* Generate arc segments */
    parsed_move_t segment_move;
    memcpy(&segment_move, arc_move, sizeof(parsed_move_t));
    segment_move.motion_mode = 1;  /* Convert arc to G1 linear moves */
    segment_move.absolute_mode = true;  /* Segments use absolute coordinates */
    
    /* CRITICAL (Oct 22, 2025): Disable position updates during segment generation!
     * Each recursive MotionBuffer_Add() call would update planned_position_mm,
     * corrupting the arc geometry since center/radius depend on starting position.
     */
    disable_position_update = true;
    
#ifdef DEBUG_MOTION_BUFFER
    UGS_Printf("[ARC] Disabled position updates, generating %u segments\r\n", segments);
#endif

    for (uint16_t i = 1U; i <= segments; i++)
    {
        /* Periodic arc correction (Oct 22, 2025): Recalculate exact radius vector
         * every N_ARC_CORRECTION segments to prevent floating-point drift.
         * This reduces endpoint error from ~1mm to <0.01mm on large arcs.
         */
        if (arc_correction_counter >= N_ARC_CORRECTION)
        {
            /* Recalculate exact position using current angle */
            float cos_ti = cosf(theta_per_segment * (float)i);
            float sin_ti = sinf(theta_per_segment * (float)i);
            r_axis0 = -arc_move->arc_center_offset[AXIS_X] * cos_ti - 
                      -arc_move->arc_center_offset[AXIS_Y] * sin_ti;
            r_axis1 = -arc_move->arc_center_offset[AXIS_X] * sin_ti + 
                      -arc_move->arc_center_offset[AXIS_Y] * cos_ti;
            arc_correction_counter = 0;
        }
        else
        {
            arc_correction_counter++;
        }

        /* Apply vector rotation matrix (fast approximation between corrections) */
        float r_axisi = r_axis0 * sin_T + r_axis1 * cos_T;
        r_axis0 = r_axis0 * cos_T - r_axis1 * sin_T;
        r_axis1 = r_axisi;

        /* Calculate segment endpoint */
        segment_move.target[axis_0] = center[axis_0] + r_axis0;
        segment_move.target[axis_1] = center[axis_1] + r_axis1;
        segment_move.target[axis_linear] = position[axis_linear] + linear_per_segment * (float)i;

        /* Mark axes as active */
        segment_move.axis_words[axis_0] = true;
        segment_move.axis_words[axis_1] = true;
        if (linear_per_segment != 0.0f)
        {
            segment_move.axis_words[axis_linear] = true;
        }

        /* Clear arc parameters (now a linear move) */
        segment_move.arc_has_ijk = false;
        segment_move.arc_has_radius = false;

        /* Add segment to buffer recursively - BLOCK on full buffer!
         * CRITICAL (Oct 25, 2025): Must NOT return false on buffer full!
         * If we return false, main.c will retry the ENTIRE arc command,
         * causing an infinite loop. Instead, spin-wait for buffer space.
         * 
         * Motion execution continues via ISRs (TMR9 prepares segments,
         * OCR callbacks execute steps), so this spin loop won't block
         * motion - it just waits for the planner to free space.
         */
        while (!MotionBuffer_Add(&segment_move))
        {
            /* Spin-wait for buffer space - ISRs continue running */
            /* TMR9 will prep segments, freeing planner blocks */
            /* Add small delay to prevent CPU hogging (1000 cycles ≈ 5µs @ 200MHz) */
            for (volatile uint32_t delay = 0; delay < 1000U; delay++) { }
        }
    }
    
    /* Re-enable position updates */
    disable_position_update = false;
    
#ifdef DEBUG_MOTION_BUFFER
    UGS_Printf("[ARC] Re-enabled position updates, updating to final target\r\n");
#endif
    
    /* Update planned position ONCE to final arc target */
    planned_position_mm[axis_0] = arc_move->target[axis_0];
    planned_position_mm[axis_1] = arc_move->target[axis_1];
    planned_position_mm[axis_linear] = arc_move->target[axis_linear];

    return true;
}

//=============================================================================
// BUFFER OPERATIONS
//=============================================================================

/**
 * @brief Add parsed G-code move to motion buffer
 *
 * Pattern from UART write logic with critical section protection.
 *
 * @param move Pointer to parsed G-code move structure
 * @return true if added successfully, false if buffer full
 *
 * MISRA Rule 17.4: Array parameter validated
 * MISRA Rule 10.3: Explicit unsigned arithmetic
 */
bool MotionBuffer_Add(const parsed_move_t *move)
{
    /* MISRA Rule 17.4: Parameter validation */
    if (move == NULL)
    {
        return false;
    }
    
    #ifdef DEBUG_MOTION_BUFFER
    UGS_Printf("[BUFFER] Add: mode=%u, ijk=%d, target=(%.3f,%.3f), I=%.3f J=%.3f\r\n",
               move->motion_mode, move->arc_has_ijk,
               move->target[AXIS_X], move->target[AXIS_Y],
               move->arc_center_offset[AXIS_X], move->arc_center_offset[AXIS_Y]);
    #endif
    
    /* ═══════════════════════════════════════════════════════════════
     * ARC CONVERSION: G2/G3 → Multiple G1 Segments (October 22, 2025)
     * ═══════════════════════════════════════════════════════════════
     * If this is an arc command (G2 or G3), convert it to many small
     * linear segments before adding to buffer. This matches GRBL's
     * proven arc-to-segment algorithm.
     * ═══════════════════════════════════════════════════════════════ */
    if (move->motion_mode == 2 || move->motion_mode == 3)  /* G2 = CW arc, G3 = CCW arc */
    {
        /* Verify arc parameters are present */
        if (!move->arc_has_ijk && !move->arc_has_radius)
        {
            UGS_Printf("error: G2/G3 requires I,J,K or R parameters\r\n");
            return false;
        }
        
        /* Convert arc to segments (recursive calls to MotionBuffer_Add) */
        return convert_arc_to_segments(move);
    }

    /* ═══════════════════════════════════════════════════════════════
     * CRITICAL FIX (October 25, 2025): For LINEAR moves, bypass motion_buffer!
     * ═══════════════════════════════════════════════════════════════
     * motion_buffer was designed for ARC-TO-SEGMENT conversion.
     * Linear moves (G0/G1) should go DIRECTLY to GRBL planner.
     * 
     * Architecture:
     *   G0/G1 → GRBLPlanner_BufferLine() DIRECTLY (this path)
     *   G2/G3 → convert_arc_to_segments() → recursive MotionBuffer_Add() → GRBL planner
     * ═══════════════════════════════════════════════════════════════ */
    
    // Build absolute target in mm for GRBL planner
    float target_mm[NUM_AXES];
    GRBLPlanner_GetPosition(target_mm);  // Start with current planned position
    
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        if (move->axis_words[axis])
        {
            if (move->absolute_mode)
            {
                // G90 (absolute): target is absolute work coordinate
                target_mm[axis] = MotionMath_WorkToMachine(move->target[axis], axis);
            }
            else
            {
                // G91 (relative): add offset to current position
                target_mm[axis] += move->target[axis];
            }
        }
    }
    
    // Build GRBL planner line data
    grbl_plan_line_data_t pl_data;
    pl_data.feed_rate = move->feedrate;
    pl_data.spindle_speed = 0.0f;
    pl_data.condition = 0U;
    if (move->motion_mode == 0)  // G0 = rapid
    {
        pl_data.condition |= PL_COND_FLAG_RAPID_MOTION | PL_COND_FLAG_NO_FEED_OVERRIDE;
    }
    
    // Call GRBL planner directly (skip motion_buffer for linear moves)
    plan_status_t plan_result = GRBLPlanner_BufferLine(target_mm, &pl_data);
    
    if (plan_result == PLAN_OK)
    {
        return true;  // Success!
    }
    else if (plan_result == PLAN_BUFFER_FULL)
    {
        return false;  // Buffer full - retry later
    }
    else  // PLAN_EMPTY_BLOCK
    {
        return true;  // Zero-length move - silently accept
    }
}

/**
 * @brief Get next motion block from buffer (dequeue)
 *
 * Pattern from UART2_Read(): Copy data and advance read pointer.
 *
 * @param block Pointer to motion block structure to fill
 * @return true if block retrieved, false if empty or paused
 *
 * MISRA Rule 17.4: Pointer parameter validated
 * MISRA Rule 10.3: Explicit modulo arithmetic
 */
bool MotionBuffer_GetNext(motion_block_t *block)
{
    /* MISRA Rule 17.4: Parameter validation */
    if (block == NULL)
    {
        return false;
    }

    /* Check if paused (flow control) */
    if (paused)
    {
        return false;
    }

    /* Check if buffer empty */
    if (MotionBuffer_IsEmpty())
    {
        buffer_state = BUFFER_STATE_IDLE;
        return false;
    }

    /* Take snapshot of read index (UART pattern for thread safety) */
    uint32_t rdIdx = rdOutIndex;

    /* Copy block from current read position
     * MISRA Rule 21.6: memcpy acceptable for struct copy */
    (void)memcpy(block, &motion_buffer[rdIdx], sizeof(motion_block_t));

    /* ═══════════════════════════════════════════════════════════════
     * CRITICAL: Advance read pointer (UART pattern)
     * ═══════════════════════════════════════════════════════════════
     * Advance AFTER data is copied.
     * MISRA Rule 10.3: Explicit modulo for wraparound.
     * ═══════════════════════════════════════════════════════════════ */
    rdIdx = (rdIdx + 1U) % MOTION_BUFFER_SIZE;
    rdOutIndex = rdIdx;

    /* Update state */
    buffer_state = BUFFER_STATE_EXECUTING;

    return true;
}

/**
 * @brief Peek at next motion block without removing it
 *
 * Similar to GetNext but does not advance read pointer.
 *
 * @param block Pointer to motion block structure to fill
 * @return true if block available, false if empty
 *
 * MISRA Rule 17.4: Pointer parameter validated
 */
bool MotionBuffer_Peek(motion_block_t *block)
{
    /* MISRA Rule 17.4: Parameter validation */
    if (block == NULL)
    {
        return false;
    }

    if (MotionBuffer_IsEmpty())
    {
        return false;
    }

    /* Copy without advancing read pointer */
    uint32_t rdIdx = rdOutIndex;
    (void)memcpy(block, &motion_buffer[rdIdx], sizeof(motion_block_t));

    return true;
}

//=============================================================================
// BUFFER QUERIES (MCC UART Pattern - MISRA Compliant)
//=============================================================================

/**
 * @brief Check if motion buffer is empty
 *
 * Pattern from UART2_ReadCountGet():
 * Empty when write index equals read index.
 *
 * @return true if buffer is empty, false otherwise
 *
 * MISRA Rule 8.13: Parameters are read-only
 */
bool MotionBuffer_IsEmpty(void)
{
    /* Take snapshot to avoid race condition (UART pattern) */
    uint32_t wrIdx = wrInIndex;
    uint32_t rdIdx = rdOutIndex;

    return (wrIdx == rdIdx);
}

/**
 * @brief Check if motion buffer is full
 *
 * Pattern from UART logic:
 * Full when next write position would equal read position.
 *
 * @return true if buffer is full, false otherwise
 */
bool MotionBuffer_IsFull(void)
{
    uint32_t nextWrIdx = next_write_index();
    uint32_t rdIdx = rdOutIndex;

    return (nextWrIdx == rdIdx);
}

/**
 * @brief Check if motion buffer has data available
 *
 * NEW FUNCTION - Requested by user.
 * Pattern from UART2_ReadCountGet():
 * Data available when write index is ahead of read index.
 *
 * @return true if head pointer ahead of tail (data available), false otherwise
 *
 * MISRA Rule 10.1: Explicit comparison operators
 */
bool MotionBuffer_HasData(void)
{
    /* Take snapshots to avoid race condition (critical for ISR safety) */
    uint32_t wrIdx = wrInIndex;
    uint32_t rdIdx = rdOutIndex;

    /* Head ahead of tail means data available */
    return (wrIdx != rdIdx);
}

/**
 * @brief Get number of blocks in buffer
 *
 * Pattern from UART2_ReadCountGet():
 * Handle wraparound case for circular buffer.
 *
 * @return Number of motion blocks available to read
 *
 * MISRA Rule 10.3: Explicit unsigned arithmetic
 */
uint8_t MotionBuffer_GetCount(void)
{
    size_t count;

    /* Take snapshots (UART pattern for thread safety) */
    uint32_t wrIdx = wrInIndex;
    uint32_t rdIdx = rdOutIndex;

    /* UART2_ReadCountGet() pattern: Handle wrap-around */
    if (wrIdx >= rdIdx)
    {
        count = wrIdx - rdIdx;
    }
    else
    {
        /* Wrapped: count = (size - tail) + head */
        count = (MOTION_BUFFER_SIZE - rdIdx) + wrIdx;
    }

    /* MISRA Rule 10.3: Explicit cast to uint8_t */
    return (uint8_t)count;
}

/**
 * @brief Get buffer state
 *
 * @return Current buffer state (IDLE, EXECUTING, FULL)
 */
buffer_state_t MotionBuffer_GetState(void)
{
    return buffer_state;
}

//=============================================================================
// BUFFER MANAGEMENT (MISRA Compliant Flow Control)
//=============================================================================

/**
 * @brief Clear all motion blocks from buffer
 *
 * Emergency stop / reset function.
 * Pattern: Reset read index to write index (buffer becomes empty).
 *
 * MISRA Rule 10.3: Explicit assignment
 */
void MotionBuffer_Clear(void)
{
    /* Make read index equal write index (buffer now empty) */
    rdOutIndex = wrInIndex;

    buffer_state = BUFFER_STATE_IDLE;
    paused = false;

    /* Resync planned position with actual machine position after clear */
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        planned_position_mm[axis] = MotionMath_GetMachinePosition(axis);
    }
}

/**
 * @brief Pause motion buffer processing (feed hold)
 *
 * Prevents GetNext() from returning blocks.
 * Implements GRBL '!' feed hold command.
 */
void MotionBuffer_Pause(void)
{
    paused = true;
}

/**
 * @brief Resume motion buffer processing (cycle start)
 *
 * Allows GetNext() to return blocks again.
 * Implements GRBL '~' cycle start command.
 */
void MotionBuffer_Resume(void)
{
    paused = false;

    // If buffer has blocks, resume execution state
    if (!MotionBuffer_IsEmpty())
    {
        buffer_state = BUFFER_STATE_EXECUTING;
    }
}

//=============================================================================
// LOOK-AHEAD PLANNER
//=============================================================================

void MotionBuffer_RecalculateAll(void)
{
    // Mark as planning
    buffer_state = BUFFER_STATE_PLANNING;

    // TODO: Implement full look-ahead algorithm
    // For now, use simple trapezoid recalculation
    recalculate_trapezoids();

    // Restore execution state
    buffer_state = BUFFER_STATE_EXECUTING;
}

float MotionBuffer_CalculateJunctionVelocity(const motion_block_t *block1,
                                             const motion_block_t *block2)
{
    // Calculate angle between move vectors
    // For simplicity, use 2D XY angle (TODO: extend to 3D)
    float angle = MotionMath_CalculateJunctionAngle(
        (float)block1->steps[AXIS_X],
        (float)block1->steps[AXIS_Y],
        0.0f, // Z component (unused for now)
        (float)block2->steps[AXIS_X],
        (float)block2->steps[AXIS_Y],
        0.0f // Z component (unused for now)
    );

    // Use motion_math helper to calculate safe junction velocity
    return MotionMath_CalculateJunctionVelocity(
        angle,
        block1->feedrate,
        block2->feedrate,
        motion_settings.junction_deviation);
}

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Calculate next write index (ring buffer wrap)
 *
 * Pattern from UART: Calculate next position with modulo wraparound.
 *
 * @return Next write index position
 *
 * MISRA Rule 8.7: Static internal helper
 * MISRA Rule 10.3: Explicit modulo arithmetic
 */
static uint32_t next_write_index(void)
{
    /* MISRA Rule 10.3: Explicit unsigned modulo for wraparound */
    return (wrInIndex + 1U) % MOTION_BUFFER_SIZE;
}

/**
 * @brief Recalculate velocity profiles for all blocks in buffer
 *
 * Simplified trapezoidal planning (placeholder for full look-ahead).
 *
 * TODO: Implement full GRBL-style look-ahead:
 * 1. Forward pass: Maximize exit velocities
 * 2. Reverse pass: Ensure acceleration limits respected
 * 3. Calculate S-curve profiles
 */
static void recalculate_trapezoids(void)
{
    if (MotionBuffer_IsEmpty())
    {
        return;
    }

    /* MISRA Rule 10.3: Use new index naming (UART pattern) */
    /* Start from read position (tail) */
    uint32_t index = rdOutIndex;
    uint8_t count = MotionBuffer_GetCount();

    /* MISRA Rule 14.2: Loop counter validated */
    for (uint8_t i = 0; i < count; i++)
    {
        motion_block_t *block = &motion_buffer[index];

        if (block->recalculate_flag)
        {
            /* For now, use nominal feedrate */
            /* TODO: Implement full GRBL look-ahead junction velocity calculation */
            block->entry_velocity = block->feedrate;
            block->exit_velocity = block->feedrate;
            block->recalculate_flag = false;
        }

        /* Move to next block (MISRA Rule 10.3: Explicit modulo) */
        index = (index + 1U) % MOTION_BUFFER_SIZE;
    }
}

//=============================================================================
// DEBUGGING (MISRA Compliant)
//=============================================================================

/**
 * @brief Get buffer statistics for debugging
 *
 * Returns current write/read indices and count.
 * Updated to use UART pattern naming (wrInIndex/rdOutIndex).
 *
 * @param head Output: Write index (head pointer)
 * @param tail Output: Read index (tail pointer)
 * @param count Output: Number of blocks in buffer
 *
 * MISRA Rule 17.4: Pointer parameters validated inline
 */
void MotionBuffer_GetStats(uint8_t *head, uint8_t *tail, uint8_t *count)
{
    /* MISRA Rule 17.4: Validate pointers before use */
    if (head != NULL)
    {
        /* MISRA Rule 10.3: Explicit cast to uint8_t */
        *head = (uint8_t)wrInIndex;
    }

    if (tail != NULL)
    {
        *tail = (uint8_t)rdOutIndex;
    }

    if (count != NULL)
    {
        *count = MotionBuffer_GetCount();
    }
}

void MotionBuffer_DumpBuffer(void)
{
    // TODO: Implement debug output
    // This would print buffer contents to UART for debugging
    // Format: [index] steps=[X,Y,Z,A] feedrate=### entry_v=### exit_v=###
}
