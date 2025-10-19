# Position Tracking Fix - October 19, 2025

## Critical Bug Discovered

**Symptom**: Machine ended at (10,10,0) instead of completing square pattern to (0,0,0)
- Test sequence: G1 Y10 → G1 X10 → G1 Y0 → G1 X0
- Expected final position: (0,0,0)
- Actual final position: (10,10,0)
- **All commands accepted and executed** but position feedback WRONG!

## Root Cause Analysis

### The Problem

**OCR callbacks always increment step counters**, even during backward motion:

```c
// multiaxis_control.c (BEFORE FIX)
static void OCMP5_StepCounter_X(uintptr_t context)
{
    axis_state[AXIS_X].step_count++;  // ❌ ALWAYS INCREMENTS!
}
```

**But the motion system tracks:**
- `axis_state[].step_count` = Progress within current move (0 → total_steps)
- `axis_state[].direction_forward` = Direction flag (forward/backward)

**The motion manager correctly applied direction:**
```
G1 Y0:  steps[AXIS_Y] = -800  → Started Y moving backward ✅
G1 X0:  steps[AXIS_X] = -800  → Started X moving backward ✅
```

**But position feedback used step_count directly:**
```c
// multiaxis_control.c (BEFORE FIX)
uint32_t MultiAxis_GetStepCount(axis_id_t axis)
{
    return axis_state[axis].step_count;  // ❌ Returns move progress, not position!
}
```

**Result**: Position appeared to increase during backward moves!

### The Architecture Gap

```
┌─────────────────────────────────────────────────────────────────┐
│ Motion Execution (multiaxis_control.c)                         │
│                                                                 │
│ axis_state[].step_count:  MOVE PROGRESS (0 → total_steps)     │
│                           • Reset to 0 at start of each move   │
│                           • Incremented by OCR callbacks       │
│                           • Counts UP for both directions!     │
│                           • NOT absolute position!             │
│                                                                 │
│ axis_state[].direction_forward:  Direction flag               │
│                                   • true = forward             │
│                                   • false = backward           │
└─────────────────────────────────────────────────────────────────┘
                            ↓
                  ❌ **MISSING LAYER** ❌
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ Position Feedback (UGS_SendStatusReport)                       │
│                                                                 │
│ MultiAxis_GetStepCount(axis):  Should return ABSOLUTE position│
│                                 • What UGS displays            │
│                                 • What GRBL tracks             │
│                                 • Signed: can be negative      │
└─────────────────────────────────────────────────────────────────┘
```

**The gap**: No absolute position tracker! System only knows:
1. "We're 200 steps into this 800-step move"
2. "This move is going backward"

But doesn't know:
- "We're at machine position X=5.5mm"

## The Solution

### Added Absolute Position Tracker

**New variable** (multiaxis_control.c):
```c
// Absolute machine position tracker (accessed from main code only)
// CRITICAL FIX (October 19, 2025): Track absolute position independently from move progress!
// - axis_state[].step_count tracks progress within current move (0 to total_steps)
// - machine_position[] tracks absolute position from power-on/homing
// - Updated at END of each move in MotionManager_TMR9_ISR()
// - Used by MultiAxis_GetStepCount() for position feedback to GRBL/UGS
static int32_t machine_position[NUM_AXES] = {0, 0, 0, 0};
```

### Updated Position Feedback

**MultiAxis_GetStepCount()** now returns absolute position:
```c
uint32_t MultiAxis_GetStepCount(axis_id_t axis)
{
    // Return absolute machine position (can be negative!)
    // GRBL expects unsigned, but we need to handle bidirectional motion
    // UGS_SendStatusReport() converts to mm and displays correctly
    return (uint32_t)machine_position[axis];
}
```

### New Update Function

**MultiAxis_UpdatePosition()** adds move delta to absolute position:
```c
void MultiAxis_UpdatePosition(const int32_t steps[NUM_AXES])
{
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        machine_position[axis] += steps[axis];  // ✅ Add SIGNED delta!
    }

    #ifdef DEBUG_MOTION_BUFFER
    UGS_Printf("[POSITION] Updated: X=%ld Y=%ld Z=%ld A=%ld\r\n",
               (long)machine_position[AXIS_X],
               (long)machine_position[AXIS_Y],
               (long)machine_position[AXIS_Z],
               (long)machine_position[AXIS_A]);
    #endif
}
```

