# Command Buffer Integration - Build & Test Guide

## Date: October 17, 2025

## Changes Summary

### New Files Created
1. **`incs/command_buffer.h`** - Command buffer API (64-entry ring buffer)
2. **`srcs/command_buffer.c`** - Command separation implementation
3. **`docs/COMMAND_BUFFER_ARCHITECTURE.md`** - Architecture documentation
4. **`docs/PHASE2_NON_BLOCKING_PROTOCOL.md`** - Non-blocking protocol guide

### Modified Files
1. **`srcs/main.c`** - Integrated three-stage pipeline:
   - Added `#include "command_buffer.h"`
   - Added `#include <string.h>`
   - Renamed `ProcessGCode()` → `ProcessSerialRx()`
   - Added `ProcessCommandBuffer()` (NEW!)
   - Updated `main()` - Added `CommandBuffer_Initialize()`
   - Updated main loop - Three-stage pipeline

---

## Architecture: Three-Stage Pipeline

```
┌──────────────────────────────────────────────────────────────┐
│ Stage 1: ProcessSerialRx() - Serial → Command Buffer        │
│ - UART RX: "G92G0X10Y10F200\n"                              │
│ - Tokenize: ["G92", "G0", "X10", "Y10", "F200"]            │
│ - Split: Command[0]=G92, Command[1]=G0 X10 Y10 F200        │
│ - Send "ok" immediately (~175µs response!)                  │
│ - Buffer: 64 commands (2KB)                                 │
└──────────────────────────────────────────────────────────────┘
                          ↓
┌──────────────────────────────────────────────────────────────┐
│ Stage 2: ProcessCommandBuffer() - Command → Motion Buffer   │
│ - Dequeue command from 64-entry buffer                      │
│ - Reconstruct line: ["G0", "X10", "Y10"] → "G0 X10 Y10"    │
│ - Parse: GCode_ParseLine() → parsed_move_t                 │
│ - Add: MotionBuffer_Add() → motion_block_t                 │
│ - Buffer: 16 motion blocks (2KB)                            │
│ - Happens in background while moving!                       │
└──────────────────────────────────────────────────────────────┘
                          ↓
┌──────────────────────────────────────────────────────────────┐
│ Stage 3: ExecuteMotion() - Motion → Hardware                │
│ - Dequeue: MotionBuffer_GetNext() → motion_block_t         │
│ - Execute: MultiAxis_ExecuteCoordinatedMove()              │
│ - Hardware: OCR modules generate step pulses                │
└──────────────────────────────────────────────────────────────┘
```

**Total Pipeline Depth**: 80 commands (64 command buffer + 16 motion buffer)

---

## Build Instructions

### Step 1: Clean Build
```powershell
make clean
make all
```

**Expected Output**:
```
######  BUILDING   ########
Compiling: command_buffer.c
Compiling: main.c
...
Linking: CS23.elf
######  BIN TO HEX  ########
######  BUILD COMPLETE  ########
```

### Step 2: Flash Firmware
```powershell
# Use your preferred programmer to flash bins/CS23.hex
```

### Step 3: Connect via UGS
- Port: COM4 (or your serial port)
- Baud: 115200
- Firmware: GRBL

---

## Testing Strategy

### Test 1: Command Separation Verification

**Objective**: Verify commands split correctly

**Test Command**:
```gcode
G92G0X10Y10F200G1X20Y20F100M5
```

**Expected Behavior**:
1. Tokenization: `["G92", "G0", "X10", "Y10", "F200", "G1", "X20", "Y20", "F100", "M5"]`
2. Command separation:
   - Command[0]: `G92`
   - Command[1]: `G0 X10 Y10 F200`
   - Command[2]: `G1 X20 Y20 F100`
   - Command[3]: `M5`
3. UGS receives "ok" immediately (within 1ms)
4. All 4 commands execute in order

**Verification**:
- ✅ Check UGS console for rapid "ok" response
- ✅ Observe position updates in UGS DRO
- ✅ Verify machine moves to (10,10) then (20,20)
- ✅ Verify spindle off (M5) at end

---

### Test 2: Deep Buffer Fill Test

**Objective**: Fill 64-command buffer and verify flow control

**Test Commands** (send rapidly):
```gcode
G90
G0 X0 Y0
G1 X10 F1000
G1 X20 F1000
G1 X30 F1000
... (repeat 70+ moves)
```

**Expected Behavior**:
1. First 64 commands: "ok" received immediately
2. Commands 65-70: No "ok" (buffer full)
3. UGS automatically retries commands 65-70
4. As motion executes, buffer space opens up
5. All 70+ commands eventually execute

**Verification**:
- ✅ Monitor command buffer count (if debug enabled)
- ✅ UGS doesn't report errors (handles retry internally)
- ✅ All moves execute without drops

---

### Test 3: Non-Blocking Protocol Speed Test

**Objective**: Measure "ok" response time

**Test Command**:
```gcode
G0 X10
```

**Expected Timing**:
- Tokenization: ~100µs
- Command split: ~50µs
- Serial TX "ok": ~20µs
- **Total: ~170µs** (vs 100ms+ with blocking protocol!)

**Verification**:
- ✅ Use oscilloscope or logic analyzer on UART TX
- ✅ Measure time from line received to "ok" sent
- ✅ Should be <200µs

---

### Test 4: Continuous Motion Smoothness

**Objective**: Verify smooth cornering with buffer full

**Test Command** (draw square):
```gcode
G90
G0 X0 Y0 F3000
G1 X50 Y0 F1000
G1 X50 Y50 F1000
G1 X0 Y50 F1000
G1 X0 Y0 F1000
```

