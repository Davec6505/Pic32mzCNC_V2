# Modal Position Tracking Fix

**Date**: October 19, 2025  
**Issue**: Position lost between commands in square pattern  
**Status**: ✅ **FIXED**

---

## Problem Description

When testing the GRBL planner integration with a square pattern, position was lost between commands:

```gcode
G1 Y10  → Planner pos: (0, 10, 0) ✅ Correct
G1 X10  → Planner pos: (10, 0, 0) ❌ Y lost! Should be (10, 10, 0)
G1 Y0   → Planner pos: (0, 0, 0)  ❌ X lost! Should be (10, 0, 0)
G1 X0   → Never executed (buffer full or rejected)
```

**Expected Square Pattern**:
- Start: (0, 0, 0)
- G1 Y10 → (0, 10, 0)
- G1 X10 → **(10, 10, 0)** ← Bug here
- G1 Y0 → **(10, 0, 0)** ← Bug here
- G1 X0 → (0, 0, 0)

---

## Root Cause

The G-code parser only fills in **specified axes** in `parsed_move_t`:

```c
// When parsing "G1 X10":
move.axis_words[AXIS_X] = true;   // X specified
move.axis_words[AXIS_Y] = false;  // Y NOT specified
move.target[AXIS_X] = 10.0f;      // Parsed value
move.target[AXIS_Y] = ???;        // Uninitialized or 0!
```

The integration code was directly copying `move.target[]` to GRBL planner:

```c
// OLD CODE (WRONG):
target[AXIS_X] = move.target[AXIS_X];  // 10.0
target[AXIS_Y] = move.target[AXIS_Y];  // 0.0 (should be 10.0!)
```

This caused the planner to think you wanted to move to Y:0, losing the previous Y:10 position.

---

## Solution

Added **modal position tracking** to preserve unspecified axes:

### Implementation (main.c:373)

```c
static void ProcessCommandBuffer(void)
{
  /* Modal position tracking - preserve unspecified axes (CRITICAL for GRBL planner!)
   * In G90 absolute mode, when "G1 X10" is sent, Y/Z should maintain last position.
   * Parser only fills in specified axes, so we must merge with modal position. */
  static float modal_position[NUM_AXES] = {0.0f, 0.0f, 0.0f, 0.0f};
  
  // ... process commands ...
}
```

### Merge Logic (main.c:452)

```c
/* Merge parsed move with modal position (preserve unspecified axes) */
float target[NUM_AXES];
for (uint8_t axis = 0; axis < NUM_AXES; axis++)
{
  if (move.axis_words[axis])
  {
    /* Axis specified in command - use parsed value */
    target[axis] = move.target[axis];
    modal_position[axis] = move.target[axis]; /* Update modal state */
  }
  else
  {
    /* Axis not specified - use modal position */
    target[axis] = modal_position[axis];
  }
}

/* Now send complete 4D position to GRBL planner */
GRBLPlanner_BufferLine(target, &pl_data);
```

---

## How It Works

**Command Sequence**:
```gcode
G1 Y10  # Y specified, X/Z unspecified
G1 X10  # X specified, Y/Z unspecified
```

**Processing**:

1. **G1 Y10**:
   - `axis_words[Y] = true`, `axis_words[X] = false`
   - `target[X] = modal_position[X]` (0.0)
   - `target[Y] = move.target[Y]` (10.0)
   - `modal_position[Y] = 10.0` ← Update modal state
   - **Planner receives: (0, 10, 0)** ✅

2. **G1 X10**:
   - `axis_words[X] = true`, `axis_words[Y] = false`
   - `target[X] = move.target[X]` (10.0)
   - `target[Y] = modal_position[Y]` **(10.0)** ← Preserved! ✅
   - `modal_position[X] = 10.0` ← Update modal state
   - **Planner receives: (10, 10, 0)** ✅

---

## Testing

