#include "Config.h"

void PinMode(){
////////////////////////////////////////////////
//Start by disabling global interrupts
     DI();

////////////////////////////////////////////////
//Unlock KSEG to write to SFR's
     SYSKEY = 0xAA996655;
     SYSKEY = 0x556699AA;
     CFGCONbits.OCACLK = 1;
     SYSKEY = 0x33333333;

////////////////////////////////////////////////////
//Turn off the JTAG Debug big
     JTAGEN_bit = 0;
     Delay_ms(100);

////////////////////////////////////////////////////
//Turn off all analogs
     ANSELA = 0X0000;
     TRISA  = 0X0000;
     ANSELB = 0X0000;
     TRISB  = 0X0000;
     ANSELC = 0X0000;
     TRISC  = 0X0000;
     ANSELD = 0X0000;
     TRISD  = 0X0000;
     ANSELE = 0X0000;
     TRISE  = 0X0000;
     ANSELG = 0X0000;
     TRISG  = 0X0000;

     CNPUB  = 0;//1 << 15;
     CNPUF  = 0;//1 << 3;
////////////////////////////////////////////////////
//outputs
    LED1_Dir = 0;
    LED2_Dir = 0;

////////////////////////////////////////////////////
//INPUTS
    SW1_Dir = 1;
    SW2_Dir = 1;

    TRISG7_bit = 1;
    TRISG8_bit = 1;
///////////////////////////////////////////////////
//setup the performance of the sys clocks
    set_performance_mode();

////////////////////////////////////////////////////
//Remapping of Uart 2 pins
    Unlock_IOLOCK();
     //UART2 PIN MAPPING
     PPS_Mapping_NoLock(_RPE8, _OUTPUT, _U2TX);    // Sets pin PORTE.B8 to be Output and maps UART2 Transmit
     PPS_Mapping_NoLock(_RPE9, _INPUT,  _U2RX);    // Sets pin PORTE.B9 to be Input and maps UART2 Receive
  //UART3 PIN MAPPING  FOR LOOPBACK DEBUGGING
     PPS_Mapping_NoLock(_RPA14, _OUTPUT, _U3TX);    // Sets pin PORTE.B8 to be Output and maps UART2 Transmit
     PPS_Mapping_NoLock(_RPF5, _INPUT,  _U3RX);    // Sets pin PORTE.B9 to be Input and maps UART2 Receive
     //???
     PPS_Mapping_NoLock(_RPB9, _OUTPUT, _NULL);
     PPS_Mapping_NoLock(_RPB10, _OUTPUT, _NULL);
     ///////////////////////////////////////////////////////////
     //OUTPUT PULSES TO STEPPERS
     #ifdef OCMODULE
     PPS_Mapping_NoLock(_RPD4, _OUTPUT, _OC5);     //X_Axis TMR2
     PPS_Mapping_NoLock(_RPD5, _OUTPUT, _OC2);     //Y_Axis TMR4
     PPS_Mapping_NoLock(_RPF0, _OUTPUT, _OC7);     //Z_Axis TMR6
     PPS_Mapping_NoLock(_RPF1, _OUTPUT, _OC3);     //A_Axis TMR5
     PPS_Mapping_NoLock(_RPG1, _OUTPUT, _OC6);     //B_Axis TMR3
     PPS_Mapping_NoLock(_RPE3, _OUTPUT, _OC8);     //C_Axis TMR7
     #endif
     //////////////////////////////////////////////////////////
     //lIMIT SWITCHES
     PPS_Mapping_NoLock(_RPF3, _INPUT, _INT1);     //X_Min_Limit
     PPS_Mapping_NoLock(_RPB15, _INPUT, _INT2);    //Y_Min_Limit
     PPS_Mapping_NoLock(_RPB1, _INPUT, _INT4);    //Z_Min_Limit
    Lock_IOLOCK();

//////////////////////////////////////////////////
//Memory manager initialize
   MM_Init();

//////////////////////////////////////////////////
//TMR 1 & 8 config
   InitTimer1();
  // InitTimer8();

///////////////////////////////////////////////
//Limits initialize
//  Limit_Initialize();
 Init_Steppers();
////////////////////////////////////////////////
//set up output compare module for oc3 RF1 pin
 // OutPutPulseXYZ();
 // SetPinMode();

////////////////////////////////////////////////
//Setup platform
//  SetInitialSizes(STPS);

 ////////////////////////////////////////////////
//DMA CONFIG

  DMA_global();
  DMA0_Enable();
  //DMA1_Enable();

//////////////////////////////////////////////////
//configure the uart2 module
 UartConfig();

//////////////////////////////////////////////////
//configure UART the interrupts
 // Uart2InterruptSetup();

//////////////////////////////////////////////////
//Enable the interrupts here so uart can report back
 EnableInterrupts();
///////////////////////////////////////////////////
//init all default settings
 //settings_init(0);
 //settings_init(1);
 //settings_read_coord_data();
}

