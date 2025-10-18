/**
 * @file motion_buffer.c
 * @brief Ring buffer implementation for motion planning with look-ahead
 *
 * This module manages a circular buffer of planned motion blocks and
 * implements look-ahead velocity optimization for smooth cornering.
 *
 * @date October 16, 2025
 */

// Debug configuration
// *****************************************************************************
// Debug Configuration
// *****************************************************************************
#define DEBUG_MOTION_BUFFER // Enable debug output for motion buffer analysis

// *****************************************************************************

#include "motion/motion_buffer.h"
#include "motion/motion_math.h"
#include "motion/multiaxis_control.h" /* For MultiAxis_GetStepCount() */
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

//=============================================================================
// FORWARD DECLARATIONS (MISRA Rule 8.4)
//=============================================================================

static uint32_t next_write_index(void);
static void plan_buffer_line(motion_block_t *block, const parsed_move_t *move);
static void recalculate_trapezoids(void);

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

    /* Check if buffer is full */
    if (MotionBuffer_IsFull())
    {
        buffer_state = BUFFER_STATE_FULL;
        return false;
    }

    /* Get next write index and current buffer slot
     * MISRA Rule 10.3: Explicit cast for array index */
    uint32_t nextWrIdx = next_write_index();
    motion_block_t *block = &motion_buffer[wrInIndex];

    /* Plan this block (convert mm to steps, calculate velocities) */
    plan_buffer_line(block, move);

    /* ═══════════════════════════════════════════════════════════════
     * CRITICAL: Commit to buffer (UART pattern)
     * ═══════════════════════════════════════════════════════════════
     * Advance write index AFTER data is written.
     * This ensures ISR/reader never sees incomplete data.
     * ═══════════════════════════════════════════════════════════════ */
    wrInIndex = nextWrIdx;

    /* Update state */
    if (buffer_state == BUFFER_STATE_IDLE || buffer_state == BUFFER_STATE_FULL)
    {
        buffer_state = BUFFER_STATE_EXECUTING;
    }

    /* Trigger look-ahead replanning if threshold reached */
    uint8_t count = MotionBuffer_GetCount();
    if (count >= LOOKAHEAD_PLANNING_THRESHOLD)
    {
        MotionBuffer_RecalculateAll();
    }

    return true;
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
 * @brief Plan a buffer line (convert parsed move to motion block)
 *
 * This function:
 * 1. Converts mm to steps using motion_math
 * 2. Determines which axes are active
 * 3. Calculates maximum entry velocity
 * 4. Initializes recalculate flag for look-ahead
 */
static void plan_buffer_line(motion_block_t *block, const parsed_move_t *move)
{
    // Clear block
    memset(block, 0, sizeof(motion_block_t));

    // Convert work coordinates to machine coordinates and calculate delta
    // CRITICAL: Use planned_position_mm (where we WILL BE) not actual position
    // This prevents race conditions when queuing multiple moves rapidly
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        if (move->axis_words[axis])
        {
            // Convert target work coordinates to machine coordinates
            // Formula: MPos = WPos + work_offset + g92_offset
            float target_mm_machine = MotionMath_WorkToMachine(move->target[axis], axis);

            // Calculate delta from planned position (NOT actual machine position!)
            float delta_mm = target_mm_machine - planned_position_mm[axis];

            // Convert delta to steps
            block->steps[axis] = MotionMath_MMToSteps(delta_mm, axis);
            block->axis_active[axis] = true;

            // Update planned position for next move
            planned_position_mm[axis] = target_mm_machine;
        }
        else
        {
            block->steps[axis] = 0;
            block->axis_active[axis] = false;
            // Planned position unchanged for this axis
        }
    }

    // Store feedrate
    block->feedrate = move->feedrate;

    // Initialize velocities (will be optimized by look-ahead)
    block->entry_velocity = 0.0f; // Start from rest
    block->exit_velocity = 0.0f;  // Assume stop at end

    // Calculate maximum entry velocity based on feedrate and acceleration
    // This is the ceiling - look-ahead will optimize below this
    float max_velocity = MotionMath_GetMaxVelocityStepsPerSec(AXIS_X) * 60.0f; // Convert to mm/min
    block->max_entry_velocity = fminf(move->feedrate, max_velocity);

    // Mark for recalculation
    block->recalculate_flag = true;

    // DEBUG: Print what we're adding to the buffer
#ifdef DEBUG_MOTION_BUFFER
    UGS_Printf("[DEBUG] Motion block: X=%ld Y=%ld Z=%ld (active: %d%d%d)\r\n",
               block->steps[AXIS_X], block->steps[AXIS_Y], block->steps[AXIS_Z],
               block->axis_active[AXIS_X], block->axis_active[AXIS_Y], block->axis_active[AXIS_Z]);
    UGS_Printf("[DEBUG] Planned pos: X=%.3f Y=%.3f Z=%.3f\r\n",
               planned_position_mm[AXIS_X], planned_position_mm[AXIS_Y], planned_position_mm[AXIS_Z]);
#endif
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
