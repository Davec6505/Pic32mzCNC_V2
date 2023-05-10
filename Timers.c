#include "Timers.h"

void (*Clock)();
<<<<<<< HEAD
void (*Dly)();
=======
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345

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


///////////////////////////////////////////////////////////////////
//TMR 8  initialized to interrupt at 1us was used for early
<<<<<<< HEAD
void InitTimer8(void (*dly)()){
  Dly = dly;
  T8CON            = 0x8050;
=======
void InitTimer8(){
  T8CON            = 0x8000;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
  T8IP0_bit        = 1;
  T8IP1_bit        = 0;
  T8IP2_bit        = 1;
  T8IS0_bit        = 0;
  T8IS1_bit        = 1;
  T8IF_bit         = 0;
  T8IE_bit         = 1;
<<<<<<< HEAD
  PR8              = 50000;
=======
  PR8              = 500;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
  TMR8             = 0;
  uSec             = 0;
}


///////////////////////////////////////////
//TMR 1 as a 10ms clock pulse ???
void Timer1Interrupt() iv IVT_TIMER_1 ilevel 6 ics ICS_SRS {
  T1IF_bit  = 0;
  //Enter your code here
  Clock();
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

void void Timer8Interrupt() iv IVT_TIMER_8 ilevel 5 ics ICS_SRS {
 T8IF_bit  = 0;
//Enter your code here
//oneShot to start the steppers runnin
<<<<<<< HEAD
  Dly();
=======
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
  uSec++;
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