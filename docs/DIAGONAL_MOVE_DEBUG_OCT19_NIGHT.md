# Diagonal Move Debug Session - October 19, 2025 (Night)

## Session Summary (Builds 15-17)

**Timeline**: 11:14 PM - 11:20 PM (6 minutes, 3 builds)  
**Focus**: Fix diagonal move failures and machine state bugs  
**Status**: ✅ Machine returns to `<Idle>` state, ❌ Position accuracy issues remain

---

## Build History

### Build 15 (11:14 PM, 222,091 bytes)
**Goal**: Accept Bresenham ±1 step rounding error instead of waiting for subordinates

**Changes**:
- **REMOVED** "wait for all axes complete" check (old lines 911-924)
- **Philosophy**: Subordinates will have ±1 step error from Bresenham rounding - this is acceptable (<0.013mm tolerance)
- **Rationale**: Can't wait for subordinates to "catch up" because they only advance when dominant ISR fires. Stopping dominant = deadlock!

**Code Changes** (`multiaxis_control.c` lines 900-910):
```c
// STEP 2: Check if segment complete (DOMINANT AXIS ONLY!)
// CRITICAL: Only check dominant axis step_count!
// Subordinate axes are bit-banged by Bresenham - they'll have +/- 1 step
// error due to rounding. This is acceptable (<0.013mm on 80 steps/mm axes).
//
// DON'T try to wait for subordinates to "catch up" - they only execute when
// dominant ISR fires, so stopping dominant = subordinates never complete!
if (state->step_count < state->current_segment->n_step)
{
    return; // Dominant axis hasn't reached n_step yet
}
```

**Test Result**: ❌ FAILURE
- Command: `G1 X10 Y10 F1000`
- Position: Stuck at **(1.413, 1.413, 0.000)** in `<Run>` state
- Problem: First segment executed, then motion stopped
- **Root cause discovered**: OCR disabled at end of segment but never re-enabled for next segment!

---

### Build 16 (11:17 PM, 222,145 bytes)
**Goal**: Re-enable OCR hardware when loading next segment

**Changes**:
- **ADDED** `axis_hw[dominant_axis].OCMP_Enable()` before `TMR_Start()` when loading next segment
- **Bug fixed**: Build 15 disabled OCR at line 919 but forgot to re-enable when loading next segment
- **Result**: First segment worked (1.413mm), then OCR stayed disabled → machine froze

**Code Changes** (`multiaxis_control.c` line 1017):
```c
// Configure OCR period for new segment
uint32_t period = next_seg->period;
if (period > 65485)
    period = 65485;
if (period <= OCMP_PULSE_WIDTH)
    period = OCMP_PULSE_WIDTH + 10;

axis_hw[dominant_axis].TMR_PeriodSet((uint16_t)period);
axis_hw[dominant_axis].OCMP_CompareValueSet((uint16_t)(period - OCMP_PULSE_WIDTH));
axis_hw[dominant_axis].OCMP_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);
axis_hw[dominant_axis].OCMP_Enable();  // ← CRITICAL FIX! Re-enable OCR
axis_hw[dominant_axis].TMR_Start();
```

**Test Result**: ❌ PARTIAL SUCCESS
- Command: `G1 X10 Y10 F1000`
- Position: Stuck at **(9.988, 9.900, 0.000)** in `<Run>` state (same as Build 12-13)
- Problem: All segments executed, but machine **never returned to `<Idle>`** state
- **Root cause discovered**: Only cleared dominant axis `active` flag, subordinates still `active = true`!

---

### Build 17 (11:20 PM, 222,206 bytes) ⭐ **CURRENT BUILD**
**Goal**: Clear ALL axes when motion completes

**Changes**:
- **CRITICAL FIX**: When no more segments (`next_seg == NULL`), clear **ALL** axes `active` flags
- **Bug fixed**: `MultiAxis_IsBusy()` checks if ANY axis has `active = true`. Build 16 only cleared dominant axis → subordinates kept machine in `<Run>` state forever

