# Linear Motion - Complete Reference

**Last Updated**: October 26, 2025  
**Status**: Consolidated documentation (replaces 20+ individual files)

---

## Table of Contents

1. [GRBL Planner Architecture](#grbl-planner-architecture)
2. [Segment Execution System](#segment-execution-system)
3. [Multi-Axis Control & S-Curves](#multi-axis-control--s-curves)
4. [OCR Hardware Integration](#ocr-hardware-integration)
5. [Dominant Axis Handoff](#dominant-axis-handoff)
6. [Position Tracking & Accuracy](#position-tracking--accuracy)
7. [Motion Math & Kinematics](#motion-math--kinematics)

---

## GRBL Planner Architecture

**Files**: `srcs/motion/grbl_planner.c`, `incs/motion/grbl_planner.h`

### Overview

The GRBL planner implements **look-ahead trajectory planning** to optimize motion velocity through corners while respecting acceleration limits.

### Block Buffer Structure

```c
#define BLOCK_BUFFER_SIZE 16  // Must be power of 2

typedef struct {
    // Step counts (absolute target position)
    int32_t steps[NUM_AXES];
    uint32_t step_event_count;  // Max steps of all axes
    
    // Velocity profile
    float entry_speed_sqr;      // Entry velocity squared (mm/min)²
    float max_entry_speed_sqr;  // Maximum allowable entry
    float millimeters;          // Block length (mm)
    float acceleration;         // Acceleration limit (mm/sec²)
    
    // Planning flags
    uint8_t recalculate_flag : 1;
    uint8_t nominal_length_flag : 1;
    
    // Direction bits
    uint8_t direction_bits;
    
} planner_block_t;
```

### Planner Position Tracking (Dual System)

**CRITICAL FIX (October 19, 2025)**: Dual exact mm tracking prevents coordinate drift.

**Problem**: Original GRBL used integer position, caused rounding errors:
```c
// WRONG - accumulates error
pl.position[AXIS_X] += target_steps[AXIS_X];  // Integer only!
```

**Solution**: Track BOTH steps (hardware) AND exact mm (floating-point):
```c
// Dual position tracking
typedef struct {
    int32_t position[NUM_AXES];      // Hardware position (steps)
    float position_exact_mm[NUM_AXES];  // Exact position (mm) - NEW!
} planner_t;

// Update both on each move
pl.position[axis] = target_steps[axis];
pl.position_exact_mm[axis] = target_mm[axis];  // Exact value retained!
```

**Benefits**:
- ✅ No coordinate drift over long paths
- ✅ Closed shapes return to exact start (0.000, 0.000, 0.000)
- ✅ Hardware uses integer steps (precise)
- ✅ Planning uses exact mm (accurate)

### Junction Velocity Calculation

**Cosine Angle Method**:
```c
float calculate_junction_velocity(
    const planner_block_t *prev_block,
    const planner_block_t *current_block
) {
    // Calculate unit vectors
    float unit_vec_prev[NUM_AXES];
    float unit_vec_current[NUM_AXES];
    
    // Dot product = cos(angle)
    float cos_theta = dot_product(unit_vec_prev, unit_vec_current);
    
    // Junction deviation formula (from GRBL)
    float sin_theta_d2 = sqrtf(0.5f * (1.0f - cos_theta));
    float v_junction = sqrtf(
        acceleration * junction_deviation * sin_theta_d2 / (1.0f - sin_theta_d2)
    );
    
    return v_junction;
}
```

**Junction Deviation** ($11 setting):
- Small value (0.01mm): Tight corners, slower motion
- Large value (0.1mm): Smooth corners, faster motion
- Default: 0.01mm (accuracy priority)

### Look-Ahead Planning Algorithm

**Three-Pass System**:

**Pass 1: Forward Pass (Block Addition)**
```c
bool GRBLPlanner_BufferLine(float *target_mm, planner_data_t *pl_data) {
    // 1. Calculate block length and unit vectors
    // 2. Compute maximum entry velocity (junction limit)
    // 3. Set initial entry speed (conservative)
    // 4. Add block to ring buffer
    // 5. Trigger replanning if threshold reached
}
```

**Pass 2: Recalculate Forward**
```c
void planner_recalculate() {
    // Iterate tail → head
    // For each block:
    //   - Calculate maximum safe exit velocity
    //   - Limited by: next junction, acceleration, feedrate
    //   - Update entry_speed_sqr for next block
}
```

**Pass 3: Recalculate Reverse**
```c
void planner_recalculate() {
    // Iterate head → tail
    // For each block:
    //   - Ensure entry velocity respects acceleration limits
    //   - Adjust exit velocities if entry reduced
    //   - Mark for recalculation if changed
}
```

### Tri-State Return System (October 25, 2025)

**CRITICAL FIX**: Prevents infinite retry loop on zero-length moves.

```c
typedef enum {
    PLAN_OK = 1,          // Block successfully added to buffer
    PLAN_BUFFER_FULL = 0, // Buffer full - temporary, RETRY after waiting
    PLAN_EMPTY_BLOCK = -1 // Zero-length block - permanent, DO NOT RETRY
} plan_status_t;
```

**Problem Solved**:
- Command `G0 Z5` when already at Z=5 → zero-length move
- Old system: Treated as buffer full → retry forever!
- New system: Returns `PLAN_EMPTY_BLOCK` → send "ok" and continue

**Main Loop Integration**:
```c
plan_status_t plan_result = GRBLPlanner_BufferLine(target_mm, &pl_data);

if (plan_result == PLAN_OK) {
    UGS_SendOK();          // ✅ SUCCESS: Send "ok" and continue
    pending_retry = false;
}
else if (plan_result == PLAN_BUFFER_FULL) {
    pending_retry = true;  // ⏳ TEMPORARY: Retry after waiting
}
else /* PLAN_EMPTY_BLOCK */ {
    UGS_SendOK();          // ❌ PERMANENT: Silently ignore, send "ok"
    pending_retry = false; // Don't retry zero-length moves!
}
```

### Planner API

```c
void GRBLPlanner_Init(void);
plan_status_t GRBLPlanner_BufferLine(float *target_mm, planner_data_t *pl_data);
void GRBLPlanner_RecalculateAll(void);
bool GRBLPlanner_GetNextBlock(motion_block_t *block);
uint8_t GRBLPlanner_GetBlockCount(void);
void GRBLPlanner_SynchronizePosition(float *position_mm);
void GRBLPlanner_SetPosition(float *position_mm);
```

---

## Segment Execution System

**Files**: `srcs/motion/grbl_stepper.c`, `incs/motion/grbl_stepper.h`

### Segment Buffer Architecture

**Purpose**: Break 2mm planner blocks into smaller segments for smooth motion control.

```c
#define SEGMENT_BUFFER_SIZE 16  // Circular buffer

typedef struct {
    uint32_t n_step;              // Number of steps in this segment
    uint32_t steps[NUM_AXES];     // Per-axis step counts
    int32_t bresenham_counter[NUM_AXES];  // Bresenham accumulators
    uint8_t direction_bits;       // Direction for all axes
    
    uint16_t isr_period;          // OCR period for this segment
    uint16_t isrPeriod_scale;     // Velocity scaling factor
} segment_t;
```

### Segment Preparation (TMR9 ISR @ 200Hz)

**Flow**:
```
TMR9 ISR fires every 5ms
    ↓
Check if planner has blocks
    ↓
Get next block from planner
    ↓
Calculate segment parameters:
    - Steps per segment (~2mm target)
    - Bresenham counters
    - Direction bits
    - OCR period (velocity)
    ↓
Add segment to buffer
    ↓
Update planner block tracking
```

**Critical Pattern** (motion_manager.c):
```c
void MotionManager_TMR9Callback(uint32_t status, uintptr_t context) {
    // Only prepare segments if:
    // 1. Segment buffer not full (< 16 segments)
    // 2. Planner has blocks available
    // 3. Current block not exhausted
    
    if (segment_buffer_full()) return;
    if (!planner_has_blocks()) return;
    
    // Prepare next segment
    segment_t *seg = get_next_segment_slot();
    fill_segment_from_current_block(seg);
    
    // Advance segment buffer
    segment_buffer_head++;
}
```

### Segment Loading (Main Loop)

**Execution Threshold**: Wait for 4 segments before starting motion (prevents starvation).

```c
// In main.c:
uint8_t planner_count = GRBLPlanner_GetBlockCount();
uint8_t segment_count = GRBLStepper_GetSegmentCount();
bool should_start_new = (planner_count >= 4);

if (should_start_new && !MultiAxis_IsBusy()) {
    segment_t seg;
    if (GRBLStepper_GetNextSegment(&seg)) {
        MultiAxis_StartSegmentExecution(&seg);
    }
}
```

### Bresenham Algorithm (Subordinate Axes)

**Purpose**: Bit-bang subordinate axis steps during dominant axis pulses.

```c
// In ProcessSegmentStep() (dominant axis ISR):
for (axis_id_t sub_axis = AXIS_X; sub_axis < NUM_AXES; sub_axis++) {
    if (sub_axis == dominant_axis) continue;
    
    uint32_t steps_sub = segment->steps[sub_axis];
    if (steps_sub == 0) continue;
    
    // Bresenham algorithm
    sub_state->bresenham_counter += steps_sub;
    
    if (sub_state->bresenham_counter >= n_step) {
        sub_state->bresenham_counter -= n_step;
        
        // Trigger subordinate pulse (set OCR to 0xFFFF for immediate fire)
        axis_hw[sub_axis].OCMP_CompareValueSet(5);
        axis_hw[sub_axis].OCMP_CompareSecondaryValueSet(36);
        *axis_hw[sub_axis].TMRx_reg = 0xFFFF;  // Force immediate rollover
        axis_hw[sub_axis].OCMP_Enable();
        
        sub_state->step_count++;
    }
}
```

---

## Multi-Axis Control & S-Curves

**Files**: `srcs/motion/multiaxis_control.c`, `incs/motion/multiaxis_control.h`

### Architecture Overview

**Time-Based Interpolation** (NOT Bresenham step counting):
- TMR1 @ 1kHz updates S-curve velocities for ALL active axes
- Independent OCR modules (OCMP1/3/4/5) generate hardware step pulses
- Coordinated moves synchronized to dominant (longest) axis time

### S-Curve Motion Profile (7 Segments)

```
Velocity
   ^
   |     ___________
   |    /           \
   |   /             \
   |  /               \
   | /                 \
   |/                   \
   +----------------------> Time
     1 2   3   4   5  6 7

Segments:
1. Jerk-limited acceleration start
2. Constant acceleration
3. Jerk-limited acceleration end
4. Constant velocity (cruise)
5. Jerk-limited deceleration start
6. Constant deceleration
7. Jerk-limited deceleration end
```

**Profile Structure**:
```c
typedef struct {
    float total_time, accel_time, const_time, decel_time;
    float peak_velocity, acceleration, distance;
    bool use_scurve;
    position_t start_pos, end_pos;
} scurve_motion_profile_t;
```

### Coordinated Move Execution

**Time Synchronization**:
```c
void MultiAxis_ExecuteCoordinatedMove(int32_t steps[NUM_AXES]) {
    // 1. Find dominant axis (max steps)
    axis_id_t dominant = find_dominant_axis(steps);
    
    // 2. Calculate dominant axis time (based on its limits)
    float dominant_time = calculate_motion_time(
        steps[dominant],
        MotionMath_GetMaxVelocityStepsPerSec(dominant),
        MotionMath_GetAccelStepsPerSec2(dominant),
        MotionMath_GetJerkStepsPerSec3(dominant)
    );
    
    // 3. Scale subordinate velocities to match dominant time
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++) {
        if (axis == dominant) {
            coord_move.axis_velocity_scale[axis] = 1.0f;
        } else {
            coord_move.axis_velocity_scale[axis] = 
                (float)steps[axis] / (float)steps[dominant];
        }
    }
    
    // 4. Start coordinated motion (all axes finish at SAME TIME)
    coord_move.dominant_axis = dominant;
    coord_move.active = true;
}
```

### TMR1 Motion Control ISR (1kHz)

**Callback Pattern**:
```c
void TMR1_MultiAxisControl(uint32_t status, uintptr_t context) {
    if (!coord_move.active) return;
    
    axis_id_t dominant = coord_move.dominant_axis;
    scurve_state_t *dominant_state = &axis_state[dominant];
    
    // Update dominant axis S-curve velocity
    update_scurve_velocity(dominant_state);
    
    // Update OCR period for dominant axis
    uint32_t period = calculate_ocr_period(dominant_state->current_velocity);
    axis_hw[dominant].TMR_PeriodSet(period);
    
    // Check if motion complete
    if (dominant_state->current_segment == SEGMENT_IDLE) {
        coord_move.active = false;
        // Stop all axes
    }
}
```

---

## OCR Hardware Integration

**PIC32MZ Output Compare Modules**: Hardware-accelerated step pulse generation.

### OCR Assignments

```
OCR Module | Axis | Timer Source | Step Pin
--------------------------------------------------
OCMP1      | Y    | TMR4         | RD0 (OC1)
OCMP3      | A    | TMR5         | RD2 (OC3)
OCMP4      | X    | TMR2         | RD3 (OC4)
OCMP5      | Z    | TMR3         | RD4 (OC5)
```

### Timer Configuration (VERIFIED - October 18, 2025)

**All motion timers use 1:32 prescaler**:
```
Peripheral Clock (PBCLK3): 50 MHz
Timer Prescaler: 1:32
Timer Clock: 50 MHz ÷ 32 = 1.5625 MHz
Resolution: 640ns per count
```

**Code Configuration**:
```c
#define TMR_CLOCK_HZ 1562500UL  // 1.5625 MHz
```

### OCR Dual-Compare Mode (OCM=0b101)

**Configuration** (CRITICAL - Exact Register Sequence):
```c
/* DRV8825 requires minimum 1.9µs pulse width - use 40 timer counts */
const uint32_t OCMP_PULSE_WIDTH = 40;  // 40 × 640ns = 25.6µs

/* Calculate OCR period from velocity */
uint32_t period = TMR_CLOCK_HZ / velocity_steps_sec;

/* Clamp to 16-bit timer maximum */
if (period > 65485) period = 65485;
if (period <= OCMP_PULSE_WIDTH) period = OCMP_PULSE_WIDTH + 10;

/* Configure OCR dual-compare mode:
 * TMRxPR = period (timer rollover)
 * OCxR = period - 40 (rising edge - variable based on velocity)
 * OCxRS = 40 (falling edge - fixed pulse width)
 */
TMRx_PeriodSet(period);
OCMPx_CompareValueSet(period - OCMP_PULSE_WIDTH);  // Rising edge
OCMPx_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);  // Falling edge
OCMPx_Enable();
TMRx_Start();  // CRITICAL: Must restart for each move!
```

**Timing Example** (period=300):
```
Count: 0....40.......260.......300 (rollover)
Pin:   LOW  HIGH     LOW       LOW
       └────────────┘
       40 counts ON (25.6µs)
```

### Step Rate Range

**With 1:32 Prescaler**:
```
Min: 23.8 steps/sec (period = 65,535 counts = 41.94ms)
Max: 31,250 steps/sec (period = 50 counts = 32µs)

Examples:
100 steps/sec:   period = 15,625 counts (10ms) ✓
1,000 steps/sec: period = 1,563 counts (1ms) ✓
5,000 steps/sec: period = 313 counts (200µs) ✓
```

### Critical Execution Pattern

**MANDATORY Sequence** (multiaxis_control.c):
```c
// 1. Set direction GPIO BEFORE enabling step pulses
if (direction_forward) {
    DirX_Set();    // GPIO high for forward
} else {
    DirX_Clear();  // GPIO low for reverse
}

// 2. Enable motor driver (active-low ENABLE pin)
MultiAxis_EnableDriver(axis);  // Sets ENABLE pin LOW

// 3. Configure OCR registers
TMR2_PeriodSet(period);
OCMP4_CompareValueSet(period - 40);
OCMP4_CompareSecondaryValueSet(40);

// 4. Enable OCR and start timer
OCMP4_Enable();
TMR2_Start();  // CRITICAL: Timers stop when motion completes!
```

**Common Mistakes to Avoid**:
- ❌ Don't forget `TMRx_Start()` - timer stops at motion completion
- ❌ Don't swap OCxR/OCxRS values
- ❌ Don't set direction after step pulses start
- ❌ Don't forget to enable driver (motor won't move!)

---

## Dominant Axis Handoff

**CRITICAL ARCHITECTURE (October 24, 2025)**: Transition-based role detection for clean handoffs.

### Transition State Tracking

**Per-Axis State Variables**:
```c
// Track previous ISR role
static volatile bool axis_was_dominant_last_isr[NUM_AXES] = {false, false, false, false};

// Zero-overhead inline helper (compiles to single AND instruction)
static inline bool __attribute__((always_inline)) IsDominantAxis(axis_id_t axis)
{
    return (segment_completed_by_axis & (1U << axis)) != 0U;
}
```

### OCR ISR Four-State Machine

```c
void OCMPx_Callback(uintptr_t context)
{
    axis_id_t axis = AXIS_<X/Y/Z/A>;
    
    // STATE 1: Subordinate → Dominant (ONE-TIME SETUP)
    if (IsDominantAxis(axis) && !axis_was_dominant_last_isr[axis])
    {
        MultiAxis_EnableDriver(axis);        // Enable motor driver (ONCE!)
        /* Set direction GPIO */
        /* Configure OCR for continuous operation */
        /* Enable OCR and start timer */
        axis_was_dominant_last_isr[axis] = true;
    }
    // STATE 2: Still Dominant (EVERY ISR)
    else if (IsDominantAxis(axis))
    {
        ProcessSegmentStep(axis);            // Run Bresenham for subordinates
        /* Update OCR period (velocity may change) */
    }
    // STATE 3: Dominant → Subordinate (ONE-TIME TEARDOWN)
    else if (axis_was_dominant_last_isr[axis])
    {
        /* Disable OCR, stop timer */
        axis_was_dominant_last_isr[axis] = false;
    }
    // STATE 4: Subordinate Pulse Completed
    else
    {
        /* Auto-disable OCR after pulse */
        axis_hw[axis].OCMP_Disable();
        /* Stop timer - wait for next Bresenham trigger */
    }
}
```

### Performance Improvements

**October 24, 2025 Optimization**:
- ✅ Stack usage: 16 bytes → 4 bytes (75% reduction via direct struct access)
- ✅ Instruction count: ~25 → ~12 cycles (52% reduction via inline helper)
- ✅ Driver enable calls: Every ISR → Only on transitions (99.9% reduction!)
- ✅ Edge-triggered logic: Check `previous != current` instead of polling state
- ✅ Zero overhead: `__attribute__((always_inline))` eliminates function call

### DRV8825 Driver Enable Integration

**CRITICAL (October 21-24, 2025)**:
- ENABLE pin is **active-low** (LOW = motor energized, HIGH = disabled)
- `MultiAxis_EnableDriver(axis)` calls `en_clear_funcs[axis]()` → sets pin LOW
- **ONLY called on subordinate → dominant transition** (not every ISR!)
- Without this: Step pulses present but motor doesn't move (driver de-energized)

---

## Position Tracking & Accuracy

### Real-Time Position Feedback

**Hardware Step Counters**:
```c
// Per-axis volatile step counters (incremented by OCR ISRs)
static volatile uint32_t step_count[NUM_AXES] = {0};

// ISR callback pattern
void OCMP4_StepCounter_X(uintptr_t context) {
    step_count[AXIS_X]++;
}
```

**Position Query**:
```c
int32_t MultiAxis_GetStepCount(axis_id_t axis) {
    return (int32_t)step_count[axis];
}
```

**Conversion to mm** (for status reports):
```c
float current_mm[NUM_AXES];
for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++) {
    int32_t steps = MultiAxis_GetStepCount(axis);
    current_mm[axis] = MotionMath_StepsToMM(steps, axis);
}

UGS_SendStatusReport(current_mm, ...);  // <Idle|MPos:10.000,20.000,5.003|...>
```

### Accuracy Verification

**Hardware Testing Results** (October 18, 2025):
- ✅ Test 1: Y-axis 800 steps = 10.000mm (exact!)
- ✅ Test 2: Coordinated X/Y diagonal completed
- ✅ Test 3: Negative moves completed successfully
- ✅ Step accuracy: ±2 steps (±0.025mm) - within tolerance
- ✅ Closed shapes return to (0.000, 0.000, 0.000)

**Rectangle Path Test**:
```gcode
G90              ; Absolute mode
G92 X0 Y0        ; Zero position
G1 X100 F1000    ; Move +100mm
G1 Y100          ; Move +100mm
G1 X0            ; Return X
G1 Y0            ; Return Y
; Final position: (0.000, 0.000) ✅
```

---

## Motion Math & Kinematics

**Files**: `srcs/motion/motion_math.c`, `incs/motion/motion_math.h`

### GRBL Settings Storage

```c
typedef struct {
    float steps_per_mm[NUM_AXES];        // $100-$103
    float max_rate[NUM_AXES];            // $110-$113 (mm/min)
    float acceleration[NUM_AXES];        // $120-$123 (mm/sec²)
    float max_travel[NUM_AXES];          // $130-$133 (mm or degrees)
    float junction_deviation;            // $11
    float arc_tolerance;                 // $12
    float jerk_limit;                    // Jerk (mm/sec³)
} motion_settings_t;
```

### Default Settings (Conservative for Testing)

```c
void MotionMath_InitializeSettings(void) {
    // X/Y/A: GT2 belt, 20-tooth pulley, 1/16 microstepping
    motion_settings.steps_per_mm[AXIS_X] = 80.0f;   // $100
    motion_settings.steps_per_mm[AXIS_Y] = 80.0f;   // $101
    motion_settings.steps_per_mm[AXIS_A] = 80.0f;   // $103
    
    // Z: 2.5mm leadscrew, 200 steps/rev, 1/16 microstepping
    motion_settings.steps_per_mm[AXIS_Z] = 1280.0f; // $102
    
    // Max rates (mm/min)
    motion_settings.max_rate[AXIS_X] = 5000.0f;     // $110
    motion_settings.max_rate[AXIS_Y] = 5000.0f;     // $111
    motion_settings.max_rate[AXIS_Z] = 2000.0f;     // $112 (slower Z)
    motion_settings.max_rate[AXIS_A] = 5000.0f;     // $113
    
    // Acceleration (mm/sec²)
    motion_settings.acceleration[AXIS_X] = 500.0f;  // $120
    motion_settings.acceleration[AXIS_Y] = 500.0f;  // $121
    motion_settings.acceleration[AXIS_Z] = 200.0f;  // $122 (slower Z)
    motion_settings.acceleration[AXIS_A] = 500.0f;  // $123
    
    // Max travel
    motion_settings.max_travel[AXIS_X] = 300.0f;    // $130
    motion_settings.max_travel[AXIS_Y] = 300.0f;    // $131
    motion_settings.max_travel[AXIS_Z] = 100.0f;    // $132
    motion_settings.max_travel[AXIS_A] = 360.0f;    // $133 (degrees)
    
    // Planning parameters
    motion_settings.junction_deviation = 0.01f;     // $11
    motion_settings.arc_tolerance = 0.01f;          // $12
    motion_settings.jerk_limit = 5000.0f;           // Jerk limit
}
```

### Unit Conversion Functions

```c
int32_t MotionMath_MMToSteps(float mm, axis_id_t axis) {
    return (int32_t)(mm * motion_settings.steps_per_mm[axis] + 0.5f);
}

float MotionMath_StepsToMM(int32_t steps, axis_id_t axis) {
    return (float)steps / motion_settings.steps_per_mm[axis];
}

float MotionMath_GetMaxVelocityStepsPerSec(axis_id_t axis) {
    // Convert mm/min → steps/sec
    return (motion_settings.max_rate[axis] / 60.0f) * 
           motion_settings.steps_per_mm[axis];
}

float MotionMath_GetAccelStepsPerSec2(axis_id_t axis) {
    // Convert mm/sec² → steps/sec²
    return motion_settings.acceleration[axis] * 
           motion_settings.steps_per_mm[axis];
}
```

### Look-Ahead Helper Functions

```c
float MotionMath_CalculateJunctionVelocity(
    const float *unit_vec_prev,
    const float *unit_vec_current,
    float acceleration
) {
    // Calculate angle between vectors
    float cos_theta = dot_product(unit_vec_prev, unit_vec_current);
    
    // Junction deviation formula
    float sin_theta_d2 = sqrtf(0.5f * (1.0f - cos_theta));
    float v_junction = sqrtf(
        acceleration * motion_settings.junction_deviation * 
        sin_theta_d2 / (1.0f - sin_theta_d2)
    );
    
    return v_junction;
}
```

---

## Debug System

### Tiered Debug Levels (8-Level Hierarchy)

**Added**: October 25, 2025

```c
#define DEBUG_LEVEL_NONE     0  // Production (no debug output)
#define DEBUG_LEVEL_CRITICAL 1  // <1 msg/sec  (buffer overflow, fatal errors)
#define DEBUG_LEVEL_PARSE    2  // ~10 msg/sec (command parsing)
#define DEBUG_LEVEL_PLANNER  3  // ~10 msg/sec (motion planning) ⭐ RECOMMENDED
#define DEBUG_LEVEL_STEPPER  4  // ~10 msg/sec (state machine transitions)
#define DEBUG_LEVEL_SEGMENT  5  // ~50 msg/sec (segment execution)
#define DEBUG_LEVEL_VERBOSE  6  // ~100 msg/sec (high-frequency events)
#define DEBUG_LEVEL_ALL      7  // >1000 msg/sec (CAUTION: floods serial!)
```

**Build Commands**:
```bash
make all BUILD_CONFIG=Debug DEBUG_MOTION_BUFFER=3  # Level 3 (RECOMMENDED)
make all BUILD_CONFIG=Debug DEBUG_MOTION_BUFFER=4  # Add stepper debug
make all BUILD_CONFIG=Debug DEBUG_MOTION_BUFFER=0  # Production (no debug)
```

**Current Testing Configuration**:
- **Active Level**: 3 (PLANNER)
- **Output**: `[PARSE]`, `[PLAN]`, `[JUNC]`, `[GRBL]` messages
- **Result**: Clean, informative output without serial flooding

---

## Known Issues & Future Work

### Current Limitations

1. **Full Circle Arcs**: Not implemented (split into half-circles)
2. **G18/G19 Planes**: Only XY plane (G17) supported
3. **Probing**: Hardware integration pending
4. **Homing**: Automatic homing sequences not implemented

### Future Enhancements

1. **Adaptive Feedrate**: Slow down for small segments
2. **Cornering Optimization**: Dynamic junction deviation
3. **Vibration Damping**: Input shaping algorithms
4. **Backlash Compensation**: Per-axis backlash tables

---

**End of Linear Motion Reference**
