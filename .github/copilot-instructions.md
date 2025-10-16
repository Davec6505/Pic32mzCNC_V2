# PIC32MZ CNC Motion Controller V2 - AI Coding Guide

## ⚠️ CURRENT BRANCH: hardware-test (Multi-Axis S-Curve Development)

**Status**: Multi-axis S-curve control WORKING ✅ (October 2025)
- Per-axis independent state management implemented
- X-axis tested and verified with oscilloscope
- Y/Z/A axes ready (hardware not yet wired)

**TODO - NEXT PRIORITY**: 
🎯 **Implement coordinated circular interpolation (G2/G3 arc motion)**
- Decision needed: Vector-based interpolation method for multi-axis arcs
- Options to evaluate:
  1. Time-based velocity scaling (all axes synchronized to dominant axis timing)
  2. Chord-based linearization with coordinated S-curve segments
  3. Native parametric arc with synchronized angular velocity
- Consider: Hardware OCR generates linear step pulses, arc must be synthesized via coordinated velocity profiles
- Goal: Smooth circular motion with proper feedrate control and S-curve acceleration/deceleration

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
| `srcs/main.c` | Entry point, TMR1 callback setup | Always call `APP_Initialize()` before `TMR1_Start()` |
| `srcs/app.c` | Button-based testing, initialization | Simple test interface: SW1/SW2 trigger single-axis moves |
| `srcs/motion/multiaxis_control.c` | **Multi-axis S-curve engine** | 773 lines: Per-axis state, hardware abstraction, MISRA compliance |
| `incs/motion/multiaxis_control.h` | **Multi-axis API** | 88 lines: 4-axis support (X/Y/Z/A), dynamic direction control |
| `srcs/motion/stepper_control.c` | **Legacy single-axis reference** | PROVEN baseline implementation (kept for reference) |
| `incs/app.h` | Main system interface | Extensive getter/setter API documentation |

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
**Current (October 2025)**: Per-axis control with hardware abstraction:

```c
// Multi-Axis Control API - Independent per-axis S-curve profiles
void MultiAxis_Initialize(void);
void MultiAxis_MoveSingleAxis(axis_id_t axis, int32_t steps, bool forward);
void MultiAxis_MoveCoordinated(int32_t steps[NUM_AXES]);
bool MultiAxis_IsBusy(void);  // Checks all axes independently
void MultiAxis_EmergencyStop(void);

// Dynamic direction control (function pointer tables)
void MultiAxis_SetDirection(axis_id_t axis);
void MultiAxis_ClearDirection(axis_id_t axis);

// Per-axis state query
bool MultiAxis_IsAxisBusy(axis_id_t axis);
int32_t MultiAxis_GetStepCount(axis_id_t axis);
```

### Hardware Abstraction Pattern
Use getter/setter functions for hardware access (never direct global access):

```c
// CORRECT: Use hardware abstraction
int32_t pos = APP_GetAxisCurrentPosition(AXIS_X);
APP_SetAxisCurrentPosition(AXIS_X, new_position);

// INCORRECT: Direct hardware access  
cnc_axes[AXIS_X].current_position = new_position; // DON'T DO THIS
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

### GRBL Settings Pattern
Full GRBL v1.1f compliance with UGS integration:

```c
// Standard GRBL settings ($100-$132):
$100-$102 = Steps per mm (X,Y,Z)
$110-$112 = Max rates (mm/min)  
$120-$122 = Acceleration (mm/sec²)
$130-$132 = Max travel (mm)
$11 = Junction deviation
$20/$21 = Soft/hard limits enable
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

## Critical Bugs Fixed (October 2025)

### Bug #1: Status Report Steps/mm Mismatch
**Symptom**: Motion appeared to stop at 62.5% of target (e.g., 6.25mm instead of 10mm)  
**Root Cause**: `gcode_helpers.c` used hardcoded 400 steps/mm for status conversion while motion execution used 250 steps/mm  
**Fix**: Changed `GCodeHelpers_GetCurrentPositionFromSteps()` fallback from 400 to 250 steps/mm  
**Impact**: Status reports now accurately reflect actual machine position

