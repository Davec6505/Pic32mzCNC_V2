# Timer Prescaler Analysis for PIC32MZ CNC Controller

## Problem Statement
**Current Configuration**:
- Peripheral Clock: 25MHz
- Prescaler: 1:2
- Timer Clock: 12.5MHz
- 16-bit Timer Max: 65,535 counts

**Issue**: Slow step rates overflow the 16-bit timer
- Example: 100 steps/sec requires 125,000 counts (OVERFLOW!)
- Hardware saturates at 65,535, giving: 12.5MHz / 65,535 = 190.7 Hz (too fast!)

## Prescaler Options Analysis

### Option 1: Keep 1:2 Prescaler (Current - BROKEN)
```
Timer Clock: 12.5MHz
Resolution: 80ns per count
Max Period: 65,535 counts = 5.24ms
Min Step Rate: 12.5MHz / 65,535 = 190.7 steps/sec ❌ TOO FAST!
Max Step Rate: 12.5MHz / 50 = 250,000 steps/sec
```
**Verdict**: INSUFFICIENT for slow speeds (< 190 steps/sec)

### Option 2: 1:4 Prescaler (MARGINAL)
```
Timer Clock: 6.25MHz
Resolution: 160ns per count
Max Period: 65,535 counts = 10.49ms
Min Step Rate: 6.25MHz / 65,535 = 95.4 steps/sec ❌ STILL TOO FAST
Max Step Rate: 6.25MHz / 50 = 125,000 steps/sec
```
**Verdict**: Better, but still insufficient for slow Z-axis moves

### Option 3: 1:8 Prescaler (GOOD) ⭐
```
Timer Clock: 3.125MHz
Resolution: 320ns per count
Max Period: 65,535 counts = 20.97ms
Min Step Rate: 3.125MHz / 65,535 = 47.7 steps/sec ✓ ACCEPTABLE
Max Step Rate: 3.125MHz / 50 = 62,500 steps/sec
Pulse Width: 40 counts × 320ns = 12.8µs (DRV8825 requires ≥1.9µs ✓)
```
**Verdict**: ✅ GOOD - Supports 48 to 62,500 steps/sec range

### Option 4: 1:16 Prescaler (BEST FOR RANGE) ⭐⭐
```
Timer Clock: 1.5625MHz
Resolution: 640ns per count
Max Period: 65,535 counts = 41.94ms
Min Step Rate: 1.5625MHz / 65,535 = 23.8 steps/sec ✓ EXCELLENT
Max Step Rate: 1.5625MHz / 50 = 31,250 steps/sec
Pulse Width: 40 counts × 640ns = 25.6µs (DRV8825 requires ≥1.9µs ✓)
```
**Verdict**: ✅ BEST - Supports 24 to 31,250 steps/sec range

### Option 5: 1:32 Prescaler (TOO SLOW)
```
Timer Clock: 781.25kHz
Resolution: 1.28µs per count
Max Period: 65,535 counts = 83.88ms
Min Step Rate: 781.25kHz / 65,535 = 11.9 steps/sec
Max Step Rate: 781.25kHz / 50 = 15,625 steps/sec ❌ TOO SLOW for rapids
Pulse Width: 40 counts × 1.28µs = 51.2µs (very long pulse)
```
**Verdict**: ❌ Too limiting for rapid moves

## Recommended Configuration: 1:16 Prescaler

### Benefits:
1. ✅ **Supports slow Z-axis**: Down to 24 steps/sec (0.019 mm/sec @ 1280 steps/mm)
2. ✅ **Still fast enough for rapids**: Up to 31,250 steps/sec (391 mm/sec @ 80 steps/mm)
3. ✅ **Good resolution**: 640ns per count (adequate for smooth motion)
4. ✅ **Safe pulse width**: 25.6µs >> 1.9µs DRV8825 minimum

### Example Calculations (1:16 Prescaler):
```
Step Rate     Period (counts)    Fits in 16-bit?
----------    ---------------    ---------------
24 steps/sec   65,364            ✓ YES (within limit)
100 steps/sec  15,625            ✓ YES
500 steps/sec  3,125             ✓ YES
1000 steps/sec 1,563             ✓ YES
5000 steps/sec 313               ✓ YES
10,000 steps/sec 156             ✓ YES
31,250 steps/sec 50              ✓ YES (minimum safe period)
```

### Your Current Settings (GRBL):
```c
// From motion_math.c defaults:
$110 = 5000 mm/min (X/Y/A max rate)
$120 = 500 mm/sec² (X/Y/A acceleration)

// At 80 steps/mm (GT2 belt):
Max velocity: 5000 mm/min = 83.3 mm/sec = 6,667 steps/sec ✓ FITS!

// At 1280 steps/mm (Z leadscrew):
$111 = 2000 mm/min (Z max rate)
Max velocity: 2000 mm/min = 33.3 mm/sec = 42,667 steps/sec ❌ TOO FAST!
```

**CRITICAL**: Z-axis max rate needs adjustment for 1:16 prescaler!

## Implementation Changes Required

### 1. MCC Configuration (MPLAB X)
Open MCC and change **all OCR timer prescalers**:
- TMR2 (X-axis): Set prescaler to **1:16**
- TMR3 (Z-axis): Set prescaler to **1:16**
- TMR4 (Y-axis): Set prescaler to **1:16**
- TMR5 (A-axis): Set prescaler to **1:16**

### 2. Code Changes (`incs/motion/motion_types.h`)
```c
// OLD (BROKEN):
#define TMR_CLOCK_HZ 12500000UL // 12.5 MHz (25MHz / 2 prescaler)

// NEW (FIXED):
#define TMR_CLOCK_HZ 1562500UL  // 1.5625 MHz (25MHz / 16 prescaler)
```

### 3. Adjust GRBL Settings (Optional - for safety)
```c
// In motion_math.c - MotionMath_InitializeSettings()
// OLD Z-axis max rate:
motion_settings.max_rate[AXIS_Z] = 2000.0f; // mm/min (42,667 steps/sec @ 1280 steps/mm)

// NEW Z-axis max rate (conservative):
motion_settings.max_rate[AXIS_Z] = 1000.0f; // mm/min (21,333 steps/sec @ 1280 steps/mm) ✓ SAFE
```

## Verification After Changes

### Test Slow Motion (After prescaler change):
```gcode
G90                    ; Absolute mode
G1 Z1 F60              ; Move Z 1mm at 60mm/min (1mm/sec = 1,280 steps/sec)
```
**Expected**: Should move smoothly without timer overflow

### Test Fast Motion:
```gcode
G1 X100 F5000          ; Move X 100mm at 5000mm/min (83mm/sec = 6,667 steps/sec)
```
**Expected**: Should complete in ~1.2 seconds with smooth acceleration

### Oscilloscope Verification:
- At 100 steps/sec: Period should be 15,625 timer counts = 10ms
- Pulse width should be 40 counts × 640ns = 25.6µs
- Verify symmetric S-curve acceleration profiles

## Summary

**ROOT CAUSE**: 1:2 prescaler creates 12.5MHz timer clock, causing 16-bit overflow at slow speeds
**SOLUTION**: Change to **1:16 prescaler** → 1.5625MHz timer clock
**RANGE**: 24 to 31,250 steps/sec (covers all practical CNC speeds)
**ACTION ITEMS**:
1. Change MCC timer prescalers (TMR2/3/4/5) to 1:16
2. Update `TMR_CLOCK_HZ` in motion_types.h to `1562500UL`
3. Regenerate MCC code
4. Rebuild firmware
5. Flash and test slow Z-axis moves

This will fix the "steppers running too fast" issue! 🎯
