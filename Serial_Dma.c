#include "Serial_Dma.h"

#define err(x,y) x##y
#define DMAx_err(x,y) (#x " " #y)


Serial serial;
char rxBuf[200] = {0}  absolute 0xA0002000 ; //resides in flash ??
char txBuf[200] = {0}  absolute 0xA0002200 ;
char cherie[] = " CHERIF Error\r";
char dma0[] = "DMA0_";
char dma1[] = "DMA1_";
const char newline[] = "\r\n";

char dma0int_flag;
char dma1int_flag;

// Pattern matching state variables
char current_pattern = '?';  // Current pattern: '?' for status queries, '\n' for commands
char pattern_switched = 0;   // Flag to indicate pattern was switched



////////////////////////////////////////////////////////////////
//DMA Config
void DMA_global(){
    //Enable the whole DMA module
    DMACONSET = 0x8000;
    DMA0();
    DMA1();
}


/************************************************************
* ac:DMA_Pic32
* This is the DMA channel 0 setup for the receiver
* it is setup to auto enable after a block transfer or
* pattern match of '\r' we can enable the 2 char pattern
* match if needed by setting PATLEN bit on.
* Destination size is 200 . a pattern match must be
* Sent or a block transfer will only take place after
* 200 bytes. An abort can be forced by setting the CABORT bit
************************************************************/
void  DMA0(){
    //Disable DMA0 IE
    IEC4CLR      = 0x40;
    IFS4CLR      = 0x40;

    //Disable DMA0 and reset priority
    DCH0CONCLR = 0x8003;

#ifdef UART1_DMA
   //INTERRUPT IRQ NUMBER for UART 1 RX (113) | [0x10 = SIRQEN] [0x30 = PATEN & SIRQEN]
    DCH0ECON      =  (113 << 8 ) | 0x30;
#else
   //INTERRUPT IRQ NUMBER for UART 2 RX (146) | [0x10 = SIRQEN] [0x30 = PATEN & SIRQEN]
    DCH0ECON      =  (146 << 8 ) | 0x30;
#endif

    //Pattern data - start with '?' for GRBL status queries, will switch to '\n' for commands
    DCH0DAT       = '?';//'\0' //0x0A0D;//'\r\n';

#ifdef UART1_DMA
    //Source address as UART1_RX
    DCH0SSA       = KVA_TO_PA(0xBF822030);    //[0xBF822030 = U1RXREG]
#else
    //Source address as UART2_RX
    DCH0SSA       = KVA_TO_PA(0xBF822230);    //[0xBF822230 = U2RXREG]
#endif
    DCH0SSIZ      = 1;                 // source size = 1byte at a time

    //Destination address  as RxBuffer
    DCH0DSA       = KVA_TO_PA(0xA0002000);    // virtual address:= IN RAM FOR RECIEVED DATA
    DCH0DSIZ      = 200  ;  // destination size = Size for the 'rxBuf' to fill up with received characters. It is = 5 in this example...

    //Cell size as 1 byte
    DCH0CSIZ      = 1  ;  // bytes transferred in the background

    //Interrupt setup
    //clear DMA channel priority and sub-priority
    IPC33CLR     = 0x160000;

    // Clear existing events, disable all interrupts
    DCH0INTCLR    = 0x00FF00FF ;

    //Set priority 5 sub-priority 1
    IPC33SET      = 0x00140000;

    //Enable [CHBCIE && CHERIE] Interrupts
    DCH0INTSET      =  0xB0000;

    //set DMA0IE bit
    IEC4SET       = 0x40;
    IFS4CLR       = 0x40;

    // Clear existing events, disable all interrupts
    DCH0INTCLR    = 0x000000FF ;

    //PATLEN[11] && CHEN[7] && CHEDT[5] && CHAEN[4] && PRIOR[1:0]
    //Set up AutoEnable & Priority as 3       .
    DCH0CONSET      = 0X0000513;//013 = 1 char || 813 = 2 char e.g. \r\n

    //set the recieve buffer counts to 0
    serial.head = serial.tail = serial.diff = 0;
}

