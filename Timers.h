#ifndef TIMERS_H
#define TIMERS_H


#include "Config.h"
#include "built_in.h"
//#include "Stepper.h"
////////////////////////////////////////////////////
//STRUCTS and ENUMS

struct Timer{
char clock;
char P1: 1;
char P2: 1;
unsigned int disable_cnt;
};
extern struct Timer TMR;


void InitTimer1();
void InitTimer2();//dummy timer temp
void InitTimer8(void (*dly)());
void SetPR8Value(unsigned int value);
void RestartTmr8();
void StopTmr8();
void InitTimer9();
void RestartTmr9();
void StopTmr9();
long getUsec();
long setUsec(long usec);
static void ClockPulse();
unsigned int ResetSteppers(unsigned int sec_to_disable,unsigned int last_sec_to_disable);


#endif