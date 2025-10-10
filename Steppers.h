#ifndef STEPPERS_H
#define STEPPERS_H


#include "Pins.h"
#include "Timers.h"
#include "Serial_Dma.h"
#include "Nuts_Bolts.h"
#include "built_in.h"


#define MAXFEED 180
<<<<<<< HEAD
void Init_Steppers();
void delay();
=======

//static void delay();
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
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