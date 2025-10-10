#ifndef SERIAL_DMA_H
#define SERIAL_DMA_H


#include <stdlib.h>
#include <stdarg.h>
#include "Config.h"

<<<<<<< HEAD
#define NULL 0
#define _DMACON_SUSPEND_MASK (1<<12)

#define UART1_DMA
//#define UART2_DMA

=======


#define NULL 0
#define _DMACON_SUSPEND_MASK (1<<12)

>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
extern char txt[];
extern char rxBuf[];
extern char txBuf[];

typedef struct{
 char temp_buffer[500];
 int head;
 int tail;
 int diff;
 char has_data: 1;
}Serial;

extern Serial serial;



////////////////////////////////////////////
//function prototypes
//Global functions
void DMA_global();
unsigned int DMA_Busy();
unsigned int DMA_Suspend();
unsigned int DMA_Resume();

////////////////////////////////////////////
//DMA0 rcv functions
void DMA0();
char DMA0_Flag();
void DMA0_Enable();
void DMA0_Disable();
unsigned int DMA0_ReadDstPtr();
void DMA0_RstDstPtr();

///////////////////////////////////////////
//DMA1 trmit functions
void DMA1();
char DMA1_Flag();
unsigned int DMA1_Enable();
void DMA1_Disable();


//////////////////////////////////////////
//DMA Generic functions
unsigned int DMA_IsOn(int channel);
unsigned int DMA_CH_Busy(int channel);
unsigned int DMA_Abort(int channel);
///////////////////////////////////////////
//Printout specific functions
void Reset_rxBuff(int dif);
int  Get_Head_Value();
int  Get_Tail_Value();
int  Get_Difference();
void Get_Line(char *str,int dif);
void Reset_Ring();
int  Loopback();
int dma_printf(char* str,...);
void lTrim(char* d,char* s);
#endif