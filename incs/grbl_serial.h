// grbl_serial.h

#ifndef GRBL_SERIAL_H
#define GRBL_SERIAL_H

#include <stdbool.h>

void GRBL_Serial_Initialize(void);
void GRBL_Process_Char(char c);
void process_line(const char *line);
void handle_real_time_character(char c);

void GRBL_RegisterMotionCallback(void (*callback)(const char *));
void GRBL_RegisterStatusCallback(void (*callback)(void));
void GRBL_RegisterEmergencyCallback(void (*callback)(void));

#endif // GRBL_SERIAL_H
