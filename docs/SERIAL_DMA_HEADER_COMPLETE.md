# Serial DMA Header - mikroC V1 API Complete (October 18, 2025)

## Summary

✅ **COMPLETE!** Rewrote `incs/serial_dma.h` with full mikroC V1 compatible API

**Status**: Header file now declares all 27 functions from mikroC V1 architecture
**User Approval**: "yes lets go" - approved starting complete port
**Code Safety**: User committed previous version before changes

## Changes Made

### 1. Header Guard and Includes
```c
// ADDED: stdarg.h for dma_printf() va_list
#include <stdarg.h>
```

### 2. Constants Updated
```c
// ADDED: TX buffer size
#define DMA_TX_BUFFER_SIZE      500

// ADDED: Channel definitions
#define DMA_CHANNEL_RX 0
#define DMA_CHANNEL_TX 1
```

### 3. Typedef Changed to mikroC V1 Format
```c
// OLD (XC32 style):
typedef struct {
    volatile char temp_buffer[500];
    volatile uint16_t head, tail, diff;
} SerialRingBuffer;

// NEW (mikroC V1 compatible):
typedef struct {
    char temp_buffer[500];      // NOT volatile
    int head, tail, diff;       // int not uint16_t
    char has_data : 1;          // ADDED flag
} Serial;
```

### 4. External Variables Added
```c
extern char rxBuf[DMA_RX_BUFFER_SIZE];
extern char txBuf[DMA_TX_BUFFER_SIZE];
extern Serial serial;
```

### 5. Function Declarations - Complete mikroC V1 API

**Section 1: Global DMA Functions (4 functions)**
- ✅ `void DMA_global(void)` - Initialize both channels
- ✅ `unsigned int DMA_Busy(void)` - Check global busy
- ✅ `unsigned int DMA_Suspend(void)` - Suspend all
- ✅ `unsigned int DMA_Resume(void)` - Resume all

**Section 2: DMA Channel 0 (RX) Functions (7 functions)**
- ✅ `void DMA0(void)` - Configure RX channel
- ✅ `char DMA0_Flag(void)` - Get interrupt flags
- ✅ `void DMA0_Enable(void)` - Enable channel
- ✅ `void DMA0_Disable(void)` - Disable channel
- ✅ `unsigned int DMA0_ReadDstPtr(void)` - Read pointer
- ✅ `void DMA0_RstDstPtr(void)` - Reset pointer
- ✅ `void DMA_CH0_ISR(uintptr_t context)` - RX interrupt

**Section 3: Ring Buffer Functions (6 functions)**
- ✅ `void Reset_rxBuff(int dif)` - Clear DMA buffer
- ✅ `int Get_Head_Value(void)` - Get head index
- ✅ `int Get_Tail_Value(void)` - Get tail index
- ✅ `int Get_Difference(void)` - Get byte count
- ✅ `void Get_Line(char *str, int length)` - Read line
- ✅ `void Reset_Ring(void)` - Reset ring buffer

**Section 4: DMA Channel 1 (TX) Functions (5 functions)**
- ✅ `void DMA1(void)` - Configure TX channel
- ✅ `char DMA1_Flag(void)` - Get interrupt flags
- ✅ `unsigned int DMA1_Enable(void)` - Start transmission
- ✅ `void DMA1_Disable(void)` - Disable channel
- ✅ `void DMA_CH1_ISR(uintptr_t context)` - TX interrupt

**Section 5: Generic DMA Functions (3 functions)**
- ✅ `unsigned int DMA_IsOn(int ch)` - Check channel enabled
- ✅ `unsigned int DMA_CH_Busy(int ch)` - Check channel busy
- ✅ `unsigned int DMA_Abort(int ch)` - Abort transfer

**Section 6: Printf and Utilities (2 functions)**
- ✅ `int dma_printf(const char *str, ...)` - **CRITICAL** formatted output
- ✅ `void lTrim(char *d, char *s)` - Left-trim zeros

## Removed Old Functions (SerialDMA_* Prefix)

**BEFORE (Partial - 8 functions):**
- ❌ `SerialDMA_Initialize()` → Replaced by `DMA_global()` + `DMA0()` + `DMA1()`
- ❌ `SerialDMA_Channel0_Callback()` → Replaced by `DMA_CH0_ISR()`
- ❌ `SerialDMA_GetDifference()` → Replaced by `Get_Difference()`
- ❌ `SerialDMA_GetLine()` → Replaced by `Get_Line()`
- ❌ `SerialDMA_ResetRing()` → Replaced by `Reset_Ring()`
- ❌ `SerialDMA_GetHead()` → Replaced by `Get_Head_Value()`
- ❌ `SerialDMA_GetTail()` → Replaced by `Get_Tail_Value()`
- ❌ `SerialDMA_GetIntFlag()` → Replaced by `DMA0_Flag()` + `DMA1_Flag()`