void UartConfig(){
//////////////////////////////////////////////////
//setup the serial comms on uart 2  using PBCLK2 at 50mhz
  UART2_Init_Advanced(115200, 200000/*PBClk x 2*/, _UART_LOW_SPEED, _UART_8BIT_NOPARITY, _UART_ONE_STOPBIT);
  UART_Set_Active(&UART2_Read, &UART2_Write, &UART2_Data_Ready, &UART2_Tx_Idle); // set UART2 active
  Delay_ms(10);                  // Wait for UART module to stabilize

#if LoopBackDebug == 1
 //////////////////////////////////////////////////
//setup the serial comms on uart 2  using PBCLK2 at 50mhz
  UART3_Init_Advanced(256000, 200000/*PBClk / 8*/, _UART_LOW_SPEED, _UART_8BIT_NOPARITY, _UART_ONE_STOPBIT);
  UART_Set_Active(&UART3_Read, &UART3_Write, &UART3_Data_Ready, &UART3_Tx_Idle); // set UART2 active
  Delay_ms(100);                  // Wait for UART module to stabilize

 // Uart2InterruptSetup();

#endif


}

////////////////////////////////////////////////
//Uart 2 interrupt setup, make sure that for DMA
//the interrupt is turned off for this module,
//only use the IRQ from the DMA controller but
//itis important it set up the irelx bits of the
// 8 level deep interrupt buffer specific to the
//UART module
void Uart2InterruptSetup(){
/*IFS4<18>
IEC4<18>
IPC36<20:18>
IPC36<17:16>
*/
    // IRQ after 1 byte is empty, buffer is 8 deep
    URXISEL0_bit = 0;
    URXISEL1_bit = 0;

    // IRQ after 1 byte is empty buffer is 8 deep
    UTXISEL0_bit = 0;
    UTXISEL1_bit = 0;

    IPC36CLR     = 0x160000;
    //Set priority 5 sub-priority 1
    IPC36SET      = 0x00140000;
    //set DMA0IE bit
    IEC4SET       = 0x40000;
    IFS4CLR       = 0x40000;

}

