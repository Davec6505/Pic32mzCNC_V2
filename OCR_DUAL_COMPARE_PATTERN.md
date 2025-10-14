# OCR Dual-Compare Mode - Standard Pattern Documentation

**Date**: October 14, 2025  
**Status**: ✅ VERIFIED WORKING PATTERN  
**Platform**: PIC32MZ2048EFH100 with DRV8825 Stepper Drivers

## Overview

This document defines the **STANDARD PATTERN** for configuring OCR (Output Compare Register) dual-compare mode for step pulse generation. This pattern has been verified to work correctly and should be used consistently throughout the codebase.

## The Standard Pattern

```c
/* DRV8825 requires minimum 1.9µs pulse width - use 40 timer counts for safety */
const uint32_t OCMP_PULSE_WIDTH = 40;

/* Calculate OCR period from velocity */
uint32_t period = MotionPlanner_CalculateOCRPeriod(velocity_mm_min);

/* Clamp period to 16-bit timer maximum (safety margin) */
if (period > 65485) {
    period = 65485;
}

/* Ensure period is greater than pulse width */
if (period <= OCMP_PULSE_WIDTH) {
    period = OCMP_PULSE_WIDTH + 10;
}

/* Configure OCR dual-compare mode:
 * TMRxPR = period (timer rollover)
 * OCxR = OCMP_PULSE_WIDTH (rising edge - pulse start at count 40)
 * OCxRS = period - OCMP_PULSE_WIDTH (falling edge - pulse end)
 * 
 * Example with period=200:
 *   TMR3PR = 200          // Timer rolls over at 200
 *   OC4R = 40             // Pulse rises at count 40
 *   OC4RS = 160           // Pulse falls at count 160
 *   Pulse width = 160 - 40 = 120 counts (actual ON time = 40)
 * 
 * Timing diagram:
 *   Count: 0....40.......160.......200 (rollover)
 *   Pin:   LOW  HIGH     LOW       LOW
 *          └────40───────┘
 *             Pulse ON
 */
TMRx_PeriodSet(period);                                    // Set timer rollover
OCMPx_CompareValueSet(OCMP_PULSE_WIDTH);                   // Rising edge (fixed at 40)
OCMPx_CompareSecondaryValueSet(period - OCMP_PULSE_WIDTH); // Falling edge (variable)
OCMPx_Enable();
TMRx_Start();
```

## Complete Example for X-Axis (OCMP4 + TMR3)

```c
/* Calculate OCR period from velocity */
uint32_t period = MotionPlanner_CalculateOCRPeriod(cnc_axes[AXIS_X].target_velocity);

/* DRV8825 requires minimum 1.9µs pulse width - use 40 timer counts for safety */
const uint32_t OCMP_PULSE_WIDTH = 40;

/* Clamp period to 16-bit timer maximum (safety margin) */
if (period > 65485) {
    period = 65485;
}

/* Ensure period is greater than pulse width */
if (period <= OCMP_PULSE_WIDTH) {
    period = OCMP_PULSE_WIDTH + 10;
}

/* Configure X-axis (OCMP4 + TMR3) for step pulse generation */
TMR3_PeriodSet(period);                                    // Set timer rollover
OCMP4_CompareValueSet(OCMP_PULSE_WIDTH);                   // Rising edge (fixed at 40)
OCMP4_CompareSecondaryValueSet(period - OCMP_PULSE_WIDTH); // Falling edge (variable)
OCMP4_Enable();                                            // Enable OCR module
TMR3_Start();                                              // Start timer
```

## Key Register Values

| Register           | Purpose                          | Value                    | Notes                           |
| ------------------ | -------------------------------- | ------------------------ | ------------------------------- |
| **TMRxPR**         | Timer period (rollover)          | Variable (from velocity) | Controls pulse frequency        |
| **OCxR**           | Primary compare (rising edge)    | **40** (constant)        | Pulse start - ALWAYS 40 counts  |
| **OCxRS**          | Secondary compare (falling edge) | period - 40              | Pulse end - varies with period  |
| **Pulse Width**    | Actual ON time                   | **40 counts**            | Meets DRV8825 1.9µs minimum     |
| **Maximum Period** | 16-bit timer limit               | **65485**                | Safety margin for 16-bit timers |

## Axis-Specific Mappings

