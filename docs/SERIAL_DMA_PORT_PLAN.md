# Serial DMA Port - Complete Implementation Plan

## Current Status (October 18, 2025)

### ✅ What We Have
- `incs/serial_dma.h` - Partial API (only basic functions)
- `srcs/serial_dma.c` - Partial implementation (only DMA0 RX)
- Pattern match configuration (manual register setup)
- Two-buffer architecture (dma_rx_buffer + ring buffer)

### ❌ What's Missing (Complete mikroC V1 Port)

**Missing Functions (from mikroC V1):**

#### 1. Global DMA Functions
- [ ] `DMA_global()` - Initialize both channels
- [ ] `DMA_Busy()` - Check global DMA busy
- [ ] `DMA_Suspend()` - Suspend all channels  
- [ ] `DMA_Resume()` - Resume all channels

#### 2. DMA Channel 0 (RX) Functions
- [ ] `DMA0()` - Rename/restructure current `SerialDMA_Initialize()`
- [ ] `DMA0_Flag()` - Get interrupt flags
- [ ] `DMA0_Enable()` - Enable channel
- [ ] `DMA0_Disable()` - Disable channel
- [ ] `DMA0_ReadDstPtr()` - Read destination pointer
- [ ] `DMA0_RstDstPtr()` - Reset destination pointer
- [x] `DMA_CH0_ISR()` - Currently `SerialDMA_Channel0_Callback()` ✅

#### 3. DMA Channel 1 (TX) Functions
- [ ] `DMA1()` - Initialize TX channel
- [ ] `DMA1_Flag()` - Get interrupt flags
- [ ] `DMA1_Enable()` - Enable and start TX
- [ ] `DMA1_Disable()` - Disable TX
- [ ] `DMA_CH1_ISR()` - TX interrupt callback

#### 4. DMA Generic Functions
- [ ] `DMA_IsOn(int channel)` - Check if channel enabled
- [ ] `DMA_CH_Busy(int channel)` - Check if busy
- [ ] `DMA_Abort(int channel)` - Abort transfer

#### 5. Ring Buffer Functions
- [ ] `Reset_rxBuff(int dif)` - Internal buffer clear
- [ ] `Get_Head_Value()` - Rename current `SerialDMA_GetHead()`
- [ ] `Get_Tail_Value()` - Rename current `SerialDMA_GetTail()`
- [ ] `Get_Difference()` - Rename current `SerialDMA_GetDifference()`
- [ ] `Get_Line(char *str, int dif)` - Rename current `SerialDMA_GetLine()`
- [ ] `Reset_Ring()` - Rename current `SerialDMA_ResetRing()`

#### 6. Printf Functions
- [ ] `dma_printf(const char *str, ...)` - **CRITICAL - Most important missing piece!**
- [ ] `lTrim(char *d, char *s)` - Left-trim zeros

---

## File Structure Reorganization Plan

### Phase 1: Update Header File (serial_dma.h)

**Replace current header with complete mikroC V1 API:**

```c
// Section 1: Global DMA Functions
void DMA_global(void);
unsigned int DMA_Busy(void);
unsigned int DMA_Suspend(void);
unsigned int DMA_Resume(void);

// Section 2: DMA Channel 0 (RX) Functions  
void DMA0(void);
char DMA0_Flag(void);
void DMA0_Enable(void);
void DMA0_Disable(void);
unsigned int DMA0_ReadDstPtr(void);
void DMA0_RstDstPtr(void);

// Section 3: DMA Channel 1 (TX) Functions
void DMA1(void);
char DMA1_Flag(void);
unsigned int DMA1_Enable(void);
void DMA1_Disable(void);

// Section 4: DMA Generic Functions
unsigned int DMA_IsOn(int channel);
unsigned int DMA_CH_Busy(int channel);
unsigned int DMA_Abort(int channel);

// Section 5: Ring Buffer Functions
void Reset_rxBuff(int dif);
int Get_Head_Value(void);
int Get_Tail_Value(void);
int Get_Difference(void);
void Get_Line(char *str, int dif);
void Reset_Ring(void);

// Section 6: Printf Functions
int dma_printf(const char *str, ...);
void lTrim(char *d, char *s);
```

