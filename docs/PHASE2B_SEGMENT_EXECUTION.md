# Phase 2B: Segment Execution Complete

**Date**: October 19, 2025  
**Status**: ✅ **BUILD SUCCESSFUL** - Ready for hardware testing  
**Hex File**: `bins/CS23.hex` (216,796 bytes)

## Overview

Phase 2B completes the GRBL stepper integration by wiring the segment buffer to the hardware OCR modules. The system now uses **segment-based execution** instead of the Phase 1 S-curve state machine.

## Architecture Changes

### Data Flow (Phase 2B)
```
G-code Parser
    ↓
GRBL Planner (ring buffer with junction velocity planning)
    ↓
Segment Prep (TMR9 @ 100Hz) - Breaks blocks into 2mm segments
    ↓
Segment Buffer (6-entry ring buffer)
    ↓
OCR Callbacks (OCMP1/3/4/5) - Execute segments with Bresenham
    ↓
Step Pulse Generation (hardware OCR modules)
```

### Key Components Modified

#### 1. **multiaxis_control.c** - OCR Callbacks Rewritten
**Before (Phase 1)**:
```c
static void OCMP5_StepCounter_X(uintptr_t context)
{
    axis_state[AXIS_X].step_count++;  // Simple counter
}
```

**After (Phase 2B)**:
```c
static void OCMP5_StepCounter_X(uintptr_t context)
{
    // Update machine position (absolute tracking)
    if (direction_bits & (1 << AXIS_X)) {
        machine_position[AXIS_X]++;
    } else {
        machine_position[AXIS_X]--;
    }
    
    // Check if segment complete
    if (++step_count >= current_segment->n_step) {
        GRBLStepper_SegmentComplete();  // Free buffer slot
        
        // Get next segment
        const st_segment_t *next = GRBLStepper_GetNextSegment();
        if (next != NULL) {
            // Configure OCR for new segment
            set_direction_gpio(next->direction_bits);
            configure_period(next->period);
            start_timer();
        } else {
            // No more segments - go idle
            axis_idle();
        }
    }
}
```

**Critical Features**:
- ✅ Absolute position tracking (machine_position[])
- ✅ Auto-advance to next segment when complete
- ✅ Hardware configuration per segment (direction + period)
- ✅ Graceful idle when buffer empty

#### 2. **Segment Execution State** - New Per-Axis Tracking
```c
typedef struct {
    const st_segment_t *current_segment;  // Pointer to active segment
    uint32_t step_count;                  // Steps executed in segment
    int32_t bresenham_counter;            // Bresenham error accumulator
    bool active;                          // Axis executing?
} axis_segment_state_t;

static volatile axis_segment_state_t segment_state[NUM_AXES];
```

#### 3. **MultiAxis_StartSegmentExecution()** - New Kickoff Function
```c
bool MultiAxis_StartSegmentExecution(void)
{
    const st_segment_t *first_seg = GRBLStepper_GetNextSegment();
    if (first_seg == NULL) return false;
    
    // For each axis with motion:
    for (axis with steps > 0) {
        set_direction_gpio();
        enable_driver();
        configure_ocr_period();
        start_hardware();
    }
    
    return true;
}
```

**Called by**: `motion_manager.c` when new planner block arrives

#### 4. **MultiAxis_IsBusy()** - Updated Check
**Before**: Checked `axis_state[].active` (S-curve phase)  
**After**: Checks `segment_state[].active` (segment execution)

#### 5. **TMR1 S-Curve ISR** - Disabled
- Entire `TMR1_MultiAxisControl()` wrapped in `#if 0` block
- Preserved for reference during Phase 2 development
- Will be removed in Phase 2C after validation

## File Changes Summary

| File                              | Lines Changed     | Description                                     |
| --------------------------------- | ----------------- | ----------------------------------------------- |
| `srcs/motion/multiaxis_control.c` | +280 lines        | New segment execution callbacks, state tracking |
| `incs/motion/multiaxis_control.h` | +15 lines         | New `MultiAxis_StartSegmentExecution()` API     |
| `srcs/motion/grbl_stepper.c`      | ~60 lines removed | Removed duplicate copy-based API                |
| `incs/motion/grbl_stepper.h`      | ~30 lines removed | Removed old copy-based declarations             |

## Memory Impact

### Removed (Phase 1 S-Curve)
- TMR1 ISR code: ~280 lines (disabled, will remove in Phase 2C)
- S-curve state: `scurve_state_t[4]` = ~800 bytes (unused)

### Added (Phase 2B Segment Execution)
- Segment state: `axis_segment_state_t[4]` = ~64 bytes
- OCR callback code: ~280 lines (replaces TMR1 ISR)

**Net Change**: ~736 bytes saved (S-curve state no longer needed)

## Segment Execution Flow

### Motion Start
1. **G-code parsed** → `GRBLPlanner_BufferLine()`
2. **Planner optimizes** junction velocities
3. **TMR9 @ 100Hz** calls `GRBLStepper_PrepSegment()`
   - Breaks block into 2mm segments
   - Calculates velocity interpolation (v² = v₀² + 2ad)
   - Computes OCR period and Bresenham ratios
   - Adds to segment buffer (6 entries)
4. **motion_manager** calls `MultiAxis_StartSegmentExecution()`
   - Gets first segment from buffer
   - Configures OCR hardware (direction + period)
   - Enables OCR modules
   - Starts timers

### During Motion
1. **OCR hardware** generates step pulses at configured period
2. **OCR callback** (ISR) called after each step:
   - Updates `machine_position[]` based on direction
   - Increments `step_count`
   - When `step_count >= n_step`:
     - Call `GRBLStepper_SegmentComplete()` (free buffer slot)
     - Get next segment
     - Reconfigure OCR period (smooth velocity transition)
     - Continue without stopping!

