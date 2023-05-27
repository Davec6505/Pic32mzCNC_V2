/**************************************************************
* Author: D Coetzee
* Date: 05/05/2023
* Subject: Investigate XY_Interpolation as per   
* ac:Pic32Timers                                 
* ac:XY_Interpolation
* ac:Pic32mz
*
**************************************************************/


#include "Config.h"




void main() {
static bit m0;
 PinMode();
 EI();
 m0 = false;
 setDragOil(100,1);
 setFeedrate(180);
 while(1){
 //code execution confirmation led on clicker2 board
  #ifdef LED_STATUS
  LED1 = TMR.clock >> 4;
  #endif
  if(!SW1 & !m0){
     m0 = true;
     setStepXY(0,0,50,20);
     doline();
  }
  if (SW1 & m0)
      m0 = false;
 }
}