**Code Changes** (`multiaxis_control.c` lines 1020-1037):
```c
else
{
    // ═══════════════════════════════════════════════════════════════════
    // No more segments - STOP ALL AXES!
    // ═══════════════════════════════════════════════════════════════════
    // CRITICAL: Must clear ALL axes, not just dominant!
    // MultiAxis_IsBusy() checks if ANY axis has active=true
    // If we only clear dominant, machine stays in <Run> state forever!
    // ═══════════════════════════════════════════════════════════════════
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        volatile axis_segment_state_t *ax_state = &segment_state[axis];
        ax_state->current_segment = NULL;
        ax_state->active = false;
        ax_state->step_count = 0;
    }
    
    // Stop dominant axis hardware
    axis_hw[dominant_axis].OCMP_Disable();
    axis_hw[dominant_axis].TMR_Stop();
}
```

**Test Result**: ✅ `<Idle>` STATE WORKING! ❌ Position accuracy issues
- Command: `G1 X10 Y10 F1000`
- **Expected**: (10.000, 10.000, 0.000)
- **Actual**: (9.988, 9.900, 0.000) in `<Idle>` state ✓
- **Errors**:
  - X-axis: -0.012mm (1 step short = 799 instead of 800)
  - Y-axis: -0.100mm (8 steps short = 792 instead of 800!) ⚠️
- Follow-up test: `G1 X0` → (-0.013, 9.900) - errors **accumulate**!
- Follow-up test: `G1 Y0` → (-0.013, -0.100) - errors **persist**!

---

## Current System Architecture (Build 17)

### Segment Completion Flow

```
┌─────────────────────────────────────────────────────────────────┐
│ OCMP ISR fires (dominant axis generates step pulse via hardware)│
└────────────────────┬────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────────┐
│ ProcessSegmentStep(dominant_axis) - CENTRAL FUNCTION            │
│                                                                 │
│ STEP 1: Call Bresenham for all axes (line 893)                 │
│   - Execute_Bresenham_Strategy_Internal(dominant_axis, segment) │
│   - Dominant: Update position, increment step_count            │
│   - Subordinates: Bresenham algorithm + bit-bang GPIO          │
│                                                                 │
│ STEP 2: Check if segment complete (lines 908-911)              │
│   if (step_count < n_step) return; // Not done yet             │
│                                                                 │
│ STEP 3: Stop dominant hardware (lines 919-920)                 │
│   axis_hw[dominant].OCMP_Disable();                            │
│   axis_hw[dominant].TMR_Stop();                                │
│                                                                 │
│ STEP 4: Advance to next segment (lines 928-930)                │
│   GRBLStepper_SegmentComplete(); // Advances tail              │
│   next_seg = GRBLStepper_GetNextSegment();                     │
│                                                                 │
│ STEP 5A: If next_seg != NULL - LOAD NEW SEGMENT                │
│   - Update segment_state[] for all active axes                 │
│   - Set direction GPIO for dominant axis                       │
│   - Configure OCR period/compare values                        │
│   - OCMP_Enable() + TMR_Start() ← BUILD 16 FIX!               │
│                                                                 │
│ STEP 5B: If next_seg == NULL - MOTION COMPLETE                 │
│   - Clear ALL axes active flags ← BUILD 17 FIX!                │
│   - Stop dominant hardware                                     │
│   - Machine returns to <Idle> ✓                                │
└─────────────────────────────────────────────────────────────────┘
```

### ISR Trampolines (4 thin wrappers)
```c
static void OCMP5_StepCounter_X(uintptr_t context) { ProcessSegmentStep(AXIS_X); }
static void OCMP1_StepCounter_Y(uintptr_t context) { ProcessSegmentStep(AXIS_Y); }
static void OCMP4_StepCounter_Z(uintptr_t context) { ProcessSegmentStep(AXIS_Z); }
static void OCMP3_StepCounter_A(uintptr_t context) { ProcessSegmentStep(AXIS_A); }
```

**Benefits**:
- Single source of truth for segment logic (no duplicate code)
- Easy to debug (one function instead of four)
- Smaller firmware (saved 2,163 bytes in Build 14)

---

## Current Critical Bug: Position Accuracy

### Problem Statement

**Diagonal moves complete but end positions are incorrect:**

