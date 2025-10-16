/**
 * @file motion_buffer.h
 * @brief Ring buffer for motion planning with look-ahead optimization
 *
 * This module implements a circular buffer of planned motion blocks that
 * feeds the real-time motion controller (multiaxis_control.c). It provides
 * look-ahead planning to optimize junction velocities and minimize total
 * move time while respecting acceleration limits.
 *
 * Design Philosophy:
 * - Ring buffer for efficient FIFO operation (O(1) add/remove)
 * - Look-ahead planning for velocity optimization at corners
 * - Pre-calculated S-curve profiles ready for execution
 * - Integration with GRBL serial protocol (flow control)
 *
 * Data Flow:
 *   G-code Parser → parsed_move_t → MotionBuffer_Add()
 *                                  ↓
 *                            Look-Ahead Planner
 *                                  ↓
 *                            motion_block_t (ring buffer)
 *                                  ↓
 *   multiaxis_control ← MotionBuffer_GetNext()
 *
 * @date October 16, 2025
 */

#ifndef MOTION_BUFFER_H
#define MOTION_BUFFER_H

#include "motion_types.h"

//=============================================================================
// CONFIGURATION
//=============================================================================

/**
 * @brief Motion buffer size (must be power of 2 for efficient modulo)
 *
 * Typical values:
 * - 8: Minimal look-ahead, low memory (testing)
 * - 16: Good balance (recommended for initial implementation)
 * - 32: Better look-ahead, smoother motion (production)
 *
 * Memory usage: 16 blocks × ~120 bytes = ~2KB
 */
#define MOTION_BUFFER_SIZE 16

/**
 * @brief Look-ahead planning threshold
 *
 * Trigger replanning when this many blocks are in buffer.
 * Lower values = more frequent planning = more CPU time
 * Higher values = better optimization = smoother motion
 */
#define LOOKAHEAD_PLANNING_THRESHOLD 4

//=============================================================================
// BUFFER STATE
//=============================================================================

/**
 * @brief Motion buffer state flags
 */
typedef enum
{
    BUFFER_STATE_IDLE = 0,  // No motion in buffer
    BUFFER_STATE_EXECUTING, // Motion in progress
    BUFFER_STATE_PLANNING,  // Look-ahead calculation in progress
    BUFFER_STATE_FULL       // Buffer full (flow control)
} buffer_state_t;

//=============================================================================
// INITIALIZATION & CONTROL
//=============================================================================

/**
 * @brief Initialize motion buffer ring buffer
 *
 * Call once at startup before using any other buffer functions.
 * Clears all buffer entries and resets head/tail pointers.
 */
void MotionBuffer_Initialize(void);

/**
 * @brief Add a parsed move to the motion buffer
 *
 * Converts parsed G-code (in mm) to motion block (in steps) and adds
 * to ring buffer. Triggers look-ahead replanning if threshold reached.
 *
 * @param move Parsed G-code move (in mm, from G-code parser)
 * @return true if added successfully, false if buffer full
 *
 * @note This function:
 *   1. Converts mm to steps using MotionMath_MMToSteps()
 *   2. Calculates maximum entry velocity based on junction angle
 *   3. Adds block to ring buffer
 *   4. Triggers replanning if LOOKAHEAD_PLANNING_THRESHOLD reached
 */
bool MotionBuffer_Add(const parsed_move_t *move);

/**
 * @brief Get next planned motion block for execution
 *
 * Removes oldest block from ring buffer and copies to output parameter.
 * This block is ready for immediate execution by multiaxis_control.
 *
 * @param block Output: Planned motion block with pre-calculated profile
 * @return true if block retrieved, false if buffer empty
 *
 * @note This function is called by the motion controller when ready
 *       for the next move. The returned block has entry/exit velocities
 *       already optimized by the look-ahead planner.
 */
bool MotionBuffer_GetNext(motion_block_t *block);

/**
 * @brief Peek at next block without removing it
 *
 * Useful for checking if motion is pending without committing to execution.
 *
 * @param block Output: Copy of next block (if available)
 * @return true if block available, false if buffer empty
 */
bool MotionBuffer_Peek(motion_block_t *block);

//=============================================================================
// BUFFER QUERIES
//=============================================================================

/**
 * @brief Check if motion buffer is empty
 *
 * Pattern from MCC UART: Empty when write index equals read index.
 *
 * @return true if no blocks in buffer
 *
 * MISRA Compliance: Thread-safe snapshot of volatile indices
 */
bool MotionBuffer_IsEmpty(void);

