# Current Architecture Analysis (October 18, 2025)

## Current Status
- ✅ Motion buffer: Calculating correct step values (800 steps for 10mm move)
- ✅ Position tracking: Planned position correct (10.000mm)
- ✅ Execution layer: Receiving correct values (X=0 Y=800 Z=0)
- ❌ Hardware output: **Position overshoot** (13.038mm instead of 10mm)

## Test Results
```
Command: G1 Y10 F1000
Expected: MPos:0.000,10.000,0.000
Actual:   MPos:0.000,13.038,0.000
```

**Analysis**: 
- Target: 800 steps
- Actual: 1043 steps (130.4% of target)
- **Root Cause**: Time-based S-curve continues beyond target step count

## Current Motion Control Flow

### Data Flow (Working Correctly)
```
G-code Parser → Motion Buffer → MultiAxis_ExecuteCoordinatedMove()
    ↓              ↓                        ↓
parsed_move_t  motion_block_t        Steps array [X,Y,Z,A]
(mm values)    (step values)         (relative steps)
```

### Hardware Control (Issue Here)
```
TMR1 @ 1kHz → S-Curve State Machine
    ↓
Calculate velocity based on TIME
    ↓
Convert velocity → OCR period
    ↓
OCR hardware generates pulses
    ↓
Step count increments (but keeps going past target!)
```

## Problem: Time vs Step Discrepancy

### Current Approach (Time-Based)
1. Calculate S-curve profile based on distance and max velocity
2. Update velocity every 1ms based on **elapsed time**
3. Stop when time expires OR step count >= target
4. **Issue**: Time-based profile doesn't perfectly match step count!

### Why Overshoot Occurs
- S-curve timing calculated from: `time = distance / velocity`
- OCR generates pulses at: `frequency = TMR_CLOCK / period`
- **Mismatch**: Slight errors in timing accumulate over move
- **Result**: Hardware generates more pulses than commanded steps

## Proposed Solution: Dominant Axis OCR Scaling

### New Approach (Step-Based Master/Slave)
1. **Dominant axis** (longest distance):
   - Calculate S-curve velocity profile (time-based)
   - Convert to OCR period
   - Generate step pulses

2. **Subordinate axes** (shorter distances):
   - **DO NOT calculate velocity**
   - Scale OCR period from dominant axis:
     ```c
     subordinate_OCR_period = dominant_OCR_period × (dominant_steps / subordinate_steps)
     ```
   - This guarantees both axes finish at SAME time with EXACT step counts!

### Benefits
- ✅ **Guaranteed step count accuracy**: Hardware stops at exact target
- ✅ **Perfect synchronization**: All axes finish simultaneously by design
- ✅ **Simpler code**: No velocity scaling calculations needed
- ✅ **Future-proof**: Easier to extend for circular interpolation

### Example: Coordinated Move
```
Move: X=400 steps, Y=800 steps

Dominant axis: Y (800 steps)
- Calculate S-curve → cruise velocity = 400 steps/sec
- OCR period = 1,562,500 / 400 = 3,906 counts

Subordinate axis: X (400 steps)  
- NO velocity calculation!
- Step ratio: 400 / 800 = 0.5
- OCR period = 3,906 × 2 = 7,812 counts (half frequency)
- Result: X generates 400 pulses while Y generates 800 pulses
- Both finish at EXACT same time! ✅
```

## Implementation Plan

### Phase 1: Add Step-Based Safety Stop (Quick Fix)
- Check `step_count >= total_steps` every TMR1 cycle
- Stop hardware immediately when target reached
- **Status**: Already attempted, still overshooting

### Phase 2: OCR Period Scaling (Proper Fix)
1. Modify `TMR1_MultiAxisControl()`:
   - Only update dominant axis velocity from S-curve
   - Calculate dominant axis OCR period
   - Scale subordinate OCR periods by step ratio

2. Remove velocity calculations from subordinate axes

3. Test with square pattern:
   ```gcode
   G1 Y10 F1000  ; Should move EXACTLY 10mm
   G1 X10 F1000  ; Should move EXACTLY 10mm  
   G1 Y0 F1000   ; Should return to EXACTLY 0mm
   G1 X0 F1000   ; Should return to EXACTLY 0mm
   ```

## Next Steps
1. ✅ Commit current state (done)
2. 📝 Document current architecture (this file)
3. 🔧 Implement OCR period scaling in `multiaxis_control.c`
4. 🧪 Test with hardware
5. 📊 Verify with oscilloscope (optional but recommended)

## Files to Modify
- `srcs/motion/multiaxis_control.c` - TMR1_MultiAxisControl() function
  - Lines ~590-760: S-curve state machine
  - Key change: Only dominant axis calculates velocity
  - Subordinate axes scale OCR period

## Architecture Decision
**Master/Slave with OCR Scaling** is the correct approach because:
- Hardware (OCR) controls step generation, not software
- Time-based calculations introduce cumulative errors
- OCR period scaling is mathematically exact
- Standard approach in CNC controllers (GRBL uses similar concept)
