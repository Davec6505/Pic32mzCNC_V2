# Arc Generator Deadlock Fix - October 25, 2025

## Problem Summary

**Critical Deadlock** discovered during TMR1 arc generator testing that caused main loop to hang after arc completion.

### Symptoms
- Arc executed successfully (geometry correct)
- TMR1 LED2 stopped (expected - arc complete)
- **LED1 never came back** (main loop hung)
- **No output from device** (serial communication dead)
- System appeared frozen after partial arc execution

### Root Cause Analysis

The deadlock occurred due to a **flow control flag management bug** when arc generation completed:

```
1. Arc finishes → TMR1 ISR sets arc_gen.active = false
2. Buffer still has 8+ segments waiting to execute
3. Main loop calls MotionBuffer_SignalArcCanContinue()
4. Function checks if (!arc_gen.active) → returns early ❌
5. arc_can_continue flag NEVER gets reset to true
6. Remaining segments can't execute (waiting for flow control)
7. Main loop stuck waiting for buffer to drain → DEADLOCK!
```

**Key Issue**: When arc completed, `arc_can_continue` was left in `false` state, but the signal function couldn't reset it because it exited early when `!arc_gen.active`.

### The Fix

**File**: `srcs/motion/motion_buffer.c`  
**Location**: Lines 372-395 (arc completion check in TMR1 ISR)

**Change**: Reset flow control flag when arc completes

```c
/* Check if arc complete */
if (arc_gen.current_segment > arc_gen.total_segments)
{
    /* Arc complete! */
    arc_gen.active = false;
    
    /* CRITICAL (Oct 25, 2025 - DEADLOCK FIX):
     * Reset flow control flag to allow remaining buffered segments to drain!
     * Without this, main loop's SignalArcCanContinue() exits early when !arc_gen.active,
     * and if arc_can_continue=false, remaining segments never execute → DEADLOCK!
     */
    arc_can_continue = true;  // ← NEW LINE - resets flow control
    
    /* Re-enable position updates */
    disable_position_update = false;
    
    /* Re-enable debug output */
    suppress_debug_output = false;
    
    /* Update planned position to final arc target */
    planned_position_mm[arc_gen.axis_0] = arc_gen.segment_template.target[arc_gen.axis_0];
    planned_position_mm[arc_gen.axis_1] = arc_gen.segment_template.target[arc_gen.axis_1];
    planned_position_mm[arc_gen.axis_linear] = arc_gen.segment_template.target[arc_gen.axis_linear];
    
    /* Stop TMR1 - no longer needed */
    TMR1_Stop();
    
    /* Set flag for main loop to send "ok" response */
    arc_complete_flag = true;
}
```

### Flow Control System (Context)

The arc generator uses adaptive flow control to prevent buffer overflow:

**TMR1 ISR (Generation Side)**:
- Pause generation when buffer count >= 8 blocks (50% full)
- Sets `arc_can_continue = false`

**Main Loop (Consumption Side)**:
- Calls `MotionBuffer_SignalArcCanContinue()` every iteration
- Resume generation when buffer count < 6 blocks (37.5% full)
- Sets `arc_can_continue = true`

**Hysteresis**: 2-block gap (pause at 8, resume at 6) prevents oscillation

### Why The Bug Happened

The `SignalArcCanContinue()` function had an early exit check:

```c
void MotionBuffer_SignalArcCanContinue(void)
{
    /* Only signal if arc is actually active */
    if (!arc_gen.active)
    {
        return;  // ← EXITS EARLY when arc completes!
    }
    
    /* Resume generation if buffer has drained below threshold */
    uint8_t count = MotionBuffer_GetCount();
    if (count < 6U)
    {
        arc_can_continue = true;  // ← NEVER REACHED after arc complete!
    }
}
```

**Intent**: Don't signal if no arc is running  
**Bug**: Also prevents cleanup when arc just finished with buffered segments remaining

### Testing Results

