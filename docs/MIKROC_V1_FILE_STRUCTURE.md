# mikroC V1 File Structure - Complete Reference

## Overview

This document provides the **complete mikroC V1 Serial_Dma implementation** for reference when porting to XC32/Harmony. We maintain the same file structure and separation of concerns.

---

## File Structure (mikroC V1)

```
Pic32mzCNC/
  ├── Serial_Dma.h      (Header - API declarations)
  └── Serial_Dma.c      (Implementation - All DMA functions)
```

**Our XC32/Harmony Structure (MUST MATCH):**

```
srcs/
  └── serial_dma.c      (Implementation - matches mikroC structure)
incs/
  └── serial_dma.h      (Header - matches mikroC API)
```

---

## Complete mikroC V1 API (Serial_Dma.h)

### Function Categories (Separation of Concerns)

#### 1. Global DMA Functions
```c
void DMA_global();              // Initialize DMA module
unsigned int DMA_Busy();        // Check global DMA busy
unsigned int DMA_Suspend();     // Suspend all channels
unsigned int DMA_Resume();      // Resume all channels
```

#### 2. DMA Channel 0 (RX) Functions
```c
void DMA0();                    // Initialize Channel 0
char DMA0_Flag();               // Get interrupt flags
void DMA0_Enable();             // Enable channel
void DMA0_Disable();            // Disable channel
unsigned int DMA0_ReadDstPtr(); // Read destination pointer
void DMA0_RstDstPtr();          // Reset destination pointer
```

#### 3. DMA Channel 1 (TX) Functions
```c
void DMA1();                    // Initialize Channel 1
char DMA1_Flag();               // Get interrupt flags
unsigned int DMA1_Enable();     // Enable channel (start TX)
void DMA1_Disable();            // Disable channel
```

#### 4. DMA Generic Functions
```c
unsigned int DMA_IsOn(int channel);      // Check if channel enabled
unsigned int DMA_CH_Busy(int channel);   // Check if channel busy
unsigned int DMA_Abort(int channel);     // Abort transfer
```

#### 5. Ring Buffer Functions
```c
void Reset_rxBuff(int dif);     // Clear RX buffer (internal)
int Get_Head_Value();           // Get head index
int Get_Tail_Value();           // Get tail index
int Get_Difference();           // Get available bytes
void Get_Line(char *str, int dif); // Copy line from buffer
void Reset_Ring();              // Clear ring buffer
```

#### 6. Printf Functions
```c
int dma_printf(char* str, ...); // Formatted output via DMA
void lTrim(char* d, char* s);   // Left-trim zeros
```

---

## Complete mikroC V1 Implementation (Serial_Dma.c)

### Section 1: Global Variables (Lines 1-15)

```c
#include "Serial_Dma.h"

#define err(x,y) x##y
#define DMAx_err(x,y) (#x " " #y)

Serial serial;                              // Ring buffer instance
char rxBuf[200] = {0} absolute 0xA0002000; // DMA RX buffer
char txBuf[200] = {0} absolute 0xA0002200; // DMA TX buffer
char cherie[] = " CHERIF Error\r";
char dma0[] = "DMA0_";
char dma1[] = "DMA1_";
const char newline[] = "\r\n";

char dma0int_flag;                         // DMA0 interrupt flags
char dma1int_flag;                         // DMA1 interrupt flags
```

### Section 2: Global DMA Functions (Lines 18-25)

```c
////////////////////////////////////////////////////////////////
//DMA Config
void DMA_global(){
    //Enable the whole DMA module
    DMACONSET = 0x8000;
    DMA0();
    DMA1();
}
```

### Section 3: DMA Channel 0 (RX) Configuration (Lines 28-122)

```c
/************************************************************
* DMA channel 0 setup for the receiver
* Pattern match on '?' with auto-enable
************************************************************/
void DMA0(){
    IEC4CLR = 0x40;
    IFS4CLR = 0x40;
    DCH0CONCLR = 0x8003;
    
    DCH0ECON = (146 << 8) | 0x30;          // IRQ 146 + PATEN + SIRQEN
    DCH0DAT = '?';                          // Pattern match byte
    
    DCH0SSA = KVA_TO_PA(0xBF822230);       // U2RXREG
    DCH0SSIZ = 1;                           // 1 byte source
    DCH0DSA = KVA_TO_PA(0xA0002000);       // rxBuf
    DCH0DSIZ = 200;                         // 200 byte buffer
    DCH0CSIZ = 1;                           // 1 byte cell size
    
    IPC33CLR = 0x160000;
    DCH0INTCLR = 0x00FF00FF;
    IPC33SET = 0x00140000;                  // Priority 5, sub-priority 1
    DCH0INTSET = 0xB0000;                   // Enable CHBCIE, CHTAIE, CHERIE
    IEC4SET = 0x40;
    IFS4CLR = 0x40;
    DCH0INTCLR = 0x000000FF;
    
    DCH0CONSET = 0X0000513;                 // PATLEN | CHEN | CHAEN | PRIOR
    
    serial.head = serial.tail = serial.diff = 0;
}

char DMA0_Flag(){ return dma0int_flag; }
void DMA0_Enable(){ DCH0CONSET = 1<<7; IFS4CLR = 0x40; }
void DMA0_Disable(){ DCH0CONCLR = 1<<7; }
unsigned int DMA0_ReadDstPtr(){ return DCH0DPTR; }
void DMA0_RstDstPtr(){ DCH0DPTRCLR = 0xFFFF; }
```