**AFTER (Complete - 27 functions):**
- All mikroC V1 function names
- No "SerialDMA_" prefix
- Easy to reference mikroC source code

## Documentation Style

All functions include:
- ✅ Brief description (`@brief`)
- ✅ Return value documentation (`@return`)
- ✅ Parameter documentation (`@param`)
- ✅ mikroC V1 line number references
- ✅ Implementation notes for critical functions

Example:
```c
/**
 * @brief Send formatted string via DMA TX (CRITICAL FUNCTION)
 * @param str Format string (printf-compatible)
 * @param ... Variable arguments
 * @return Number of bytes sent, 0 if busy
 * @details Replaces UGS_Printf for debug output during motion
 * 
 * Supported format specifiers:
 *   %c - char, %d - int, %u - unsigned, %l - long
 *   %X - hex, %f/%F - float, %p - pointer, %s - string
 * 
 * mikroC Reference: Serial_Dma.c line 447
 */
int dma_printf(const char *str, ...);
```

## mikroC V1 Reference Mapping

| Header Line | mikroC Source    | Function          | Notes                    |
| ----------- | ---------------- | ----------------- | ------------------------ |
| 86          | Serial_Dma.c:18  | DMA_global()      | Initialize both channels |
| 93          | Serial_Dma.c:347 | DMA_Busy()        | Global busy check        |
| 100         | Serial_Dma.c:367 | DMA_Suspend()     | Suspend all DMA          |
| 107         | Serial_Dma.c:378 | DMA_Resume()      | Resume all DMA           |
| 123         | Serial_Dma.c:41  | DMA0()            | RX channel config        |
| 130         | Serial_Dma.c:91  | DMA0_Flag()       | RX interrupt flag        |
| 137         | Serial_Dma.c:95  | DMA0_Enable()     | Enable RX                |
| 143         | Serial_Dma.c:100 | DMA0_Disable()    | Disable RX               |
| 150         | Serial_Dma.c:105 | DMA0_ReadDstPtr() | Read RX pointer          |
| 156         | Serial_Dma.c:112 | DMA0_RstDstPtr()  | Reset RX pointer         |
| 168         | Serial_Dma.c:125 | DMA_CH0_ISR()     | RX interrupt handler     |
| 184         | Serial_Dma.c:170 | Reset_rxBuff()    | Clear RX buffer          |
| 191         | Serial_Dma.c:174 | Get_Head_Value()  | Get ring head            |
| 198         | Serial_Dma.c:178 | Get_Tail_Value()  | Get ring tail            |
| 205         | Serial_Dma.c:182 | Get_Difference()  | Get byte count           |
| 213         | Serial_Dma.c:195 | Get_Line()        | Read line                |
| 219         | Serial_Dma.c:211 | Reset_Ring()      | Reset ring buffer        |
| 235         | Serial_Dma.c:245 | DMA1()            | TX channel config        |
| 242         | Serial_Dma.c:303 | DMA1_Flag()       | TX interrupt flag        |
| 249         | Serial_Dma.c:307 | DMA1_Enable()     | Start TX                 |
| 255         | Serial_Dma.c:312 | DMA1_Disable()    | Disable TX               |
| 267         | Serial_Dma.c:324 | DMA_CH1_ISR()     | TX interrupt handler     |
| 284         | Serial_Dma.c:389 | DMA_IsOn()        | Check channel enabled    |
| 292         | Serial_Dma.c:402 | DMA_CH_Busy()     | Check channel busy       |
| 300         | Serial_Dma.c:356 | DMA_Abort()       | Abort transfer           |
| 330         | Serial_Dma.c:447 | dma_printf()      | **Formatted output**     |
| 338         | Serial_Dma.c:539 | lTrim()           | Left-trim utility        |

## Benefits of mikroC V1 Compatible API

### 1. **Maintainability** ✅
- Same function names as reference code
- Easy to compare with working mikroC implementation
- Copy-paste mikroC algorithms directly

### 2. **Feature Parity** ✅
- All 27 functions available
- Complete DMA control (enable, disable, abort, suspend, resume)
- Debug output via dma_printf()

### 3. **Separation of Concerns** ✅
- 6 logical sections (Global, DMA0, Ring, DMA1, Generic, Printf)
- Clear organization matching mikroC structure
- Each section has specific responsibility

