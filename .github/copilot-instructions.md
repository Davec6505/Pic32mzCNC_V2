# PIC32MZ CNC Motion Controller V2 - AI Coding Guide

## ⚠️ CURRENT STATUS: Centralized Types + Motion Buffer Foundation (October 16, 2025)

**Latest Progress**: Added centralized type system and ring buffer for G-code planning.

**Current Working System** ✅:
- **Button-driven testing**: SW1/SW2 trigger predefined motion patterns
- **Multi-axis S-curve control**: TMR1 @ 1kHz drives 7-segment jerk-limited profiles per axis
- **Hardware pulse generation**: OCR modules (OCMP1/4/5/3) generate step pulses independently
- **X-axis VERIFIED**: Tested with oscilloscope - smooth S-curve motion profiles confirmed
- **Y/Z/A axes ready**: Hardware configured, awaiting physical wiring
- **Motion buffer infrastructure**: Ring buffer with look-ahead planning API ready for G-code parser

**Recent Additions** ✅:
- ✅ **motion_types.h**: Centralized type definitions (single source of truth)
- ✅ **motion_buffer.c/h**: Ring buffer for look-ahead planning (16 blocks)
- ✅ **Type system**: Eliminated all duplicate type definitions

**Current File Structure**:
```
srcs/
  ├── main.c                        // Entry point, SYS_Initialize, main loop
  ├── app.c                         // Button handling, LED indicators, test patterns
  └── motion/
      ├── multiaxis_control.c       // Time-based S-curve interpolation (1169 lines)
      ├── motion_math.c             // Kinematics & GRBL settings (733 lines)
      ├── motion_buffer.c           // Ring buffer for look-ahead planning (284 lines) ✨NEW
      └── stepper_control.c         // Legacy single-axis reference (unused)

incs/motion/
  ├── motion_types.h                // Centralized type definitions (235 lines) ✨NEW
  ├── motion_buffer.h               // Ring buffer API (207 lines) ✨NEW
  ├── multiaxis_control.h           // Multi-axis API
  ├── motion_math.h                 // Unit conversions, look-ahead support (398 lines)
  └── stepper_control.h             // Legacy reference
```

**Design Philosophy**:
- **Time-based interpolation** (NOT Bresenham step counting)
- **Hardware-accelerated pulse generation** (OCR modules, no software interrupts)
- **Per-axis motion limits** (Z can be slower than XY)
- **Centralized settings** (motion_math.c for GRBL $100-$133)
- **Centralized types** (motion_types.h - single source of truth)
- **Separation of concerns** (math library vs motion control vs planning)

**TODO - NEXT PRIORITY**: 
🎯 **Implement G-code parser to feed motion buffer**
- Parse G0/G1 linear moves → `parsed_move_t` structures
- Feed to `MotionBuffer_Add()` for look-ahead planning
- Implement GRBL serial protocol with ring buffer for commands
- Add G2/G3 arc interpolation (eventually)

## Architecture Overview

This is a **modular embedded CNC controller** for the PIC32MZ2048EFH100 microcontroller with **hardware-accelerated multi-axis S-curve motion profiles**. The system uses independent OCR (Output Compare) modules for pulse generation, eliminating the need for GRBL's traditional 30kHz step interrupt.

### Core Architecture Pattern (Current - October 2025)
```
TMR1 (1kHz) → Multi-Axis S-Curve State Machine
           ↓
      Per-Axis Control (multiaxis_control.c)
      ├── Independent S-Curve Profiles (7 segments: jerk-limited)
      ├── Hardware Pulse Generation (OCMP1/3/4/5 + TMR2/3/4/5)
      ├── Dynamic Direction Control (function pointer tables)
      └── Step Counter Callbacks (OCR interrupts)
           ↓
      Hardware Layer (PIC32MZ OCR Modules)
      └── Dual-Compare PWM Mode (40-count pulse width for DRV8825)
```

### Critical Data Flow
- **Real-time control**: TMR1 @ 1kHz updates S-curve velocities for ALL active axes
- **Step generation**: Independent OCR hardware modules (OCMP1/3/4/5) generate pulses
- **Position feedback**: OCR callbacks increment `step_count` (volatile, per-axis)
- **Synchronization**: All axes share segment timing from dominant axis (for coordinated moves)
- **Safety**: Per-axis active flags, step count validation, velocity clamping

## Key Files & Responsibilities

| File | Purpose | Critical Patterns |
|------|---------|------------------|
| `srcs/main.c` | Entry point, system initialization | Calls `SYS_Initialize()` → `APP_Initialize()` → infinite loop |
| `srcs/app.c` | Button-based testing, LED indicators | SW1/SW2 trigger predefined motion via `multiaxis_control` API |
| `incs/motion/motion_types.h` | **Centralized type definitions** | 235 lines: Single source of truth for ALL motion types - axis_id_t, position_t, motion_block_t, parsed_move_t, etc. |
| `srcs/motion/motion_buffer.c` | **Ring buffer for look-ahead planning** | 284 lines: 16-block ring buffer, converts parsed_move_t to motion_block_t, triggers replanning at threshold |
| `incs/motion/motion_buffer.h` | **Motion buffer API** | 207 lines: Add/GetNext/Recalculate functions, flow control (Pause/Resume/Clear), buffer queries |
| `srcs/motion/multiaxis_control.c` | **Time-based S-curve interpolation** | 1169 lines: 7-segment profiles, TMR1 @ 1kHz, per-axis limits from motion_math |
| `incs/motion/multiaxis_control.h` | **Multi-axis API** | 4-axis support (X/Y/Z/A), coordinated/single-axis moves, driver enable control |
| `srcs/motion/motion_math.c` | **Kinematics & settings library** | 733 lines: Unit conversions, GRBL settings, look-ahead helpers, time-based calculations |
| `incs/motion/motion_math.h` | **Motion math API** | 398 lines: Settings management, velocity calculations, junction planning, S-curve timing |
| `srcs/motion/stepper_control.c` | **Legacy single-axis reference** | UNUSED - kept for reference only |