### Section 4: DMA Channel 0 ISR (Lines 125-168)

```c
////////////////////////////////////////
//DMA0 IRQ UART2 RX
void DMA_CH0_ISR() iv IVT_DMA0 ilevel 5 ics ICS_AUTO{
    int i = 0;
    
    dma0int_flag = DCH0INT & 0x00FF;
    
    // Channel address error
    if(CHERIF_bit == 1){
        strcpy(rxBuf, DMAx_err(dma0,cherie));
    }
    
    // Block complete (pattern match or buffer full)
    if (DCH0INTbits.CHBCIF == 1){
        i = strlen(rxBuf);
    }
    
    // Wrap ring buffer if needed
    if(serial.head + i > 499)
        serial.head = 0;
    
    // Copy from DMA buffer to ring buffer
    strncpy(serial.temp_buffer + serial.head, rxBuf, i);
    serial.head += i;
    memset(rxBuf, 0, i+2);
    
    DCH0INTCLR = 0x000000ff;
    IFS4CLR = 0x40;
}
```

### Section 5: Ring Buffer Functions (Lines 170-216)

```c
//reset rxBuff
static void Reset_rxBuff(int dif){
    memset(rxBuf, 0, dif);
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
        serial.diff = serial.head;
    else
        serial.diff = 0;
    
    return serial.diff;
}

void Reset_Ring(){
    serial.tail = serial.head = 0;
}

// Extract line from ring buffer
void Get_Line(char *str, int dif){
    dif = Get_Difference();
    
    if(serial.tail + dif > 499)
        serial.tail = 0;
    
    strncpy(str, serial.temp_buffer + serial.tail, dif);
    dma_printf("\n\t%s", str);
    serial.tail += dif;
}
```

### Section 6: DMA Channel 1 (TX) Configuration (Lines 220-321)

```c
/******************************************************
* DMA Channel 1 setup for UART2 transmitter
* NOT auto-enabled - must call DMA1_Enable() to start
*******************************************************/
void DMA1(){
    IPC33CLR = 0x17000000;
    IEC4CLR = 0x7;
    DCH1CONCLR = 0x8003;
    
    DCH1ECON = (147 << 8) | 0x30;          // IRQ 147 + PATEN + SIRQEN
    DCH1DAT = '\0';                         // Pattern not used for TX
    
    DCH1SSA = KVA_TO_PA(0xA0002200);       // txBuf
    DCH1SSIZ = 1000;                        // Max 1000 bytes to send
    
    U1TXREG = 0x00;
    DCH1DSA = KVA_TO_PA(0xBF822220);       // U2TXREG
    DCH1DSIZ = 1;                           // 1 byte destination
    DCH1CSIZ = 1;                           // 1 byte cell size
    
    IPC33CLR = 0x16000000;
    DCH1INTCLR = 0x00FF00FF;
    IPC33SET = 0x16000000;                  // Priority 5, sub-priority 2
    IEC4SET = 0x80;
    IFS4CLR = 0x80;
    DCH1INTCLR = 0x00FF;
    
    DCH1CONSET = 0x00000003;                // NO CHAEN! Priority 3 only
}

char DMA1_Flag(){ return dma1int_flag; }

unsigned int DMA1_Enable(){
    DCH1CONSET = 1<<7;
    return (DCH1CON & 0x80) >> 7;
}

void DMA1_Disable(){
    DCH1CONCLR = 1<<7;
}
```

### Section 7: DMA Generic Functions (Lines 324-411)