/**
 * @brief Check if motion buffer is full
 *
 * Used for flow control with GRBL serial protocol.
 * When full, don't send "ok" to UGS - pause G-code transmission.
 *
 * Pattern from MCC UART: Full when (wrInIndex + 1) % SIZE == rdOutIndex
 *
 * @return true if buffer cannot accept more blocks
 *
 * MISRA Compliance: Thread-safe modulo arithmetic
 */
bool MotionBuffer_IsFull(void);

/**
 * @brief Check if motion buffer has data available (NEW FUNCTION)
 *
 * Pattern from MCC UART2_ReadCountGet():
 * Checks if write pointer (head) is ahead of read pointer (tail).
 *
 * This is the requested "has data" function - returns true when
 * wrInIndex != rdOutIndex (data available to read).
 *
 * @return true if head pointer ahead of tail (data available), false if empty
 *
 * MISRA Compliance:
 * - Thread-safe snapshots of volatile indices
 * - Explicit comparison (wrIdx != rdIdx)
 *
 * @note Equivalent to !MotionBuffer_IsEmpty() but explicitly named
 *       to match UART pattern and user's request for "poll has data function"
 */
bool MotionBuffer_HasData(void);

/**
 * @brief Get number of blocks currently in buffer
 *
 * Pattern from MCC UART2_ReadCountGet():
 * Handles wraparound case for circular buffer.
 *
 * @return Number of planned moves waiting for execution (0 to MOTION_BUFFER_SIZE-1)
 *
 * MISRA Compliance: Thread-safe calculation with explicit unsigned arithmetic
 */
uint8_t MotionBuffer_GetCount(void);

/**
 * @brief Get current buffer state
 *
 * @return Current state (IDLE, EXECUTING, PLANNING, FULL)
 */
buffer_state_t MotionBuffer_GetState(void);

//=============================================================================
// BUFFER MANAGEMENT
//=============================================================================

/**
 * @brief Clear all blocks from buffer
 *
 * Emergency stop function - discards all planned moves.
 * Call this on:
 * - Feed hold (!) command
 * - Soft reset (Ctrl-X)
 * - Hard limit triggered
 * - Error condition
 */
void MotionBuffer_Clear(void);

/**
 * @brief Pause motion execution
 *
 * Sets buffer state to prevent new blocks from being retrieved.
 * Current move continues to completion.
 *
 * Used for: Feed hold (!) command
 */
void MotionBuffer_Pause(void);

/**
 * @brief Resume motion execution
 *
 * Clears pause state, allows block retrieval to continue.
 *
 * Used for: Cycle start (~) command
 */
void MotionBuffer_Resume(void);

//=============================================================================
// LOOK-AHEAD PLANNER
//=============================================================================

/**
 * @brief Recalculate all velocities with look-ahead optimization
 *
 * This is the heart of the motion planner. It performs:
 * 1. Forward pass: Propagate exit velocities based on junction angles
 * 2. Reverse pass: Ensure entry velocities respect acceleration limits
 * 3. S-curve calculation: Generate profiles for each block
 *
 * Called automatically when:
 * - Buffer reaches LOOKAHEAD_PLANNING_THRESHOLD
 * - New block added after pause
 * - Junction angle requires velocity reduction
 *
 * @note Uses algorithms from motion_math.c:
 *   - MotionMath_CalculateJunctionVelocity()
 *   - MotionMath_CalculateJunctionAngle()
 *   - MotionMath_CalculateSCurveTiming()
 */
void MotionBuffer_RecalculateAll(void);

/**
 * @brief Calculate junction velocity between two blocks
 *
 * Helper function for look-ahead planning.
 * Determines safe cornering speed based on junction angle and acceleration.
 *
 * @param block1 First motion block (current move)
 * @param block2 Second motion block (next move)
 * @return Maximum safe velocity at junction (mm/min)
 */
float MotionBuffer_CalculateJunctionVelocity(const motion_block_t *block1,
                                             const motion_block_t *block2);

//=============================================================================
// DEBUGGING & DIAGNOSTICS
//=============================================================================

/**
 * @brief Get buffer statistics for debugging
 *
 * @param head Output: Current head index
 * @param tail Output: Current tail index
 * @param count Output: Number of blocks in buffer
 */
void MotionBuffer_GetStats(uint8_t *head, uint8_t *tail, uint8_t *count);

/**
 * @brief Dump buffer contents for debugging
 *
 * Prints all buffer entries to debug UART.
 * Useful for diagnosing planning issues.
 */
void MotionBuffer_DumpBuffer(void);

#endif // MOTION_BUFFER_H