## Development Workflow

### Build System (Cross-Platform Make)
```bash
# From project root directory:
make all                    # Clean build with hex generation
make build_dir             # Create directory structure (run first)
make clean                 # Clean all outputs  
make platform             # Show build system info
make debug                # Show detailed build configuration
```

**Critical**: Build system expects specific directory structure:
- `srcs/` - Source files (.c, .S)  
- `incs/` - Headers (.h)
- `objs/` - Object files (auto-generated)
- `bins/` - Final executables (.elf, .hex)

### Testing with PowerShell Scripts
The project uses **PowerShell scripts for hardware-in-the-loop testing**:

```powershell
# Basic motion testing
.\motion_test.ps1 -Port COM4 -BaudRate 115200

# UGS compatibility testing  
.\ugs_test.ps1 -Port COM4 -GCodeFile modular_test.gcode

# Real-time debugging
.\monitor_debug.ps1 -Port COM4
```

**Pattern**: All test scripts use `Send-GCodeCommand` function with timeout/retry logic for reliable serial communication.

## Code Patterns & Conventions

### Modular API Design
**Current (October 2025)**: Time-based interpolation with centralized settings:

```c
// Multi-Axis Control API - Time-based S-curve profiles
void MultiAxis_Initialize(void);  // Calls MotionMath_InitializeSettings()
void MultiAxis_MoveSingleAxis(axis_id_t axis, int32_t steps, bool forward);
void MultiAxis_MoveCoordinated(int32_t steps[NUM_AXES]);
bool MultiAxis_IsBusy(void);  // Checks all axes independently
void MultiAxis_EmergencyStop(void);

// Dynamic direction control (function pointer tables)
void MultiAxis_SetDirection(axis_id_t axis);
void MultiAxis_ClearDirection(axis_id_t axis);

// Per-axis state query
bool MultiAxis_IsAxisBusy(axis_id_t axis);
uint32_t MultiAxis_GetStepCount(axis_id_t axis);

// Motion Math API - Kinematics & Settings
void MotionMath_InitializeSettings(void);  // Load GRBL defaults
float MotionMath_GetMaxVelocityStepsPerSec(axis_id_t axis);
float MotionMath_GetAccelStepsPerSec2(axis_id_t axis);
float MotionMath_GetJerkStepsPerSec3(axis_id_t axis);

// Unit conversions (for future G-code parser)
int32_t MotionMath_MMToSteps(float mm, axis_id_t axis);
float MotionMath_StepsToMM(int32_t steps, axis_id_t axis);
uint32_t MotionMath_FeedrateToOCRPeriod(float feedrate_mm_min, axis_id_t axis);

// Look-ahead planning (for future motion buffer)
float MotionMath_CalculateJunctionVelocity(...);
float MotionMath_CalculateJunctionAngle(...);
bool MotionMath_CalculateSCurveTiming(...);
```

### Motion Math Settings Pattern
**GRBL v1.1f Compatibility**: Centralized settings in motion_math module

```c
// Default settings (loaded by MotionMath_InitializeSettings)
motion_settings.steps_per_mm[AXIS_X] = 250.0f;    // $100: Steps per mm
motion_settings.max_rate[AXIS_X] = 5000.0f;       // $110: Max rate (mm/min)
motion_settings.acceleration[AXIS_X] = 500.0f;    // $120: Acceleration (mm/sec²)
motion_settings.max_travel[AXIS_X] = 300.0f;      // $130: Max travel (mm)
motion_settings.junction_deviation = 0.01f;       // $11: Junction deviation
motion_settings.jerk_limit = 5000.0f;             // Jerk limit (mm/sec³)

// CORRECT: Use motion_math for conversions
int32_t steps = MotionMath_MMToSteps(10.0f, AXIS_X);  // 10mm → 2500 steps
float max_vel = MotionMath_GetMaxVelocityStepsPerSec(AXIS_X);  // 5000mm/min → steps/sec

// INCORRECT: Don't hardcode motion parameters
static float max_velocity = 5000.0f;  // ❌ Use motion_math instead!
```

### OCR Hardware Integration
**PIC32MZ-specific**: Hardware pulse generation using Output Compare modules:

```c
// OCR assignments (fixed in hardware):
// OCMP1 → Y-axis step pulses  
// OCMP3 → A-axis step pulses (4th axis)
// OCMP4 → X-axis step pulses
// OCMP5 → Z-axis step pulses

// Timer sources (VERIFIED per MCC configuration - October 2025):
// TMR1 → 1kHz motion control timing (MotionPlanner_UpdateTrajectory)
// TMR2 → OCMP4 time base for X-axis step pulse generation
// TMR3 → OCMP5 time base for Z-axis step pulse generation
// TMR4 → OCMP1 time base for Y-axis step pulse generation
// TMR5 → OCMP3 time base for A-axis step pulse generation
```

**PIC32MZ Timer Source Options (Table 18-1):**
```
Output Compare Module | Available Timer Sources | ACTUAL Assignment
------------------------------------------------------------------
OC1 (Y-axis)         | Timer2 or Timer3        | TMR4 (per MCC)
OC2 (unused)         | Timer4 or Timer5        | N/A
OC3 (A-axis)         | Timer4 or Timer5        | TMR5 (per MCC)
OC4 (X-axis)         | Timer2 or Timer3        | TMR2 (per MCC)
OC5 (Z-axis)         | Timer2 or Timer3        | TMR3 (per MCC)
```

**OCR Dual-Compare Architecture - VERIFIED WORKING PATTERN (Oct 2025):**

This is the **PRODUCTION-PROVEN CONFIGURATION** tested with hardware oscilloscope verification:

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

/* Configure OCR dual-compare mode (CRITICAL - exact register sequence):
 * TMRxPR = period (timer rollover)
 * OCxR = period - OCMP_PULSE_WIDTH (rising edge)
 * OCxRS = OCMP_PULSE_WIDTH (falling edge)
 * 
 * IMPORTANT: OCxR and OCxRS appear reversed from intuition but this is CORRECT!
 * The hardware generates rising edge at OCxR and falling edge at OCxRS.
````
 * 
 * Example with period=300:
 *   TMR2PR = 300          // Timer rolls over at 300
 *   OC4R = 260            // Pulse rises at count 260 (300-40)
 *   OC4RS = 40            // Pulse falls at count 40
 *   Result: Pin HIGH from count 40 to 260, LOW from 260 to 300, then repeat
 *   Effective pulse width = 40 counts (meets DRV8825 1.9µs minimum)
 * 
 * Timing diagram:
 *   Count: 0....40.......260.......300 (rollover)
 *   Pin:   LOW  HIGH     LOW       LOW
 *          └────────────┘
 *          40 counts ON
 */
TMRx_PeriodSet(period);                                    // Set timer rollover
OCMPx_CompareValueSet(period - OCMP_PULSE_WIDTH);         // Rising edge (variable)
OCMPx_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);         // Falling edge (fixed at 40)
OCMPx_Enable();
TMRx_Start();                                              // CRITICAL: Must restart timer for each move
```

**CRITICAL MOTION EXECUTION PATTERN (October 2025):**

This sequence is **MANDATORY** for reliable bidirectional motion:

```c
// 1. Set direction GPIO BEFORE enabling step pulses (DRV8825 requirement)
if (direction_forward) {
    DirX_Set();    // GPIO high for forward
} else {
    DirX_Clear();  // GPIO low for reverse
}

// 2. Configure OCR registers with proven pattern
TMR2_PeriodSet(period);
OCMP4_CompareValueSet(period - 40);  // Rising edge
OCMP4_CompareSecondaryValueSet(40);  // Falling edge

// 3. Enable OCR module
OCMP4_Enable();

// 4. **ALWAYS** restart timer for each move (even if already running)
TMR2_Start();  // Critical! Timers are stopped when motion completes
```

**Common Mistakes to Avoid:**
- ❌ **Don't forget `TMRx_Start()`** - Timer stops when motion completes, must restart for next move
- ❌ **Don't swap OCxR/OCxRS values** - Use exact pattern above (period-40 / 40)
- ❌ **Don't set direction after step pulses start** - DRV8825 needs direction stable before first pulse
- ❌ **Don't use wrong timer** - X=TMR2, Y=TMR4, Z=TMR3 per MCC configuration

**Key Register Values:**
- **TMRxPR**: Timer period register (controls pulse frequency)
- **OCxR**: Primary compare (rising edge - varies: period-40)
- **OCxRS**: Secondary compare (falling edge - fixed at 40)
- **Pulse Width**: Always 40 counts (meeting DRV8825 1.9µs minimum)
- **Maximum Period**: 65485 counts (16-bit timer limit with safety margin)

### DRV8825 Stepper Driver Interface
**Hardware**: Pololu DRV8825 carrier boards (or compatible) for bipolar stepper motors

**Control signals** (microcontroller → driver):
- **STEP**: Pulse input - each rising edge = one microstep (pulled low by 100kΩ)
- **DIR**: Direction control - HIGH/LOW sets rotation direction (pulled low by 100kΩ)
  - **CRITICAL**: Must be set BEFORE first step pulse and remain stable during motion
  - **Our implementation**: Set via `DirX_Set()`/`DirX_Clear()` before `OCMPx_Enable()`
- **ENABLE**: Active-low enable (can leave disconnected for always-enabled)
- **RESET/SLEEP**: Pulled high by 1MΩ/10kΩ respectively (normal operation)

**Timing requirements** (DRV8825 datasheet):
- **Minimum STEP pulse width**: 1.9µs HIGH + 1.9µs LOW
- **Our implementation**: 40 timer counts @ 1MHz = 40µs (safe margin above minimum)
- **Why 40 counts**: Ensures reliable detection across all microstepping modes
- **Direction setup time**: 200ns minimum (our GPIO write provides this)

**Microstepping configuration** (MODE0/1/2 pins with 100kΩ pull-downs):
```
MODE2  MODE1  MODE0  Resolution
--------------------------------
 Low    Low    Low   Full step