| Command | Expected Position | Actual Position | Error |
|---------|------------------|-----------------|-------|
| `G1 X10 Y10` | (10.000, 10.000) | (9.988, 9.900) | X: -1 step, Y: -8 steps |
| `G1 X0` | (0.000, 9.900) | (-0.013, 9.900) | X: -1 step (accumulated!) |
| `G1 Y0` | (-0.013, 0.000) | (-0.013, -0.100) | Y: -8 steps (accumulated!) |

**Analysis**:
- **X-axis**: 1 step short = acceptable Bresenham rounding (±0.013mm tolerance)
- **Y-axis**: 8 steps short = **NOT** acceptable! (0.100mm = 10x expected error)
- Errors **accumulate** across moves → position tracking fundamentally broken

### Expected vs Actual Steps

```
Expected for G1 X10 Y10:
  X: 10mm × 80 steps/mm = 800 steps
  Y: 10mm × 80 steps/mm = 800 steps

Actual (from position feedback):
  X: 9.988mm × 80 = 799 steps (1 short)
  Y: 9.900mm × 80 = 792 steps (8 short!)
```

### Hypotheses (To Investigate Tomorrow)

1. **Segment preparation bug**: Total steps across all segments ≠ 800?
   - Need to verify: Sum of all `segment->steps[AXIS_Y]` == 800
   - Need to verify: Sum of all `segment->n_step` matches dominant axis