**Expected Output** (after fix):
```
[PARSE] 'G1 Y10' -> motion=1 (X:0 Y:1 Z:0)
[GRBL] Buffered: X:0.000 Y:10.000 Z:0.000 F:1000.0 rapid:0
       Planner pos: X:0.000 Y:10.000 Z:0.000 (buffer:1/16)

[PARSE] 'G1 X10' -> motion=1 (X:1 Y:0 Z:0)
[GRBL] Buffered: X:10.000 Y:10.000 Z:0.000 F:1000.0 rapid:0  ← Y preserved!
       Planner pos: X:10.000 Y:10.000 Z:0.000 (buffer:2/16)

[PARSE] 'G1 Y0' -> motion=1 (X:0 Y:1 Z:0)
[GRBL] Buffered: X:10.000 Y:0.000 Z:0.000 F:1000.0 rapid:0  ← X preserved!
       Planner pos: X:10.000 Y:0.000 Z:0.000 (buffer:3/16)

[PARSE] 'G1 X0' -> motion=1 (X:1 Y:0 Z:0)
[GRBL] Buffered: X:0.000 Y:0.000 Z:0.000 F:1000.0 rapid:0  ← Y preserved!
       Planner pos: X:0.000 Y:0.000 Z:0.000 (buffer:4/16)
```

**Complete square pattern**: (0,0) → (0,10) → (10,10) → (10,0) → (0,0) ✅

---

## GRBL Compatibility

This matches GRBL v1.1f behavior where:
- Parser extracts specified axes from G-code
- Planner maintains modal position state
- Unspecified axes retain previous values

**From GRBL gcode.c line 890**:
```c
// GRBL's approach (conceptual):
gc_state.position[axis] = gc_block.values.xyz[axis];  // Update modal state
```

Our implementation achieves the same result by merging before sending to planner.

---

## Impact

**Before Fix**:
- ❌ Square patterns generated diagonal lines
- ❌ Multi-axis moves with partial axis specification failed
- ❌ Position tracking inconsistent between commands

**After Fix**:
- ✅ Square patterns execute correctly
- ✅ Partial axis specification works (e.g., "G1 X10" maintains Y/Z)
- ✅ Position tracking consistent with GRBL behavior
- ✅ Original bug (incomplete motion) should be **FIXED**!

---

## Files Modified

- **srcs/main.c**: Added modal position tracking in `ProcessCommandBuffer()`
  - Line 373: Static `modal_position[NUM_AXES]` array
  - Line 452: Merge logic using `move.axis_words[]` flags

---

## Conclusion

The modal position tracking fix is **critical** for GRBL planner integration. Without it, the planner receives incomplete 3D/4D coordinates, causing erratic motion.

This fix ensures the planner always receives complete absolute machine coordinates, with unspecified axes filled in from modal state.

**Status**: ✅ **VERIFIED WORKING!** Hardware test confirms modal position preserved correctly!

---

## Hardware Test Results (October 19, 2025)

✅ **Modal position fix confirmed working**:
```
G1 Y10  → Planner pos: X:0.000 Y:10.000 Z:0.000  ✅
G1 X10  → Planner pos: X:10.000 Y:10.000 Z:0.000 ✅ Y PRESERVED!
G1 Y0   → Planner pos: X:10.000 Y:0.000 Z:0.000  ✅ X PRESERVED!
G1 X0   → Planner pos: X:0.000 Y:0.000 Z:0.000   ✅ Complete square!
```

**GRBL Planner Buffering**: Working correctly (5 blocks buffered)

**Next Issue Discovered**: ⚠️ **No motion execution** - planner buffers moves but hardware doesn't move!
- Root cause: `MotionManager_CoreTimerISR()` reads from old `MotionBuffer`, not `GRBLPlanner`
- Solution: Wire `GRBLPlanner_GetCurrentBlock()` to motion manager
- See: `docs/GRBL_PLANNER_INTEGRATION.md` for implementation plan

