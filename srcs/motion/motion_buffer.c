/**
 * @file motion_buffer.c
 * @brief Ring buffer implementation for motion planning with look-ahead
 * 
 * This module manages a circular buffer of planned motion blocks and
 * implements look-ahead velocity optimization for smooth cornering.
 * 
 * @date October 16, 2025
 */

#include "motion/motion_buffer.h"
#include "motion/motion_math.h"
#include <string.h>
#include <math.h>

//=============================================================================
// RING BUFFER STATE
//=============================================================================

/**
 * @brief Ring buffer storage
 * 
 * Circular buffer using head/tail pointers:
 * - head: Next write position
 * - tail: Next read position
 * - Empty when head == tail
 * - Full when (head + 1) % SIZE == tail
 */
static motion_block_t motion_buffer[MOTION_BUFFER_SIZE];
static volatile uint8_t buffer_head = 0;  // Next write index
static volatile uint8_t buffer_tail = 0;  // Next read index
static volatile buffer_state_t buffer_state = BUFFER_STATE_IDLE;
static volatile bool paused = false;

//=============================================================================
// FORWARD DECLARATIONS
//=============================================================================

static uint8_t next_buffer_head(void);
static void plan_buffer_line(motion_block_t* block, const parsed_move_t* move);
static void recalculate_trapezoids(void);

//=============================================================================
// INITIALIZATION
//=============================================================================

void MotionBuffer_Initialize(void) {
    buffer_head = 0;
    buffer_tail = 0;
    buffer_state = BUFFER_STATE_IDLE;
    paused = false;
    
    // Clear all buffer entries
    memset(motion_buffer, 0, sizeof(motion_buffer));
}

//=============================================================================
// BUFFER OPERATIONS
//=============================================================================

bool MotionBuffer_Add(const parsed_move_t* move) {
    // Check if buffer is full
    if (MotionBuffer_IsFull()) {
        buffer_state = BUFFER_STATE_FULL;
        return false;
    }
    
    // Get next buffer slot
    uint8_t next_head = next_buffer_head();
    motion_block_t* block = &motion_buffer[buffer_head];
    
    // Plan this block (convert mm to steps, calculate velocities)
    plan_buffer_line(block, move);
    
    // Commit to buffer (advance head pointer)
    buffer_head = next_head;
    
    // Update state
    if (buffer_state == BUFFER_STATE_IDLE || buffer_state == BUFFER_STATE_FULL) {
        buffer_state = BUFFER_STATE_EXECUTING;
    }
    
    // Trigger look-ahead replanning if threshold reached
    uint8_t count = MotionBuffer_GetCount();
    if (count >= LOOKAHEAD_PLANNING_THRESHOLD) {
        MotionBuffer_RecalculateAll();
    }
    
    return true;
}

bool MotionBuffer_GetNext(motion_block_t* block) {
    // Check if paused
    if (paused) {
        return false;
    }
    
    // Check if buffer empty
    if (MotionBuffer_IsEmpty()) {
        buffer_state = BUFFER_STATE_IDLE;
        return false;
    }
    
    // Copy block from tail position
    memcpy(block, &motion_buffer[buffer_tail], sizeof(motion_block_t));
    
    // Advance tail pointer
    buffer_tail = (buffer_tail + 1) % MOTION_BUFFER_SIZE;
    
    // Update state
    buffer_state = BUFFER_STATE_EXECUTING;
    
    return true;
}

bool MotionBuffer_Peek(motion_block_t* block) {
    if (MotionBuffer_IsEmpty()) {
        return false;
    }
    
    // Copy without advancing tail
    memcpy(block, &motion_buffer[buffer_tail], sizeof(motion_block_t));
    return true;
}

//=============================================================================
// BUFFER QUERIES
//=============================================================================

bool MotionBuffer_IsEmpty(void) {
    return (buffer_head == buffer_tail);
}

bool MotionBuffer_IsFull(void) {
    return (next_buffer_head() == buffer_tail);
}

uint8_t MotionBuffer_GetCount(void) {
    if (buffer_head >= buffer_tail) {
        return buffer_head - buffer_tail;
    } else {
        return MOTION_BUFFER_SIZE - (buffer_tail - buffer_head);
    }
}

buffer_state_t MotionBuffer_GetState(void) {
    return buffer_state;
}

//=============================================================================
// BUFFER MANAGEMENT
//=============================================================================

