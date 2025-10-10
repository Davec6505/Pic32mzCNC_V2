/*******************************************************************************
  Direct Port of MikroC Serial_Dma.c - Your proven working implementation

  File Name:
    gcode_parser_dma.h

  Summary:
    Header for uart_grbl_simple.c - Direct port of your MikroC code

  Description:
    This header provides the interface for uart_grbl_simple.c which is a
    direct port of your proven MikroC Serial_Dma.c implementation.
    No Harmony complexity, just your working DMA pattern matching logic.
*******************************************************************************/

#ifndef GCODE_PARSER_DMA_H
#define GCODE_PARSER_DMA_H

#include <stdbool.h>

// *****************************************************************************
// *****************************************************************************
// Section: Interface Functions for compatibility with existing app.c code
// *****************************************************************************
// *****************************************************************************

// Initialize the UART GRBL system
void UART_GRBL_Initialize(void);

// Main task function - call this repeatedly in your main loop
void UART_GRBL_Tasks(void);

// Compatibility functions for existing app.c code
bool GCODE_DMA_Initialize(void);
void GCODE_DMA_RegisterMotionCallback(void (*callback)(const char*));
void GCODE_DMA_RegisterStatusCallback(void (*callback)(void));
void GCODE_DMA_RegisterEmergencyCallback(void (*callback)(void));
void GCODE_DMA_Enable(void);
void GCODE_DMA_SendOK(void);
void GCODE_DMA_SendError(int errorCode);
void GCODE_DMA_SendResponse(const char* response);
int GCODE_DMA_GetCommandCount(void);

#endif // GCODE_PARSER_DMA_H