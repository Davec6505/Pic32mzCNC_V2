# DMA Two-Buffer Architecture - October 18, 2025

## Overview

This document explains the **two-buffer DMA architecture** used for UART2 reception, matching the proven mikroC V1 implementation.

---

## Two-Buffer Architecture

### mikroC V1 Reference (Pic32mzCNC/Serial_Dma.c)

```c
// Buffer 1: DMA hardware destination (lines 7-8)
char rxBuf[200] = {0} absolute 0xA0002000;  // DMA fills this

// Buffer 2: Ring buffer for main loop (Serial_Dma.h)
typedef struct {
    char temp_buffer[500];  // Main loop reads from this
    int head;
    int tail;
    int diff;
    char has_data: 1;
} Serial;

Serial serial;  // Global ring buffer instance
```

### Data Flow Diagram

```
UART2 Hardware (U2RXREG)
    ↓ (DMA Channel 0 - automatic transfer, byte by byte)
rxBuf[200] (DMA destination buffer)
    ↓ (DMA ISR - triggered by pattern match '?' or buffer full)
serial.temp_buffer[500] (Ring buffer)
    ↓ (Main loop - Get_Difference() and Get_Line())
Application (G-code parser)
```

---

## Why Two Buffers?

### Buffer 1: DMA Hardware Destination (`rxBuf[200]`)

**Purpose**: Receive bytes from UART2 hardware via DMA

**Characteristics:**
- Fixed address in RAM (mikroC: `0xA0002000`)
- DMA hardware writes bytes here automatically
- Pattern match or buffer full triggers ISR
- **Small** (200 bytes in mikroC, 500 in our implementation)

**Why separate?**
- DMA needs a **contiguous, fixed buffer** to write to
- DMA can't write to a ring buffer (circular logic is software)
- Hardware fills this linearly until pattern match or full

### Buffer 2: Ring Buffer (`serial.temp_buffer[500]`)

**Purpose**: Store complete lines for main loop processing

**Characteristics:**
- Larger buffer (500 bytes) for buffering multiple commands
- Ring buffer logic (head/tail pointers)
- Main loop reads from here using `Get_Difference()` and `Get_Line()`
- **Persistent** across multiple DMA transfers

**Why separate?**
- Main loop may be slow (parsing, motion planning, etc.)
- Multiple complete lines can queue up
- Decouples DMA speed from main loop speed

---

## mikroC DMA ISR Operation

### DMA_CH0_ISR (Serial_Dma.c lines 132-165)

```c
void DMA_CH0_ISR() {
    int i = 0;
    
    // Check if block complete (pattern match or buffer full)
    if (DCH0INTbits.CHBCIF == 1) {
        i = strlen(rxBuf);  // Get length of received data
    }
    
    // Wrap ring buffer if needed
    if (serial.head + i > 499)
        serial.head = 0;
    
    // Copy from DMA buffer → ring buffer
    strncpy(serial.temp_buffer + serial.head, rxBuf, i);
    serial.head += i;
    
    // Clear DMA buffer for next transfer
    memset(rxBuf, 0, i+2);
    
    // Clear interrupt flags
    DCH0INTCLR = 0x000000ff;
    IFS4CLR = 0x40;
    
    // DMA auto-enables for next transfer (CHAEN=1)
}
```

**Key Operations:**
1. **Check CHBCIF** (block complete interrupt flag)
2. **Calculate length** using `strlen(rxBuf)`
3. **Wrap check** (if head + i > 499, reset head to 0)
4. **Copy data** from `rxBuf` → `serial.temp_buffer`
5. **Advance head** pointer
6. **Clear DMA buffer** (`memset(rxBuf, 0, i+2)`)
7. **Clear interrupt flags**
8. **Auto-enable** - DMA restarts automatically (CHAEN bit)

---

## Our Implementation (serial_dma.c)

### Matching mikroC Architecture

```c
// Buffer 1: DMA hardware destination (line 34)
static uint8_t dma_rx_buffer[DMA_RX_BUFFER_SIZE] = {0};  // 500 bytes

// Buffer 2: Ring buffer (lines 37-42)
static volatile SerialRingBuffer serial = {
    .temp_buffer = {0},  // 500 bytes
    .head = 0,
    .tail = 0,
    .diff = 0
};
```

### Our DMA Callback (serial_dma.c lines 154-215)

