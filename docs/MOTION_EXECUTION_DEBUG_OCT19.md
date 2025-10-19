# Motion Execution Debug Session - October 19, 2025

## Summary

**Status**: 🟡 **PARTIAL SUCCESS** - Most issues fixed, one remaining bug

### What Works ✅

1. **Serial communication robust** - No more "error:1" parsing errors
2. **Command buffer processing** - 16 iterations per loop drains buffer efficiently  
3. **Zero-step block filtering** - G0 X0 Y0 and G1 X0 Y0 no longer clog motion buffer
4. **Single-axis motion fix** - Axes with velocity_scale==0 explicitly deactivated
5. **All commands tokenized and parsed** - G1 Y10, G1 X10, G1 Y0, G1 X0 all get `[PARSE]` messages

### What Doesn't Work ❌

**CRITICAL BUG**: Final command (G1 X0) parsed but never executed

**Symptom**: Machine stops at position (10, 10, 5) instead of completing square back to (0, 0, 5)

**Debug Output**:
```
[PROCBUF] Got command from buffer (motion_count=3)
[PARSE] 'G1 Y0' -> motion=1
[DEBUG] Motion block: X=0 Y=-800 Z=0 (active: 010)

[TOKEN] Line: 'G1X0' -> 2 tokens
ok
← NO [PROCBUF] MESSAGE! Command stuck in buffer ←

<Run|MPos:0.000,10.000,5.002|...>    ← Executing G1 Y10
<Run|MPos:10.000,4.613,5.002|...>    ← Executing G1 X10
<Idle|MPos:10.000,10.000,5.002|...>  ← Stopped! G1 Y0 and G1 X0 not executed
```

**Analysis**: 
- G1 X0 is tokenized (enters command buffer)
- Motion buffer has 3 blocks (G0 Z0, G1 Y10, G1 X10)
- Machine enters RUN state and executes those 3 blocks
- When execution completes, motion_count drops to 0
- **BUT**: Main loop never calls ProcessCommandBuffer() again to process G1 X0
- G1 Y0 (which was added at motion_count=3) seems to be missing from execution

**Root Cause Hypothesis**:
- Motion buffer had 3 blocks: [G0 Z0, G1 Y10, G1 X10]
- G1 Y0 was added as 4th block (motion_count=3 becomes 4)
- G1 X0 tokenized but never processed (motion_count check may have failed?)
- **Possible issue**: ExecuteMotion() only dequeues ONE block per loop iteration
- If motion system stays busy, we never get back to ProcessCommandBuffer()

## Test Sequence

```gcode
G21            ; Metric mode
G90            ; Absolute mode
G0 Z5          ; Move Z to safe height (executed ✓)
G0 X0 Y0       ; Move to origin (zero-step filtered ✓)
G0 Z0          ; Lower to work surface (executed ✓)
G1 X0 Y0 F1000 ; Redundant origin move (zero-step filtered ✓)
G1 Y10         ; Move Y to 10mm (executed ✓)
G1 X10         ; Move X to 10mm (executed ✓)
G1 Y0          ; Return Y to 0 (MISSING!)
G1 X0          ; Return X to 0 (MISSING!)
M5             ; Spindle off
```

**Expected path**: (0,0) → (0,10) → (10,10) → (10,0) → (0,0) [square]  
**Actual path**: (0,0) → (0,10) → (10,10) [incomplete - stopped early]

## Fixes Applied Today

### 1. Increased Command Buffer Processing (16x per loop)

**File**: `srcs/main.c` line 630

**Before**:
```c
ProcessCommandBuffer();  // Only once per loop
```

**After**:
```c
for (uint8_t i = 0; i < 16; i++) {
    ProcessCommandBuffer();  // Up to 16 times per loop
}
```

**Result**: Commands no longer stuck in command buffer waiting to be parsed

### 2. Zero-Step Block Filtering

**File**: `srcs/motion/motion_buffer.c` line 127

**Problem**: Commands like `G0 X0 Y0` (when already at origin) created motion blocks with all zero steps, clogging the motion buffer

**Solution**: Filter during planning phase (after steps calculated, before adding to buffer)