### Bug #2: Missing Timer Restart
**Symptom**: First move worked, second move failed (no motion, position stuck)  
**Root Cause**: OCR callbacks stopped timers (TMR2/3/4) on completion, but motion execution didn't restart them  
**Fix**: Added `TMR2_Start()`, `TMR3_Start()`, `TMR4_Start()` calls in `APP_ExecuteMotionBlock()` for each axis  
**Impact**: Multi-move sequences now work correctly; timers restart for each new move

### Bug #3: Missing Direction Pin Control
**Symptom**: Reverse motion didn't execute (position stayed at target)  
**Root Cause**: Direction GPIO pins (DirX/Y/Z) were never set by motion execution code  
**Fix**: Added direction pin control before OCR enable:
```c
switch (axis) {
    case 0: cnc_axes[0].direction_forward ? DirX_Set() : DirX_Clear(); break;
    case 1: cnc_axes[1].direction_forward ? DirY_Set() : DirY_Clear(); break;
    case 2: cnc_axes[2].direction_forward ? DirZ_Set() : DirZ_Clear(); break;
}
```
**Impact**: Bidirectional motion now fully functional

### Bug #4: Parser Position Updated Too Late
**Symptom**: Multi-block sequences parsed with incorrect positions - UGS sends rapid bulk commands, all parsed before motion starts  
**Root Cause**: `MotionGCodeParser_SetPosition()` called in motion completion handler (after block finishes), but next blocks already parsed with stale (0,0,0) position  
**Fix**: Moved parser position update to immediately after `MotionBuffer_Add()` - each block uses correct expected end position:
```c
if (MotionBuffer_Add(&motion_block)) {
    MotionGCodeParser_SetPosition(motion_block.target_pos[0], 
                                  motion_block.target_pos[1], 
                                  motion_block.target_pos[2]);
}
```
**Impact**: Parser now maintains correct position state for rapid command sequences from UGS

### Bug #5: State Machine Never Triggered
**Symptom**: No `[PLANNING]` or `[EXEC]` debug messages, motion appeared to execute via different path  
**Root Cause**: Adding blocks to buffer didn't trigger state machine transition - remained in IDLE/SERVICE_TASKS  
**Fix**: Added explicit state transition when blocks added:
```c
if (appData.state == APP_STATE_MOTION_IDLE || 
    appData.state == APP_STATE_MOTION_EXECUTING)
{
    appData.state = APP_STATE_MOTION_PLANNING;
}
```
**Impact**: State machine now properly initiates motion execution sequence

### Bug #6: Duplicate Execution Paths with Broken MM-to-Steps Conversion
**Symptom**: X-axis moved only 0.58mm instead of 10mm (moved ~10 steps instead of 2500 steps)  
**Root Cause**: TMR1 @ 1kHz was calling `MotionPlanner_ExecuteBlock()` → `APP_ExecuteMotionBlock()` which had catastrophic bug:
```c
// BROKEN CODE (old):
int32_t target_pos = (int32_t)block->target_pos[i];  // MM cast to int: 10.000 → 10
int32_t current_pos = APP_GetAxisCurrentPosition(i);  // STEPS: 0
if (target_pos != current_pos)  // Compares 10 (MM) vs 0 (STEPS)!
{
    cnc_axes[i].target_position = target_pos;  // Sets target to 10 STEPS instead of 2500!
}
```
**Fix**: Replaced entire `APP_ExecuteMotionBlock()` function to use working MM→steps conversion from `APP_ExecuteNextMotionBlock()`  
**Impact**: X-axis (and all axes) now move correct distances with proper unit conversion