**Expected Behavior**:
- All commands buffered before first move completes
- Motion buffer has 4 linear moves
- Machine executes continuously (no stops at corners)
- **Current Phase 2**: May slow at corners (no look-ahead yet)
- **Future Phase 3**: Will optimize corner speeds

**Verification**:
- ✅ Use oscilloscope on step pulses
- ✅ Verify velocity doesn't drop to zero between moves
- ✅ Motion is continuous (even if slow at corners)

---

### Test 5: Real-Time Command Priority

**Objective**: Verify real-time commands bypass buffer

**Test Sequence**:
```gcode
# Fill buffer with long move
G1 X100 F500

# Immediately send feed hold
!

# Verify immediate stop
?

# Resume
~
```

**Expected Behavior**:
- `!` (feed hold) executes immediately (bypasses command buffer)
- Motion pauses instantly
- `?` (status query) shows "Hold" state
- `~` (cycle start) resumes motion

**Verification**:
- ✅ Feed hold stops within 1ms
- ✅ Status report shows correct state
- ✅ Resume continues from paused position

---

## Debugging

### Enable Command Buffer Debug Output

Add to `ProcessSerialRx()` after `CommandBuffer_SplitLine()`:

```c
uint8_t commands_added = CommandBuffer_SplitLine(&tokenized);

// DEBUG: Print command buffer contents
#ifdef DEBUG_COMMAND_BUFFER
CommandBuffer_DebugPrint();
#endif

if (commands_added > 0) {
    UGS_SendOK();
}
```

**Expected Debug Output**:
```
[CMD_BUF] Count: 2/64
[0] Type: 6, Tokens: G92 
[1] Type: 0, Tokens: G0 X10 Y10 F200
```

### Monitor Buffer Levels

Add to main loop:

```c
// DEBUG: Monitor buffer levels every 100ms
static uint32_t last_print = 0;
uint32_t now = millis();  // Implement if needed

if (now - last_print > 100) {
    UGS_Printf("[DEBUG] CMD:%u MOTION:%u\r\n", 
               CommandBuffer_GetCount(), 
               MotionBuffer_GetCount());
    last_print = now;
}
```

**Expected Output** (during streaming):
```
[DEBUG] CMD:32 MOTION:16  ← Both buffers filling
[DEBUG] CMD:45 MOTION:16  ← Command buffer growing
[DEBUG] CMD:64 MOTION:16  ← Command buffer full (flow control)
```

---

## Performance Metrics

### Memory Usage
```
Command Buffer: 64 × 32 bytes = 2,048 bytes (0.1% of 2MB RAM)
Motion Buffer:  16 × 128 bytes = 2,048 bytes (0.1% of 2MB RAM)
Total Overhead: 4,096 bytes (0.2% of 2MB RAM) ← TRIVIAL!
```

### Response Time
```
Old (Blocking):   ~100ms per command (wait for motion)
New (Non-Blocking): ~175µs per command (tokenize + split)
Speedup: ~570x faster!
```

### Buffering Capacity
```
At 1000mm/min with 10mm moves:
- Move time: 600ms
- Pipeline: 80 commands
- Buffered time: ~48 seconds of motion!
```

---

## Troubleshooting

### Issue: "Tokenization error"
**Cause**: Invalid G-code syntax
**Solution**: Check line for special characters, verify GRBL v1.1f syntax

### Issue: Commands not executing
**Cause**: Command buffer or motion buffer full
**Solution**: Monitor buffer counts, verify ExecuteMotion() is draining motion buffer

### Issue: Motion stops between moves
**Cause**: Expected! Look-ahead planning not yet implemented (Phase 3)
**Solution**: This is normal for Phase 2. Motion is continuous but may slow at corners.

### Issue: Rapid "ok" but slow motion
**Cause**: Commands buffering faster than execution
**Solution**: Normal! 80-command pipeline allows deep look-ahead

---

## Next Steps

### Phase 3: Look-Ahead Planning (Future)

Implement junction velocity optimization:

```c
void MotionBuffer_RecalculateAll(void)
{
    // Forward pass: Calculate maximum exit velocities
    for (each block in buffer) {
        max_exit_velocity = calculate_junction_velocity(current, next);
        block->exit_velocity = min(max_exit_velocity, max_entry_velocity);
    }
    
    // Reverse pass: Ensure acceleration limits
    for (each block in reverse) {
        if (exit_velocity > entry_velocity + accel * time) {
            entry_velocity = exit_velocity - accel * time;
        }
    }
    
    // Generate S-curve profiles with optimized velocities
    for (each block) {
        MotionMath_CalculateSCurveTiming(
            block->entry_velocity,
            block->exit_velocity,
            &block->profile
        );
    }
}
```

**Result**: TRUE smooth cornering - velocity never drops to zero!

---

## Success Criteria

✅ **Build Success**: No compilation errors  
✅ **Flash Success**: Firmware uploads without errors  
✅ **UGS Connection**: Connects as "GRBL 1.1f"  
✅ **Command Separation**: "G92G0X10" splits into 2 commands  
✅ **Fast Response**: "ok" within 1ms  
✅ **Buffer Fill**: Accepts 64+ commands  
✅ **Continuous Motion**: No complete stops between moves  
✅ **Real-Time Commands**: ?, !, ~, ^X work immediately  

---

**Document Version**: 1.0  
**Last Updated**: October 17, 2025  
**Status**: Ready for Testing 🚀