////////////////////////////////////////
//Test dma0_Flag for indication
char DMA0_Flag(){
    return dma0int_flag;
}

////////////////////////////////////////
//DMA0 on control
void DMA0_Enable(){
   //CHEN[7]
   //Turn on DMA0
   DCH0CONSET  = 1<<7;
   IFS4CLR     = 0x40;
}

////////////////////////////////////////
//DMA0 off control
void DMA0_Disable(){
  //Disable DAM0 module and clear priority level
   DCH0CONCLR  = 1<<7;
   //DCH0CONbits.CHEN = 0;
}


////////////////////////////////////////
//DMA0 not at initial regester
unsigned int DMA0_ReadDstPtr(){
    return DCH0DPTR;
}

///////////////////////////////////////
//Reset Dst pointer
void DMA0_RstDstPtr(){
   DCH0DPTRCLR = 0xFFFF;
}

////////////////////////////////////////
//DMA0 IRQ   UART2 RX
void DMA_CH0_ISR() iv IVT_DMA0 ilevel 5 ics ICS_AUTO{
 int i = 0;

   //flags to sample in code if needed
    dma0int_flag = DCH0INT & 0x00FF;

  // CHANNEN ADDRESS ERROR FLAF
    if( CHERIF_bit == 1){       // test error int flag
       //LOOPBACK RECIEVE ERROR COULD BE SPECIFIC MSG
       strcpy(rxBuf,DMAx_err(dma0,cherie));
       //Can uncomment if needed
       //DCH1SSIZ = 13;           //set block size of transfer
       //DCH1ECONbits.CFORCE = 1 ;// force DMA1 interrupt trigger
    }
 //   if(CHTAIF_bit){
 //       i = strlen(rxBuf);
 //   }
 // THIS CHANNEL IS AUTOMATICALLY ENABLED AFTER A BLOCK
 // OR ERROR ABORT EVENT, THIS SHOULD TAKE PLACE IF A
 // pattern match HAS BEEN RECEIVED or 200 BYTES EXCEEDED
    if (DCH0INTbits.CHBCIF == 1){
         i = strlen(rxBuf);
         
         // Check if we received a pattern match character
         if (i > 0) {
             char last_char = rxBuf[i-1];
             
             // If we received '?' and current pattern is '?', switch to '\n' pattern
             if (last_char == '?' && current_pattern == '?') {
                 DMA_Switch_Pattern('\n');
             }
             // If we received '\n' and current pattern is '\n', handle the switch
             else if (last_char == '\n' && current_pattern == '\n') {
                 DMA_Handle_Pattern_Switch();
                 // Switch back to '?' pattern for next status query
                 DMA_Switch_Pattern('?');
             }
         }
    }

    // copy RxBuf -> temp_buffer  BUFFER_LENGTH
    //make sure that head + i don't exceed max buffer length
    if(serial.head + i > 499)
       serial.head = 0;

    strncpy(serial.temp_buffer+serial.head, rxBuf, i);
    serial.head += i;
    memset(rxBuf,0,i+2);
    //*(rxBuf+0) = '\0';

    DCH0INTCLR    = 0x000000ff;
    IFS4CLR       = 0x40;

}

//reset rxBuff
static void Reset_rxBuff(int dif){
  memset(rxBuf,0,dif);
}

//Head index
int Get_Head_Value(){
 return serial.head;
}

//Tail index
int Get_Tail_Value(){
  return serial.tail;
}

//get the difference in indexes
int Get_Difference(){

  if(serial.head > serial.tail)
    serial.diff = serial.head - serial.tail;
  else if(serial.tail > serial.head)
    serial.diff =  serial.head;
  else
    serial.diff = 0;

  return serial.diff;
}

void Reset_Ring(){
   serial.tail = serial.head = 0;
}

//read the line from thebuffer
void Get_Line(char *str,int dif){

   if(serial.tail + dif > 499)
      serial.tail = 0;

    strncpy(str,serial.temp_buffer+serial.tail,dif);

    //Reset_rxBuff(dif);
    //incriment the tail
    serial.tail += dif;
}