### 4. **Future-Proof** ✅
- Easy to add new mikroC functions
- Clear API contract
- Extensible for additional DMA channels if needed

### 5. **No Prefix Pollution** ✅
- Functions use natural names (DMA0, Get_Line, dma_printf)
- NOT generic names that clash (uart_init, get_data)
- Matches industrial embedded patterns (mikroC, Harmony)

## Next Steps (Priority Order)

### IMMEDIATE: Restructure serial_dma.c (Task 2)
1. Rename global variables (rxBuf, txBuf, serial)
2. Rename existing functions (remove SerialDMA_ prefix)
3. Add txBuf[500] array
4. Add dma1int_flag variable
5. Reorganize into 10 sections matching mikroC

### Priority 2: Implement DMA1 Functions (Task 3)
6. DMA1() - TX channel configuration
7. DMA1_Enable() - Start transmission
8. DMA1_Disable() - Stop transmission
9. DMA1_Flag() - Get TX interrupt flag
10. DMA_CH1_ISR() - TX interrupt callback

### Priority 3: Implement dma_printf (Task 4) **CRITICAL**
11. dma_printf() - Use vsnprintf() for XC32 advantage
12. Check DMA_CH_Busy(1) before transmission
13. Copy to txBuf, set DCH1SSIZ, call DMA1_Enable()

### Priority 4: Implement Remaining Functions (Task 5)
14. DMA_global() - Initialize both channels
15. DMA_Busy() - Check global busy
16. DMA_Suspend() - Suspend all transfers
17. DMA_Resume() - Resume all transfers
18. DMA0_Flag() - Get RX interrupt flag
19. DMA0_Enable() - Enable RX channel
20. DMA0_Disable() - Disable RX channel
21. DMA0_ReadDstPtr() - Read RX pointer
22. DMA0_RstDstPtr() - Reset RX pointer
23. Reset_rxBuff() - Clear RX buffer
24. DMA_IsOn() - Check channel enabled
25. DMA_CH_Busy() - Check channel busy
26. DMA_Abort() - Abort transfer
27. lTrim() - Left-trim zeros

### Priority 5: Update main.c (Task 6)
28. Replace SerialDMA_Initialize() with DMA_global()
29. Replace SerialDMA_GetDifference() with Get_Difference()
30. Replace SerialDMA_GetLine() with Get_Line()
31. Replace UGS_Printf() with dma_printf() (throughout codebase)

### Priority 6: Testing (Tasks 7-9)
32. Test RX pattern match with UGS '?' message
33. Test TX with dma_printf() formatted output
34. Test coordinate systems (G92, G91) with working serial
35. Full motion testing with debug output

## Verification Checklist

**Header File (serial_dma.h):**
- ✅ All 27 function prototypes declared
- ✅ mikroC V1 compatible naming
- ✅ Sections 1-6 organized
- ✅ Complete documentation with line references
- ✅ Typedef changed to mikroC format (Serial, not SerialRingBuffer)
- ✅ External variables declared (rxBuf, txBuf, serial)
- ✅ stdarg.h included for dma_printf

**Implementation File (serial_dma.c):**
- ⏸️ **NEXT TASK** - Rename existing functions
- ⏸️ Add missing 19 functions
- ⏸️ Reorganize into 10 sections

**Main Application (main.c):**
- ⏸️ Update function calls to mikroC names
- ⏸️ Replace UGS_Printf with dma_printf

**Build System:**
- ⏸️ Verify compilation with new API
- ⏸️ Fix any undefined reference errors
- ⏸️ Test hex file generation

## Success Criteria

**Phase 1 (Header) - COMPLETE** ✅:
- ✅ All 27 functions declared
- ✅ mikroC V1 compatible names
- ✅ Complete documentation

**Phase 2 (Implementation) - IN PROGRESS** 🔄:
- Restructure serial_dma.c
- Implement all 27 functions
- Build without errors

**Phase 3 (Integration) - PENDING** ⏸️:
- Update main.c to use new API
- Replace UGS_Printf with dma_printf
- Test with hardware

**Phase 4 (Validation) - PENDING** ⏸️:
- RX pattern match working
- TX dma_printf working
- Coordinate systems working
- Full motion control validated

## Status

✅ **Task 1 COMPLETE!** Header file rewritten with complete mikroC V1 API
🔄 **Task 2 STARTING!** Restructure serial_dma.c implementation

**User Decision**: "yes lets go" + "i have already commit our current version"
**Safe to Proceed**: Previous code committed, can refactor aggressively
**Target**: Match mikroC V1 exactly for maintainability and feature parity