### Integration Points

**1. Initialize on startup** (MultiAxis_Initialize):
```c
for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
{
    // ... other initialization ...
    
    // Initialize absolute machine position (all axes at origin on startup)
    machine_position[axis] = 0;
}
```

**2. Save move deltas** (MotionManager_TMR9_ISR before starting move):
```c
// Save step deltas for position update when move completes
for (uint8_t axis = 0; axis < NUM_AXES; axis++)
{
    last_move_steps[axis] = steps[axis];  // ✅ Save SIGNED steps!
}

MultiAxis_ExecuteCoordinatedMove(steps);
```

**3. Update when move completes** (MotionManager_TMR9_ISR after motion idle):
```c
if (!block_discarded)
{
    // Update absolute machine position!
    MultiAxis_UpdatePosition(last_move_steps);
    
    GRBLPlanner_DiscardCurrentBlock();
    block_discarded = true;
}
```

## Complete Data Flow (FIXED)

```
┌─────────────────────────────────────────────────────────────────┐
│ Motion Planning (motion_manager.c)                             │
│                                                                 │
│ GRBL Block:  steps[X]=800, direction_bits=0x01 (X backward)   │
│ Convert:     signed_steps[X] = -800                            │
│ Save:        last_move_steps[X] = -800  ← CRITICAL!           │
│ Execute:     MultiAxis_ExecuteCoordinatedMove(signed_steps)   │
└────────────────────┬────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────────┐
│ Motion Execution (multiaxis_control.c)                         │
│                                                                 │
│ Setup:  axis_state[X].total_steps = 800 (absolute value)      │
│         axis_state[X].step_count = 0                           │
│         axis_state[X].direction_forward = false                │
│                                                                 │
│ Hardware:  DirX_Clear()  → GPIO LOW (backward)                │
│            OCMP5_Enable() → Start step pulses                  │
│                                                                 │
│ OCR Callback:  axis_state[X].step_count++  (progress tracker) │
│                • Counts 0 → 800 regardless of direction!      │
│                • When reaches 800, move complete               │
└────────────────────┬────────────────────────────────────────────┘
                     ↓ (motion completes)
┌─────────────────────────────────────────────────────────────────┐
│ Position Update (motion_manager.c TMR9 ISR)                    │
│                                                                 │
│ Detect:  MultiAxis_IsBusy() returns false                      │
│ Update:  MultiAxis_UpdatePosition(last_move_steps)             │
│          • machine_position[X] += last_move_steps[X]           │
│          • machine_position[X] += (-800)                       │
│          • machine_position[X] decreases by 800! ✅            │
└────────────────────┬────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────────┐
│ Position Feedback (ugs_interface.c)                            │
│                                                                 │
│ Query:  MultiAxis_GetStepCount(AXIS_X)                         │
│ Returns:  machine_position[X]  (absolute position)             │
│ Convert:  MotionMath_StepsToMM(machine_position[X], AXIS_X)   │
│ Display:  MPos:X.XXX,Y.YYY,Z.ZZZ in UGS ✅                     │
└─────────────────────────────────────────────────────────────────┘
```

## Files Modified

### multiaxis_control.c
- **Added**: `static int32_t machine_position[NUM_AXES]` (line ~420)
- **Modified**: `MultiAxis_Initialize()` - Initialize machine_position[] to zeros
- **Modified**: `MultiAxis_GetStepCount()` - Return machine_position[] instead of step_count
- **Added**: `MultiAxis_UpdatePosition()` - Update absolute position after move

### multiaxis_control.h
- **Modified**: `MultiAxis_GetStepCount()` - Updated documentation (returns absolute position)
- **Added**: `MultiAxis_UpdatePosition()` - Function declaration

### motion_manager.c
- **Added**: `static int32_t last_move_steps[NUM_AXES]` (line ~88)
- **Modified**: TMR9 ISR - Save steps before starting move
- **Modified**: TMR9 ISR - Call `MultiAxis_UpdatePosition()` when move completes

## Expected Test Results

### With DEBUG Build (DEBUG=1)

When running the square pattern test, you should see:

```
>>> G1 Y10 F1000
[MODAL] After merge: target_work=(0.000,10.000,0.000) target_machine=(0.000,10.000,0.000)
[GRBL] Buffered: Work(0.000,10.000,0.000) -> Machine(0.000,10.000,0.000)
[TMR9] Started: X=0 Y=800 Z=0 A=0
[TMR9] Discarding previous block...
[POSITION] Updated: X=0 Y=800 Z=0 A=0  ← NEW DEBUG OUTPUT!
[TMR9] Block completed and discarded
Status: <Idle|MPos:0.000,10.000,0.000|WPos:0.000,10.000,0.000> ✅

>>> G1 X10
[TMR9] Started: X=800 Y=0 Z=0 A=0
[POSITION] Updated: X=800 Y=800 Z=0 A=0  ← Shows X increased!
Status: <Idle|MPos:10.000,10.000,0.000|WPos:10.000,10.000,0.000> ✅

>>> G1 Y0
[TMR9] Started: X=0 Y=-800 Z=0 A=0  ← Negative steps!
[POSITION] Updated: X=800 Y=0 Z=0 A=0  ← Y decreased! ✅
Status: <Idle|MPos:10.000,0.000,0.000|WPos:10.000,0.000,0.000> ✅

>>> G1 X0
[TMR9] Started: X=-800 Y=0 Z=0 A=0  ← Negative steps!
[POSITION] Updated: X=0 Y=0 Z=0 A=0  ← X decreased! ✅
Status: <Idle|MPos:0.000,0.000,0.000|WPos:0.000,0.000,0.000> ✅ **ORIGIN!**
```

### Without DEBUG Build (Production)

Same test, but without `[POSITION]` messages:

```
>>> G1 Y10 F1000
Status: <Idle|MPos:0.000,10.000,0.000|WPos:0.000,10.000,0.000> ✅

>>> G1 X10
Status: <Idle|MPos:10.000,10.000,0.000|WPos:10.000,10.000,0.000> ✅

>>> G1 Y0
Status: <Idle|MPos:10.000,0.000,0.000|WPos:10.000,0.000,0.000> ✅

>>> G1 X0
Status: <Idle|MPos:0.000,0.000,0.000|WPos:0.000,0.000,0.000> ✅ **RETURNS TO ORIGIN!**
```

## Key Architectural Insight

**The system has TWO distinct concepts**:

1. **Move Progress** (axis_state[].step_count):
   - Tracks how far through CURRENT move (0 → total_steps)
   - Always counts UP (even when moving backward!)
   - Used by motion execution to know when to stop
   - Reset at start of each move

2. **Absolute Position** (machine_position[]):
   - Tracks where machine IS relative to power-on/homing
   - Increases/decreases based on move direction
   - Used by GRBL/UGS for position feedback
   - **NEW in this fix!**

**The bug was using (1) for (2)'s job!**

## Future Considerations

### Homing Integration

When homing is implemented, `machine_position[]` should be:
1. Reset to zero when homing completes (or set to home offsets)
2. Used as basis for soft limit checking
3. Synced with GRBL planner's position tracking

### Position Recovery

After power cycle:
- `machine_position[]` resets to {0,0,0,0}
- User must re-home to establish known position
- Work offsets (G54-G59) stored separately in motion_math

### G92 Integration

G92 (coordinate system offset) should:
- NOT modify `machine_position[]` (absolute never changes)
- Only affect work coordinate calculation in main.c
- Formula: `MPos = WPos + work_offset + g92_offset`

## Testing Checklist

- [x] Build successful with DEBUG=1
- [ ] Flash firmware to hardware
- [ ] Run square pattern test via PowerShell script
- [ ] Verify `[POSITION]` debug shows correct updates
- [ ] Verify final position is (0,0,0) in UGS status
- [ ] Test negative moves: G1 X-10, G1 X0 (should work correctly)
- [ ] Test mixed moves: G1 X10 Y-5, G1 X-5 Y10 (diagonal with negatives)
- [ ] Rebuild production (make clean; make all) and re-test without DEBUG

## Conclusion

This fix separates **move execution state** from **absolute position tracking**, which are fundamentally different concepts that were incorrectly conflated. The OCR callbacks count steps *completed*, not *position achieved*, because they don't know about direction. The absolute position must be calculated by *adding signed deltas* when moves complete.

**Status**: ✅ Code complete, ready for hardware testing!
