#ifndef _APP_H
#define _APP_H

#include <stdint.h>
#include <stdbool.h>
#include "definitions.h"

// *****************************************************************************
// Application States
// *****************************************************************************

typedef enum
{
    APP_STATE_INIT,
    APP_STATE_SERVICE_TASKS,
} APP_STATES;

// *****************************************************************************
// Application Data
// *****************************************************************************

typedef struct
{
    APP_STATES state;
} APP_DATA;

// Global application data
extern APP_DATA appData;

// *****************************************************************************
// Application API
// *****************************************************************************

void APP_Initialize(void);
void APP_Tasks(void);

#endif /* _APP_H */