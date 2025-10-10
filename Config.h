#ifndef CONFIG_H
#define CONFIG_H

#include "Pins.h"
#include "Timers.h"
#include "Serial_Dma.h"
#include "Nuts_Bolts.h"
#include "Steppers.h"
#include "built_in.h"
///////////////////////////////////////////////////
//DEFINES
<<<<<<< HEAD
//UART Module for clicker2 mz
//#define UART1_TEST
//#define UART2_TEST

#define UART1_
//#define UART2_
//#define UART3_
=======
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345

//blockers
#define LED_STATUS

//types
#define false 0
<<<<<<< HEAD
#define true  1
=======
#define true 1
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345



////////////////////////////////////////////////////
//constants
extern unsigned char LCD_01_ADDRESS; //PCF8574T

////////////////////////////////////////////////////
//STRUCTS and ENUMS


////////////////////////////////////////////////////
//function prototypes
void PinMode();             //pin mode configuration
void UartConfig();          //setupUart
void set_performance_mode();//sys clk performance setup
<<<<<<< HEAD
void Uart1InterruptSetup(); //uart1 interrupt on recieve turned off
void Uart2InterruptSetup(); //uart2 interrupt on recieve turned off
=======
void Uart2InterruptSetup(); //uart2 interrupt on recieve turned off
//void LcdI2CConfig();      //configure the i2c_lcd 4line 16ch display
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
void OutPutPulseXYZ();      // setup output pulse OC3

//Group 1 G4,G10,G28,G30,G53,G92,G92.1] Non-modal
static int Modal_Group_Actions0(int action);

//Group 2 [G0,G1,G2,G3,G80] Motion
static int Modal_Group_Actions1(int action);

//[M0,M1,M2,M30] Stopping
static int Modal_Group_Actions4(int action);

//[M3,M4,M5] Spindle turning
static int Modal_Group_Actions7(int action);

//[G54...] Coordinate system selection
static int Modal_Group_Actions12(int action);



#endif