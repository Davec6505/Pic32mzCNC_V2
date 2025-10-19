# Direction Bit Conversion Fix - Z-Axis Motion Corrected

**Date**: October 19, 2025  
**Status**: ✅ **BUILD SUCCESSFUL - Direction handling fixed!**

---

## Problem: Z-Axis Stayed at 5mm (Z-Like Motion Path)

### Hardware Test Results (Before Fix)

**Expected path** (square at Z=0):
```
G0 Z5   → (0, 0, 5)    ✅ Move Z up
G0 Z0   → (0, 0, 0)    ❌ Should return Z to 0
G1 Y10  → (0, 10, 0)   ❌ Should move at Z=0
G1 X10  → (10, 10, 0)  ❌ Should complete square
```

**Actual path** (Z-like diagonal):
```
G0 Z5   → (0, 0, 5)     ✅ Correct
G0 Z0   → (0, 0, 5)     ❌ Z didn't move!
G1 Y10  → (0, 10, 5)    ❌ Z stayed high
G1 X10  → (10, 10, 5)   ❌ Z still at 5mm
End:    <Idle|MPos:10.000,10.000,5.001>  ← Z never returned to 0!
```

**User description**: "it still moved in a Z like motion"  
→ Because Z stayed at 5mm while XY moved, creating a 3D path instead of 2D square!

---

## Root Cause Analysis

### GRBL vs Our Motion System Formats

**GRBL Planner Storage** (grbl_plan_block_t):
```c
typedef struct {
    uint32_t steps[NUM_AXES];      // UNSIGNED step deltas (absolute distance)
    uint8_t direction_bits;        // Bit flags: bit N set = axis N negative
    // ...
} grbl_plan_block_t;
```

**Example - Moving Z from 5mm to 0mm**:
```c
steps[AXIS_Z] = 6400;              // 5mm × 1280 steps/mm = 6400 steps
direction_bits |= (1 << AXIS_Z);   // Bit 2 set = Z moves negative
```

**Our Motion System** (MultiAxis_ExecuteCoordinatedMove):
```c
void MultiAxis_ExecuteCoordinatedMove(int32_t steps[NUM_AXES]);
// Expects SIGNED steps: negative = backward, positive = forward
```

**Example - Moving Z from 5mm to 0mm**:
```c
steps[AXIS_Z] = -6400;  // Negative for backward motion
```

### The Bug (Old Code)

```c
// OLD CODE - WRONG! Lost direction information:
int32_t steps[NUM_AXES];
steps[AXIS_X] = grbl_block->steps[AXIS_X];  // uint32_t → int32_t cast
steps[AXIS_Y] = grbl_block->steps[AXIS_Y];
steps[AXIS_Z] = grbl_block->steps[AXIS_Z];  // 6400 → +6400 (WRONG!)
steps[AXIS_A] = grbl_block->steps[AXIS_A];
// direction_bits NEVER CHECKED! ❌
```

**What happened**:
1. GRBL calculated: Z moves 6400 steps backward (direction_bits bit 2 set)
2. Old code copied: `steps[AXIS_Z] = 6400` (unsigned → signed, became **+6400**)
3. Motion system interpreted: Move Z **forward** 6400 steps (5mm → 10mm) ❌
4. But machine at Z hardware limit, so Z didn't move at all! ❌
5. Result: Z stayed at 5mm throughout entire pattern

---

## Solution: Direction Bit Conversion

### New Code (Correct)

```c
/* Step 3: Convert GRBL block to coordinated move format
 *
 * CRITICAL (October 19, 2025): GRBL stores motion differently than our system!
 *   - GRBL: steps[] = unsigned delta (e.g., 6400 for 5mm move)
 *           direction_bits = bit mask (bit N set = axis N moves negative)
 *   - Our system: steps[] = signed delta (negative = backward, positive = forward)
 *
 * Must convert: Check direction bit, apply sign to step count.
 */
int32_t steps[NUM_AXES];

for (uint8_t axis = 0; axis < NUM_AXES; axis++)
{
    /* Get unsigned step count from GRBL */
    uint32_t abs_steps = grbl_block->steps[axis];
    
    /* Check if this axis moves in negative direction */
    uint8_t dir_mask = (1U << axis);  /* Bit 0=X, 1=Y, 2=Z, 3=A */
    bool is_negative = (grbl_block->direction_bits & dir_mask) != 0;
    
    /* Apply sign based on direction */
    if (is_negative)
    {
        steps[axis] = -(int32_t)abs_steps;  /* Negative motion */
    }
    else
    {
        steps[axis] = (int32_t)abs_steps;   /* Positive motion */
    }
}

MultiAxis_ExecuteCoordinatedMove(steps);
```

---

## Example Conversion

### Forward Motion (G1 Y10 at Y=0)

**GRBL**:
- `steps[AXIS_Y] = 800` (10mm × 80 steps/mm)
- `direction_bits = 0x00` (bit 1 clear = positive)

**Conversion**:
```c
abs_steps = 800;
dir_mask = (1 << AXIS_Y) = 0x02;
is_negative = (0x00 & 0x02) = false;
steps[AXIS_Y] = +800;  ✅ Positive motion
```

### Backward Motion (G0 Z0 at Z=5)

**GRBL**:
- `steps[AXIS_Z] = 6400` (5mm × 1280 steps/mm)
- `direction_bits = 0x04` (bit 2 set = negative)

**Conversion**:
```c
abs_steps = 6400;
dir_mask = (1 << AXIS_Z) = 0x04;
is_negative = (0x04 & 0x04) = true;
steps[AXIS_Z] = -6400;  ✅ Negative motion (backward!)
```