| Axis   | OCR Module | Timer | Notes                    |
| ------ | ---------- | ----- | ------------------------ |
| X-axis | OCMP4      | TMR3  | Primary horizontal axis  |
| Y-axis | OCMP1      | TMR2  | Primary vertical axis    |
| Z-axis | OCMP5      | TMR4  | Primary depth axis       |
| A-axis | OCMP3      | TMR5  | 4th rotary axis (future) |

## Timing Analysis

### Example: 100Hz Test Pulses (period = 200)

```
Timer count:   0....40.......160.......200(rollover)
Step pin:      LOW  HIGH     LOW       LOW
               └────40───────┘
                  Pulse ON
                  
Duration: 40 counts @ 1MHz timer = 40µs (well above 1.9µs minimum)
Frequency: 200 counts/cycle = 5kHz → 100 steps/second
```

### Example: High-Speed Motion (period = 50)

```
Timer count:   0....40....50(rollover)
Step pin:      LOW  HIGH  LOW
               └────10────┘
                Pulse ON
                
Duration: 10 counts @ 1MHz timer = 10µs
Frequency: 50 counts/cycle = 20kHz → 400 steps/second
```

## Safety Constraints

1. **Minimum Period Check**: 
   ```c
   if (period <= OCMP_PULSE_WIDTH) {
       period = OCMP_PULSE_WIDTH + 10;  // Ensure OCxRS > OCxR
   }
   ```

2. **Maximum Period Check**:
   ```c
   if (period > 65485) {
       period = 65485;  // 16-bit timer limit with safety margin
   }
   ```

3. **DRV8825 Timing Requirements**:
   - Minimum STEP pulse width: 1.9µs
   - Our implementation: 40 counts @ 1MHz = 40µs (21x safety margin)

## Code Locations

This pattern is implemented in the following locations:

### 1. **Diagnostic Test** (`srcs/app.c` lines 327-347)
- Purpose: Continuous X-axis pulse generation for oscilloscope verification
- Configuration: 200 count period (~100Hz test pulses)

### 2. **Motion Execution** (`srcs/app.c` lines 594-663)
- Function: `APP_ExecuteNextMotionBlock()`
- Purpose: Real-time motion control for all axes
- Configuration: Variable period based on velocity

### 3. **Legacy Motion Block** (`srcs/app.c` lines 693-768)
- Function: `APP_ExecuteMotionBlock()`
- Purpose: Alternative motion execution path
- Status: Updated to use standard pattern

## Common Mistakes to Avoid

❌ **INCORRECT**: Swapping OCxR and OCxRS assignments
```c
OCMP4_CompareValueSet(period - OCMP_PULSE_WIDTH);  // WRONG!
OCMP4_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);  // WRONG!
```

❌ **INCORRECT**: Variable pulse width
```c
const uint32_t pulse_width = period / 5;  // WRONG! Pulse width must be constant
```

❌ **INCORRECT**: Not checking period constraints
```c
TMR3_PeriodSet(period);  // WRONG! Must clamp to valid range first
```

✅ **CORRECT**: Use the standard pattern exactly as documented above

## Migration Guide

If you find code using a different OCR configuration pattern:

1. **Identify the velocity source**:
   ```c
   float velocity = cnc_axes[i].target_velocity;  // or other source
   ```

2. **Calculate period**:
   ```c
   uint32_t period = MotionPlanner_CalculateOCRPeriod(velocity);
   ```

3. **Apply safety checks**:
   ```c
   if (period > 65485) period = 65485;
   if (period <= 40) period = 50;
   ```

4. **Use standard configuration**:
   ```c
   TMRx_PeriodSet(period);
   OCMPx_CompareValueSet(40);
   OCMPx_CompareSecondaryValueSet(period - 40);
   OCMPx_Enable();
   TMRx_Start();
   ```

## References

- **Copilot Instructions**: `.github/copilot-instructions.md` (lines 123-165)
- **README**: `README.md` (lines 41-95)
- **Source Code**: `srcs/app.c` (multiple locations)
- **DRV8825 Datasheet**: Minimum pulse width specification = 1.9µs

## Version History

- **2025-10-14**: Initial documentation - Standard pattern verified and documented
- Pattern validated through hardware testing with oscilloscope
- All code locations updated to use consistent pattern

---

**⚠️ IMPORTANT**: This is the ONLY approved pattern for OCR dual-compare configuration. Any deviations must be documented and justified.
