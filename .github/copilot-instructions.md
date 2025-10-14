# PIC32MZ CNC Motion Controller V2 - AI Coding Guide

## Architecture Overview

This is a **modular embedded CNC controller** for the PIC32MZ2048EFH100 microcontroller implementing GRBL v1.1f protocol with advanced S-curve motion profiles. The system was recently refactored (2024) from a monolithic to a clean modular architecture.

### Core Architecture Pattern
```
TMR1 (1kHz) → MotionPlanner_UpdateTrajectory() 
           ↓
      Motion Planner (motion_planner.c)
      ├── Motion Buffer (motion_buffer.c) - 16-cmd circular buffer
      ├── G-code Parser (motion_gcode_parser.c) - G0/G1/G2/G3 parsing  
      └── Hardware Layer (app.c) - OCR interrupts & GPIO
```

### Critical Data Flow
- **Real-time control**: 1kHz TMR1 timer drives `MotionPlanner_UpdateTrajectory()`
- **Step generation**: OCRx hardware modules (OCMP1/3/4/5) with period calculations
- **Position feedback**: OCR interrupt callbacks update position via `MotionPlanner_UpdateAxisPosition()`
- **Safety**: Hard limits via GPIO interrupts, soft limits via pre-move validation

## Key Files & Responsibilities

| File | Purpose | Critical Patterns |
|------|---------|------------------|
| `srcs/main.c` | Entry point, TMR1 callback setup | Always call `APP_Initialize()` before `TMR1_Start()` |
| `srcs/app.c` | Hardware abstraction, OCR/GPIO management | Use getter/setter pattern for hardware access |
| `srcs/motion_planner.c` | Real-time trajectory calculation | S-curve profiles, 1kHz update frequency via TMR1 |
| `srcs/motion_buffer.c` | 16-block look-ahead buffer | Thread-safe circular buffer operations |
| `srcs/motion_gcode_parser.c` | G-code command parsing | Parse G0/G1/G2/G3 into `motion_block_t` |
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
**NEW (2024)**: Clean module boundaries with encapsulated state:

```c
// Motion Buffer API - Thread-safe circular buffer
bool MotionBuffer_Add(motion_block_t *block);
motion_block_t *MotionBuffer_GetNext(void);
void MotionBuffer_Complete(void);

// Motion Planner API - Real-time control
void MotionPlanner_UpdateTrajectory(void);  // Called at 1kHz
uint32_t MotionPlanner_CalculateOCRPeriod(float velocity_mm_min);
void MotionPlanner_UpdateAxisPosition(uint8_t axis, int32_t position);
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
1. Parse in `motion_gcode_parser.c` → create `motion_block_t`
2. Add to buffer via `MotionBuffer_Add()`  
3. Process in `MotionPlanner_ProcessBuffer()`
4. Execute via `MotionPlanner_ExecuteBlock()`

### Debugging Motion Issues
1. Use `motion_debug_analysis.ps1` for real-time monitoring
2. Check OCR interrupt callbacks in `app.c`
3. Verify motion buffer status with `MotionBuffer_GetStatus()`
4. Monitor S-curve profile calculations

### Hardware Testing
1. **Always** test with limit switches connected
2. Use conservative velocities for initial testing (`DEFAULT_MAX_VELOCITY = 50.0f`)  
3. Verify OCR period calculations with oscilloscope
4. Test emergency stop functionality first

Remember: This is a **safety-critical real-time system**. Always validate motion commands and maintain proper error handling in interrupt contexts.