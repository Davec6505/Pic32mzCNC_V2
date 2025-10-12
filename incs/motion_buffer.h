/*******************************************************************************
  Motion Buffer Management Header File

  Company:
    Microchip Technology Inc.

  File Name:
    motion_buffer.h

  Summary:
    This header file provides the prototypes and definitions for motion buffer
    management functionality.

  Description:
    This file declares the functions and data structures used for managing a
    16-command look-ahead motion buffer system. The buffer provides:
    - Circular buffer management for motion commands
    - Thread-safe buffer operations
    - Motion block storage and retrieval
    - Buffer state monitoring (full/empty/space available)
*******************************************************************************/

#ifndef MOTION_BUFFER_H
#define MOTION_BUFFER_H

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************
#include <stdint.h>
#include <stdbool.h>
#include "app.h" // For motion_block_t definition

// *****************************************************************************
// *****************************************************************************
// Section: Constants
// *****************************************************************************
// *****************************************************************************

// Buffer size is defined in app.h as MOTION_BUFFER_SIZE (16)

// *****************************************************************************
// *****************************************************************************
// Section: Data Types
// *****************************************************************************
// *****************************************************************************

// Buffer status information
typedef struct
{
    uint8_t count; // Number of blocks currently in buffer
    uint8_t head;  // Buffer head index
    uint8_t tail;  // Buffer tail index
    bool full;     // Buffer full flag
    bool empty;    // Buffer empty flag
} motion_buffer_status_t;

// *****************************************************************************
// *****************************************************************************
// Section: Interface Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void MotionBuffer_Initialize(void)

  Summary:
    Initializes the motion buffer system.

  Description:
    This function initializes the motion buffer by clearing all entries and
    resetting head/tail pointers to zero. Should be called during system
    initialization.

  Parameters:
    None

  Returns:
    None
*******************************************************************************/
void MotionBuffer_Initialize(void);

/*******************************************************************************
  Function:
    bool MotionBuffer_HasSpace(void)

  Summary:
    Checks if the motion buffer has space for a new block.

  Description:
    This function checks whether there is space in the circular buffer for
    adding a new motion block.

  Parameters:
    None

  Returns:
    true - Buffer has space for a new block
    false - Buffer is full
*******************************************************************************/
bool MotionBuffer_HasSpace(void);

/*******************************************************************************
  Function:
    bool MotionBuffer_Add(motion_block_t *block)

  Summary:
    Adds a motion block to the buffer.

  Description:
    This function adds a new motion block to the circular buffer if space is
    available. The block is copied into the buffer.

  Parameters:
    block - Pointer to the motion block to add

  Returns:
    true - Block successfully added
    false - Buffer full, block not added
*******************************************************************************/
bool MotionBuffer_Add(motion_block_t *block);

/*******************************************************************************
  Function:
    motion_block_t* MotionBuffer_GetNext(void)

  Summary:
    Gets a pointer to the next motion block to execute.

  Description:
    This function returns a pointer to the next motion block in the buffer
    without removing it. Returns NULL if buffer is empty.

  Parameters:
    None

  Returns:
    Pointer to next motion block, or NULL if buffer is empty
*******************************************************************************/
motion_block_t *MotionBuffer_GetNext(void);

/*******************************************************************************
  Function:
    void MotionBuffer_Complete(void)

  Summary:
    Marks the current motion block as complete and advances the buffer.

  Description:
    This function marks the current motion block as invalid and advances the
    tail pointer to the next block. Should be called when motion execution
    is complete.

  Parameters:
    None

  Returns:
    None
*******************************************************************************/
void MotionBuffer_Complete(void);

/*******************************************************************************
  Function:
    bool MotionBuffer_IsEmpty(void)

  Summary:
    Checks if the motion buffer is empty.

  Description:
    This function checks whether the motion buffer contains any valid blocks.

  Parameters:
    None

  Returns:
    true - Buffer is empty
    false - Buffer contains blocks
*******************************************************************************/
bool MotionBuffer_IsEmpty(void);

/*******************************************************************************
  Function:
    uint8_t MotionBuffer_GetCount(void)

  Summary:
    Gets the number of blocks currently in the buffer.

  Description:
    This function returns the current number of valid motion blocks in the
    circular buffer.

  Parameters:
    None

  Returns:
    Number of blocks in buffer (0 to MOTION_BUFFER_SIZE-1)
*******************************************************************************/
uint8_t MotionBuffer_GetCount(void);

/*******************************************************************************
  Function:
    motion_buffer_status_t MotionBuffer_GetStatus(void)

  Summary:
    Gets detailed status information about the motion buffer.

  Description:
    This function returns comprehensive status information about the motion
    buffer including count, indices, and flags.

  Parameters:
    None

  Returns:
    Buffer status structure
*******************************************************************************/
motion_buffer_status_t MotionBuffer_GetStatus(void);

/*******************************************************************************
  Function:
    void MotionBuffer_Clear(void)

  Summary:
    Clears all motion blocks from the buffer.

  Description:
    This function clears the motion buffer by marking all blocks as invalid
    and resetting head/tail pointers. Used for emergency stops or system reset.

  Parameters:
    None

  Returns:
    None
*******************************************************************************/
void MotionBuffer_Clear(void);

/*******************************************************************************
  Function:
    motion_block_t* MotionBuffer_Peek(uint8_t offset)

  Summary:
    Peeks at a motion block at a specific offset without removing it.

  Description:
    This function allows looking ahead in the buffer at blocks beyond the
    current one. Useful for velocity optimization and look-ahead planning.

  Parameters:
    offset - Number of blocks ahead to peek (0 = current, 1 = next, etc.)

  Returns:
    Pointer to motion block at offset, or NULL if offset is beyond buffer end
*******************************************************************************/
motion_block_t *MotionBuffer_Peek(uint8_t offset);

#endif /* MOTION_BUFFER_H */