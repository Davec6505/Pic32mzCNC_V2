# OCR Single-Pulse Implementation (October 21, 2025)

## Summary

Implemented **OCM=0b101 (Dual Compare Continuous Mode) with ISR auto-disable** pattern for clean single-pulse-on-demand operation for subordinate axes while maintaining continuous pulsing for dominant axes.

## Architecture

### OCR Mode Configuration

**All axes use OCM=0b101** (Dual Compare Continuous Pulse mode):
- Configured once by MCC at initialization
- No runtime mode switching required
- OCxR = 5 (rising edge at 5 counts)
- OCxRS = 50 (falling edge at 50 counts, ~32µs pulse width @ 1.5625MHz)
- ISR fires on **FALLING EDGE** (datasheet section 16.3.2.4)

### ISR Logic - Role-Based Behavior

Each axis ISR checks the `segment_completed_by_axis` bitmask to determine its role:

**DOMINANT Axis** (bit SET in bitmask):
```c
void OCMPx_Callback(uintptr_t context)
{
    axis_id_t axis = AXIS_X;  // (or Y/Z/A)
    
    if (segment_completed_by_axis & (1 << axis))
    {
        // DOMINANT: Process segment step
        ProcessSegmentStep(axis);  // Runs Bresenham for subordinates
        // OCR stays enabled → continuous pulsing at segment rate
    }
    else
    {
        // SUBORDINATE: Auto-disable OCR
        axis_hw[axis].OCMP_Disable();  // Stops continuous pulsing
    }
}
```

**SUBORDINATE Axis** (bit NOT SET in bitmask):
- ISR fires on falling edge (pulse just completed)
- Calls `axis_hw[axis].OCMP_Disable()` to stop continuous pulsing
- Waits for Bresenham to re-enable OCR for next pulse

### Bresenham Trigger (Subordinate Axes)

When Bresenham algorithm determines subordinate axis needs a step:

```c
// In ProcessSegmentStep(), when bresenham_counter overflows:
axis_hw[sub_axis].OCMP_Enable();  // One line - pulse fires automatically!
```

**What happens:**
1. Bresenham enables OCR module
2. OCR hardware generates pulse (rising at count 5, falling at count 50)
3. ISR fires on falling edge
4. ISR disables OCR (stops continuous pulsing)
5. Cycle repeats when Bresenham triggers next step

## Evolution of Solution

### Attempts That Failed

1. **OCM=0b001 (Single Compare, Set OCx High)**
   - Problem: Pin stays HIGH until manually disabled
   - Issue: Timing complexity, not self-limiting

2. **OCM=0b011 (Toggle Mode)**
   - Problem: Pin toggles on each compare match
   - Issue: Need two matches per pulse, complex state tracking

3. **OCM=0b110 (Dual Compare Single Pulse)**
   - Problem: Still requires module restart
   - Issue: Similar complexity to 0b001

4. **OCM=0b100 (Dual Compare Single) with Timer Rollover Trick**
   - Pattern: mikroC proven working code used this
   - Implementation: Set TMRx = 0xFFFF to force rollover
   - Problem: Caused continuous pulsing in our implementation
   - Issue: Timer keeps running, generates pulses on every rollover

### Final Solution - OCM=0b101

**Key Insight from User:**
> "We leave the mode alone, all we do is when axis is not active we disable OCR every ISR as we get the ISR at the falling edge"

**Why OCM=0b101 is Perfect:**
- ISR fires on **falling edge** (per datasheet 16.3.2.4)
- This is the perfect timing to auto-disable!
- No mode switching needed
- Same mode works for both dominant and subordinate roles
- Clean, elegant, hardware-centric solution

## Benefits

✅ **No Mode Switching** - OCM=0b101 set once by MCC, never changed  
✅ **No Timer Tricks** - No rollover manipulation required  
✅ **Self-Limiting** - ISR auto-disables after pulse completes  
✅ **One-Line Trigger** - Bresenham just calls `OCMP_Enable()`  
✅ **Hardware-Centric** - Leverages OCR hardware capabilities  
✅ **Role Agnostic** - Same ISR code handles both dominant/subordinate  
✅ **Clean Code** - Uses existing PLIB function pointers from `axis_hw[]`  

## Code Locations

**ISR Callbacks:** `srcs/motion/multiaxis_control.c` lines ~1134-1237
- `OCMP5_StepCounter_X()` - X-axis ISR
- `OCMP1_StepCounter_Y()` - Y-axis ISR
- `OCMP4_StepCounter_Z()` - Z-axis ISR
- `OCMP3_StepCounter_A()` - A-axis ISR (if enabled)

**Bresenham Trigger:** `srcs/motion/multiaxis_control.c` lines ~605-640
- `ProcessSegmentStep()` function
- Enables subordinate OCR when step needed

**Hardware Configuration:** `srcs/motion/multiaxis_control.c` lines ~60-140
- `axis_hw[]` array with PLIB function pointers
- `OCMP_Enable()`, `OCMP_Disable()` abstraction

## Testing Status

- **Build:** ✅ Compiles successfully
- **Flash:** ⏳ Pending user testing
- **Hardware Test:** ⏳ Awaiting oscilloscope verification

## Expected Behavior

**Test Command:** `G1 X100 Y100 F200`

**Expected Results:**
1. Both X and Y axes should show pulses on oscilloscope
2. Y pulses at SAME RATE as X (not double rate)
3. Pulses clean, proper width (~32µs)
4. Motion completes to correct position (X=100mm, Y=100mm)
5. No continuous pulsing after motion stops

**Debug Points:**
- Monitor `segment_completed_by_axis` bitmask values
- Verify ISR behavior: Dominant processes, subordinate disables
- Check Bresenham re-enables subordinate OCR at correct times
- Oscilloscope: Confirm pulse rates match between axes

## Historical Context

This solution concludes a multi-day debugging journey:
- **Oct 20:** Removed PPS remapping approach, explored multiple OCR modes
- **Oct 20-21:** Analyzed mikroC proven code, deep datasheet study
- **Oct 21:** User insight led to OCM=0b101 with ISR disable pattern
- **Oct 21:** Implemented final solution using PLIB function pointers

## References

- **PIC32MZ Datasheet:** Section 16.3.2.4 - Dual Compare Continuous Output Pulses Mode
- **mikroC Code:** `Pic32mzCNC/` folder - proven working toggleOCx() pattern
- **GRBL Stepper:** Original architecture inspiration for dominant/subordinate pattern
- **Copilot Instructions:** `.github/copilot-instructions.md` - Complete architecture documentation
