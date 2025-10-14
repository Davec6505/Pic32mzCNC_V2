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

// Timer sources (selected from available options):
// TMR1 → 1kHz motion control timing (MotionPlanner_UpdateTrajectory)
// TMR2 → OCMP1 time base for Y-axis step pulse generation
// TMR3 → OCMP4 time base for X-axis step pulse generation
// TMR4 → OCMP5 time base for Z-axis step pulse generation
// TMR5 → OCMP3 time base for A-axis step pulse generation
```

**PIC32MZ Timer Source Options (Table 18-1):**
```
Output Compare Module | Available Timer Sources
--------------------------------------------------
OC1 (Y-axis)         | Timer2 or Timer3
OC2                  | Timer4 or Timer5
OC3 (A-axis)         | Timer4 or Timer5
OC4 (X-axis)         | Timer2 or Timer3
OC5 (Z-axis)         | Timer2 or Timer3
```

**OCR Dual-Compare Architecture:**
- **OCxRS Register**: Primary compare (pulse width = 40 counts fixed)
- **OCxR Register**: Secondary compare (pulse period = variable for speed)
- **TMRxPR Register**: Timer period (must satisfy: TMRxPR > OCxRS > OCxR)
- **Constraint**: Maximum period = 65485 counts (16-bit timer limit minus safety margin)

**Critical timing pattern:**
```c
#define OCMP_PULSE_WIDTH 40  // DRV8825 requires min 1.9µs, we use ~40 timer counts

uint32_t period = MotionPlanner_CalculateOCRPeriod(velocity_mm_min);
TMRx_PeriodSet(period);                                      // Set timer rollover
OCMPx_CompareSecondaryValueSet(period - OCMP_PULSE_WIDTH);  // Falling edge position
OCMPx_CompareValueSet(OCMP_PULSE_WIDTH);                    // Rising edge position (constant)
```

### DRV8825 Stepper Driver Interface
**Hardware**: Pololu DRV8825 carrier boards (or compatible) for bipolar stepper motors

**Control signals** (microcontroller → driver):
- **STEP**: Pulse input - each rising edge = one microstep (pulled low by 100kΩ)
- **DIR**: Direction control - HIGH/LOW sets rotation direction (pulled low by 100kΩ)
- **ENABLE**: Active-low enable (can leave disconnected for always-enabled)
- **RESET/SLEEP**: Pulled high by 1MΩ/10kΩ respectively (normal operation)

**Timing requirements** (DRV8825 datasheet):
- **Minimum STEP pulse width**: 1.9µs HIGH + 1.9µs LOW
- **Our implementation**: 40 timer counts ≈ safe margin above minimum
- **Why 40 counts**: Ensures reliable detection across all microstepping modes

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