**Before Fix**:
- ❌ Arc executes partially then hangs
- ❌ LED1 stops (main loop deadlock)
- ❌ No serial output
- ❌ System appears frozen

**After Fix**:
- ✅ Arc completes fully
- ✅ LED1 continues blinking (main loop responsive)
- ✅ Serial output resumes
- ✅ System ready for next command

### Related Fixes This Session

This deadlock fix was the final piece in a series of arc generator improvements:

1. **Arc Completion Acknowledgment** (first fix)
   - Added `MotionBuffer_CheckArcComplete()` to main loop
   - Sends "ok" from safe main loop context (not ISR)
   - Prevents crash on arc completion

2. **Arc Correction Disabled** (second fix)
   - Set `N_ARC_CORRECTION = 0`
   - Eliminates "jiggle" at quadrant boundaries
   - Smooth motion using pure rotation matrix

3. **TMR1 Rate Throttling** (third fix)
   - Changed from 20ms → 40ms (50 Hz → 25 Hz)
   - Reduces buffer pressure

4. **Flow Control Implementation** (fourth fix - user's idea!)
   - Added `arc_can_continue` volatile flag
   - TMR1 checks flag before generating segment
   - Main loop signals when buffer has space

5. **Flow Control Threshold Tuning** (fifth fix)
   - Lowered pause threshold: 12 → 8 blocks
   - Lowered resume threshold: 12 → 6 blocks
   - Added 2-block hysteresis

6. **Deadlock Fix** (sixth fix - THIS ONE)
   - Reset `arc_can_continue = true` when arc completes
   - Allows buffered segments to drain properly
   - Main loop no longer hangs

### Lessons Learned

1. **State Management is Critical**: Flags must be properly reset during state transitions
2. **Early Returns Can Hide Bugs**: The `!arc_gen.active` guard prevented cleanup
3. **Flow Control Needs Cleanup**: Production vs. cleanup are different states
4. **LED Diagnostics Invaluable**: LED1 stopping revealed main loop hang immediately
5. **User's Insight Was Key**: The flow control idea solved the buffer overflow

### Future Improvements (Optional)

**Separate Cleanup State**:
```c
typedef enum {
    ARC_IDLE,           // No arc active
    ARC_GENERATING,     // TMR1 generating segments
    ARC_DRAINING        // Arc complete, draining buffered segments
} arc_state_t;
```

This would make state transitions explicit and prevent similar bugs.

### Files Modified

1. **srcs/motion/motion_buffer.c** (line 381)
   - Added `arc_can_continue = true;` in arc completion block
   - Single-line fix with critical impact

### Build Command

```bash
make all BUILD_CONFIG=Debug DEBUG_MOTION_BUFFER=0
```

### Hardware Test Results (October 25, 2025)

- ✅ Quarter circle executes completely
- ✅ Half circle executes completely
- ✅ No "BUFFER FULL!" errors
- ✅ No planner starvation
- ✅ LED1 continues blinking throughout
- ✅ LED2 behavior correct (blinks during arc, stops after)
- ✅ System responsive after arc completion
- ✅ Serial communication maintained

## Conclusion

**Single-line fix with massive impact**: Adding `arc_can_continue = true;` when arc completes eliminated a critical deadlock that made the arc generator unusable in production.

The bug was subtle because:
- It only manifested AFTER arc completion
- The flow control system worked perfectly DURING generation
- The cleanup path was overlooked during implementation

This highlights the importance of:
- Thorough state transition testing
- LED diagnostics for embedded debugging
- User feedback during development (deadlock discovered by user)

**Status**: ✅ **PRODUCTION READY** - Arc generator now fully operational!

---

**Date**: October 25, 2025  
**Branch**: feature/tmr1-arc-generator  
**Commit**: [Pending user commit]  
**Tested By**: User (davec)  
**Issue**: Deadlock after arc completion  
**Resolution**: Reset flow control flag during cleanup  
**Impact**: Arc generator now production-ready