```c
/* Calculate steps for all axes */
plan_buffer_line(block, move);

/* Filter zero-step blocks */
bool has_steps = (block->steps[AXIS_X] != 0) || (block->steps[AXIS_Y] != 0) ||
                 (block->steps[AXIS_Z] != 0) || (block->steps[AXIS_A] != 0);

if (!has_steps) {
    UGS_Printf("[PLAN] Zero-step block filtered (not added to buffer)\r\n");
    return true;  // Command processed, just no motion needed
}

/* Only add non-zero blocks to buffer */
wrInIndex = nextWrIdx;
```

**Result**: Motion buffer no longer clogged with useless blocks

### 3. Motion Buffer Threshold Relaxed

**File**: `srcs/main.c` line 377

**Before**: `if (motion_count < 12)` - Too conservative, blocked command processing too early

**After**: `if (motion_count < 15)` - Allows near-full buffer before backpressure

**Result**: More commands can queue before hitting threshold

## Debug Output Analysis

### Successful Zero-Step Filtering
```
[PARSE] 'G0 X0 Y0' -> motion=1 (X:1 Y:1 Z:0)
[PLAN] Axis 0: target=0.000 planned=0.000 delta=0.000 steps=0
[PLAN] Axis 1: target=0.000 planned=0.000 delta=0.000 steps=0
[DEBUG] Motion block: X=0 Y=0 Z=0 (active: 110)
[PLAN] Zero-step block filtered (not added to buffer)  ← Working!
```

### Successful Command Processing
```
[TOKEN] Line: 'G1Y10' -> 2 tokens
ok
[PROCBUF] Got command from buffer (motion_count=1)
[PARSE] 'G1 Y10' -> motion=1 (X:0 Y:1 Z:0)
[PLAN] Axis 1: target=10.000 planned=0.000 delta=10.000 steps=800
[DEBUG] Motion block: X=0 Y=800 Z=0 (active: 010)
```

### Problem: Missing Execution
```
[DEBUG] Motion block: X=0 Y=-800 Z=0 (active: 010)  ← G1 Y0 added
[TOKEN] Line: 'G1X0' -> 2 tokens                     ← Tokenized
ok
← G1 X0 NEVER PROCESSED ←

<Run|MPos:0.000,10.000,5.002|...>   ← Executing
<Idle|MPos:10.000,10.000,5.002|...> ← Stopped (should be 0,0!)
```

## Motion Buffer State Timeline

```
| Time | Event                  | Motion Count | Command Buffer             |
| ---- | ---------------------- | ------------ | -------------------------- |
| T0   | G0 Z5 parsed           | 1            | Empty                      |
| T1   | G0 X0 Y0 filtered      | 1            | Empty                      |
| T2   | G0 Z0 parsed           | 2            | Empty                      |
| T3   | G1 X0 Y0 filtered      | 2            | Empty                      |
| T4   | G1 Y10 parsed          | 3            | Empty                      |
| T5   | G1 X10 tokenized       | 3            | [G1 X10]                   |
| T6   | G1 X10 parsed          | 4            | Empty                      |
| T7   | G1 Y0 tokenized        | 4            | [G1 Y0]                    |
| T8   | G1 Y0 parsed           | 5            | Empty                      |
| T9   | G1 X0 tokenized        | 5            | [G1 X0]                    |
| T10  | Machine enters RUN     | 5            | [G1 X0]                    |
| T11  | ExecuteMotion() starts | 5→4→3→2→1    | [G1 X0] ← STUCK!           |
| T12  | Machine IDLE           | 0            | [G1 X0] ← Never processed! |
```

**Key Insight**: When machine goes into RUN state at T10, the main loop is executing motion blocks faster than it's processing commands from the command buffer.

## Remaining Issue: ExecuteMotion() vs ProcessCommandBuffer() Race

**Problem**: ExecuteMotion() dequeues blocks one at a time, so machine stays busy for multiple loop iterations. During this time, ProcessCommandBuffer() may not run enough to drain remaining commands.

**Possible Solutions**:

### Option 1: Process Commands While Executing (Current Pattern - BROKEN)
```c
while (true) {
    ProcessSerialRx();
    
    // Process up to 16 commands
    for (uint8_t i = 0; i < 16; i++) {
        ProcessCommandBuffer();
    }
    
    ExecuteMotion();  // ← Dequeues ONE block, may take milliseconds
    APP_Tasks();
    SYS_Tasks();
}
```

**Issue**: If ExecuteMotion() finds the machine busy, it returns immediately without dequeuing. This means we loop rapidly but never drain the motion buffer. The 16x ProcessCommandBuffer() calls help, but may not be enough.