//loopback the message
int  Loopback(){
char str[50];
int dif;

   dif = Get_Difference();

   if(serial.tail + dif > 499)
      serial.tail = 0;

    strncpy(str,serial.temp_buffer+serial.tail,dif);
    dma_printf("\n\t%s",str);
    //incrament the tail
    serial.tail += dif;
}


/******************************************************
* This is the DMA setup for the UART2 transmitter and
* is not auto enabled, this will be done everytime a
* UART transfer out need to take place. The loopback
* is temporary and is done from within the DMA0 IRQ
* Vector, The steps within this Vector to loopback
* will be used from within code to perfoem a data send
* A pattern match should abort the transfer and wait
* for the next CFORCE bit to be set, DCH1SSIZ can be
* dynamically assigned to force and abort but block
* transfer having been finnished
*******************************************************/
void DMA1(){
    //Disable DMA1 IE and clear IF
    //clear DMA channel priority and sub-priority
     IPC33CLR      = 0x17000000;
     IEC4CLR       = 0x7;

     //Disable DMA0 and reset priority
    DCH1CONCLR = 0x8003;

<<<<<<< HEAD
#ifdef UART1_
    //INTERRUPT IRQ NUMBER for UART 1 TX (114) | [0x10 = SIRQEN] [0x30 = PATEN & SIRQEN]
    DCH1ECON=(114 << 8)| 0x30;
#elif UART2_
    //INTERRUPT IRQ NUMBER for UART 2 TX (147) | [0x10 = SIRQEN] [0x30 = PATEN & SIRQEN]
    DCH1ECON=(147 << 8)| 0x30;
#endif
=======
    //INTERRUPT IRQ NUMBER for UART 2 TX (147) | [0x10 = SIRQEN] [0x30 = PATEN & SIRQEN]
    DCH1ECON=(147 << 8)| 0x30;

>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
    //Pattern Length and char to match not needed here ????
    //Pattern length = 0 = 1 byte
    DCH1DAT       = '\0';

    //Source address and size of transfer buffer
    DCH1SSA   = KVA_TO_PA(0xA0002200) ;  //0xA0002200 virtual address of txBuf
    DCH1SSIZ  = 1000;  //' This is how many bytes you want to send out in a block transfer for UART transmitter

    //Destination Address and size which is 1byte
    //U1TX2REG for reply  [0xBF822220 = U1TXREG]
    U1TXREG   = 0x00;
<<<<<<< HEAD
    #ifdef UART1_DMA
    DCH1DSA   = KVA_TO_PA(0xBF822020) ;
    #elif UART2_DMA
    DCH1DSA   = KVA_TO_PA(0xBF822220) ;
    #endif
=======
    DCH1DSA   = KVA_TO_PA(0xBF822220) ;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
    DCH1DSIZ  = 1;

    //Cell size to transfer each transfer
    DCH1CSIZ  = 1;    //' x bytes from txBuf in a cell waiting to send out 1 byte at a time to U1TXREG / DCH1DSIZ

    //Interrupt setup
    //clear DMA channel priority and sub-priority
    IPC33CLR  = 0x16000000;

    //Disable all interrupts, Clear existing events
    DCH1INTCLR = 0x00FF00FF ;

    //Set priority 5 sub-priority 2
    IPC33SET  = 0x16000000;

    //[CHERIE]
    //DCH1INTSET    =  0x10000;

    //set DMA1IE bit
    IEC4SET   = 0x80;
    //Clear DMA1IF bit
    IFS4CLR   = 0x80;
    // Clear existing events
    DCH1INTCLR = 0x00FF ;

    //PATLEN[11] && CHEN[7] && CHAEN[4] && PRIOR[1:0]
    //Set up no AutoEnable & Priority as 3        .
    DCH1CONSET    = 0x00000003;

}

////////////////////////////////////////
//Test dma0_Flag for indication
char DMA1_Flag(){
    return dma1int_flag;
}

///////////////////////////////////////