### Phase 2: Reorganize Implementation (serial_dma.c)

**Restructure to match mikroC V1 exactly:**

```c
// Section 1: Global Variables
char rxBuf[500];  // DMA RX buffer
char txBuf[500];  // DMA TX buffer  
Serial serial;    // Ring buffer
char dma0int_flag;
char dma1int_flag;

// Section 2: Global DMA Functions
void DMA_global() { ... }
unsigned int DMA_Busy() { ... }
unsigned int DMA_Suspend() { ... }
unsigned int DMA_Resume() { ... }

// Section 3: DMA Channel 0 (RX) Configuration
void DMA0() { ... }  // Replaces SerialDMA_Initialize()
char DMA0_Flag() { ... }
void DMA0_Enable() { ... }
void DMA0_Disable() { ... }
unsigned int DMA0_ReadDstPtr() { ... }
void DMA0_RstDstPtr() { ... }

// Section 4: DMA Channel 0 ISR
void SerialDMA_Channel0_Callback() { ... }  // Harmony callback

// Section 5: Ring Buffer Functions
void Reset_rxBuff(int dif) { ... }
int Get_Head_Value() { ... }  // Replaces SerialDMA_GetHead()
int Get_Tail_Value() { ... }  // Replaces SerialDMA_GetTail()
int Get_Difference() { ... }  // Replaces SerialDMA_GetDifference()
void Get_Line(char *str, int dif) { ... }  // Replaces SerialDMA_GetLine()
void Reset_Ring() { ... }  // Replaces SerialDMA_ResetRing()

// Section 6: DMA Channel 1 (TX) Configuration
void DMA1() { ... }
char DMA1_Flag() { ... }
unsigned int DMA1_Enable() { ... }
void DMA1_Disable() { ... }

// Section 7: DMA Generic Functions
unsigned int DMA_IsOn(int channel) { ... }
unsigned int DMA_CH_Busy(int channel) { ... }
unsigned int DMA_Abort(int channel) { ... }

// Section 8: DMA Channel 1 ISR
void SerialDMA_Channel1_Callback() { ... }  // Harmony callback

// Section 9: dma_printf Function
int dma_printf(const char *str, ...) { ... }

// Section 10: lTrim Function
void lTrim(char *d, char *s) { ... }
```

### Phase 3: Update main.c Integration

**Change from:**
```c
SerialDMA_Initialize();
if (SerialDMA_GetDifference() > 0) {
    SerialDMA_GetLine(line_buffer, SerialDMA_GetDifference());
}
```

**To:**
```c
DMA_global();  // Initialize both channels
if (Get_Difference() > 0) {
    Get_Line(line_buffer, Get_Difference());
}
```

---

## Critical: dma_printf Implementation

**This is the most important missing piece!** mikroC V1 uses this extensively for debug output.

### mikroC V1 Pattern
```c
int dma_printf(const char* str, ...) {
    va_list va;
    char buff[1500];
    
    // Check if TX busy
    if(DMA_CH_Busy(1)) return 0;
    
    // Format string with variable arguments
    va_start(va, str);
    // ... sprintf magic for %c, %d, %u, %l, %X, %f, %s ...
    
    // Copy to TX buffer
    strncpy(txBuf, buff, j+1);
    DCH1SSIZ = j;  // Set TX size
    while(!DMA1_Enable());  // Start transmission
    
    return j;
}
```

### Our XC32 Port Pattern
```c
int dma_printf(const char *str, ...) {
    va_list va;
    char buff[1500];
    
    // Check if TX busy
    if(DMA_CH_Busy(1)) return 0;
    
    // Format string with variable arguments
    va_start(va, str);
    vsnprintf(buff, sizeof(buff), str, va);  // XC32 supports this!
    va_end(va);
    
    // Copy to TX buffer
    int len = strlen(buff);
    strncpy(txBuf, buff, len+1);
    
    // Configure DMA Channel 1 transfer size
    DCH1SSIZ = len;
    
    // Start DMA transmission
    DMA1_Enable();
    
    return len;
}
```

**XC32 Advantage:** We can use `vsnprintf()` instead of manual parsing! Much simpler.

---

## Backward Compatibility with Current Code