```c
void SerialDMA_Channel0_Callback(DMAC_TRANSFER_EVENT event, uintptr_t context)
{
    int i = 0;
    
    // Handle errors
    if (event == DMAC_TRANSFER_EVENT_ERROR) {
        // Clear and restart
        memset((void*)dma_rx_buffer, 0, DMA_RX_BUFFER_SIZE);
        DMAC_ChannelTransfer(...);  // Restart DMA
        return;
    }
    
    // Transfer complete (pattern match or buffer full)
    if (event == DMAC_TRANSFER_EVENT_COMPLETE || 
        event == DMAC_TRANSFER_EVENT_HALF_COMPLETE)
    {
        // Calculate length (same as mikroC)
        i = strlen((char*)dma_rx_buffer);
        
        if (i > 0)
        {
            // Wrap ring buffer if needed (same as mikroC)
            if (serial.head + i > (SERIAL_RING_BUFFER_SIZE - 1))
            {
                serial.head = 0;
            }
            
            // Copy from DMA buffer → ring buffer (same as mikroC)
            strncpy((char*)(serial.temp_buffer + serial.head), 
                    (char*)dma_rx_buffer, 
                    i);
            serial.head += i;
            
            // Clear DMA buffer (same as mikroC)
            memset((void*)dma_rx_buffer, 0, i + 2);
        }
    }
    
    // Restart DMA (replaces mikroC's auto-enable)
    DMAC_ChannelTransfer(...);
}
```

---

## Key Differences: mikroC vs XC32/Harmony

### Address Conversion

**mikroC:**
```c
DCH0SSA = KVA_TO_PA(0xBF822230);  // U2RXREG
DCH0DSA = KVA_TO_PA(0xA0002000);  // rxBuf (absolute address)
```

**Our XC32/Harmony:**
```c
DMAC_ChannelTransfer(
    DMAC_CHANNEL_0,
    (const void*)&U2RXREG,      // Virtual address
    1,
    (const void*)dma_rx_buffer, // Virtual address
    DMA_RX_BUFFER_SIZE,
    1
);
// Harmony's DMAC_ChannelSetAddresses() handles conversion internally
```

**Why different?**
- mikroC uses absolute memory addresses (`0xA0002000`)
- XC32 uses virtual addresses with automatic conversion
- Harmony's `DMAC_ChannelSetAddresses()` handles:
  - KSEG0/KSEG1 conversion (standard RAM)
  - KSEG2 conversion (EBI, SQI special regions)
  - Physical address calculation

### Auto-Enable Behavior

**mikroC:**
```c
DCH0CONSET = 0x0513;  // CHAEN=1 (bit 4) - hardware auto-enables
// After ISR completes, DMA restarts automatically
```

**Our XC32/Harmony:**
```c
DCH0CONSET = 0x0513;  // Same CHAEN=1 bit set
// BUT: We also call DMAC_ChannelTransfer() at end of ISR for safety
// This ensures DMA restarts even if auto-enable doesn't work
```

**Why both?**
- mikroC's `CHAEN=1` should auto-restart
- Harmony's callback may interfere with auto-enable
- Calling `DMAC_ChannelTransfer()` guarantees restart
- Belt-and-suspenders approach for reliability

---

## Pattern Match Trigger Behavior

### Pattern Match on '?' Character

**mikroC Configuration (Serial_Dma.c lines 48-50):**
```c
DCH0ECON = (146 << 8) | 0x30;  // PATEN | SIRQEN
DCH0DAT = '?';                 // Pattern match byte
DCH0CONSET = 0x0513;           // PATLEN=1 | CHEN | CHAEN | PRIOR
```

**Our Configuration (serial_dma.c lines 88-101):**
```c
DCH0DAT = '?';          // Pattern match byte (0x3F)
DCH0ECONSET = 0x20;     // Set PATEN bit (enable pattern match)
DCH0CONSET = 0x0513;    // PATLEN=1 | CHEN | CHAEN | PRIOR=3
```

### When DMA Triggers

**Trigger Condition 1: Pattern Match**
- DMA sees '?' (0x3F) in data stream
- Immediately sets CHBCIF flag (block complete)
- ISR fires, `rxBuf` contains all bytes up to and including '?'

**Example:**
```
Input: "G90" + '?' + "G1 X10\n"
DMA triggers at '?', rxBuf = "G90?"
ISR copies "G90?" → serial.temp_buffer
DMA resumes, will trigger again at next '?' or buffer full
```

**Trigger Condition 2: Buffer Full**
- DMA fills all 500 bytes of `dma_rx_buffer`
- Automatically sets CHBCIF flag
- ISR fires, `rxBuf` contains 500 bytes

**Example:**
```
Input: Long G-code line without '?'
After 500 bytes, DMA triggers anyway
ISR copies 500 bytes → serial.temp_buffer
```

---

## Main Loop Access Pattern

### mikroC Main Loop (Pic32mzCNC/Main.c)

```c
if (Get_Difference() > 0) {  // Check if data available
    Get_Line(str, Get_Difference());  // Copy from ring buffer
    // Process str...
}
```

### Our Main Loop (main.c)

```c
if (SerialDMA_GetDifference() > 0) {  // Check if data available
    int line_length = SerialDMA_GetDifference();
    SerialDMA_GetLine(line_buffer, line_length);  // Copy from ring buffer
    // Process line_buffer...
}
```