////////////////////////////////////////
//DMA1 on control
unsigned int DMA1_Enable(){
   DCH1CONSET = 1<<7;
   return (DCH1CON & 0x80) >> 7;
}

////////////////////////////////////////
//DMA1 off control ?? should compliment
//need to come back to this eventually
void DMA1_Disable(){
    DCH1CONCLR = 1<<7;
}

////////////////////////////////////////
//DMA1 Abort abort channel transfer
unsigned int DMA_Abort(int channel){
  if(channel == 0){
    //must investigate the effects of CABORT ??, it is not
    //resettting the channel!!!
    //DMA0 CABOART bit
     DCH0ECONSET |= 1<<6;
    // DMA0_Disable();
     //wait for BMA to finnish
     while(DMA_IsOn(0));
     //force pointers to reset
    // DCH0DSA       = KVA_TO_PA(0xA0002000);    // virtual address:= IN RAM FOR RECIEVED DATA
     //ReEnable DMA0
     DMA0_Enable();
     while(!DMA_IsOn(0));

     //return enable bit
     return (DCH0CON & 0x0080 ) >> 7;

   }else if(channel == 1){
     //DMA1 CABOART bit
     DCH1ECONSET |= 1<<6;
    // DMA1_Disable();
     //wait for DMA1 to finish
     while(DMA_IsOn(1));
     //force a reset of the pointers
   //  DCH1SSA   = KVA_TO_PA(0xA0002200) ;  //0xA0002200 virtual address of txBuf
     while(!DMA_IsOn(1));
     //ReEnable DMA1
     DMA1_Enable();
     return (DCH1CON & 0x0080 ) >> 7;
   }

   return 255;
}

///////////////////////////////////////
//Check if DMA channel on bit15 is true
unsigned int DMA_IsOn(int channel){
   if(channel == 0)
     return (DCH0CON & 0x8000)>>15;
   else
     return (DCH1CON & 0x8000)>>15;
}
///////////////////////////////////////
//Get the status of the respective DMA
//channel, decide whether or not to send
//new data  [1 = busy || 0 = free]
//checking the state oF the DMABUSY bit 15
//FOR EACH CHANNEL
unsigned int DMA_CH_Busy(int channel){
   if(channel == 0)
     return (DCH0CON & 0x8000)>>15;
   else
     return (DCH1CON & 0x8000)>>15;
}


////////////////////////////////////////
//DMA SUSPEND bit12 force to true to
//suspend the channel
unsigned int DMA_Suspend(){
  DMACONSET = (1 << 12);
  //while(DMA_Busy());
//return the state of the SUSPEND bit
  return ((DMACON & 0x1000)>>12);
}

////////////////////////////////////////
//DMA resume the SUSPEND by forcing bit12
//to false
unsigned int DMA_Resume(){
  DMACONCLR = (1 << 12);
  //while(DMA_Busy());
 //return the SUSPEN bit state
  return (DMACON & 0x1000)>>12;
}

////////////////////////////////////////
//Global DMA busy bit not channel busy bit!
//DMACON.DMABUSY
unsigned int DMA_Busy(){
 return ((DMACON & 0x800)>>11);
}

/////////////////////////////////////////////////////
//UART2 TX Interrupt should be handed automatically
//from within the DMA controller, this IRQ vector
//is only used if errors are need to be checked
//or some otherfunctionality is need after a block
//transfer has completed
void DMA_CH1_ISR() iv IVT_DMA1 ilevel 5 ics ICS_SRS {

    // user flag to inform this int was triggered. should be cleared in software
    dma1int_flag = DCH1INT & 0x00FF;
    //Channel Block Transfer Complete Interrupt Flag bit
    if (DCH1INTbits.CHBCIF){
       //DMA1_Disable();
    }
     //Channel Address Error Interrupt Flag bit
    if( CHERIF_DCH1INT_bit == 1){
        CABORT_DCH1ECON_bit = 1;
    }

    //However, for the interrupt controller, there is
    //just one dedicated interrupt flag bit per channel,
    //DMAxIF, and the corresponding interrupt enable/mask bits, DMAxIE.
    DCH1INTCLR  = 0x00FF;
    IFS4CLR     = 0x80;

}