### Option 1: Keep Both APIs (Wrapper Pattern)
```c
// Old API (keep for backward compatibility)
void SerialDMA_Initialize(void) { DMA_global(); }
int SerialDMA_GetDifference(void) { return Get_Difference(); }
void SerialDMA_GetLine(char *str, int len) { Get_Line(str, len); }
// ... etc

// New mikroC-compatible API (primary)
void DMA_global(void) { /* implementation */ }
int Get_Difference(void) { /* implementation */ }
void Get_Line(char *str, int dif) { /* implementation */ }
```

### Option 2: Complete Replacement (Cleaner)
- Remove all `SerialDMA_*` functions
- Use only mikroC V1 function names
- Update main.c to use new API
- **Recommended approach** for consistency

---

## Implementation Priority

### Priority 1: Essential Functions (Must have for basic operation)
1. `DMA_global()` - Initialize both channels
2. `DMA0()` - Configure RX channel
3. `DMA1()` - Configure TX channel
4. `Get_Difference()` - Check if data available
5. `Get_Line()` - Retrieve data

### Priority 2: TX Functions (Critical for debug output)
6. `dma_printf()` - **MOST IMPORTANT!** Debug and status output
7. `DMA1_Enable()` - Start TX transfer
8. `DMA_CH_Busy()` - Check TX busy before printf

### Priority 3: Control Functions (Nice to have)
9. `DMA0_Enable()` / `DMA0_Disable()` - RX control
10. `DMA1_Disable()` - TX control
11. `DMA_Suspend()` / `DMA_Resume()` - Global control

### Priority 4: Status Functions (Debugging)
12. `DMA0_Flag()` / `DMA1_Flag()` - Interrupt flags
13. `DMA_IsOn()` - Channel status
14. `DMA0_ReadDstPtr()` - Destination pointer

### Priority 5: Advanced Functions (Future)
15. `DMA_Abort()` - Abort transfer
16. `lTrim()` - String utilities
17. `Reset_rxBuff()` - Internal buffer management

---

## Testing Strategy

### Phase 1: RX Testing (Current)
- [x] DMA0 receives data ✅
- [x] Pattern match on '?' works ✅
- [x] Ring buffer fills correctly ✅
- [x] Get_Difference() works ✅
- [x] Get_Line() retrieves data ✅

### Phase 2: TX Testing (Next)
- [ ] DMA1 initializes correctly
- [ ] dma_printf("Hello\n") sends via DMA
- [ ] DMA_CH_Busy() prevents buffer overrun
- [ ] Multiple dma_printf() calls queue correctly

### Phase 3: Bidirectional Testing
- [ ] Receive command via DMA0
- [ ] Process command
- [ ] Send response via dma_printf()
- [ ] Verify UGS receives response

### Phase 4: Full Integration
- [ ] Replace all UGS_Printf() with dma_printf()
- [ ] Test status reports via dma_printf()
- [ ] Test error messages via dma_printf()
- [ ] Performance testing (streaming G-code)

---

## Benefits of Complete Port

### 1. Separation of Concerns ✅
- **Hardware config**: DMA0(), DMA1()
- **Channel control**: Enable/Disable functions
- **Status monitoring**: Flag, Busy, IsOn functions
- **Data management**: Ring buffer functions
- **Formatted output**: dma_printf()

### 2. Maintainability ✅
- Same function names as mikroC V1
- Easy to reference original code
- Clear organization (10 sections)
- Self-documenting structure

### 3. Feature Completeness ✅
- Full TX support (currently missing)
- Debug output via dma_printf()
- Error handling (Abort, Suspend)
- Status queries (Flags, Busy)

### 4. Future-Proof ✅
- All mikroC V1 features available
- Easy to add more features
- Consistent API for new code

---

## Next Steps

1. **Update serial_dma.h** - Add all mikroC V1 function declarations
2. **Reorganize serial_dma.c** - Match mikroC V1 structure (10 sections)
3. **Implement DMA1 functions** - TX channel support
4. **Implement dma_printf()** - **CRITICAL** for debug output
5. **Update main.c** - Use mikroC-compatible API
6. **Test TX/RX** - Bidirectional communication
7. **Replace UGS_Printf()** - Use dma_printf() throughout

**Ready to start implementation?** 🚀