High    Low    Low   Half step
 Low   High    Low   1/4 step
High   High    Low   1/8 step
 Low    Low   High   1/16 step
High    Low   High   1/32 step
```

**Power and current limiting**:
- **VMOT**: 8.2V - 45V motor supply (100µF decoupling capacitor required)
- **Current limit**: Set via VREF potentiometer using formula `Current = VREF × 2`
- **Current sense resistors**: 0.100Ω (DRV8825) vs 0.330Ω (A4988)
- **CRITICAL**: Never connect/disconnect motors while powered - will destroy driver

**Fault protection**:
- **FAULT pin**: Pulls low on over-current, over-temperature, or under-voltage
- **Protection resistor**: 1.5kΩ in series allows safe connection to logic supply
- Our system can monitor this pin for real-time error detection

### Time-Based Interpolation Architecture
**CRITICAL**: This system uses **time-based velocity interpolation**, NOT Bresenham step counting!

```c
// How it works (TMR1 @ 1kHz):
TMR1_MultiAxisControl() {
    // Get per-axis motion limits from motion_math
    float max_velocity = MotionMath_GetMaxVelocityStepsPerSec(axis);
    float max_accel = MotionMath_GetAccelStepsPerSec2(axis);
    float max_jerk = MotionMath_GetJerkStepsPerSec3(axis);
    
    // Update S-curve velocity profile every 1ms
    velocity = calculate_scurve_velocity(time_elapsed, max_accel, max_jerk);
    
    // Convert velocity to OCR period for hardware pulse generation
    OCR_period = 1MHz / velocity_steps_sec;
    
    // All axes finish at SAME TIME (coordinated via dominant axis)
}
```

**Key Differences from Bresenham**:
- ✅ **Velocity-driven**: OCR hardware generates pulses at calculated rates
- ✅ **Time-synchronized**: All axes finish simultaneously (coordinated moves)
- ✅ **Per-axis limits**: Z can have different acceleration than XY
- ❌ **NOT step counting**: No error accumulation or step ratios

### GRBL Settings Pattern
Full GRBL v1.1f compliance (ready for G-code parser):

```c
// Standard GRBL settings ($100-$133):
$100-$103 = Steps per mm (X,Y,Z,A)
$110-$113 = Max rates (mm/min)  
$120-$123 = Acceleration (mm/sec²)
$130-$133 = Max travel (mm)
$11 = Junction deviation (for look-ahead)
$12 = Arc tolerance (for G2/G3)
```

### S-Curve Motion Profiles
**Advanced feature**: 7-segment jerk-limited motion profiles:

```c
// S-curve profile structure
typedef struct {
    float total_time, accel_time, const_time, decel_time;
    float peak_velocity, acceleration, distance;
    bool use_scurve;
    position_t start_pos, end_pos;
} scurve_motion_profile_t;
```

### Error Handling & Safety
**Critical safety patterns**:

```c  
// Hard limit handling (immediate response)
void APP_ProcessLimitSwitches(void) {
    bool x_limit = !GPIO_PinRead(GPIO_PIN_RA7);  // Active low
    if (x_limit) {
        INTERP_HandleHardLimit(AXIS_X, true, false);
        appData.state = APP_STATE_MOTION_ERROR;  // Immediate stop
    }
}

