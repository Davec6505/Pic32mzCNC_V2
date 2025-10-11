/*******************************************************************************
  Simple UART GRBL Header - Port of MikroC Serial_Dma.h logic
*******************************************************************************/

#ifndef UART_GRBL_SIMPLE_H
#define UART_GRBL_SIMPLE_H

#include <stdint.h>
#include <stdbool.h>

// Function prototypes
void UART_GRBL_Initialize(void);
void UART_GRBL_Tasks(void);
void UART_GRBL_ResetForNextConnection(void); // Reset DMA pattern for UGS reconnection

// Additional utility functions
void UART_GRBL_ManualReset(void); // Manual reset (same as ResetForNextConnection)

#endif // UART_GRBL_SIMPLE_H