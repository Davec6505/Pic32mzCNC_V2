# Quick Start - October 20, 2025 Morning

## Current Status (Build 17 - Last Night 11:20 PM)

**What Works** ✅:
- Machine returns to `<Idle>` state after motion completes
- Multiple segments execute and chain correctly
- Single-axis moves are PERFECT (10.000mm exactly)
- Square patterns return to origin (0, 0, 0)
- State transitions work: `<Idle>` → `<Run>` → `<Idle>`

**What's Broken** ❌:
- **Diagonal move position accuracy**: `G1 X10 Y10` → (9.988, 9.900) instead of (10.000, 10.000)
- **Y-axis 8 steps short**: 792 steps instead of 800 (0.100mm error - NOT acceptable!)
- **X-axis 1 step short**: 799 steps instead of 800 (0.013mm error - acceptable Bresenham rounding)
- **Errors accumulate**: Position tracking fundamentally broken

---

## First Thing This Morning (30 min total)

### 1. Add Debug Output (15 min)

**File**: `srcs/motion/grbl_stepper.c` (in segment prep function, after line 250)
```c
// After preparing segment:
printf("SEG_PREP: n_step=%u, steps[X]=%u, steps[Y]=%u\n",
       segment->n_step, segment->steps[AXIS_X], segment->steps[AXIS_Y]);
```

**File**: `srcs/motion/multiaxis_control.c` line 651 (in Bresenham subordinate section)
```c
// After bit-banging Y-axis step (inside case AXIS_Y:):
static uint32_t debug_y_steps = 0;
if (sub_axis == AXIS_Y) {
    debug_y_steps++;
}
```

**File**: `srcs/motion/multiaxis_control.c` line 928 (segment completion)
```c
// When segment completes:
printf("SEG_DONE: dom_steps=%u, Y_steps=%u\n",
       segment_state[dominant_axis].step_count,
       segment_state[AXIS_Y].step_count);
```

### 2. Build and Test (10 min)

```bash
make all
# Flash bins/CS23.hex
```

```gcode
G0 X0 Y0 Z0
G1 X10 Y10 F1000
?
```

### 3. Analyze Debug Output (5 min)

Expected output (example):
```
SEG_PREP: n_step=100, steps[X]=100, steps[Y]=100  (segment 1)
SEG_PREP: n_step=100, steps[X]=100, steps[Y]=100  (segment 2)
... (more segments)
SEG_DONE: dom_steps=100, Y_steps=100
SEG_DONE: dom_steps=100, Y_steps=100
... (more completions)
```

**Calculate totals**:
- Sum all `n_step` values → should be ~800
- Sum all `steps[Y]` values → should be 800
- Check debug_y_steps final value → should be 800
- **If any don't match 800, that's where the bug is!**

---

## Quick Architecture Reminder

```
OCMP ISR (dominant X-axis) fires every step
    ↓
ProcessSegmentStep(AXIS_X) - Central function
    ↓
Execute_Bresenham_Strategy_Internal()
    - X-axis: position++, step_count++ (hardware did pulse)
    - Y-axis: Bresenham algorithm
        - If counter >= n_step: bit-bang GPIO, position++, step_count++
    ↓
Check if (step_count >= n_step) → segment complete
    ↓
Stop hardware, advance to next segment OR stop all axes
```

---

## Key Files

- **Current firmware**: `bins/CS23.hex` (222,206 bytes, Build 17)
- **Main code**: `srcs/motion/multiaxis_control.c` (2,076 lines)
- **Segment prep**: `srcs/motion/grbl_stepper.c`
- **Full session doc**: `docs/DIAGONAL_MOVE_DEBUG_OCT19_NIGHT.md`

---

## Test Results (Build 17 - Last Night)

| Test            | Expected         | Actual         | Status                |
| --------------- | ---------------- | -------------- | --------------------- |
| G1 X10          | (10.000, 0, 0)   | (10.000, 0, 0) | ✅ PERFECT             |
| G1 Y10          | (0, 10.000, 0)   | (0, 10.000, 0) | ✅ PERFECT             |
| G1 X10 Y10      | (10.000, 10.000) | (9.988, 9.900) | ❌ 8 steps short on Y! |
| Returns to Idle | `<Idle>`         | `<Idle>`       | ✅ FIXED!              |

---

## Hypothesis

**Most likely**: Segment preparation bug
- GRBL planner distributing steps incorrectly across segments
- Total steps in all segments might not sum to 800 for Y-axis
- Debug output will confirm this

**Less likely**: Bresenham execution bug
- Single-axis Y moves are perfect (10.000mm exactly)
- Bresenham algorithm itself works
- But coordination with X-axis might break it

---

## After Debug - Next Steps

Once we find where the 8 steps are lost:

1. **If segment prep bug**: Fix in `grbl_stepper.c`
2. **If Bresenham bug**: Fix in `Execute_Bresenham_Strategy_Internal()`
3. **If timing bug**: Adjust completion logic

Then:
4. Test comprehensive motion patterns
5. Verify no accumulated errors
6. Clean up debug output
7. Commit to git

---

## Git Commit Plan

**After position accuracy is fixed**:

```bash
git add .
git commit -m "Build 17: Fix diagonal move execution

- ✅ Machine now returns to <Idle> state after motion completes
- ✅ Fixed OCR re-enable when loading next segment (Build 16)
- ✅ Fixed state clearing - all axes cleared on completion (Build 17)
- ✅ Removed deadlock-prone 'wait for subordinates' check (Build 15)
- ❌ Position accuracy bug remains (Y-axis 8 steps short)

Builds 15-17 focused on fixing state machine bugs. Position
accuracy will be addressed in next session with debug output.

See docs/DIAGONAL_MOVE_DEBUG_OCT19_NIGHT.md for full session."
```

---

**Good luck this morning, Claude!** 🚀

**Remember**: Add debug output first, analyze data, THEN fix root cause!