// Soft limit validation (preventive)
if (!INTERP_CheckSoftLimits(target_position)) {
    return false;  // Reject unsafe move
}
```

## Hardware-Specific Considerations

### PIC32MZ Memory Management
- **Heap**: 20KB configured in Makefile
- **Stack**: 20KB configured in Makefile
- **Flash**: 2MB total, use efficiently for look-ahead buffers

### Pin Assignments (Active Low Logic)
```c
// Limit switches (check app.h for current assignments):
GPIO_PIN_RA7  → X-axis negative limit
GPIO_PIN_RA9  → X-axis positive limit  
GPIO_PIN_RA10 → Y-axis negative limit
GPIO_PIN_RA14 → Y-axis positive limit
GPIO_PIN_RA15 → Z-axis negative limit
```

### Pick-and-Place Mode
**Special feature**: Z-axis limit masking for spring-loaded operations:

```c
APP_SetPickAndPlaceMode(true);   // Mask Z minimum limit
// ... perform pick/place operations ...
APP_SetPickAndPlaceMode(false);  // Restore normal limits
```

## Integration Points

### Hardware Testing Current Capabilities
- **SW1 button**: Triggers X-axis single move (5000 steps forward)
- **SW2 button**: Triggers coordinated 3-axis move (X=5000, Y=5000, Z=10000)
- **LED1**: Heartbeat @ 1Hz when idle, solid during motion (driven by TMR1 callback)
- **LED2**: Power-on indicator, toggles when axis processing occurs
- **X-axis verified**: Oscilloscope confirms smooth S-curve velocity profiles
- **Y/Z/A axes ready**: Hardware configured, awaiting physical wiring

### Future Integration Points (To Be Re-implemented)
- **Universal G-code Sender (UGS)**: GRBL v1.1f protocol compatibility
- **Serial Protocol**: Real-time commands (feed hold, cycle start, reset)
- **Arc Support**: Native G2/G3 processing (requires interpolation engine)

### Cross-Platform Build  
- **Windows**: PowerShell-based testing, MPLAB X IDE v6.25
- **Linux**: Make-based build system, XC32 v4.60 compiler
- **Paths**: Auto-detected OS with proper path separators

### Version Control
- **Git workflow**: Use raw git commands (not GitKraken)
- Standard git add/commit/push workflow for version control

## Common Tasks

### Testing Current System
1. **Flash firmware** to PIC32MZ board (`bins/CS23.hex`)
2. **Press SW1** to trigger X-axis single move (5000 steps forward)
3. **Press SW2** to trigger coordinated 3-axis move (X/Y/Z)
4. **Observe LED1** for heartbeat (1Hz idle) or solid (motion active)
5. **Observe LED2** for power-on and axis processing activity
6. **Use oscilloscope** to verify S-curve velocity profiles on step/dir pins

### Adding New Motion Commands
1. **For testing (steps-based)**:
   ```c
   MultiAxis_MoveSingleAxis(AXIS_X, 5000, true);  // 5000 steps forward
   ```

2. **For G-code (mm-based)** - when parser is added:
   ```c
   // Parse "G1 X10 F1500"
   int32_t steps = MotionMath_MMToSteps(10.0f, AXIS_X);  // 10mm → 2500 steps
   MultiAxis_MoveSingleAxis(AXIS_X, steps, true);
   ```

3. **Monitor completion**:
   ```c
   while (MultiAxis_IsBusy()) { }  // Wait for all axes
   // or per-axis:
   while (MultiAxis_IsAxisBusy(AXIS_X)) { }
   ```

4. **Edit button handlers** in `app.c` to test different patterns

### Debugging Motion Issues
1. Check TMR1 @ 1kHz callback: `TMR1_MultiAxisControl()` in `multiaxis_control.c`
2. Check OCR interrupt callbacks: `OCMP4_StepCounter_X()`, `OCMP1_StepCounter_Y()`, `OCMP5_StepCounter_Z()`
3. Verify S-curve profile calculations with oscilloscope (expect symmetric velocity ramps)
4. Monitor per-axis `active` flags and `step_count` values
5. Verify LED1 heartbeat confirms TMR1 is running @ 1Hz

### Hardware Testing
1. **Always** use conservative velocities for initial testing (max_velocity = 5000 steps/sec)
2. Verify OCR period calculations with oscilloscope (expect symmetric S-curve)
3. Test emergency stop functionality first (`MultiAxis_StopAll()`)
4. Test single-axis moves before coordinated multi-axis moves
5. X-axis is proven working, Y/Z/A axes configured but not physically wired yet

## Motion Math Integration (October 16, 2025)

### Architecture Overview
The motion system now uses a **two-layer architecture**:

1. **Motion Math Layer** (`motion_math.c/h`):
   - Centralized GRBL settings storage
   - Unit conversions (mm ↔ steps, mm/min ↔ steps/sec)
   - Look-ahead planning helpers (junction velocity, S-curve timing)
   - Pure functions (no side effects, easy to test)

2. **Motion Control Layer** (`multiaxis_control.c/h`):
   - Time-based S-curve interpolation (7 segments)
   - Per-axis state machines (TMR1 @ 1kHz)
   - Hardware OCR pulse generation
   - Gets motion limits from motion_math

### Integration Points

**Initialization**:
```c
void MultiAxis_Initialize(void) {
    MotionMath_InitializeSettings();  // Load GRBL defaults
    // ... register callbacks, start TMR1
}
```

**Per-Axis Motion Limits**:
```c
// In calculate_scurve_profile() and TMR1_MultiAxisControl():
float max_velocity = MotionMath_GetMaxVelocityStepsPerSec(axis);
float max_accel = MotionMath_GetAccelStepsPerSec2(axis);
float max_jerk = MotionMath_GetJerkStepsPerSec3(axis);
```

**Default Settings** (Conservative for Testing):
```c
Steps/mm:     250 (all axes) - GT2 belt with 1/16 microstepping
Max Rate:     5000 mm/min (X/Y/A), 2000 mm/min (Z)
Acceleration: 500 mm/sec² (X/Y/A), 200 mm/sec² (Z)
Max Travel:   300mm (X/Y), 100mm (Z), 360° (A)
Junction Dev: 0.01mm - Tight corners for accuracy
Jerk Limit:   5000 mm/sec³ - Smooth S-curve transitions
```

## Centralized Type System (October 16, 2025)

### Architecture Philosophy

**Problem Solved**: Previous architecture had duplicate type definitions across multiple headers (`axis_id_t` in both `multiaxis_control.h` and `motion_math.h`, causing compilation errors and maintenance headaches).

**Solution**: Single `motion_types.h` header as the **authoritative source** for all motion-related data structures.

### Type System Organization

**File**: `incs/motion/motion_types.h` (235 lines)

All motion modules include this header:
```c
#include "motion_types.h"  // Gets ALL motion types
```

**Critical Rule**: 🚫 **NEVER define types elsewhere!** Always use motion_types.h

### Core Type Categories

#### **1. Axis Definitions**
```c
typedef enum {
    AXIS_X = 0,
    AXIS_Y = 1,
    AXIS_Z = 2,
    AXIS_A = 3,
    NUM_AXES = 4
} axis_id_t;
```
Used by: ALL motion modules for axis identification

#### **2. Position Tracking**
```c
typedef struct {
    int32_t x, y, z, a;
} position_t;
```
Used by: Tracking current/target positions in steps

#### **3. Motion Block (Ring Buffer Entry)** ⭐ **CRITICAL**
```c
typedef struct {
    int32_t steps[NUM_AXES];        // Target position (absolute steps)
    float feedrate;                 // Requested feedrate (mm/min)
    float entry_velocity;           // From look-ahead planner (mm/min)
    float exit_velocity;            // From look-ahead planner (mm/min)
    float max_entry_velocity;       // Junction limit (mm/min)
    bool recalculate_flag;          // Needs replanning
    bool axis_active[NUM_AXES];     // Which axes move
    scurve_motion_profile_t profile; // Pre-calculated S-curve
} motion_block_t;
```
Used by: Motion buffer ring buffer - **this is what feeds multiaxis_control**

#### **4. Parsed G-Code Move** ⭐ **CRITICAL**
```c
typedef struct {
    float target[NUM_AXES];         // Target position (mm or degrees)
    float feedrate;                 // Feed rate (mm/min)
    bool absolute_mode;             // G90 (true) or G91 (false)
    bool axis_words[NUM_AXES];      // Which axes specified
    uint8_t motion_mode;            // G0, G1, G2, G3, etc.
} parsed_move_t;
```
Used by: G-code parser output → input to motion buffer

#### **5. S-Curve Motion Profile**
```c
typedef struct {
    float total_time, accel_time, const_time, decel_time;
    float peak_velocity, acceleration, distance;
    bool use_scurve;
    position_t start_pos, end_pos;
} scurve_motion_profile_t;
```
Used by: multiaxis_control for 7-segment jerk-limited motion

#### **6. Other Important Types**
- `coordinated_move_t` - Multi-axis move request (steps + active flags)
- `velocity_profile_t` - Look-ahead velocity optimization data
- `scurve_timing_t` - Detailed 7-segment timing calculations
- `motion_settings_t` - GRBL v1.1f settings structure ($100-$133)
- `motion_coordinated_move_t` - Coordination analysis (dominant axis, ratios)

### Benefits of Centralization

1. ✅ **No Duplicate Definitions** - Compiler catches redefinitions immediately
2. ✅ **Single Source of Truth** - Update type once, all modules see it
3. ✅ **Clear Dependencies** - `#include "motion_types.h"` documents what's used
4. ✅ **Easy Maintenance** - Add new fields without hunting through multiple files
5. ✅ **Scalability** - Add new types (spindle_state_t, coolant_state_t) in one place