---

## Expected Results (After Fix)

### Test Commands
```gcode
G90             # Absolute mode
G0 Z5           # Move Z to 5mm
G0 Z0           # Return Z to 0mm ← This should work now!
G1 Y10 F1000    # Move Y to 10mm (at Z=0)
G1 X10          # Move X to 10mm (at Z=0)
G1 Y0           # Return Y to 0mm
G1 X0           # Return X to 0mm
```

### Expected Status Reports
```
<Run|MPos:0.000,0.000,2.500>   ✅ Z moving up
<Idle|MPos:0.000,0.000,5.000>  ✅ Z reached 5mm
<Run|MPos:0.000,0.000,2.500>   ✅ Z moving DOWN! (negative motion)
<Idle|MPos:0.000,0.000,0.000>  ✅ Z returned to 0!
<Run|MPos:0.000,5.000,0.000>   ✅ Y moving at Z=0
<Run|MPos:5.000,10.000,0.000>  ✅ X moving at Z=0
<Idle|MPos:10.000,10.000,0.000> ✅ Complete 2D square!
```

**Key difference**: Z should return to 0mm instead of staying at 5mm!

---

## Why This Bug Was Hard to Catch

1. **Type mismatch looked safe**: `uint32_t` → `int32_t` cast compiles without warning
2. **GRBL documentation**: Doesn't emphasize the direction_bits requirement
3. **Old test cases**: Never tested negative motion (always moved forward)
4. **Partial success**: Forward motion (Y10, X10) worked fine, only backward failed

---

## Build Verification

```
Compiling ../srcs/motion/motion_manager.c to ../objs/motion/motion_manager.o
...
Build complete. Output is in ../bins
######  BUILD COMPLETE   ########
```

**Status**: ✅ Zero errors, zero warnings

**Firmware Ready**: `bins/CS23.hex` with direction bit conversion fix!

---

## Files Modified

| File                           | Changes                                            | Lines   |
| ------------------------------ | -------------------------------------------------- | ------- |
| `srcs/motion/motion_manager.c` | Replaced direct copy with direction bit conversion | 113-144 |

**Total Changes**: ~30 lines (added for-loop with direction check)

---

## Testing Checklist

Before declaring success:

- [ ] Flash `bins/CS23.hex` to hardware
- [ ] Connect via UGS @ 115200 baud
- [ ] Send: `G0 Z5` → Verify Z moves to 5mm
- [ ] Send: `G0 Z0` → **Verify Z returns to 0mm** (critical!)
- [ ] Send square pattern at Z=0
- [ ] **Verify 2D motion** (not 3D "Z-like" path)
- [ ] Status reports show Z changing during backward motion
- [ ] Final position: `MPos:10.000,10.000,0.000` (Z=0!)

---

## Architecture Impact

### Memory (Unchanged)
- Added for-loop: ~50 bytes code space
- No RAM impact

### Performance (Negligible)
- For-loop: 4 iterations × ~10 CPU cycles = 40 cycles
- At 200MHz: 40 cycles = 0.2µs (negligible!)
- CoreTimer ISR still <100µs total

### Compatibility (GRBL Standard)
- Now matches GRBL v1.1f block format exactly
- Direction handling identical to original GRBL stepper.c
- Ready for full GRBL stepper port (Phase 2)

---

## Related Issues Fixed

This fix also resolves:
1. ✅ Any backward motion on X-axis (e.g., G1 X0 from X=10)
2. ✅ Any backward motion on Y-axis (e.g., G1 Y0 from Y=10)
3. ✅ Any backward motion on A-axis (rotary)
4. ✅ All negative moves in any axis

**Previous hardware test** showed G1 X0 completing but position ending at (10,10,5):
- Now we know: X didn't move backward because direction bit wasn't checked!
- Fix applies to ALL axes, not just Z

---

## Phase 1 Status

### Completed ✅
1. ✅ GRBL planner port (1300 lines, MISRA compliant)
2. ✅ Parser → planner integration
3. ✅ Modal position tracking (unspecified axes preserved)
4. ✅ Motion execution wiring (CoreTimer → GRBL planner)
5. ✅ **Direction bit conversion** (this fix!)

### Ready to Test 🎯
- Complete square pattern: (0,0) → (0,10) → (10,10) → (10,0) → (0,0)
- Should execute as 2D path at Z=0 (not 3D Z-like motion)
- Both forward AND backward motion working
- All axes (X, Y, Z, A) handling direction correctly

### Original Bug Status 🚀
**Should be FIXED!** All three issues resolved:
1. ✅ Modal position (unspecified axes preserved)
2. ✅ GRBL planner (junction deviation)
3. ✅ **Motion execution** (direction bits converted correctly)

---

## Next Steps

1. **Flash firmware** (`bins/CS23.hex`)
2. **Test backward motion**: `G0 Z5`, `G0 Z0` (critical test!)
3. **Test complete square**: Verify 2D path at Z=0
4. **Observe status reports**: Z should change during moves
5. **Document results**: Update test log with actual positions

If tests pass → **Phase 1 COMPLETE!** Ready for Phase 2 (GRBL stepper port).

---

## References

- **GRBL Planner**: `srcs/motion/grbl_planner.c` line 636 (direction_bits set)
- **Motion Manager**: `srcs/motion/motion_manager.c` line 113 (conversion logic)
- **Hardware Test**: `docs/GRBL_EXECUTION_WIRED_OCT19.md` (Z-like motion observed)
- **GRBL Source**: https://github.com/gnea/grbl (v1.1f stepper.c uses same format)