2. **Bresenham execution bug**: Not firing for all calculated steps?
   - Y-axis is subordinate (bit-banged by X's ISR)
   - Maybe Bresenham counter not triggering enough times?
   - Need debug: Count how many times Y-axis Bresenham fires

3. **Early segment termination**: Stopping before all subordinate steps execute?
   - Check: Does dominant reach `n_step` before subordinates finish?
   - Timing issue: ISR might be checking completion too early?

4. **Multiple segment coordination bug**: 
   - G1 X10 Y10 likely generates ~8 segments (GRBL typical)
   - Maybe losing steps between segment transitions?
   - Need to verify segment handoff logic

---

## Debug Strategy for Tomorrow

### Phase 1: Add Debug Output (30 min)

Add printf statements to track:

1. **Segment preparation** (`grbl_stepper.c`):
   ```c
   printf("SEG: n_step=%u, X=%u, Y=%u, Z=%u, dir=0x%02X\n",
          seg->n_step, seg->steps[AXIS_X], seg->steps[AXIS_Y], 
          seg->steps[AXIS_Z], seg->direction_bits);
   ```

2. **Bresenham execution** (`multiaxis_control.c` line 651):
   ```c
   // In subordinate bit-bang section:
   if (sub_axis == AXIS_Y) {
       debug_y_step_count++; // Track Y-axis steps
   }
   ```

3. **Segment completion** (`ProcessSegmentStep()` line 928):
   ```c
   printf("SEG_DONE: dom=%u, dom_steps=%u, Y_steps=%u\n",
          dominant_axis, dom_state->step_count, 
          segment_state[AXIS_Y].step_count);
   ```

### Phase 2: Analyze Data (30 min)

Run test sequence:
```gcode
G0 X0 Y0 Z0
G1 X10 Y10 F1000
?
```

Expected debug output (example):
```
SEG: n_step=100, X=100, Y=100, Z=0, dir=0x00  (segment 1)
SEG: n_step=100, X=100, Y=100, Z=0, dir=0x00  (segment 2)
... (more segments)
SEG_DONE: dom=0 (X), dom_steps=100, Y_steps=100
SEG_DONE: dom=0 (X), dom_steps=100, Y_steps=100
... (more completions)
TOTAL X STEPS: 800 ← Should match
TOTAL Y STEPS: 792 ← BUG! Should be 800!
```

### Phase 3: Root Cause Fix (1-2 hours)

Once we identify where the 8 missing Y-axis steps are lost:
- Option A: Segment prep bug → fix in `grbl_stepper.c`
- Option B: Bresenham bug → fix in `Execute_Bresenham_Strategy_Internal()`
- Option C: Timing bug → adjust completion check logic

---

## Key Insights from Tonight's Session

### 1. Dave's Architectural Question (Critical!)

**Dave asked**: *"IF YOU STOP THE DOMINANT AXIS HARDWARE HOW WILL THE OTHERS CATCH UP AS THEY WONT FIRE THEIR ISRS"*

This exposed the **fundamental flaw** in Build 14's approach:
- ❌ **Wrong**: Stop dominant, wait for subordinates → DEADLOCK (subordinates depend on dominant ISR)
- ✅ **Correct**: Accept ±1 step Bresenham error, don't wait for perfect synchronization

### 2. Hybrid OCR/Bit-bang Implications

**Architecture**:
- Dominant axis: Hardware OCR generates pulses → ISR fires at step rate
- Subordinate axes: Bit-banged from dominant's ISR using Bresenham

**Constraints**:
- Subordinates can ONLY advance when dominant ISR fires
- Can't stop dominant without stopping subordinates
- Must accept Bresenham rounding error (±1 step is acceptable)

### 3. State Management is Critical

**Build 17 lesson**: `MultiAxis_IsBusy()` checks **ALL** axes:
```c
bool MultiAxis_IsBusy(void) {
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++) {
        if (segment_state[axis].active) {
            return true; // ANY axis active = machine busy
        }
    }
    return false;
}
```

**Implication**: When motion completes, must clear ALL axes, not just dominant!

### 4. Segment Chaining Works! (Major Progress)

**Success**: Machine now executes multiple segments and returns to `<Idle>`
- ✅ First segment loads correctly
- ✅ Subsequent segments load and execute
- ✅ OCR re-enabled for each new segment
- ✅ Last segment completes and clears all axes
- ✅ Machine state transitions: `<Idle>` → `<Run>` → `<Idle>`

**Remaining issue**: Position accuracy (8 steps lost on Y-axis)

---

## Test Results Summary

### Single-Axis Moves (From Earlier Today)
✅ **G1 X10** → (10.000, 0, 0) - PERFECT  
✅ **G1 Y10** → (0, 10.000, 0) - PERFECT  
✅ **G0 Z5** → (0, 0, 5.000) - PERFECT  

### Square Pattern (Manual Spacing)
✅ **G1 Y10, G1 X10, G1 Y0, G1 X0** → (0, 0, 0) - PERFECT

### Diagonal Moves (Current Bug)
❌ **G1 X10 Y10** → (9.988, 9.900) - X: -1 step, Y: -8 steps  
❌ **G1 X0** → (-0.013, 9.900) - Error accumulates  
❌ **G1 Y0** → (-0.013, -0.100) - Error persists  

---

## Code Changes Summary (Builds 15-17)

### Build 15: Remove "wait for subordinates" check
```diff
- // Check if ALL axes complete
- bool all_axes_complete = true;
- for (axis_id_t check_axis = AXIS_X; check_axis < NUM_AXES; check_axis++) {
-     if (check_state->step_count < segment->steps[check_axis]) {
-         all_axes_complete = false;
-         break;
-     }
- }
- if (!all_axes_complete) {
-     return; // Wait for subordinates (DEADLOCK!)
- }

+ // CRITICAL: Only check dominant axis step_count!
+ // Subordinates will have +/- 1 step error from Bresenham rounding
+ if (state->step_count < state->current_segment->n_step) {
+     return; // Dominant hasn't reached n_step yet
+ }
```

### Build 16: Re-enable OCR for next segment
```diff
  axis_hw[dominant_axis].TMR_PeriodSet((uint16_t)period);
  axis_hw[dominant_axis].OCMP_CompareValueSet(...);
  axis_hw[dominant_axis].OCMP_CompareSecondaryValueSet(...);
+ axis_hw[dominant_axis].OCMP_Enable();  // ← CRITICAL FIX!
  axis_hw[dominant_axis].TMR_Start();
```

### Build 17: Clear ALL axes when motion completes
```diff
  else {
-     // No more segments - STOP
-     state->current_segment = NULL;
-     state->active = false;
-     state->step_count = 0;

+     // No more segments - STOP ALL AXES!
+     for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++) {
+         segment_state[axis].current_segment = NULL;
+         segment_state[axis].active = false;
+         segment_state[axis].step_count = 0;
+     }
      
      axis_hw[dominant_axis].OCMP_Disable();
      axis_hw[dominant_axis].TMR_Stop();
  }
```

---

## Files Modified Tonight

1. **`srcs/motion/multiaxis_control.c`** (2,076 lines)
   - Lines 900-911: Simplified segment completion check (Build 15)
   - Line 1017: Added `OCMP_Enable()` (Build 16)
   - Lines 1020-1037: Clear all axes on completion (Build 17)

2. **`bins/CS23.hex`** (222,206 bytes, Build 17)
   - Ready for testing tomorrow

---

## Tomorrow's Action Plan

### Morning Session (1-2 hours)

1. ✅ **Add debug output** to track:
   - Total steps per segment (sum of all `n_step` values)
   - Y-axis Bresenham fire count
   - Segment completion step counts

2. ✅ **Run diagnostic test**:
   ```gcode
   G0 X0 Y0 Z0
   G1 X10 Y10 F1000
   ```
   
3. ✅ **Analyze debug output**:
   - Where are the 8 Y-axis steps lost?
   - Is it segment prep or execution?

### Afternoon Session (2-3 hours)

4. ✅ **Fix root cause** based on morning analysis

5. ✅ **Comprehensive testing**:
   - Various diagonal angles (30°, 45°, 60°)
   - 3-axis simultaneous motion
   - Rapid direction changes
   - 100-move stress test

6. ✅ **Verify position accuracy**:
   - All moves within ±0.013mm (1 step tolerance)
   - No accumulated errors

7. ✅ **Documentation update**:
   - Update this document with fix
   - Create git commit with summary

---

## Why We're Taking This Route

### Decision: Hybrid OCR/Bit-bang with ±1 Step Tolerance

**Rationale**:
1. **Hardware acceleration**: Dominant axis uses OCR for precise pulse generation (no CPU overhead)
2. **Timing independence**: Each axis has independent timer/OCR hardware
3. **Scalability**: Can handle different velocities per axis (Z slower than XY)
4. **Simplicity**: Single ISR callback per dominant axis (not 4 separate ISRs)
5. **Proven pattern**: GRBL uses similar approach with Bresenham for step distribution

**Trade-off**: Accept ±1 step Bresenham rounding error
- **Acceptable**: 1 step = 0.0125mm on 80 steps/mm axes
- **CNC tolerance**: Typically ±0.05mm, so 0.0125mm is 4x better than required
- **Alternative rejected**: Waiting for perfect subordinate synchronization → deadlock

### Why NOT Pure Bit-bang?

**Rejected approach**: Single timer for all axes (like original GRBL)
- ❌ CPU overhead: Bit-banging 4 axes at high step rates
- ❌ Interrupt latency: Jitter affects pulse timing
- ❌ No hardware acceleration: Wastes PIC32MZ OCR capabilities

### Why NOT Pure OCR?

**Rejected approach**: All axes use independent OCR hardware
- ❌ Synchronization complexity: Need to coordinate 4 independent timers
- ❌ Bresenham still needed: Step ratios don't perfectly divide
- ❌ No clear benefit: Hybrid approach already uses hardware for dominant axis

---

## Session Metrics

- **Duration**: 6 minutes (11:14 PM - 11:20 PM)
- **Builds**: 3 (Build 15, 16, 17)
- **Bugs Fixed**: 2 critical (OCR re-enable, state clearing)
- **Bugs Remaining**: 1 (position accuracy - 8 steps short on Y-axis)
- **Code Size**: 222,206 bytes (Build 17)
- **Progress**: ✅ State machine working, ❌ Position tracking needs fix

---

## References

- **Previous session**: `docs/MOTION_EXECUTION_DEBUG_OCT19.md` (Build 1-14)
- **Architecture docs**: `docs/PHASE2B_SEGMENT_EXECUTION.md`
- **GRBL reference**: `docs/GRBL_PLANNER_PORT_COMPLETE.md`
- **Timer analysis**: `docs/TIMER_PRESCALER_ANALYSIS.md`

---

**Session End**: October 19, 2025, 11:25 PM  
**Next Session**: October 20, 2025 (morning) - Position accuracy debugging  
**Status**: Ready to commit Build 17 and continue debugging tomorrow