### Migration Pattern

**BEFORE (Duplicate Types)**:
```c
// multiaxis_control.h
typedef enum { AXIS_X, AXIS_Y, AXIS_Z, AXIS_A } axis_id_t;

// motion_math.h  
typedef enum { AXIS_X, AXIS_Y, AXIS_Z, AXIS_A } axis_id_t;  // ❌ Duplicate!
```

**AFTER (Centralized)**:
```c
// motion_types.h
typedef enum { AXIS_X, AXIS_Y, AXIS_Z, AXIS_A } axis_id_t;

// multiaxis_control.h
#include "motion_types.h"  // ✅ Import types

// motion_math.h
#include "motion_types.h"  // ✅ Import types
```

---

## Motion Buffer & Ring Buffer Architecture (October 16, 2025)

### Overview

The motion buffer implements a **circular FIFO queue** of planned motion blocks that feeds the real-time motion controller. It provides look-ahead planning to optimize junction velocities and minimize total move time.

### Ring Buffer Design

**File**: `srcs/motion/motion_buffer.c` (284 lines)  
**API**: `incs/motion/motion_buffer.h` (207 lines)

#### **Configuration**
```c
#define MOTION_BUFFER_SIZE 16               // Must be power of 2
#define LOOKAHEAD_PLANNING_THRESHOLD 4      // Trigger replanning at this count
```

#### **Buffer State**
```c
static motion_block_t motion_buffer[MOTION_BUFFER_SIZE];  // Circular array
static volatile uint8_t buffer_head = 0;  // Next write index
static volatile uint8_t buffer_tail = 0;  // Next read index
```

#### **Ring Buffer Properties**
- **Empty**: `head == tail`
- **Full**: `(head + 1) % SIZE == tail`
- **Count**: `(head - tail + SIZE) % SIZE`
- **Modulo Arithmetic**: Efficient wraparound using `% MOTION_BUFFER_SIZE`

### Complete Data Flow (Serial to Motion)

