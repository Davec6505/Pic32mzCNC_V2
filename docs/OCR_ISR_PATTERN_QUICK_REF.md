# OCR ISR Pattern - Quick Reference

## One Pattern, Two Behaviors

All axis ISRs use the **same code pattern** but behave differently based on role:

```c
static void OCMPx_StepCounter_<AXIS>(uintptr_t context)
{
    axis_id_t axis = AXIS_<X/Y/Z/A>;
    
    // Check role via bitmask
    if (segment_completed_by_axis & (1 << axis))
    {
        // DOMINANT: Process segment, run Bresenham
        ProcessSegmentStep(axis);
    }
    else
    {
        // SUBORDINATE: Disable OCR after pulse
        axis_hw[axis].OCMP_Disable();
    }
}
```

## Hardware Configuration

**Timer/OCR Assignments:**
- X-axis: OCMP5 + TMR3
- Y-axis: OCMP1 + TMR4
- Z-axis: OCMP4 + TMR2
- A-axis: OCMP3 + TMR5

**OCR Mode (ALL axes):**
- OCM = 0b101 (Dual Compare Continuous)
- Set once by MCC at initialization
- Never changed at runtime

**Compare Values:**
- OCxR = 5 (rising edge at count 5)
- OCxRS = 50 (falling edge at count 50)
- Pulse width = 45 counts × 640ns = 28.8µs

**ISR Timing:**
- Fires on **FALLING EDGE** (when pulse completes)
- Per datasheet section 16.3.2.4

## Bitmask Table

The `segment_completed_by_axis` bitmask determines which axis is dominant:

| Bitmask | Binary | Dominant Axis | Subordinates |
| ------- | ------ | ------------- | ------------ |
| 0x01    | 0b0001 | X (bit 0)     | Y, Z, A      |
| 0x02    | 0b0010 | Y (bit 1)     | X, Z, A      |
| 0x04    | 0b0100 | Z (bit 2)     | X, Y, A      |
| 0x08    | 0b1000 | A (bit 3)     | X, Y, Z      |

**Example:** `G1 X100 Y50`
- X has most steps → X is dominant
- `segment_completed_by_axis = 0x01`
- OCMP5 ISR (X): bit 0 SET → calls `ProcessSegmentStep(AXIS_X)`
- OCMP1 ISR (Y): bit 1 CLEAR → calls `axis_hw[AXIS_Y].OCMP_Disable()`

## Data Flow

```
DOMINANT AXIS (e.g., X with 100 steps):
┌─────────────────────────────────────────────┐
│ OCR5 generates continuous pulses            │
│   ↓ Every pulse (at segment rate)           │
│ OCMP5 ISR fires on falling edge            │
│   ↓ Bitmask check: 0x01 & (1<<0) = TRUE    │
│ ProcessSegmentStep(AXIS_X)                 │
│   ↓ Updates position, runs Bresenham        │
│ Check subordinate Y needs step?            │
│   ↓ If yes: axis_hw[AXIS_Y].OCMP_Enable()  │
│ OCR5 stays enabled → next pulse            │
└─────────────────────────────────────────────┘

SUBORDINATE AXIS (e.g., Y with 50 steps):
┌─────────────────────────────────────────────┐
│ Bresenham: axis_hw[AXIS_Y].OCMP_Enable()   │
│   ↓ OCR1 generates ONE pulse                │
│ OCMP1 ISR fires on falling edge            │
│   ↓ Bitmask check: 0x01 & (1<<1) = FALSE   │
│ axis_hw[AXIS_Y].OCMP_Disable()             │
│   ↓ OCR1 disabled, stops pulsing            │
│ Wait for next Bresenham trigger            │
└─────────────────────────────────────────────┘
```

## Key Functions

**ISR Callbacks** (multiaxis_control.c):
```c
OCMP5_StepCounter_X()  // X-axis ISR (lines ~1134-1152)
OCMP1_StepCounter_Y()  // Y-axis ISR (lines ~1154-1172)
OCMP4_StepCounter_Z()  // Z-axis ISR (lines ~1174-1192)
OCMP3_StepCounter_A()  // A-axis ISR (lines ~1194-1212)
```

**Segment Processing** (multiaxis_control.c):
```c
ProcessSegmentStep()   // Line ~540, called by dominant ISR
```

**Hardware Abstraction** (multiaxis_control.c):
```c
axis_hw[axis].OCMP_Enable()   // Enable OCR module
axis_hw[axis].OCMP_Disable()  // Disable OCR module
```

## Debugging Tips

**Check Bitmask:**
```c
// Add to ProcessSegmentStep() or ISR:
UGS_Printf("Bitmask: 0x%02X, Axis: %d\r\n", 
           segment_completed_by_axis, axis);
```

**Monitor ISR Behavior:**
```c
// Add counter in ISR:
static volatile uint32_t dominant_calls = 0;
static volatile uint32_t subordinate_disables = 0;

if (segment_completed_by_axis & (1 << axis)) {
    dominant_calls++;
    ProcessSegmentStep(axis);
} else {
    subordinate_disables++;
    axis_hw[axis].OCMP_Disable();
}
```

**Oscilloscope Verification:**
- Probe STEP pins for X and Y axes
- Send `G1 X100 Y100 F200`
- Verify both axes pulsing at same rate
- Confirm no continuous pulsing after motion stops

## Common Issues

**Issue:** Subordinate pulses twice as fast
- **Cause:** ISR not disabling OCR (continuous mode running)
- **Fix:** Verify `axis_hw[axis].OCMP_Disable()` is called
- **Check:** Bitmask value - should be 0 for subordinate bit

**Issue:** No subordinate pulses visible
- **Cause:** Bresenham not re-enabling OCR
- **Fix:** Verify `axis_hw[sub_axis].OCMP_Enable()` called in ProcessSegmentStep
- **Check:** Bresenham counter overflow logic

**Issue:** Dominant axis stops pulsing
- **Cause:** ISR mistakenly disabling OCR
- **Fix:** Verify bitmask check condition correct
- **Check:** `segment_completed_by_axis` value during segment execution

## Performance Notes

- **ISR Overhead:** Minimal - one bitmask check, one function call
- **Context Switch:** ~200ns @ 200MHz CPU
- **PLIB Function:** `OCMP_Disable()` is inline, ~5ns overhead
- **Total ISR Time:** <500ns (well within 1µs minimum step period)

## Migration from Previous Patterns

**Old Pattern (Bit-Bang):**
```c
// GPIO direct manipulation
StepY_Set();
delay_us(10);
StepY_Clear();
```

**New Pattern (OCR Hardware):**
```c
// Enable OCR, hardware generates pulse
axis_hw[AXIS_Y].OCMP_Enable();
// ISR auto-disables after pulse completes
```

**Benefits:**
- No software delays (non-blocking)
- Precise timing (hardware-controlled)
- CPU free for other tasks
- DRV8825 timing guaranteed (28.8µs pulse > 1.9µs minimum)