```c
////////////////////////////////////////
//DMA Abort channel transfer
unsigned int DMA_Abort(int channel){
    if(channel == 0){
        DCH0ECONSET |= 1<<6;              // Set CABORT bit
        while(DMA_IsOn(0));
        DMA0_Enable();
        while(!DMA_IsOn(0));
        return (DCH0CON & 0x0080) >> 7;
    }
    else if(channel == 1){
        DCH1ECONSET |= 1<<6;
        while(DMA_IsOn(1));
        while(!DMA_IsOn(1));
        DMA1_Enable();
        return (DCH1CON & 0x0080) >> 7;
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
//Get channel busy status (bit 15)
unsigned int DMA_CH_Busy(int channel){
    if(channel == 0)
        return (DCH0CON & 0x8000)>>15;
    else
        return (DCH1CON & 0x8000)>>15;
}

////////////////////////////////////////
//DMA SUSPEND bit12
unsigned int DMA_Suspend(){
    DMACONSET = (1 << 12);
    return ((DMACON & 0x1000)>>12);
}

////////////////////////////////////////
//DMA resume
unsigned int DMA_Resume(){
    DMACONCLR = (1 << 12);
    return (DMACON & 0x1000)>>12;
}

////////////////////////////////////////
//Global DMA busy bit (DMACON.DMABUSY)
unsigned int DMA_Busy(){
    return ((DMACON & 0x800)>>11);
}
```

### Section 8: DMA Channel 1 ISR (Lines 414-444)

```c
/////////////////////////////////////////////////////
//UART2 TX Interrupt
void DMA_CH1_ISR() iv IVT_DMA1 ilevel 5 ics ICS_SRS {
    dma1int_flag = DCH1INT & 0x00FF;
    
    // Block Transfer Complete
    if (DCH1INTbits.CHBCIF){
        // DMA1_Disable(); // Optional
    }
    
    // Channel Address Error
    if(CHERIF_DCH1INT_bit == 1){
        CABORT_DCH1ECON_bit = 1;
    }
    
    DCH1INTCLR = 0x00FF;
    IFS4CLR = 0x80;
}
```

### Section 9: dma_printf Function (Lines 447-536)

```c
//////////////////////////////////////////////////////
//DMA Print strings and variable arguments formatting
int dma_printf(const char* str, ...){
    va_list va;
    int i = 0, j = 0;
    char buff[1500]={0}, tmp[20], tmp1[20];
    char *str_arg, *tmp_;
    
    if(str == 0) return;
    
    // Check if DMA channel busy
    if(DMA_CH_Busy(1)){
        return 0;
    }
    
    va_start(va, str);
    
    i = j = 0;
    while(*(str+i) != '\0'){
        if(*(str+i) == '%'){
            i++;  // step over % char
            switch(*(str+i)){
                case 'c':  // char
                    buff[j] = (char)va_arg(va, char);
                    j++;
                    break;
                case 'd':  // int decimal
                    sprintf(tmp1, "%d", va_arg(va, int));
                    strcat(buff+j, tmp1);
                    j += strlen(tmp1);
                    break;
                case 'u':  // unsigned decimal
                    sprintf(tmp1, "%u", va_arg(va, unsigned int));
                    strcat(buff+j, tmp1);
                    j += strlen(tmp1);
                case 'l':  // long decimal
                    sprintf(tmp, "%ld", va_arg(va, long));
                    strcat(buff+j, tmp);
                    j += strlen(tmp);
                    break;
                case 'X':  // hex
                    sprintf(tmp, "%X", va_arg(va, int));
                    strcat(buff+j, tmp);
                    j += strlen(tmp);
                    break;
                case 'f':  // float
                    sprintf(tmp, "%08.3f", va_arg(va, float));
                    strcat(buff+j, tmp);
                    j += strlen(tmp);
                    break;
                case 'F':  // double
                    sprintf(tmp, "%E", va_arg(va, double));
                    strcat(buff+j, tmp);
                    j += strlen(tmp);
                    break;
                case 'p':  // pointer
                    sprintf(tmp, "%p", va_arg(va, void*));
                    strcat(buff+j, tmp);
                    j += strlen(tmp);
                    break;
                case 's':  // string
                    str_arg = va_arg(va, char*);
                    strcat(buff+j, str_arg);
                    j += strlen(str_arg);
                    break;
            }
        }
        else{
            *(buff+j) = *(str+i);
            j++;
        }
        i++;
    }
    *(buff+j+1) = 0;
    
    // Copy to TX buffer
    strncpy(txBuf, buff, j+1);
    DCH1SSIZ = j;
    while(!DMA1_Enable());
    
    return j;
}
```

### Section 10: lTrim Function (Lines 539-557)

```c
///////////////////////////////////////////////////
//left trim the string of zeros
void lTrim(char *d, char* s){
    char* temp;
    int i=0, j, k;
    k = i;
    j = strlen(s);
    
    while(*s != '\0'){
        if((*s > 0x30) || (k>0)){
            k = 1;
            *d = *s;
            d++;
        }
        else
            i++;
        s++;
    }
    
    if(i == j){
        *d = '0';
        d++;
    }
    *d = 0;
}
```

