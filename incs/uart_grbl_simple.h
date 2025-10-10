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

#endif // UART_GRBL_SIMPLE_H