```
┌─────────────────────────────────────────────────────────────────┐
│ Stage 1: Serial Reception (Future)                             │
│ Serial RX Interrupt → Serial Ring Buffer (raw bytes)           │
└────────────────────┬────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────────┐
│ Stage 2: Command Parsing (Future - G-code Parser)              │
│ Extract lines → Parse tokens → Generate parsed_move_t          │
│                                                                 │
│ Example: "G1 X10 Y20 F1500" →                                  │
│   parsed_move_t {                                               │
│     target = {10.0, 20.0, 0.0, 0.0},  // mm                    │
│     feedrate = 1500.0,                // mm/min                │
│     axis_words = {true, true, false, false}                    │
│   }                                                             │
└────────────────────┬────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────────┐
│ Stage 3: Motion Planning (motion_buffer.c) ⭐ IMPLEMENTED       │
│ MotionBuffer_Add(parsed_move_t*) {                             │
│   1. Convert mm to steps (MotionMath_MMToSteps)                │
│   2. Calculate max entry velocity (junction angle)             │
│   3. Add to ring buffer → motion_block_t                        │
│   4. Trigger replanning if threshold reached                   │
│ }                                                               │
│                                                                 │
│ Ring Buffer: [motion_block_t, motion_block_t, ...]             │
│              ↑tail (read)          ↑head (write)                │
└────────────────────┬────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────────┐
│ Stage 4: Look-Ahead Planning (motion_buffer.c) ⭐ PLACEHOLDER   │
│ MotionBuffer_RecalculateAll() {                                 │
│   Forward Pass: Maximize exit velocities                       │
│   Reverse Pass: Ensure accel limits respected                  │
│   Calculate S-curve profiles for each block                    │
│ }                                                               │
│                                                                 │
│ Output: Optimized motion_block_t with:                          │
│   - entry_velocity (safe cornering speed)                      │
│   - exit_velocity (limited by next junction)                   │
│   - scurve_motion_profile_t (ready for execution)              │
└────────────────────┬────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────────┐
│ Stage 5: Motion Execution (multiaxis_control.c) ✅ WORKING      │
│ Main Loop:                                                      │
│   if (!MultiAxis_IsBusy() && !MotionBuffer_IsEmpty()) {        │
│     motion_block_t block;                                       │
│     MotionBuffer_GetNext(&block);  // Dequeue from buffer      │
│     MultiAxis_MoveCoordinated(block.steps);  // Execute!       │
│   }                                                             │
│                                                                 │
│ TMR1 @ 1kHz → S-Curve Interpolation → OCR Pulse Generation     │
└─────────────────────────────────────────────────────────────────┘
```

### Motion Buffer API

#### **Core Functions**

**Initialization**:
```c
void MotionBuffer_Initialize(void);  // Clear buffer, reset pointers
```

**Adding Moves** (from G-code parser):
```c
bool MotionBuffer_Add(const parsed_move_t* move);
// Returns: true if added, false if buffer full
// Side effects:
//   - Converts mm to steps using motion_math
//   - Calculates max_entry_velocity
//   - Triggers replanning if threshold reached
```

**Retrieving Planned Moves** (for execution):
```c
bool MotionBuffer_GetNext(motion_block_t* block);
// Returns: true if block available, false if empty
// Side effects:
//   - Advances tail pointer
//   - Copies block with pre-calculated S-curve profile
```

**Buffer Queries**:
```c
bool MotionBuffer_IsEmpty(void);     // Check if no moves pending
bool MotionBuffer_IsFull(void);      // Check if can accept more moves
uint8_t MotionBuffer_GetCount(void); // Get number of blocks in buffer
```

**Flow Control** (for GRBL protocol):
```c
void MotionBuffer_Pause(void);       // Feed hold (!) - stop retrieving blocks
void MotionBuffer_Resume(void);      // Cycle start (~) - resume motion
void MotionBuffer_Clear(void);       // Emergency stop - discard all blocks
```

**Look-Ahead Planning**:
```c
void MotionBuffer_RecalculateAll(void);  // Optimize all block velocities
float MotionBuffer_CalculateJunctionVelocity(
    const motion_block_t* block1,
    const motion_block_t* block2
);  // Calculate safe cornering speed between two moves
```

### GRBL Serial Protocol Integration

**Flow Control Pattern** (when G-code parser is added):
```c
// In G-code parser main loop:
if (GCode_ParseLine(line, &parsed_move)) {
    if (MotionBuffer_Add(&parsed_move)) {
        Serial_SendOK();  // ✅ Buffer accepted move - UGS can send next
    } else {
        // ❌ Buffer full - DON'T send "ok"
        // UGS will wait, retry on next loop iteration
    }
}
```

**Real-Time Commands** (bypass buffer):
```c
// Check for real-time commands BEFORE parsing:
if (serial_char == '?') {
    GRBL_SendStatusReport();  // Immediate response
    return;
}
if (serial_char == '!') {
    MotionBuffer_Pause();  // Feed hold - stop motion
    return;
}
if (serial_char == '~') {
    MotionBuffer_Resume();  // Cycle start - resume motion
    return;
}
```

### Look-Ahead Planning Algorithm (TODO)

**Current Status**: Placeholder implementation in `recalculate_trapezoids()`

**Full Algorithm** (GRBL-style):
1. **Forward Pass**: Iterate tail → head
   - Calculate maximum exit velocity for each block
   - Limited by: feedrate, acceleration, next junction angle
   
2. **Reverse Pass**: Iterate head → tail
   - Ensure entry velocities respect acceleration limits
   - Adjust exit velocities if entry velocity reduced
   