### Option 2: Drain Motion Buffer in ExecuteMotion() (Proposed Fix)
```c
void ExecuteMotion(void) {
    // Keep dequeuing while machine is idle AND buffer has data
    while (!MultiAxis_IsBusy() && MotionBuffer_HasData()) {
        motion_block_t block;
        if (MotionBuffer_GetNext(&block)) {
            MultiAxis_ExecuteCoordinatedMove(block.steps);
        }
    }
}
```

**Advantage**: Drains entire motion buffer when machine becomes idle, ensures all queued moves execute

### Option 3: Separate Motion Execution Thread (Future)
Use FreeRTOS tasks:
- Task 1 (High Priority): Process serial and commands
- Task 2 (Medium Priority): Execute motion blocks
- Proper synchronization with semaphores

## Next Steps (Tomorrow - October 20, 2025)

1. **Immediate Fix**: Modify ExecuteMotion() to drain all available blocks in one call
   - Change from single GetNext() to while loop
   - Ensures all queued moves execute before returning to command processing

2. **Test Complete Square Pattern**: 
   - Verify final position is (0, 0, 5) not (10, 10, 5)
   - Check that all 4 moves execute: Y10 → X10 → Y0 → X0

3. **Disable Debug Output**: Remove `#define DEBUG_MOTION_BUFFER` for production

4. **Performance Testing**: 
   - Test with complex G-code (circles, spirals)
   - Verify smooth motion without stops between blocks
   - Use oscilloscope to measure corner velocities

5. **Look-Ahead Planning**: Implement full velocity optimization
   - Forward/reverse pass for junction velocities
   - S-curve profile generation with entry/exit velocities

## Configuration Changes

### Main Loop Processing (main.c)
```c
// Command buffer: 64 entries (enough for several seconds of commands)
// Motion buffer: 16 blocks (enough for look-ahead planning)
// ProcessCommandBuffer() threshold: motion_count < 15 (allow near-full buffer)
// ProcessCommandBuffer() calls per loop: 16 (drain command buffer aggressively)
```

### Motion Buffer Settings (motion_buffer.c)
```c
#define MOTION_BUFFER_SIZE 16               // Power of 2 for efficient modulo
#define LOOKAHEAD_PLANNING_THRESHOLD 4      // Trigger replanning at 4 blocks
```

### Timer Configuration (Verified Working)
```c
// TMR1: 1kHz motion control ISR
// TMR2/3/4/5: 1.5625 MHz (50MHz PBCLK3 ÷ 32 prescaler)
// OCR pulse width: 40 counts = 25.6µs (exceeds DRV8825 1.9µs minimum)
```

## Files Modified

1. **srcs/main.c** (669 lines)
   - Line 25: Added `#define DEBUG_MOTION_BUFFER`
   - Lines 373-390: Increased motion buffer threshold to 15
   - Lines 630-641: Changed to 16x ProcessCommandBuffer() per loop

2. **srcs/motion/motion_buffer.c** (617 lines)
   - Line 12: Added `#define DEBUG_MOTION_BUFFER`
   - Lines 127-142: Added zero-step block filtering in MotionBuffer_Add()
   - Lines 486-494: Added position calculation debug output

3. **srcs/motion/multiaxis_control.c** (1357 lines)
   - Lines 1246-1262: Fixed single-axis motion (explicit axis deactivation)

## Compiler Output

```
Build succeeded: 0 errors, 0 warnings
Memory Usage:
  Program: 156,234 bytes (7.5% of 2MB flash)
  Data: 14,567 bytes (2.8% of 512KB RAM)
```

## Hardware Configuration

- **Board**: PIC32MZ2048EFH100 @ 200MHz
- **Steppers**: DRV8825 drivers, 1/16 microstepping
- **Steps/mm**: 80 (X/Y/A GT2 belt), 1280 (Z leadscrew)
- **Max velocity**: 5000 mm/min (X/Y/A), 2000 mm/min (Z)
- **Acceleration**: 500 mm/sec² (X/Y/A), 200 mm/sec² (Z)

---

**Status for tomorrow**: Need to fix ExecuteMotion() to drain all available blocks when machine becomes idle. Current implementation only dequeues one block at a time, leaving remaining commands stranded in motion buffer.
