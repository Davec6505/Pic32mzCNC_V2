# GRBL Timer ISR vs PIC32MZ OCR Architecture

## GRBL's AVR Architecture (Software-Based)

### Two Timer Interrupts:

**1. TIMER1_COMPA_vect (~30kHz ISR)**
- Runs Bresenham algorithm
- Calculates which axes need to step
- Sets GPIO pins HIGH for step pulse
- Triggers TIMER0 to reset pins

**2. TIMER0_OVF_vect (Pulse Width)**
- Waits `settings.pulse_microseconds` (1-10µs)
- Resets GPIO pins LOW
- Completes the step pulse

### Key Point:
**EVERY SINGLE STEP** requires **TWO ISR calls** + Bresenham calculations!
This is CPU-intensive but necessary on AVR because there's no hardware pulse generation.

---

## Our PIC32MZ Architecture (Hardware-Based)

### The Revolutionary Difference: **NO ISR FOR STEP PULSES!**

**OCR Dual-Compare Continuous Pulse Mode:**
```
Configuration (done ONCE per move):
  TMR2_PeriodSet(period);              // Pulse frequency
  OCMP4_CompareValueSet(period - 40);  // Rising edge
  OCMP4_CompareSecondaryValueSet(40);  // Falling edge (1.9µs)
  OCMP4_Enable();
  TMR2_Start();

Result: Hardware generates pulses AUTOMATICALLY!
  - No ISR per step
  - No Bresenham in ISR
  - CPU is FREE for planning
```

### OCR Callbacks (Only for Position Tracking):
```c
void APP_OCMP4_Callback(uintptr_t context)  // Called per step
{
    cnc_axes[AXIS_X].current_position++;  // Just count steps
    cnc_axes[AXIS_X].steps_executed++;
    
    if (steps_executed >= steps_to_execute) {
        OCMP4_Disable();  // Motion complete
        TMR2_Stop();
    }
}
```

**Key difference:** Callback only updates position counters, doesn't generate pulses!

---

## The Critical Question: Where Does TMR1 Fit?

### In GRBL (AVR):
```
TMR1 ISR @ 30kHz:
  1. Pop segment from buffer
  2. Run Bresenham for all axes
  3. Toggle GPIO pins
  → Directly generates step pulses
```

### In Our PIC32MZ:
```
Option A: NO TMR1 ISR AT ALL! ❌
  - OCR hardware does everything
  - Problem: No segment transitions!
  
Option B: TMR1 @ 1kHz for Segment Management ✅
  TMR1_Callback():
    1. Check if current segment complete
    2. Load next segment from buffer
    3. Calculate new OCR periods for each axis
    4. Update OCR hardware with new rates
    → Manages motion profile, NOT individual steps!
```

---

## Recommended Architecture

### Layer 1: GRBL Planning (Unchanged)
```
planner.c → Creates motion blocks with velocities
↓
st_prep_buffer() → Breaks blocks into segments
```

### Layer 2: PIC32 Segment Executor (pic32_stepper.c)
```c
// TMR1 callback @ 1kHz
void TMR1_Callback(void)
{
    // Get next segment from GRBL buffer
    segment_t *segment = get_next_segment();
    
    if (segment) {
        // Calculate OCR periods for each axis
        for (each axis) {
            float step_rate = calculate_step_rate(segment);
            uint32_t period = APP_CalculateOCRPeriod(step_rate);
            
            // Update OCR hardware
            APP_UpdateAxisRate(axis, period);
        }
    }
}
```

### Layer 3: OCR Hardware (app.c)
```c
// Runs continuously at variable frequency (10Hz - 30kHz)
void APP_OCMP4_Callback(uintptr_t context)
{
    // Just count steps
    cnc_axes[AXIS_X].current_position++;
    
    // Stop when segment complete
    if (segment_steps_done) {
        APP_StopAxisMotion(AXIS_X);
    }
}
```

---

## The Genius of This Architecture

**GRBL (AVR):**
- ISR @ 30kHz = 33,333 ISR calls per second
- Each ISR: Bresenham + GPIO toggle
- CPU usage: HIGH

**PIC32MZ (Our System):**
- TMR1 @ 1kHz = 1,000 ISR calls per second
- Each ISR: Load new segment parameters
- OCR callbacks: ~1,000-30,000/sec (only position update)
- CPU usage: LOW (33x reduction in ISR frequency!)

---

## Implementation Plan

### 1. Keep TMR1 for Segment Management
```c
// srcs/pic32_stepper.c
void TMR1_SegmentCallback(uintptr_t context)
{
    st_prep_buffer();  // GRBL function - prepares segments
    
    // Update OCR hardware with new segment data
    if (new_segment_ready) {
        update_ocr_from_segment();
    }
}
```

### 2. OCR Callbacks Only Track Position
```c
// srcs/app.c (already done!)
void APP_OCMP4_Callback(uintptr_t context)
{
    cnc_axes[AXIS_X].current_position++;  // No pulse generation!
}
```

### 3. Remove Bresenham from ISR
We DON'T need Bresenham line algorithm in ISR because:
- GRBL's planner already did the math
- Each segment has fixed step rate
- OCR hardware generates the pulses

---

## Summary

**Where TMR1 Lives:**
- `pic32_stepper.c` - Segment buffer management

**What TMR1 Does:**
- Pop next segment (@ 1kHz)
- Calculate new OCR periods
- Update hardware

**What TMR1 Does NOT Do:**
- Generate step pulses (OCR does this)
- Run Bresenham (not needed)
- Toggle GPIO (OCR does this)

**Result:** 33x less CPU load, smoother motion, GRBL's proven planning!