3. **S-Curve Generation**: For each block
   - Call `MotionMath_CalculateSCurveTiming()` with entry/exit velocities
   - Store profile in `motion_block_t.profile`

### Memory Usage

**Total**: ~2KB for motion buffer
```c
sizeof(motion_block_t) ≈ 120 bytes
16 blocks × 120 bytes = 1920 bytes
Plus state variables: ~100 bytes
Total: ~2KB
```

### Ring Buffer Benefits

1. ✅ **O(1) Operations** - Add and remove are constant time (modulo arithmetic)
2. ✅ **Memory Efficient** - Fixed size, no dynamic allocation
3. ✅ **Cache Friendly** - Contiguous array, good locality
4. ✅ **FIFO Ordering** - Oldest moves executed first (natural for G-code)
5. ✅ **Flow Control Ready** - Buffer full condition for GRBL protocol
6. ✅ **Look-Ahead Ready** - Can iterate forward/backward for velocity planning

### Integration with Motion Math

The motion buffer is the **bridge** between G-code parsing and motion execution:

```c
// Step 1: G-code parser creates parsed_move_t (in mm)
parsed_move_t move = {
    .target = {10.0, 20.0, 0.0, 0.0},  // X=10mm, Y=20mm
    .feedrate = 1500.0,                 // 1500 mm/min
    .axis_words = {true, true, false, false}
};

// Step 2: Motion buffer converts using motion_math
int32_t steps[NUM_AXES];
steps[AXIS_X] = MotionMath_MMToSteps(10.0, AXIS_X);  // 10mm → 2500 steps
steps[AXIS_Y] = MotionMath_MMToSteps(20.0, AXIS_Y);  // 20mm → 5000 steps

// Step 3: Add to ring buffer with calculated velocities
motion_block_t block = {
    .steps = {2500, 5000, 0, 0},
    .feedrate = 1500.0,
    .entry_velocity = calculated_junction_velocity,
    .exit_velocity = next_junction_velocity,
    // ... pre-calculated S-curve profile
};

// Step 4: Execution retrieves from buffer
if (MotionBuffer_GetNext(&block)) {
    MultiAxis_MoveCoordinated(block.steps);  // Execute with S-curve
}
```

---

## Motion Math Integration (October 16, 2025)

### Architecture Overview
The motion system now uses a **two-layer architecture**:

1. **Motion Math Layer** (`motion_math.c/h`):
   - Centralized GRBL settings storage
   - Unit conversions (mm ↔ steps, mm/min ↔ steps/sec)
   - Look-ahead planning helpers (junction velocity, S-curve timing)
   - Pure functions (no side effects, easy to test)

2. **Motion Control Layer** (`multiaxis_control.c/h`):
   - Time-based S-curve interpolation (7 segments)
   - Per-axis state machines (TMR1 @ 1kHz)
   - Hardware OCR pulse generation
   - Gets motion limits from motion_math

### Integration Points

**Initialization**:
```c
void MultiAxis_Initialize(void) {
    MotionMath_InitializeSettings();  // Load GRBL defaults
    // ... register callbacks, start TMR1
}
```

**Per-Axis Motion Limits**:
```c
// In calculate_scurve_profile() and TMR1_MultiAxisControl():
float max_velocity = MotionMath_GetMaxVelocityStepsPerSec(axis);
float max_accel = MotionMath_GetAccelStepsPerSec2(axis);
float max_jerk = MotionMath_GetJerkStepsPerSec3(axis);
```

**Default Settings** (Conservative for Testing):
```c
Steps/mm:     250 (all axes) - GT2 belt with 1/16 microstepping
Max Rate:     5000 mm/min (X/Y/A), 2000 mm/min (Z)
Acceleration: 500 mm/sec² (X/Y/A), 200 mm/sec² (Z)
Max Travel:   300mm (X/Y), 100mm (Z), 360° (A)
Junction Dev: 0.01mm - Tight corners for accuracy
Jerk Limit:   5000 mm/sec³ - Smooth S-curve transitions
```

### Benefits of This Architecture

1. ✅ **Per-axis tuning**: Z can be slower/more precise than XY
2. ✅ **GRBL compatibility**: Settings use standard $100-$133 format
3. ✅ **Testability**: Motion math is pure functions (easy unit tests)
4. ✅ **Separation of concerns**: Math library vs real-time control
5. ✅ **Ready for G-code**: Conversion functions already in place
6. ✅ **Look-ahead ready**: Junction velocity helpers for future planner
7. ✅ **Centralized types**: motion_types.h prevents duplicate definitions
8. ✅ **Ring buffer ready**: Motion buffer bridges parser and execution

### Critical Design Principles
Total: ~2KB
```

### Critical Design Principles

⚠️ **Time-based interpolation** - NOT Bresenham step counting  
⚠️ **Hardware pulse generation** - OCR modules, no software step interrupts  
⚠️ **Coordinated motion** - All axes synchronized to dominant axis TIME  
⚠️ **Per-axis limits** - Each axis has independent velocity/accel/jerk  
⚠️ **Centralized settings** - motion_math is single source of truth  
⚠️ **Centralized types** - motion_types.h is single source for all type definitions  
⚠️ **Ring buffer architecture** - Motion buffer bridges parser and execution  

Remember: This is a **safety-critical real-time system**. Always validate motion commands and maintain proper error handling in interrupt contexts.