void set_performance_mode(){
unsigned long cp0;
////////////////////////////////////////////////
//setup the clks performance for all periphials
    DI(); // Disable all interrupts

   //Enable multi vector, and set TPC<2:0> to <= 4
   //MULTI Vect, TPC & EDGE Detect refer to
   // ac:Interrupts  app note.
   INTCONSET = 0x00001400;
    // Unlock Sequence
    SYSKEY = 0xAA996655;
    SYSKEY = 0x556699AA;

    // Peripheral Bus 1 cannot be turned off, so there's no need to turn it on
    PB1DIVbits.PBDIV = 1; // Peripheral Bus 1 Clock Divisor Control (PBCLK1 is SYSCLK divided by 2)

    // PB2DIV  UART / SPI / I2C / PMP
    UEN0_bit = 1;
    UEN1_bit = 1;
    PB2DIVbits.ON = 1; // Peripheral Bus 2 Output Clock Enable (Output clock is enabled)
    while(!PB2DIVbits.PBDIVRDY);
    PB2DIVbits.PBDIV = 0x07; // Peripheral Bus 2 Clock Divisor Control (PBCLK2 is SYSCLK "200MHZ" / 8)

    // PB3DIV   TIMERS
    PB3DIVbits.ON = 1; // Peripheral Bus 2 Output Clock Enable (Output clock is enabled)
    while(!PB3DIVbits.PBDIVRDY);
    PB3DIVbits.PBDIV = 3; // Peripheral Bus 3 Clock Divisor Control (PBCLK3 is SYSCLK divided by 4)

    // PB4DIV
    PB4DIVbits.ON = 1; // Peripheral Bus 4 Output Clock Enable (Output clock is enabled)
    while (!PB4DIVbits.PBDIVRDY); // Wait until it is ready to write to
    PB4DIVbits.PBDIV = 0; // Peripheral Bus 4 Clock Divisor Control (PBCLK4 is SYSCLK divided by 1)

    // PB5DIV
    PB5DIVbits.ON = 1; // Peripheral Bus 5 Output Clock Enable (Output clock is enabled)
    while(!PB5DIVbits.PBDIVRDY);
    PB5DIVbits.PBDIV = 1; // Peripheral Bus 5 Clock Divisor Control (PBCLK5 is SYSCLK divided by 2)

    // PB7DIV
    PB7DIVbits.ON = 1; // Peripheral Bus 7 Output Clock Enable (Output clock is enabled)
    while(!PB7DIVbits.PBDIVRDY);
    PB7DIVbits.PBDIV = 0; // Peripheral Bus 7 Clock Divisor Control (PBCLK7 is SYSCLK divided by 1)

    // PB8DIV
    PB8DIVbits.ON = 1; // Peripheral Bus 8 Output Clock Enable (Output clock is enabled)
    while(!PB8DIVbits.PBDIVRDY);
    PB8DIVbits.PBDIV = 1; // Peripheral Bus 8 Clock Divisor Control (PBCLK8 is SYSCLK divided by 2)

    // PRECON - Set up prefetch
    PRECONbits.PFMSECEN = 0; // Flash SEC Interrupt Enable (Do not generate an interrupt when the PFMSEC bit is set)
    PRECONbits.PREFEN = 0b11; // Predictive Prefetch Enable (Enable predictive prefetch for any address)
    PRECONbits.PFMWS = 0b100; // PFM Access Time Defined in Terms of SYSCLK Wait States (Two wait states)

    // Set up caching

    cp0 = CP0_GET(CP0_CONFIG);
    cp0 &= ~0x07;
    cp0 |= 0b011; // K0 = Cacheable, non-coherent, write-back, write allocate
    CP0_SET(CP0_CONFIG,cp0);

    // Lock Sequence
    SYSKEY = 0x33333333;
    PRISS = 0x76543210;   //SRS all priorities
    // SETTING UP PREFETCH MODULE
    PREFEN0_bit = 1;
    PREFEN1_bit = 1;
    PFMWS0_bit = 0;
    PFMWS1_bit = 1;
    PFMWS2_bit = 0;
}
//////////////////////////////////////////////////////////////////
//configure the output pulse mode OCx  use 16bit as 1.28us tick on tmrs
void OutPutPulseXYZ(){
/*
* The following code will set the Output Compare module 3 = X, 6 = Y , 4 = Z
* for interrupts on single pulse event and select Timer2  32 bit mode
* as the clock source for the compare time-base.
*/
  OC5CON = 0x0000; // disable OC3 module |_using TMR2_3 in 32bit mode
  OC2CON = 0x0000; // disable OC5 module |
  OC7CON = 0X0000; // disable OC8 module |_using tmr6
  OC3CON = 0x0000; // disable OC3 module |_using TMR2_3 in 32bit mode
  OC6CON = 0x0000; // disable OC5 module |
  OC8CON = 0X0000; // disable OC8 module |_using tmr6

//clear  Tmrs
  T2CON  = 0x0000;  // disable Timer2  OC5
  T3CON  = 0x0000;  // disable Timer4  OC3
  T4CON  = 0x0000;  // disable Timer6  OC8
  T5CON  = 0x0000;  // disable Timer2  OC5
  T6CON  = 0x0000;  // disable Timer4  OC3
  T7CON  = 0x0000;  // disable Timer6  OC8
//setup  Tmr2,3,4,5,6&7 as 1:64 prescaler 16bit
  T2CON  = 0x0030;  //   a prescaler of 1:8 to get 1.28usec tick resolution
  T3CON  = 0x0030;  //   a prescaler of 1:8 to get 1.28usec tick resolution
  T4CON  = 0x0030;  //   a prescaler of 1:8 to get 1.28usec tick resolution
  T5CON  = 0x0030;  //   a prescaler of 1:8 to get 1.28usec tick resolution
  T6CON  = 0x0030;  //   a prescaler of 1:8 to get 1.28usec tick resolution
  T7CON  = 0x0030;  //   a prescaler of 1:8 to get 1.28usec tick resolution

// Set period (PR2 is 32-bits wide) and common to all OCx compares
  PR2    = 0xFFFF;   //OC5
  PR3    = 0xFFFF;   //OC6
  PR4    = 0xFFFF;   //OC2
  PR5    = 0xFFFF;   //OC3
  PR6    = 0xFFFF;   //OC7
  PR7    = 0xFFFF;   //OC8

//setup OC3_OC6 32bit but only using in 16bit mode
  OC5CON = 0x0004; //X Conf OC5 module for dual single Pulse output 16bit tmr2
  OC2CON = 0x0004; //Y Conf OC2 module for dual single Pulse output 16bit tmr4
  OC7CON = 0x0004; //Z Conf OC7 module for dual single Pulse output 16bit tmr6
  OC3CON = 0x000C; //A Conf OC3 module for dual single Pulse output 16bit tmr5
  OC6CON = 0x000C; //B Conf OC6 module for dual single Pulse output 16bit tmr3
  OC8CON = 0x000C; //C Conf OC8 module for dual single Pulse output 16bit tmr7
/*
 * Initialize PR2 to a value  >  OCxRS  >  OC3R, to start output compare.
 * TMR2 must be forced to PR2's value then this will force OC3 bit on as
 * soon as a match between TMR2 and PR2 is made a force on OC3 bit is done,
 * TMR continues to count from 0 to OC3RS value, when a match is made a
 * reset on OC3 bit and interrupt OC3IF is set.
 */
  OC5R   = 0x5;        // X_Axis Initialize Compare Register 1
  OC5RS  = 0x234;      // X_Axis Initialize Secondary Compare Register 1
  OC2R   = 0x5;        // Y_Axis Initialize Compare Register 1
  OC2RS  = 0x234;      // Y_Axis Initialize Secondary Compare Register 1
  OC7R   = 0x5;        // Z_Axis Initialize Compare Register 1
  OC7RS  = 0x234;      // Z_Axis Initialize Secondary Compare Register 1
  OC3R   = 0x5;        // A_Axis Initialize Compare Register 1
  OC3RS  = 0x234;      // A_Axis Initialize Secondary Compare Register 1
  OC6R   = 0x5;        // B_Axis Initialize Compare Register 1
  OC6RS  = 0x234;      // B_Axis Initialize Secondary Compare Register 1
  OC8R   = 0x5;        // C_Axis Initialize Compare Register 1
  OC8RS  = 0x234;      // C_Axis Initialize Secondary Compare Register 1


//interrupt priority and enable set X_Axis
  OC5IP0_bit = 1;  // Set OC5 interrupt priority to 3
  OC5IP1_bit = 1;
  OC5IP2_bit = 0;
  OC5IS0_bit = 0;  // Set OC5 sub priority 0
  OC5IS1_bit = 0;
  OC5IF_bit  = 0;  // reset interrupt flag
  OC5IE_bit  = 0;  // enable interrupt

//interrupt priority and enable set Y_Axis
  OC2IP0_bit = 1;  // Set OC3 interrupt priority to 3
  OC2IP1_bit = 1;
  OC2IP2_bit = 0;
  OC2IS0_bit = 1;  // Set OC3 sub priority 1
  OC2IS1_bit = 0;
  OC2IF_bit  = 0;   // reset interrupt flag
  OC2IE_bit  = 0;   // enable interrupt

//interrupt priority and enable set Z_Axis
  OC7IP0_bit = 1;  // Set OC8 interrupt priority to 6
  OC7IP1_bit = 1;
  OC7IP2_bit = 0;
  OC7IS0_bit = 0;  // Set OC8 sub priority 2
  OC7IS1_bit = 1;
  OC7IF_bit  = 0;  // reset interrupt flag
  OC7IE_bit  = 0;  // enable interrupt

//interrupt priority and enable set A_Axis
  OC3IP0_bit = 1;  // Set OC3 interrupt priority to 6
  OC3IP1_bit = 1;
  OC3IP2_bit = 0;
  OC3IS0_bit = 1;  // Set OC3 sub priority 3
  OC3IS1_bit = 1;
  OC3IF_bit  = 0;   // reset interrupt flag
  OC3IE_bit  = 0;   // enable interrupt

//interrupt priority and enable set B_Axis
  OC6IP0_bit = 1;  // Set OC5 interrupt priority to 6
  OC6IP1_bit = 1;
  OC6IP2_bit = 0;
  OC6IS0_bit = 1;  // Set OC6 sub priority 3
  OC6IS1_bit = 1;
  OC6IF_bit  = 0;  // reset interrupt flag
  OC6IE_bit  = 0;  // enable interrupt

//interrupt priority and enable set C_Axis
  OC8IP0_bit = 1;  // Set OC8 interrupt priority to 6
  OC8IP1_bit = 1;
  OC8IP2_bit = 0;
  OC8IS0_bit = 1;  // Set OC8 sub priority 2
  OC8IS1_bit = 1;
  OC8IF_bit  = 0;  // reset interrupt flag
  OC8IE_bit  = 0;  // enable interrupt

//set Timers on
  T2CONSET  = 0x8000; //X Enable Timer2 0C5
  T4CONSET  = 0x8000; //Y Enable Timer4 OC2
  T6CONSET  = 0x8000; //Z Enable Timer6 OC7
  T5CONSET  = 0x8000; //A Enable Timer5 0C3
  T3CONSET  = 0x8000; //B Enable Timer3 OC6
  T7CONSET  = 0x8000; //C Enable Timer6 OC8

//wait for usgage of these modules before enaBling them
 // OC3CONSET = 0x8000; // Enable OC3
 // OC5CONSET = 0x8000; // Enable OC6
 // OC8CONSET = 0x8000; // Enable OC8
}

/////////////////////////////////////////////////////
//only if DMA is turned off
void UART2() iv IVT_UART2_RX ilevel 5 ics ICS_SOFT {
   IFS4CLR  = 0x40000;

   UART3_Write(U2RXREG);

}