### Motion Complete
- Last segment executed
- `GRBLStepper_GetNextSegment()` returns NULL
- OCR callback disables hardware
- Axis goes idle
- `MultiAxis_IsBusy()` returns false

## Dave's Key Understanding

**"We don't stop - we adjust segments down to F500!"**

This is now **IMPLEMENTED**:
- Each segment is a small portion of the move (2mm)
- Segments have pre-calculated velocities from planner
- OCR callback smoothly transitions between segments
- **NO STOPS** at segment boundaries - just period changes
- Junction velocity optimization ensures smooth cornering

### Example: 100mm Move @ F1000 → F500 Junction

```
Segment 1: 2mm @ 1000mm/min (period = 4800 counts)
Segment 2: 2mm @ 950mm/min  (period = 5053 counts) ← Decel starts
Segment 3: 2mm @ 900mm/min  (period = 5333 counts)
...
Segment 48: 2mm @ 550mm/min (period = 8727 counts)
Segment 49: 2mm @ 500mm/min (period = 9600 counts) ← Junction speed
Segment 50: 2mm @ 550mm/min (period = 8727 counts) ← Next move starts
...
```

**Critical**: OCR callback reconfigures period EVERY SEGMENT (every 2mm or ~0.12 seconds). Smooth velocity profile with NO mechanical stops!

## Differences from Phase 1

| Aspect                | Phase 1 (S-Curve)               | Phase 2B (Segments)                      |
| --------------------- | ------------------------------- | ---------------------------------------- |
| **Timing Source**     | TMR1 @ 1kHz (software)          | OCR period (hardware)                    |
| **Velocity Control**  | 7-segment S-curve equations     | Velocity interpolation (v² = v₀² + 2ad)  |
| **Move Segmentation** | Single profile per move         | Multiple 2mm segments per move           |
| **Junction Handling** | Stop between moves              | Continuous motion with velocity planning |
| **CPU Load**          | High (1kHz ISR with float math) | Low (OCR callback just counts steps)     |
| **Look-Ahead**        | None                            | Full GRBL junction velocity optimization |
| **Memory**            | 800 bytes S-curve state         | 64 bytes segment state                   |

## Testing Checklist (Phase 2B Step 4)

### Single-Axis Motion
- [ ] Flash new firmware (`bins/CS23.hex`)
- [ ] Send `G0 X10` via UGS
- [ ] Verify with oscilloscope:
  - [ ] Step pulses generated on OCMP5 (X-axis)
  - [ ] Pulse width = 25.6µs (40 counts @ 1.5625MHz)
  - [ ] Smooth acceleration (period decreases)
  - [ ] Direction GPIO set correctly
- [ ] Verify position feedback: `<Idle|MPos:10.000,0.000,0.000>`

### Multi-Axis Motion (Phase 2C)
- [ ] Test square pattern: `G1 Y10 X10 Y0 X0 F1000`
- [ ] Verify returns to origin: `<Idle|MPos:0.000,0.000,0.000>`
- [ ] Test junction velocity: `G1 X100 F1000` → `G1 X200 F500`
- [ ] Oscilloscope: Verify smooth velocity transition (NO STOP)

### Edge Cases
- [ ] Zero-length moves filtered (no hang)
- [ ] Single-axis moves work (no diagonal drift)
- [ ] Direction reversals correct
- [ ] Position tracking accurate in both directions

## Known Limitations

1. **Bresenham not fully used**: Current implementation uses dominant axis only
   - Each axis executes `n_step` pulses from segment
   - Bresenham counter prepared but not used for subordinate axes
   - TODO: Add Bresenham logic for multi-axis coordination
   
2. **No segment validation**: Assumes segment prep always fills buffer
   - If TMR9 prep too slow, OCR callback sees NULL segment
   - System goes idle (graceful, but could underrun)
   - TODO: Add statistics tracking for segment buffer underruns

3. **TMR1 code still present**: Wrapped in `#if 0` for reference
   - Will remove entirely in Phase 2C after validation
   - Currently no side effects (just occupies flash space)

## Next Steps

### Phase 2B Step 4: Hardware Testing (IMMEDIATE)
1. Flash firmware to hardware
2. Test single-axis motion with oscilloscope
3. Verify position feedback accuracy
4. Document any issues

### Phase 2C: Multi-Axis Validation (NEXT)
1. Test coordinated moves (square pattern)
2. Test junction velocity transitions
3. Verify smooth velocity profiles (no stops)
4. Performance profiling (CPU usage, segment prep rate)

### Phase 2D: Cleanup & Documentation (FINAL)
1. Remove TMR1 S-curve code completely
2. Add full Bresenham for multi-axis coordination
3. Add segment buffer statistics
4. Update PlantUML diagrams
5. Create Phase 2 completion summary
6. Git commit with tag

## Build Information

**Compiler**: XC32 v4.60  
**Build Command**: `make all`  
**Warnings**: 1 (bootloader warning - expected)  
**Errors**: 0  
**Output Size**: 216,796 bytes (vs 213,074 Phase 2A = +3.7KB for segment execution)  
**Build Time**: October 19, 2025 5:35 PM

## References

- **GRBL Source**: `gnea/grbl` v1.1f stepper.c
- **Phase 2A Summary**: `docs/PHASE2A_COMPLETE_SUMMARY.md`
- **Segment Buffer**: `incs/motion/grbl_stepper.h` lines 35-115
- **OCR Architecture**: `docs/OCR_IMPLEMENTATION_NOTES.md`
- **Timer Prescaler**: `docs/TIMER_PRESCALER_ANALYSIS.md`
