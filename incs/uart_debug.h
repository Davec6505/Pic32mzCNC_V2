/*******************************************************************************
  UART2 Debug Test for Clicker2 Hardware

  File Name:
    uart_debug.h

  Summary:
    Debug functions to test UART2 communication on Clicker2 board

  Description:
    Simple test functions to verify UART2 functionality and identify
    communication issues with the hardware.
*******************************************************************************/

#ifndef UART_DEBUG_H
#define UART_DEBUG_H

#include <stdint.h>
#include <stdbool.h>

// Function prototypes
void UART_DEBUG_Initialize(void);
void UART_DEBUG_SendString(const char* str);
void UART_DEBUG_SendCharacter(char c);
bool UART_DEBUG_IsTransmitReady(void);
bool UART_DEBUG_IsReceiveReady(void);
char UART_DEBUG_ReadCharacter(void);
void UART_DEBUG_TestLoop(void);
void UART_DEBUG_PrintSystemInfo(void);

// Test functions
void UART_DEBUG_BasicTest(void);
void UART_DEBUG_EchoTest(void);
void UART_DEBUG_BaudRateTest(void);
void UART_DEBUG_TestMultipleBaudRates(void);

#endif /* UART_DEBUG_H */