#ifndef STEPPERS_H
#define STEPPERS_H


#include "Pins.h"
#include "Timers.h"
#include "Serial_Dma.h"
#include "Nuts_Bolts.h"
#include "built_in.h"


#define MAXFEED 180

//static void delay();
void setStepXY(int _x1,int _y1,int _x3,int _y3);
void setDragOil(int _feedrate,int _drag,int _oil);
void doline();
/*
void getdir();
void delay();
void setdirection();
void doline();
*/

#endif