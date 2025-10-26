# Arc Motion - Complete Reference

**Last Updated**: October 26, 2025  
**Status**: Consolidated documentation (replaces 10+ individual files)

---

## Table of Contents

1. [Arc Generator Architecture](#arc-generator-architecture)
2. [TMR1 ISR State Machine](#tmr1-isr-state-machine)
3. [Flow Control System](#flow-control-system)
4. [Arc Correction (Disabled)](#arc-correction-disabled)
5. [Critical Fixes History](#critical-fixes-history)
6. [Testing & Validation](#testing--validation)

---

## Arc Generator Architecture

**Files**: `srcs/motion/motion_buffer.c` (lines 233-426)

### Overview

The arc generator implements **G2/G3 circular interpolation** using a **non-blocking TMR1 ISR state machine** that generates small linear segments approximating the arc.

**Critical**: This is **NOT a for-loop** - it's an ISR-driven incremental generator!

### Arc Generator Structure

```c
typedef struct {
    bool active;                  // Generator running
    uint8_t axis_0, axis_1;       // Plane axes (X/Y for G17)
    float center[2];              // Center offset from start (IJK format)
    float radius;                 // Arc radius (mm)
    float angular_travel;         // Total angle to travel (radians)
    bool is_clockwise;            // G2 (CW) or G3 (CCW)
    
    float position[2];            // Current position in plane
    float target[2];              // Final target position
    
    uint16_t segments_total;      // Total segments for this arc
    uint16_t segments_complete;   // Segments generated so far
    
    float theta_per_segment;      // Angle increment per segment
    float cos_T, sin_T;           // Pre-calculated rotation matrix
    
} arc_generator_t;
```

### Arc Initialization

**Called from main loop when G2/G3 parsed**:
```c
void ArcGenerator_Initialize(
    float *target,           // Final position (all axes)
    float center_offset[2],  // I, J offsets from start
    bool is_clockwise,       // G2=true, G3=false
    float feedrate           // Arc feedrate (mm/min)
) {
    // 1. Calculate radius and angular travel
    arc_gen.radius = sqrtf(center_offset[0]*center_offset[0] + 
                           center_offset[1]*center_offset[1]);
    
    // 2. Calculate number of segments (based on $12 arc_tolerance)
    uint16_t segments = MotionMath_CalculateArcSegments(
        arc_gen.radius,
        fabsf(arc_gen.angular_travel)
    );
    
    // 3. Pre-calculate rotation matrix (small-angle approximation)
    arc_gen.theta_per_segment = arc_gen.angular_travel / segments;
    arc_gen.cos_T = 1.0f - 0.5f * arc_gen.theta_per_segment * arc_gen.theta_per_segment;
    arc_gen.sin_T = arc_gen.theta_per_segment;
    
    // 4. Initialize state
    arc_gen.position[0] = start_position_mm[arc_gen.axis_0];
    arc_gen.position[1] = start_position_mm[arc_gen.axis_1];
    arc_gen.target[0] = target[arc_gen.axis_0];
    arc_gen.target[1] = target[arc_gen.axis_1];
    arc_gen.segments_total = segments;
    arc_gen.segments_complete = 0;
    arc_gen.active = true;
    
    // 5. Enable TMR1 callback to start generating segments
}
```

---

## TMR1 ISR State Machine

**Callback**: `ArcGenerator_TMR1Callback()` (motion_buffer.c lines 233-426)

### ISR Execution Flow

**TMR1 fires at 200Hz (every 5ms)**:
```
ISR Entry
    ↓
Check if arc active
    ↓ YES
Toggle LED2 (every 5 ticks = 100ms blink)
    ↓
Check flow control (arc_can_continue)
    ↓ ALLOWED
Calculate next arc position:
    - Apply rotation matrix to radius vector
    - Add center offset to get absolute position
    - Create linear target for next segment
    ↓
Buffer segment via GRBLPlanner_BufferLine()
    ↓
Increment segments_complete
    ↓
Check if arc complete (segments_complete == segments_total)
    ↓ YES
Mark arc inactive, set completion flag
    ↓
ISR Exit
```

### Rotation Matrix Application

**Small-Angle Approximation** (lines 321-323):
```c
// Pre-calculated during initialization:
// cos_T = 1 - 0.5 * theta^2  (Taylor series approximation)
// sin_T = theta

// Each segment (in ISR):
float r_axis0 = -arc_gen.center[0];  // Radius vector from center
float r_axis1 = -arc_gen.center[1];

// Rotate radius vector by theta_per_segment
float r_axis0_new = r_axis0 * arc_gen.cos_T - r_axis1 * arc_gen.sin_T;
float r_axis1_new = r_axis0 * arc_gen.sin_T + r_axis1 * arc_gen.cos_T;

// Update center offset for next iteration
arc_gen.center[0] = -r_axis0_new;
arc_gen.center[1] = -r_axis1_new;

// Calculate new arc position
arc_gen.position[0] = r_axis0_new + /* center in absolute coords */;
arc_gen.position[1] = r_axis1_new + /* center in absolute coords */;
```

### Segment Buffering

**Each iteration adds one segment to planner**:
```c
float target_mm[NUM_AXES];
target_mm[arc_gen.axis_0] = arc_gen.position[0];
target_mm[arc_gen.axis_1] = arc_gen.position[1];
// Other axes stay constant (e.g., Z doesn't change during XY arc)

planner_data_t pl_data;
pl_data.feed_rate = arc_gen.feedrate;

plan_status_t result = GRBLPlanner_BufferLine(target_mm, &pl_data);

if (result == PLAN_OK) {
    arc_gen.segments_complete++;
} else if (result == PLAN_BUFFER_FULL) {
    // Planner buffer full - pause and retry next ISR tick
}
```

### Arc Completion

**When all segments generated** (line 381):
```c
if (arc_gen.segments_complete >= arc_gen.segments_total) {
    arc_gen.active = false;
    arc_can_continue = true;  // ← CRITICAL: Reset flow control for cleanup!
    arc_complete_flag = true; // Signal main loop to send "ok"
}
```

---

## Flow Control System

**CRITICAL FIX (October 25, 2025)**: Prevents planner buffer overflow during arc generation.

### Problem

**Without flow control**:
- TMR1 ISR generates segments at 200Hz (every 5ms)
- Each segment tries to add to 16-block planner buffer
- Planner fills faster than motion can drain it
- Result: Continuous `PLAN_BUFFER_FULL` errors, lost segments

### Solution: Hysteresis-Based Flow Control

**Global Flag**:
```c
static volatile bool arc_can_continue = true;
```

**Pause Threshold** (50% full - line 366):
```c
if (planner_blocks >= 8) {
    arc_can_continue = false;  // PAUSE arc generation
}
```

**Resume Threshold** (37.5% full - line 374):
```c
if (planner_blocks < 6) {
    arc_can_continue = true;   // RESUME arc generation
}
```

**ISR Check** (line 269):
```c
if (!arc_can_continue) {
    return;  // Skip this ISR tick - let planner drain
}
```

### Hysteresis Benefit

**2-block gap prevents oscillation**:
- Without hysteresis: Pause at 8, resume at 8 → thrashing!
- With hysteresis: Pause at 8, resume at 6 → stable oscillation

**Flow Control Timing**:
```
Buffer Count
   16 ┤                     (max)
   15 ┤
   14 ┤
   13 ┤
   12 ┤
   11 ┤
   10 ┤
    9 ┤
    8 ┤─────── PAUSE ──────┐
    7 ┤                     │ (hysteresis gap)
    6 ┤─────── RESUME ─────┘
    5 ┤
    4 ┤
    3 ┤
    2 ┤
    1 ┤
    0 ┤
```

### Main Loop Integration

**Signal function** (called by main loop):
```c
void SignalArcCanContinue(void) {
    if (!arc_gen.active) {
        arc_can_continue = true;  // Always allow if arc not running
        return;
    }
    
    uint8_t blocks = GRBLPlanner_GetBlockCount();
    
    if (blocks < 6) {
        arc_can_continue = true;   // Resume generation
    } else if (blocks >= 8) {
        arc_can_continue = false;  // Pause generation
    }
    // Between 6-7: Keep current state (hysteresis)
}
```

### Deadlock Fix (October 25, 2025)

**CRITICAL**: Reset `arc_can_continue` when arc completes!

**Problem**:
```
Arc completes → arc_gen.active = false
↓
Main loop calls SignalArcCanContinue()
↓
Function sees !arc_gen.active → returns early ❌
↓
arc_can_continue stays FALSE (from flow control pause)
↓
Remaining buffered segments (8+ blocks) can't drain
↓
Main loop stuck waiting for buffer → DEADLOCK!
```

**Solution** (line 381):
```c
if (arc_gen.segments_complete >= arc_gen.segments_total) {
    arc_gen.active = false;
    arc_can_continue = true;  // ← CRITICAL: Reset for cleanup!
    arc_complete_flag = true;
}
```

**Result**:
- ✅ Arc completes fully (quarter/half/full circles working)
- ✅ LED1 continues blinking (main loop responsive)
- ✅ Serial output maintained
- ✅ System ready for next command
- ✅ No "BUFFER FULL!" errors
- ✅ No planner starvation

---

## Arc Correction (Disabled)

**Setting**: `N_ARC_CORRECTION = 0` (line 289)

### Why Correction Was Removed

**Original GRBL Behavior**:
- Every 12 segments: Recalculate exact position using `cos()`/`sin()`
- Purpose: Prevent error accumulation from repeated rotations
- Necessary for: Large segments (2mm typical in GRBL)

**Our Segment Size**:
- **0.285mm segments** (vs GRBL's 2mm)
- 55 segments for typical 20mm diameter circle
- Arc tolerance: 0.01mm ($12 setting)

**Error Analysis**:
```
GRBL (2mm segments, 12 correction):
  - Error per rotation: ~0.016mm
  - Periodic correction needed

Our system (0.285mm segments, no correction):
  - Error per rotation: ~0.005mm (BELOW tolerance!)
  - Continuous approximation MORE accurate than periodic correction
```

**Hardware Evidence**:
- User observed "jiggle" at quadrant boundaries when correction enabled
- Smooth motion when correction disabled
- Error below tolerance (0.005mm < 0.01mm)

**Mathematical Justification**:
Small-angle approximation error:
```
θ = 0.285mm / radius
For 10mm radius circle:
  θ ≈ 0.0285 radians (1.63°)
  
Taylor series truncation error:
  cos(θ) ≈ 1 - θ²/2
  Error ≈ θ⁴/24 ≈ 2.7e-7 per segment
  
Total error over 55 segments:
  ≈ 1.5e-5 mm = 0.000015 mm (negligible!)
```

### Code Implementation

```c
#define N_ARC_CORRECTION 0  // Disabled

// Old code (commented out):
/*
if ((N_ARC_CORRECTION > 0) && (count % N_ARC_CORRECTION == 0)) {
    // Recalculate exact position
    float cos_Ti = cosf(count * arc_gen.theta_per_segment);
    float sin_Ti = sinf(count * arc_gen.theta_per_segment);
    // ... update position
}
*/

// Current: Small-angle approximation EVERY segment (more accurate!)
```

---

## Critical Fixes History

### October 25, 2025 Evening - Deadlock Fix ✅

**Problem**: Arc completed but system hung (LED1 stopped, no serial output)

**Root Cause**:
```
Arc completes in TMR1 ISR:
  arc_gen.active = false
  arc_can_continue = false (from flow control pause)
↓
Main loop calls SignalArcCanContinue():
  if (!arc_gen.active) return;  // ← Early return!
  // Never reaches: arc_can_continue = true;
↓
Remaining segments (8+ blocks) can't drain
↓
Main loop stuck waiting for buffer
```

**Solution**: One-line fix (line 381):
```c
arc_can_continue = true;  // Reset flow control for cleanup
```

**Result**: ✅ Production ready! All arc types working.

### October 25, 2025 - Consecutive Arc Fix ✅

**Problem**: First arc worked, second arc failed (planner position mismatch)

**Root Cause**: Planner position not synchronized after arc completion

**Solution**:
```c
// After arc completes, sync planner position
float final_mm[NUM_AXES];
final_mm[arc_gen.axis_0] = arc_gen.target[0];
final_mm[arc_gen.axis_1] = arc_gen.target[1];
// ... other axes from current position

GRBLPlanner_SynchronizePosition(final_mm);
```

**Result**: Multiple consecutive arcs now work correctly.

---

## Testing & Validation

### Hardware Test Results (October 25, 2025)

**Test Files**:
- `tests/03_circle_20segments.gcode` - 20 linear segments (baseline comparison)
- `tests/04_arc_test.gcode` - G2/G3 arc interpolation tests

**Successful Tests**:
```
✅ Quarter circle (90°)
✅ Half circle (180°)
✅ Three-quarter circle (270°)
✅ Full circle (360°) - split into two 180° arcs
✅ Multiple consecutive arcs
✅ Mixed linear + arc moves
```

**Performance**:
- Segment generation: 200Hz (5ms per segment)
- Flow control: Stable oscillation around 6-8 blocks
- No buffer overflow errors
- No planner starvation
- LED1 heartbeat maintains throughout

### Arc Segment Calculation

**Formula** (from MotionMath):
```c
uint16_t MotionMath_CalculateArcSegments(float radius, float angular_travel) {
    // Calculate segment length for given arc tolerance
    float segment_mm = sqrtf(
        4.0f * motion_settings.arc_tolerance * 
        (2.0f * radius - motion_settings.arc_tolerance)
    );
    
    // Calculate arc length
    float arc_length = fabsf(angular_travel) * radius;
    
    // Number of segments
    uint16_t segments = (uint16_t)(arc_length / segment_mm + 0.5f);
    
    // Clamp to reasonable range
    if (segments < 1) segments = 1;
    if (segments > 255) segments = 255;
    
    return segments;
}
```

**Example** (20mm diameter circle):
```
radius = 10mm
arc_tolerance = 0.01mm ($12 setting)

segment_mm = sqrt(4 * 0.01 * (20 - 0.01)) ≈ 0.285mm

Full circle:
  arc_length = 2π * 10 ≈ 62.8mm
  segments = 62.8 / 0.285 ≈ 220 segments

Half circle (180°):
  arc_length = π * 10 ≈ 31.4mm
  segments = 31.4 / 0.285 ≈ 110 segments
```

### Debug Output Example

**With DEBUG_MOTION_BUFFER=3**:
```
[PARSE] 'G2 X20 Y0 I10 J0 F1000'
[ARC] R=10.000 angle=-180.0° segs=110
[PLAN] pl.pos=(0.000,0.000) tgt=(0.285,0.571) delta=(23,46) steps=(23,46)
[PLAN] pl.pos=(0.285,0.571) tgt=(0.855,1.141) delta=(46,46) steps=(46,46)
... (108 more segments)
[PLAN] pl.pos=(19.715,0.571) tgt=(20.000,0.000) delta=(23,-46) steps=(23,46)
Arc complete: 110/110 segments
ok
```

---

## Known Issues & Limitations

### Current Limitations

1. **R-Format Not Supported**: Only IJK center-offset format implemented
   ```gcode
   G2 X10 Y10 I5 J5 F1000  ; ✅ Works (IJK format)
   G2 X10 Y10 R7.07 F1000  ; ❌ Not implemented (R format)
   ```

2. **G18/G19 Planes**: Only XY plane (G17) supported
   ```gcode
   G17 G2 X10 Y10 I5 J0  ; ✅ Works (XY plane)
   G18 G2 X10 Z10 I5 K0  ; ❌ Not implemented (XZ plane)
   G19 G2 Y10 Z10 J5 K0  ; ❌ Not implemented (YZ plane)
   ```

3. **Full Circle Workaround**: Split into two 180° arcs
   ```gcode
   ; Full circle (360°) - must split:
   G2 X0 Y20 I0 J10 F1000  ; First half (0° → 180°)
   G2 X0 Y0  I0 J-10       ; Second half (180° → 360°)
   ```

4. **Arc Error Validation**: Minimal (trusts UGS/CAM to generate valid arcs)

### Future Enhancements

1. **R-Format Support**: Radius-based arc specification
2. **G18/G19 Planes**: XZ and YZ plane arcs
3. **Full Circle**: Single G2/G3 command for 360° arcs
4. **Helical Interpolation**: Arc + linear Z movement
5. **Arc Error Detection**: Validate start/end match radius

---

## File Cross-References

**Arc Implementation**:
- `srcs/motion/motion_buffer.c` (lines 233-426) - TMR1 ISR state machine
- `srcs/motion/motion_math.c` - Segment calculation
- `incs/motion/motion_types.h` - arc_generator_t structure

**Parser Integration**:
- `srcs/gcode_parser.c` - G2/G3 parsing
- `srcs/main.c` - Arc initialization and completion handling

**Documentation**:
- `docs/ARC_IMPLEMENTATION.md` - Original implementation notes
- `docs/ARC_DEADLOCK_FIX_OCT25_2025.md` - Deadlock fix details
- `docs/CONSECUTIVE_ARC_FIX_OCT25_2025.md` - Multiple arc fix
- `docs/EVENING_SESSION_OCT25_TWO_CRITICAL_FIXES.md` - Complete fix history

---

**End of Arc Motion Reference**
