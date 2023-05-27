#include "Timers.h"

void (*Clock)();
void (*Dly)();

struct Timer TMR;

volatile static long uSec;
static unsigned int ms100, ms300, ms500, ms800, sec1, sec1_5, sec2;

 /////////////////////////////////////////////////////////////////
//TMR 1 setup for 1us pusles as a dummy axis for single pulse to
//keep the step equivilant to Bres algo dual axis.
void InitTimer1(){
//Clock pulses 100ms 500ms 800ms 1sec
  Clock = ClockPulse;
//TMR1 setup to 10ms clock
  T1CON         = 0x8010;
  //PRIORITY 6 SUB-PRIORTY 2
  IPC1SET       = 0x1A;
  //SET IE FLAG
  IEC0       |= 1<<4;
  //CLEAR IF FLAG
  IFS0       |= ~(1<<4);

  PR1           = 62500;
  TMR1          = 0;
  
}

//Timer2  dummy Timer
//Prescaler 1:1; PR3 Preload = 50; Actual Interrupt Time = 1 us
void InitTimer2(){
  T2CON         = 0x8000;
  T2IP0_bit     = 1;
  T2IP1_bit     = 0;
  T2IP2_bit     = 1;
  T2IS0_bit     = 0;
  T2IS1_bit     = 0;

  T2IF_bit      = 0;
  T2IE_bit      = 1;
  PR2           = 50;
  TMR2         = 0;
}
///////////////////////////////////////////////////////////////////
//TMR 8 initialized to interrupt at intervals after cslculated vslue
void InitTimer8(void (*dly)()){
  Dly = dly;
  T8CON            = 0x0050;
  T8IP0_bit        = 1;
  T8IP1_bit        = 0;
  T8IP2_bit        = 1;
  T8IS0_bit        = 0;
  T8IS1_bit        = 1;
  T8IF_bit         = 0;
  T8IE_bit         = 1;
  PR8              = 50000;
  TMR8             = 0;
  uSec             = 0;
}

//Timer9  to reset output once pulsed
//Prescaler 1:16; PR3 Preload = 500; Actual Interrupt Time = 50 us
void InitTimer9(){
  T9CON         = 0x0050;
  T9IP0_bit     = 1;
  T9IP1_bit     = 0;
  T9IP2_bit     = 1;
  T9IS0_bit     = 1;
  T9IS1_bit     = 0;
  
  T9IF_bit      = 0;
  T9IE_bit      = 1;
  PR9           = 5000;
  TMR9          = 0;
}


///////////////////////////////////////////
//TMR 1 as a 10ms clock pulse ???
void Timer1Interrupt() iv IVT_TIMER_1 ilevel 6 ics ICS_SRS {
  T1IF_bit  = 0;
  //Enter your code here
  Clock();
}

///////////////////////////////////////////
//TMR 1 as a 10ms clock pulse ???
void Timer2Interrupt() iv IVT_TIMER_2 ilevel 5 ics ICS_OFF {
  T2IF_bit  = 0;
  //Enter your code here
  uSec++;
}

//////////////////////////////////////////
//Tmp8 vars getter and setters
long getUsec(){
   return uSec;
}

long setUsec(long usec){
  uSec = usec;
  return uSec;
}

//////////////////////////////////////////
// TMR 8 interrupts
void SetPR8Value(unsigned int value){
 PR8  = value;
 TMR8 = 0;
 T8IF_bit = false;
}
void RestartTmr8(){
  T8CONbits.TON = true;
  T8IF_bit      = false;
  T8IE_bit      = true;
  PR8           = 50000;
  TMR8          = 0;
}

void StopTmr8(){
  T8CONbits.TON = false;
}

void Timer8Interrupt() iv IVT_TIMER_8 ilevel 5 ics ICS_SRS {
 T8IF_bit  = false;
 //Enter your code here
 Dly();
}

//////////////////////////////////////////
// TMR 9 interrupts OUTPUT resets
void RestartTmr9(){
  T9CONbits.TON = true;
  T9IF_bit      = 0;
  T9IE_bit      = 1;
  PR9           = 5000;
  TMR9          = 0;
}

void StopTmr9(){
  T9CONbits.TON = false;
}

void Timer9Interrupt() iv IVT_TIMER_9 ilevel 5 ics ICS_SRS {
 T9IF_bit  = false;
 //Enter your code here
 StopTmr9();//T9IE_bit  = false;
 LED2      = false;
}

//////////////////////////////////////////
//Do Clock pulses
static void ClockPulse(){
 ms100++;
 ms300++;
 ms500++;
 sec1++;
 
   TMR.clock.B0 = !TMR.clock.B0;
   if(ms100 > 9){
      ms100 = 0;
      TMR.clock.B1 = !TMR.clock.B1;
   }
   if(ms300 > 29){
      ms300 = 0;
      TMR.clock.B2 = !TMR.clock.B2;
   }
   if(ms500 > 49){
      ms500 = 0;
      TMR.clock.B3 = !TMR.clock.B3;
   }
   if(sec1 > 99){
      sec1 = 0;
      TMR.clock.B4 = !TMR.clock.B4;
   }

}