### Bug #7: Dual Execution Paths Causing Simultaneous Axis Activation
**Symptom**: Diagonal motion instead of sequential axis moves - XY moving together instead of X then Y  
**Root Cause**: Two execution paths both pulling from same buffer:
1. State machine (PLANNING state) → `APP_ExecuteNextMotionBlock()` 
2. TMR1 @ 1kHz → `MotionPlanner_UpdateTrajectory()` → `MotionPlanner_ExecuteBlock()` → `APP_ExecuteMotionBlock()`

Both paths active simultaneously caused all buffered blocks to execute at once, activating all axes before any completed.

**Fix**: Disabled TMR1 execution path - made `APP_ExecuteMotionBlock()` a no-op:
```c
bool APP_ExecuteMotionBlock(motion_block_t *block)
{
    /* TMR1 execution path disabled - state machine has exclusive control */
    return true;  // Keep motion planner happy
}
```
**Impact**: Sequential axis motion restored - state machine has exclusive execution control, blocks execute one at a time

### Bug #8: Interpolation Engine Auto-Advance Creating Vector Motion
**Symptom**: Triangle/diagonal motion instead of square - blocks executing as coordinated multi-axis vector moves  
**Root Cause**: Interpolation engine's `update_motion_state()` automatically started next block on completion:
```c
// OLD CODE - caused diagonal motion:
if (!INTERP_PlannerIsBufferEmpty())
{
    INTERP_ExecuteMove(); // Start next block immediately!
}
```
This created continuous vector motion (0,0)→(0,10)→(10,10) became a diagonal path because all blocks fed to interpolation engine at once, which calculated direct vector paths between points.

**Fix**: Disabled auto-advance in interpolation engine, let state machine control block sequencing:
```c
// Block completed - let state machine handle next block
if (interp_context.motion_complete_callback)
{
    interp_context.motion_complete_callback();
}
```
**Impact**: Blocks now execute sequentially as independent moves, not as vectorized continuous path. State machine (APP_STATE_MOTION_EXECUTING) detects completion and transitions to PLANNING for next block.

## Integration Points

### Universal G-code Sender (UGS)
- **Protocol**: GRBL v1.1f compatible responses ("ok", "error", status reports)
- **Real-time commands**: Feed hold (?), cycle start (~), reset (Ctrl-X)
- **Arc support**: Native G2/G3 processing (no linearization required)

### Cross-Platform Build  
- **Windows**: PowerShell-based testing, MPLAB X IDE v6.25
- **Linux**: Make-based build system, XC32 v4.60 compiler
- **Paths**: Auto-detected OS with proper path separators

### Version Control
- **Git workflow**: Use raw git commands (not GitKraken)
- Standard git add/commit/push workflow for version control

## Common Tasks

### Adding New Motion Commands
1. Call `MultiAxis_MoveSingleAxis()` for individual axis moves
2. Call `MultiAxis_MoveCoordinated()` for synchronized multi-axis moves
3. Monitor completion with `MultiAxis_IsBusy()` or per-axis `MultiAxis_IsAxisBusy()`
4. Use hardware abstraction via `APP_GetAxisCurrentPosition()` / `APP_SetAxisCurrentPosition()`

### Debugging Motion Issues
1. Use `motion_debug_analysis.ps1` for real-time monitoring
2. Check OCR interrupt callbacks in `multiaxis_control.c` (OCMP4/1/5/3_StepCounter)
3. Verify S-curve profile calculations with oscilloscope
4. Monitor per-axis `active` flags and `step_count` values
5. Check TMR1 @ 1kHz callback: `TMR1_MultiAxisControl()`

### Hardware Testing
1. **Always** test with limit switches connected
2. Use conservative velocities for initial testing (max_velocity = 5000 steps/sec)
3. Verify OCR period calculations with oscilloscope (expect symmetric S-curve)
4. Test emergency stop functionality first
5. Test single-axis moves before coordinated multi-axis moves

Remember: This is a **safety-critical real-time system**. Always validate motion commands and maintain proper error handling in interrupt contexts.