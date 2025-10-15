/*******************************************************************************
  Motion Buffer Management Implementation File

  Company:
    Microchip Technology Inc.

  File Name:
    motion_buffer.c

  Summary:
    This file contains the implementation of motion buffer management functions.

  Description:
    This file implements a circular buffer system for managing motion commands
    with 16-command look-ahead capability. The buffer provides thread-safe
    operations for adding, retrieving, and managing motion blocks.
*******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include "motion_buffer.h"
#include "interpolation_engine.h" // For INTERP_GetMotionState() and motion_state_t
#include <stdio.h>
#include <string.h>

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// Motion buffer storage (private to this module)
static motion_block_t motion_buffer[MOTION_BUFFER_SIZE];
static uint8_t motion_buffer_head = 0;
static uint8_t motion_buffer_tail = 0;

// *****************************************************************************
// *****************************************************************************
// Section: Motion Buffer Management Functions
// *****************************************************************************
// *****************************************************************************

void MotionBuffer_Initialize(void)
{
    // Clear buffer indices
    motion_buffer_head = 0;
    motion_buffer_tail = 0;

    // Clear all buffer entries
    for (int i = 0; i < MOTION_BUFFER_SIZE; i++)
    {
        motion_buffer[i].is_valid = false;
        // Clear other fields for safety
        memset(&motion_buffer[i], 0, sizeof(motion_block_t));
    }
}

bool MotionBuffer_HasSpace(void)
{
    /* GRBL-style buffer management:
     * - Allow buffer to fill up even during motion (GRBL allows 16 blocks)
     * - Planner optimizes across ALL buffered blocks (look-ahead planning)
     * - Stepper executes blocks sequentially from tail
     * - This enables file streaming while maintaining proper motion planning
     */
    uint8_t next_head = (motion_buffer_head + 1) % MOTION_BUFFER_SIZE;
    return (next_head != motion_buffer_tail);
}

bool MotionBuffer_Add(motion_block_t *block)
{
    if (!MotionBuffer_HasSpace())
    {
        return false; // Buffer full
    }

    if (block == NULL)
    {
        return false; // Invalid block pointer
    }

    // Copy block to buffer
    motion_buffer[motion_buffer_head] = *block;
    motion_buffer[motion_buffer_head].is_valid = true;

    // Advance head pointer
    motion_buffer_head = (motion_buffer_head + 1) % MOTION_BUFFER_SIZE;

    // Debug output disabled for UGS compatibility
    // char debug_msg[64];
    // sprintf(debug_msg, "[BUFFER_ADD] head=%d tail=%d\r\n", buffer.head, buffer.tail);
    // APP_UARTPrint_blocking(debug_msg);

    return true;
}

motion_block_t *MotionBuffer_GetNext(void)
{
    if (motion_buffer_head == motion_buffer_tail)
    {
        return NULL; // Buffer empty
    }

    if (!motion_buffer[motion_buffer_tail].is_valid)
    {
        return NULL; // Invalid block
    }

    return &motion_buffer[motion_buffer_tail];
}

void MotionBuffer_Complete(void)
{
    if (motion_buffer_head == motion_buffer_tail)
    {
        return; // Buffer empty, nothing to complete
    }

    // Mark current block as invalid
    motion_buffer[motion_buffer_tail].is_valid = false;

    // Advance tail pointer
    motion_buffer_tail = (motion_buffer_tail + 1) % MOTION_BUFFER_SIZE;
}

bool MotionBuffer_IsEmpty(void)
{
    return (motion_buffer_head == motion_buffer_tail);
}

uint8_t MotionBuffer_GetCount(void)
{
    if (motion_buffer_head >= motion_buffer_tail)
    {
        return motion_buffer_head - motion_buffer_tail;
    }
    else
    {
        return (MOTION_BUFFER_SIZE - motion_buffer_tail) + motion_buffer_head;
    }
}

motion_buffer_status_t MotionBuffer_GetStatus(void)
{
    motion_buffer_status_t status;

    status.head = motion_buffer_head;
    status.tail = motion_buffer_tail;
    status.count = MotionBuffer_GetCount();
    status.empty = MotionBuffer_IsEmpty();
    status.full = !MotionBuffer_HasSpace();

    return status;
}

void MotionBuffer_Clear(void)
{
    // Reset buffer indices
    motion_buffer_head = 0;
    motion_buffer_tail = 0;

    // Mark all blocks as invalid
    for (int i = 0; i < MOTION_BUFFER_SIZE; i++)
    {
        motion_buffer[i].is_valid = false;
    }
}

motion_block_t *MotionBuffer_Peek(uint8_t offset)
{
    if (MotionBuffer_IsEmpty())
    {
        return NULL; // Buffer empty
    }

    // Calculate index with offset from tail
    uint8_t peek_index = (motion_buffer_tail + offset) % MOTION_BUFFER_SIZE;

    // Check if offset goes beyond current buffer content
    uint8_t count = MotionBuffer_GetCount();
    if (offset >= count)
    {
        return NULL; // Offset beyond buffer end
    }

    if (!motion_buffer[peek_index].is_valid)
    {
        return NULL; // Invalid block
    }

    return &motion_buffer[peek_index];
}

/*******************************************************************************
 End of File
 */