//////////////////////////////////////////////////////
//DMA Print strings and variable arguments formating
int dma_printf(const char* str,...){
 //Variable decleration of type va_list
 va_list va;
 int i = 0, j = 0;
 char buff[1500]={0},tmp[20],tmp1[20];
 char *str_arg,*tmp_;

 //check that str is not null
 if(str == 0)
     return;

 //can only call this once the va_list has bee declared
 //or the compiler throws an undefined error!!! not sure
 //about the compiler not implimenting va_end????
 if(DMA_CH_Busy(1)){
   return 0;
 }

 //initialize the va_list via the macro va_start(arg1,arg2)
 //arg1 is type va_list and arg2 is type var preceding elipsis
 va_start(va,str);

 i = j = 0;
 while(*(str+i) != '\0'){
   if(*(str+i) == '%'){
     i++;  //step over % char
     switch(*(str+i)){
        case 'c':
             //convert char to ASCII char
             buff[j] = (char)va_arg(va,char);
             j++;
             break;
        case 'd':
             //convert int to decimal
             sprintf(tmp1,"%d",va_arg(va,int));
             strcat(buff+j, tmp1);
             j += strlen(tmp1);
             break;
        case 'u':
             //convert unsigned to decimal
             sprintf(tmp1,"%u",va_arg(va,unsigned int));
             strcat(buff+j, tmp1);
             j += strlen(tmp1);
        case 'l':
             //convert long to decimal
             sprintf(tmp,"%ld",va_arg(va,long));
             //lTrim(tmp_,&tmp);
             strcat(buff+j, tmp);
             j += strlen(tmp);
             break;
        case 'X':
             //convert int to hex
             sprintf(tmp,"%X",va_arg(va,int));
             strcat(buff+j, tmp);
             j += strlen(tmp);
             break;
        case 'X':
             //convert long to hex
             sprintf(tmp,"%lX",va_arg(va,long));
             strcat(buff+j, tmp);
             j += strlen(tmp);
             break;
        case 'f':
             sprintf(tmp,"%08.3f",va_arg(va,float));
             strcat(buff+j, tmp);
             j += strlen(tmp);
             break;
        case 'F':
             sprintf(tmp,"%E",va_arg(va,double));
             strcat(buff+j, tmp);
             j += strlen(tmp);
             break;
        case 'p':
             sprintf(tmp,"%p",va_arg(va,void*));
             strcat(buff+j, tmp);
             j += strlen(tmp);
             break;
        case 's':
             //copy string
             str_arg = va_arg( va, char* );
             strcat(buff+j, str_arg);
             j += strlen(str_arg);
             break;
     }
  }else{
       *(buff+j) = *(str+i);
       j++;
  }
   i++;
 }
 *(buff+j+1) = 0;
 strncpy(txBuf,buff,j+1);
 DCH1SSIZ    = j ;
 while(!DMA1_Enable());
 //DCH1ECONSET = 1<<7;
 return j;

}

///////////////////////////////////////////////////
//left trim the string of zeros
void lTrim(char *d,char* s){
char* temp;
int i=0,j,k;
  k = i;
  j = strlen(s);
 while(*s != '\0'){
      if((*s > 0x30)||(k>0)){
         k = 1;
         *d = *s;
         d++;
      }else
         i++;
      s++;
 }
 if(i == j){
   *d = '0';
   d++;
 }
 *d = 0;
}

// Function to switch DMA pattern matching
void DMA_Switch_Pattern(char new_pattern) {
    if (current_pattern != new_pattern) {
        current_pattern = new_pattern;
        DCH0DAT = new_pattern;
        pattern_switched = 1;
    }
}

// Function to add \r\n when pattern switches from '?' to '\n'
void DMA_Handle_Pattern_Switch(void) {
    if (pattern_switched && current_pattern == '\n') {
        // Add \r\n to the output buffer when switching from '?' to '\n'
        dma_printf("\r\n");
        pattern_switched = 0;  // Reset the flag
    }
}