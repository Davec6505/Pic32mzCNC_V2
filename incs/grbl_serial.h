// grbl_serial.h

#ifndef GRBL_SERIAL_H
#define GRBL_SERIAL_H

#include <stdint.h>
#include <stdbool.h>

// --- Type Definitions for GRBL Callbacks ---
typedef void (*grbl_write_callback_t)(const char *message);
typedef void (*grbl_motion_callback_t)(const char *line);
typedef void (*grbl_status_callback_t)(void);
typedef void (*grbl_emergency_callback_t)(void);

// --- Public Function Prototypes ---

void GRBL_Serial_Initialize(void);
void GRBL_Tasks(void);

// Functions to register callbacks
void GRBL_RegisterWriteCallback(grbl_write_callback_t callback);
void GRBL_RegisterMotionCallback(grbl_motion_callback_t callback);
void GRBL_RegisterStatusCallback(grbl_status_callback_t callback);
void GRBL_RegisterEmergencyCallback(grbl_emergency_callback_t callback);

void grbl_send_response(const char *message);

#endif // GRBL_SERIAL_H
