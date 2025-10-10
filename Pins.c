#include "Pins.h"


//////////////////////////////////////////////////////////
//                       clicker2                       //
//////////////////////////////////////////////////////////

//////////////////////////////////////
//ONBOARD SWITCHES AND LEDS
 sbit LED2 at LATA9_bit;
 sbit LED2_Dir at TRISA9_bit;
 sbit LED1 at LATE7_bit;
 sbit LED1_Dir at TRISE7_bit;

 sbit SW1 at RC3_bit;
 sbit SW1_Dir at TRISC3_bit;
 sbit SW2 at RB0_bit;
 sbit SW2_Dir at TRISB0_bit;
 
 
 //////////////////////////////////
//PIN OUT FOR X Y Z A PINS

sbit EN_StepX at LATG14_bit;
sbit EN_Step_PinDirX at TRISG14_bit;
sbit DIR_StepX at LATG12_bit;
sbit DIR_Step_PinDirX at TRISG12_bit;
//sbit RST_StepX at LATA10_bit;
//sbit RST_Step_PinDirX at TRISA10_bit;
//sbit SLP_FLT_StepX at LATB15_bit;
//sbit SLP_FLT_Step_PinDirX at TRISB15_bit;
sbit PLS_StepX at LATD4_bit;
sbit PLS_Step_PinDirX at TRISD4_bit;


sbit EN_StepY at LATG9_bit;
sbit EN_Step_PinDirY at TRISG9_bit;
sbit DIR_StepY at LATE2_bit;
sbit DIR_Step_PinDirY at TRISE2_bit;
//sbit RST_StepX at LATA10_bit;
//sbit RST_Step_PinDirX at TRISA10_bit;
//sbit SLP_FLT_StepY at LATB10_bit;
//sbit SLP_FLT_Step_PinDirY at TRISB10_bit;
sbit PLS_StepY at LATD5_bit;
sbit PLS_Step_PinDirY at TRISD5_bit;


sbit EN_StepZ at LATE4_bit;
sbit EN_Step_PinDirZ at TRISE4_bit;
sbit DIR_StepZ at LATG15_bit;
sbit DIR_Step_PinDirZ at TRISG15_bit;
//sbit RST_StepX at LATA10_bit;
//sbit RST_Step_PinDirX at TRISA10_bit;
//sbit SLP_FLT_StepY at LATB10_bit;
//sbit SLP_FLT_Step_PinDirY at TRISB10_bit;
sbit PLS_StepZ at LATF0_bit;
sbit PLS_Step_PinDirZ at TRISF0_bit;


sbit EN_StepA at LATA5_bit;
sbit EN_Step_PinDirA at TRISA5_bit;
sbit DIR_StepA at LATE5_bit;
sbit DIR_Step_PinDirA at TRISE5_bit;
//sbit RST_StepX at LATA10_bit;
//sbit RST_Step_PinDirX at TRISA10_bit;
//sbit SLP_FLT_StepY at LATB10_bit;
//sbit SLP_FLT_Step_PinDirY at TRISB10_bit;
sbit PLS_StepA at LATF1_bit;
sbit PLS_Step_PinDirA at TRISF1_bit;

//////////////////////////////////////////
//Pinouts for Limits
sbit X_Min_Limit at RF3_bit;
sbit X_Min_Limit_Dir at TRISF3_bit;
sbit Y_Min_Limit at RB15_bit;
sbit Y_Min_Limit_Dir at TRISB15_bit;
sbit Z_Min_Limit at RB1_bit;
sbit Z_Min_Limit_Dir at TRISB1_bit;