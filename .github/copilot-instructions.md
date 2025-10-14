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
OC6-OC9              | Timer6/Timer7 (not used)
```

**Timer pairing capability:**
- PIC32MZ timers can operate as 16-bit or be paired for 32-bit mode
- TMR2/TMR3 can combine into one 32-bit timer
- TMR4/TMR5 can combine into one 32-bit timer
- **This project uses separate 16-bit mode** for independent axis control

**Output Compare Module Hardware (Figure 18-1):**

The OCR module uses a dual-compare architecture for pulse generation:

```
Timer → Comparator → Output Logic → OCx Pin
         ↑     ↑
      OCxRS  OCxR
      (16bit)(16bit)
```

**Key hardware components:**
- **OCxRS Register**: Primary compare value (sets pulse width - fixed at 40 counts)
- **OCxR Register**: Secondary compare value (sets pulse period - variable for speed control)
- **Comparator**: Continuously compares timer count against OCxRS and OCxR
- **Output Logic**: Generates pulse edges when timer matches compare values
- **Mode Select**: OCM<2:0> bits configure continuous pulse mode

**Critical timing relationships in continuous pulse mode (Dual Compare Mode):**

The OCx pin operates in a low-to-high and high-to-low sequence:
1. Timer counts from 0 to TMRxPR (period register) then resets to 0x0000
2. **Rising edge**: When Timer = OCxRS → Output goes HIGH
3. **Falling edge**: When Timer = OCxR → Output goes LOW
4. Timer rolls over at TMRxPR and the cycle repeats

**Key timing requirements from PIC32 Reference Manual (Section 16.3.2.4):**
- OCx pin is driven HIGH one PBCLK after compare match with OCxRS
- OCx pin remains HIGH until next match between timer and OCxR
- Pulse generation continues until mode change or module disable
- Timer base will count up to PRy value, then reset to 0x0000

**CRITICAL CONSTRAINTS (Table 16-2 - Special Cases):**
- **PRy ≥ OCxRS and OCxRS ≥ OCxR**: Normal continuous pulse operation
- **OCxRS > PRy and PRy ≥ OCxR**: Only rising edge generated, OCxIF not set (invalid mode)
- **OCxR > PRy**: Unsupported mode; timer resets prior to match condition
- **If OCxRS ≤ PRy**: Must ensure OCxR < OCxRS for falling edge generation
- **MUST maintain**: PRy > (OCxR + OCxRS) for proper edge detection at all times

**CRITICAL: OCR Continuous Pulse Mode Constraints**

For continuous pulse generation in Dual Compare Mode, the timer Period Register (PRy) and compare registers must maintain specific relationships per PIC32 Reference Manual Table 16-2:

```c
// REQUIRED RELATIONSHIP (from PIC32 Reference Manual Section 16.3.2.4):
// Normal operation requires: PRy ≥ OCxRS and OCxRS ≥ OCxR
// 
// Pulse generation sequence:
// 1. When Timer = OCxRS → OCx pin goes HIGH (rising edge)
// 2. When Timer = OCxR → OCx pin goes LOW (falling edge)  
// 3. When Timer = PRy → Timer resets to 0x0000
// 4. Cycle repeats
//
// Therefore: OCxR must be less than OCxRS for falling edge to occur
//           PRy must be greater than OCxRS for proper timer rollover

// IMPLEMENTATION PATTERN:
#define OCMP_PULSE_WIDTH 40  // Constant value for OCxRS register
                             // Sets rising edge at timer count = 40
                             // Just long enough for stepper driver detection

// When updating step rate:
uint32_t period = MotionPlanner_CalculateOCRPeriod(velocity_mm_min);

// CRITICAL: Update TMRxPR and OCRxR together to maintain proper pulse timing
TMRx_PeriodSet(period);                                      // PRy (period register)
OCMPx_CompareSecondaryValueSet(period - OCMP_PULSE_WIDTH);  // OCxR (falling edge position)
OCMPx_CompareValueSet(OCMP_PULSE_WIDTH);                    // OCxRS = 40 (rising edge - constant)

// This maintains: PRy > OCxRS > OCxR
// Where: PRy = period
//        OCxRS = 40 (rising edge at count 40)
//        OCxR = period - 40 (falling edge before rollover)
```

**Velocity range constraints:**
- **Timer configuration**: TMR2/3/4/5 configured as 16-bit timers (not 32-bit mode)
  - Note: PIC32MZ timers can be paired (e.g., TMR2/TMR3) for 32-bit operation, but we use separate 16-bit mode
- **Slowest speed**: TMRxPR must not exceed maximum 16-bit value minus safety margin
  - Maximum period = `0xFFFF - OCMP_PULSE_WIDTH - 10`
  - Maximum period = `65535 - 40 - 10 = 65485` counts
  - The -10 count safety margin ensures rising/falling edges are properly detected
  - OCRxR calculation at slowest speed: `TMRxPR - OCMP_PULSE_WIDTH`
  - This guarantees edge detection: TMRxPR > (OCRxR + OCRxRS) by at least 10 counts

**Why this matters:**
- OCxRS sets the **rising edge position** (fixed at 40 counts - when output goes HIGH)
- OCxR sets the **falling edge position** (variable - when output goes LOW before period rollover)
- **TMRxPR and OCxR must be updated together** - they track each other for proper pulse period adjustment
- The relationship PRy > OCxRS > OCxR ensures both edges occur within each timer cycle
- Violating these constraints causes pulse generation failure (see Table 16-2 Special Cases)
- Always calculate OCxR as `TMRxPR - constant_offset` to maintain proper timing
- The 10-count safety margin ensures reliable edge detection at slowest speeds

**Note on startup timing:**
- There may be scan penalties when starting TMR and OCR modules simultaneously
- Investigation ongoing for startup lag behavior between timer and compare modules
- Current implementation prioritizes correct pulse generation over startup optimization

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