void MotionBuffer_Clear(void) {
    buffer_head = buffer_tail;  // Reset pointers (buffer now empty)
    buffer_state = BUFFER_STATE_IDLE;
    paused = false;
}

void MotionBuffer_Pause(void) {
    paused = true;
}

void MotionBuffer_Resume(void) {
    paused = false;
    
    // If buffer has blocks, resume execution state
    if (!MotionBuffer_IsEmpty()) {
        buffer_state = BUFFER_STATE_EXECUTING;
    }
}

//=============================================================================
// LOOK-AHEAD PLANNER
//=============================================================================

void MotionBuffer_RecalculateAll(void) {
    // Mark as planning
    buffer_state = BUFFER_STATE_PLANNING;
    
    // TODO: Implement full look-ahead algorithm
    // For now, use simple trapezoid recalculation
    recalculate_trapezoids();
    
    // Restore execution state
    buffer_state = BUFFER_STATE_EXECUTING;
}

float MotionBuffer_CalculateJunctionVelocity(const motion_block_t* block1,
                                              const motion_block_t* block2) {
    // Calculate angle between move vectors
    // For simplicity, use 2D XY angle (TODO: extend to 3D)
    float angle = MotionMath_CalculateJunctionAngle(
        (float)block1->steps[AXIS_X],
        (float)block1->steps[AXIS_Y],
        0.0f,  // Z component (unused for now)
        (float)block2->steps[AXIS_X],
        (float)block2->steps[AXIS_Y],
        0.0f   // Z component (unused for now)
    );
    
    // Use motion_math helper to calculate safe junction velocity
    return MotionMath_CalculateJunctionVelocity(
        angle,
        block1->feedrate,
        block2->feedrate,
        motion_settings.junction_deviation
    );
}

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Calculate next head index (ring buffer wrap)
 */
static uint8_t next_buffer_head(void) {
    return (buffer_head + 1) % MOTION_BUFFER_SIZE;
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
static void plan_buffer_line(motion_block_t* block, const parsed_move_t* move) {
    // Clear block
    memset(block, 0, sizeof(motion_block_t));
    
    // Convert mm to steps for each axis
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++) {
        if (move->axis_words[axis]) {
            block->steps[axis] = MotionMath_MMToSteps(move->target[axis], axis);
            block->axis_active[axis] = true;
        } else {
            block->steps[axis] = 0;
            block->axis_active[axis] = false;
        }
    }
    
    // Store feedrate
    block->feedrate = move->feedrate;
    
    // Initialize velocities (will be optimized by look-ahead)
    block->entry_velocity = 0.0f;  // Start from rest
    block->exit_velocity = 0.0f;   // Assume stop at end
    
    // Calculate maximum entry velocity based on feedrate and acceleration
    // This is the ceiling - look-ahead will optimize below this
    float max_velocity = MotionMath_GetMaxVelocityStepsPerSec(AXIS_X) * 60.0f;  // Convert to mm/min
    block->max_entry_velocity = fminf(move->feedrate, max_velocity);
    
    // Mark for recalculation
    block->recalculate_flag = true;
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
static void recalculate_trapezoids(void) {
    if (MotionBuffer_IsEmpty()) {
        return;
    }
    
    // Simple approach: Iterate through buffer and set velocities
    uint8_t index = buffer_tail;
    uint8_t count = MotionBuffer_GetCount();
    
    for (uint8_t i = 0; i < count; i++) {
        motion_block_t* block = &motion_buffer[index];
        
        if (block->recalculate_flag) {
            // For now, use nominal feedrate
            // TODO: Implement junction velocity calculation
            block->entry_velocity = block->feedrate;
            block->exit_velocity = block->feedrate;
            block->recalculate_flag = false;
        }
        
        // Move to next block
        index = (index + 1) % MOTION_BUFFER_SIZE;
    }
}

//=============================================================================
// DEBUGGING
//=============================================================================

void MotionBuffer_GetStats(uint8_t* head, uint8_t* tail, uint8_t* count) {
    if (head) *head = buffer_head;
    if (tail) *tail = buffer_tail;
    if (count) *count = MotionBuffer_GetCount();
}

void MotionBuffer_DumpBuffer(void) {
    // TODO: Implement debug output
    // This would print buffer contents to UART for debugging
    // Format: [index] steps=[X,Y,Z,A] feedrate=### entry_v=### exit_v=###
}