**Identical Pattern:**
1. Check `head - tail` to see if bytes available
2. Copy exactly `diff` bytes from `temp_buffer[tail]` to output
3. Advance `tail` pointer
4. Process the line

---

## Buffer Size Rationale

### mikroC Sizes

- `rxBuf[200]` - DMA destination
- `serial.temp_buffer[500]` - Ring buffer

**Why 200 for DMA buffer?**
- Typical G-code line: 20-50 bytes
- Pattern match '?' should trigger before 200 bytes
- Safety margin for long lines or burst data

**Why 500 for ring buffer?**
- Store ~10-25 complete lines
- Main loop can be slow (parsing, motion planning)
- Decouples DMA speed from main loop speed

### Our Sizes

- `dma_rx_buffer[500]` - DMA destination
- `serial.temp_buffer[500]` - Ring buffer

**Why 500 for both?**
- Extra safety margin for burst commands from UGS
- Modern CPU (200MHz) can handle larger buffers
- UGS can send rapid commands during streaming
- Memory not constrained (2MB RAM available)

---

## Critical Design Decisions

### 1. Pattern Match on '?' (Not '\n')

**Reasoning:**
- UGS first message is '?' without '\n'
- If pattern was '\n', first message would hang
- '?' triggers immediately, handles initialization

**Trade-off:**
- Commands without '?' accumulate until buffer full
- UGS sends '?' every 200ms, so acceptable delay
- Alternative: Use 2-byte pattern '\r\n' (more complex)

### 2. Two Buffers (Not One)

**Reasoning:**
- DMA needs fixed, contiguous buffer
- Main loop needs ring buffer for queuing
- Separating concerns improves reliability

**Trade-off:**
- Extra memory (1000 bytes total vs 500)
- Extra copy operation (DMA buffer → ring buffer)
- But: Proven working in mikroC for years

### 3. Manual Register Configuration

**Reasoning:**
- MCC doesn't expose pattern match in GUI
- Must write DCH0DAT, DCH0ECONSET, DCH0CONSET directly
- Matches mikroC register values exactly

**Trade-off:**
- MCC regeneration could overwrite (mitigated by comments)
- Less portable to other chips
- But: Only way to enable pattern matching

### 4. Harmony Address Conversion

**Reasoning:**
- XC32 and mikroC use different address spaces
- Harmony's DMAC_ChannelTransfer() handles conversion
- Avoids XC32-specific KVA_TO_PA issues

**Trade-off:**
- Must call DMAC_ChannelTransfer() at end of ISR
- Can't rely on CHAEN auto-enable alone
- But: More reliable, less compiler-dependent

---

## Testing Verification

### Expected Behavior

1. **Power-on**: DMA starts automatically (CHEN=1)
2. **First '?' from UGS**: DMA triggers, ISR copies to ring buffer
3. **Main loop**: Retrieves '?' from ring buffer, processes status query
4. **Subsequent commands**: Accumulate until next '?' or buffer full
5. **UGS periodic '?'**: Every 200ms, flushes accumulated commands

### Debug Output (with DEBUG_MOTION_BUFFER defined)

```
[DMA] Initialized with pattern='?' (0x3F)
[DMA] Buffer size=500 bytes
[DMA] Ring buffer=500 bytes
[DMA] Received 1 bytes, head=1
[DMA] Data: ?
[MAIN] Received line (1 bytes): ?
```

### Troubleshooting

**Problem**: No data received
- Check: `DCH0CON` has CHEN=1 (bit 7)
- Check: `DCH0ECON` has PATEN=1 (bit 5)
- Check: `DCH0DAT` = 0x3F ('?')

**Problem**: Data accumulates but doesn't trigger
- Check: UGS sending periodic '?' queries
- Check: DMA buffer size (should be 500 bytes)
- Try: Send '?' manually via serial terminal

**Problem**: Partial lines received
- Check: ISR copies all bytes (`strlen()` correct?)
- Check: Ring buffer wrap logic (head reset at 499)
- Check: DMAC_ChannelTransfer() restarts DMA

---

## Summary

**Architecture**: Two-buffer DMA with pattern matching on '?'
- Buffer 1: `dma_rx_buffer[500]` (DMA hardware destination)
- Buffer 2: `serial.temp_buffer[500]` (Ring buffer for main loop)

**mikroC Compatibility**: Exact register values, same ISR logic
- Pattern match byte: '?' (0x3F)
- Auto-enable: CHAEN=1 (bit 4 of DCH0CON)
- Priority: 3 (bits 1-0 of DCH0CON)

**XC32 Adaptation**: Use Harmony address conversion
- DMAC_ChannelTransfer() handles virtual→physical addresses
- Manual register writes for pattern match configuration
- Explicit DMA restart in ISR for reliability

**Status**: Ready for testing! 🚀