---

## Separation of Concerns (mikroC V1 Pattern)

### Concern 1: DMA Hardware Configuration
**Functions:** `DMA_global()`, `DMA0()`, `DMA1()`
**Responsibility:** Configure hardware registers for DMA channels

### Concern 2: DMA Channel Control
**Functions:** `DMA0_Enable()`, `DMA0_Disable()`, `DMA1_Enable()`, `DMA1_Disable()`
**Responsibility:** Start/stop DMA transfers

### Concern 3: DMA Status Monitoring
**Functions:** `DMA_Busy()`, `DMA_CH_Busy()`, `DMA_IsOn()`, `DMA0_Flag()`, `DMA1_Flag()`
**Responsibility:** Query DMA state

### Concern 4: DMA Error Handling
**Functions:** `DMA_Abort()`, `DMA_Suspend()`, `DMA_Resume()`
**Responsibility:** Handle errors and special conditions

### Concern 5: Ring Buffer Management
**Functions:** `Get_Difference()`, `Get_Line()`, `Get_Head_Value()`, `Get_Tail_Value()`, `Reset_Ring()`
**Responsibility:** Manage circular buffer for received data

### Concern 6: Formatted Output
**Functions:** `dma_printf()`, `lTrim()`
**Responsibility:** Format strings and transmit via DMA

### Concern 7: Interrupt Handling
**Functions:** `DMA_CH0_ISR()`, `DMA_CH1_ISR()`
**Responsibility:** Process DMA interrupts

---

## Our XC32/Harmony Port - Matching Structure

### File: `incs/serial_dma.h`
**Structure:** Exact same function declarations as mikroC Serial_Dma.h
**Sections:**
1. Global DMA functions
2. DMA Channel 0 (RX) functions
3. DMA Channel 1 (TX) functions
4. DMA generic functions
5. Ring buffer functions
6. Printf functions

### File: `srcs/serial_dma.c`
**Structure:** Exact same organization as mikroC Serial_Dma.c
**Sections:**
1. Global variables
2. Global DMA functions
3. DMA Channel 0 configuration
4. DMA Channel 0 ISR (Harmony callback)
5. Ring buffer functions
6. DMA Channel 1 configuration
7. DMA generic functions
8. DMA Channel 1 ISR (Harmony callback)
9. dma_printf function
10. lTrim function

### Key Differences (XC32 vs mikroC)
1. **Address conversion:** Use Harmony's `DMAC_ChannelTransfer()` instead of direct `KVA_TO_PA()`
2. **ISR syntax:** Harmony callbacks instead of mikroC interrupt vectors
3. **Absolute addressing:** Dynamic allocation instead of `absolute 0xA0002000`
4. **Buffer sizes:** 500 bytes instead of 200 for extra safety

---

## Complete Function Mapping

| mikroC V1           | Our XC32 Port                   | Status      |
| ------------------- | ------------------------------- | ----------- |
| `DMA_global()`      | `DMA_global()`                  | ✅ Implement |
| `DMA0()`            | `DMA0()`                        | ✅ Implement |
| `DMA1()`            | `DMA1()`                        | ✅ Implement |
| `DMA_CH0_ISR()`     | `SerialDMA_Channel0_Callback()` | ⏸️ Partial   |
| `DMA_CH1_ISR()`     | `SerialDMA_Channel1_Callback()` | ❌ Missing   |
| `Get_Difference()`  | `Get_Difference()`              | ✅ Implement |
| `Get_Line()`        | `Get_Line()`                    | ✅ Implement |
| `dma_printf()`      | `dma_printf()`                  | ❌ Missing   |
| All other functions | Exact same names                | ❌ Missing   |

---

## Summary

**mikroC V1 File Structure:**
- **Serial_Dma.h** (API declarations, 70 lines)
- **Serial_Dma.c** (Complete implementation, 557 lines)

**Organization (10 sections):**
1. Global variables
2. Global DMA functions
3. DMA Channel 0 (RX) configuration
4. DMA Channel 0 ISR
5. Ring buffer functions
6. DMA Channel 1 (TX) configuration
7. DMA generic functions
8. DMA Channel 1 ISR
9. dma_printf function
10. lTrim function

**Our Port MUST:**
- ✅ Keep same file structure (serial_dma.h + serial_dma.c)
- ✅ Keep same function names (exact API match)
- ✅ Keep same organization (10 sections in same order)
- ✅ Keep same separation of concerns
- ✅ Port ALL functions (not just partial)

**Next Steps:**
1. Complete `serial_dma.h` with ALL function declarations
2. Complete `serial_dma.c` with ALL implementations
3. Maintain mikroC V1 structure throughout

This ensures easy maintenance and future reference! 🎯
