# General System - Complete Reference

**Last Updated**: October 26, 2025  
**Status**: Consolidated documentation (replaces 25+ individual files)

---

## Table of Contents

1. [Build System & Configuration](#build-system--configuration)
2. [Debug System & Levels](#debug-system--levels)
3. [Hardware Configuration](#hardware-configuration)
4. [Coding Standards & Patterns](#coding-standards--patterns)
5. [Testing & Validation](#testing--validation)
6. [Critical Timing Systems](#critical-timing-systems)
7. [Function Pointer Dispatch](#function-pointer-dispatch)
8. [Architecture Overview](#architecture-overview)

---

## Build System & Configuration

**Files**: `Makefile`, `srcs/Makefile`

### Multi-Configuration Build System

**October 24, 2025 Update**: Eliminated Default config, kept Debug and Release only.

**Configurations**:
```bash
# Release (default): -g -O1 (fast builds, debuggable)
make                               # Incremental Release build
make all                           # Full Release rebuild
make OPT_LEVEL=2                   # Release with -O2
make OPT_LEVEL=3                   # Release with -O3

# Debug: -g3 -O0 -DDEBUG -DDEBUG_MOTION_BUFFER
make BUILD_CONFIG=Debug            # Incremental Debug build
make all BUILD_CONFIG=Debug        # Full Debug rebuild

# Debug levels (Debug config only)
make all BUILD_CONFIG=Debug DEBUG_MOTION_BUFFER=0  # No motion debug
make all BUILD_CONFIG=Debug DEBUG_MOTION_BUFFER=3  # Planner debug (recommended)
make all BUILD_CONFIG=Debug DEBUG_MOTION_BUFFER=7  # Full debug (floods serial!)
```

### Directory Structure

```
bins/{Debug,Release}/              # Executables (both configs kept)
libs/{BUILD_CONFIG}/               # Libraries (current config only)
libs/*.c                           # Source files for library compilation
objs/{BUILD_CONFIG}/               # Object files (current config only)
other/{BUILD_CONFIG}/              # Map files (current config only)
```

### Configuration Flags

**Release** (default):
- `-g` - Debug symbols for profiling
- `-O$(OPT_LEVEL)` - Optimization (default: 1, override: 2 or 3)
- Balanced for debugging + performance

**Debug**:
- `-g3` - Maximum debug symbols
- `-O0` - No optimization
- `-DDEBUG` - Debug assertions enabled
- `-DDEBUG_MOTION_BUFFER` - Motion logging enabled

### Build Targets

```bash
make                    # Incremental build (only changed files)
make all                # Full rebuild (clean + build)
make build_dir          # Create directory structure
make clean              # Clean current configuration
make clean_all          # Clean both Debug and Release
make help               # Show color-coded help (platform-specific)
make quiet              # Filtered build output (errors/warnings only)
```

### Shared Library System

**Purpose**: Modular compilation for faster builds.

**Usage**:
```bash
# Build library from libs/*.c
make shared_lib

# Link against pre-built library
make all USE_SHARED_LIB=1
```

**Benefits**:
- Only files in `libs/` become library
- Main code links against pre-built `.a` file
- Faster incremental builds
- Flexible workflow (direct or library)

**Current Example**: `libs/test_ocr_direct.c` (OCR hardware testing)

### Critical Makefile Architecture

**⚠️ TWO-TIER DESIGN - DO NOT VIOLATE!**

**Root Makefile** (project root):
- Controls ALL build parameters
- Passes parameters to `srcs/Makefile`
- **This is where configuration changes belong**

**Sub Makefile** (`srcs/Makefile`):
- Receives parameters from root
- Implements build logic
- **Should NOT define new config variables**

**User's Design Methodology**:
- Root controls policy (what options available)
- Sub implements policy (how options compiled)
- Respect this architecture!

---

## Debug System & Levels

**Added**: October 25, 2025

### 8-Level Hierarchical Debug System

**Problem Solved**: Previous system flooded serial with 100Hz ISR messages.

**Solution**: Selective verbosity tiers.

```c
// motion_types.h - Debug Level Definitions
#define DEBUG_LEVEL_NONE     0  // Production (no debug output)
#define DEBUG_LEVEL_CRITICAL 1  // <1 msg/sec  (buffer overflow, fatal errors)
#define DEBUG_LEVEL_PARSE    2  // ~10 msg/sec (command parsing)
#define DEBUG_LEVEL_PLANNER  3  // ~10 msg/sec (motion planning) ⭐ RECOMMENDED
#define DEBUG_LEVEL_STEPPER  4  // ~10 msg/sec (state machine transitions)
#define DEBUG_LEVEL_SEGMENT  5  // ~50 msg/sec (segment execution)
#define DEBUG_LEVEL_VERBOSE  6  // ~100 msg/sec (high-frequency events)
#define DEBUG_LEVEL_ALL      7  // >1000 msg/sec (CAUTION: floods serial!)
```

### Build Commands

```bash
make all BUILD_CONFIG=Debug DEBUG_MOTION_BUFFER=3  # Level 3 (RECOMMENDED)
make all BUILD_CONFIG=Debug DEBUG_MOTION_BUFFER=4  # Add stepper debug
make all BUILD_CONFIG=Debug DEBUG_MOTION_BUFFER=0  # Production (no debug)
```

### Debug Output Examples

**Level 3 (PLANNER) - Recommended**:
```
[PARSE] 'G1 X10 Y20 F1000'
[PLAN] pl.pos=(0.000,0.000) tgt=(10.000,20.000) delta=(800,1600) steps=(800,1600)
[JUNC] prev→curr angle=90.0° v_junction=500.0 mm/min
[GRBL] Block added: 16mm, entry=0.0, exit=500.0 mm/min
ok
```

**Level 7 (ALL) - Serial Flooding**:
```
[TMR1] ISR tick=12345 axis=X velocity=1000.0 steps/sec
[OCR4] Period=1563 counts (1ms)
[STEP] X step_count=123
[TMR1] ISR tick=12346 axis=X velocity=1050.0 steps/sec
[OCR4] Period=1488 counts (0.95ms)
... (1000+ messages per second!)
```

### Current Testing Configuration

**Active Level**: 3 (PLANNER)
**Output**: `[PARSE]`, `[PLAN]`, `[JUNC]`, `[GRBL]` messages
**Result**: Clean, informative output without serial flooding

### Debug Guards (main.c lines 305-373)

**Purpose**: Catch motion state machine bugs during development.

**Guard A**: "Busy but no queues" (walkabout protection):
```c
#ifdef DEBUG_MOTION_BUFFER
if ((plan_cnt == 0) && (seg_cnt == 0) && axisBusy) {
    if (!guard_busy_no_queue_active) {
        guard_busy_no_queue_start = _CP0_GET_COUNT();
        guard_busy_no_queue_active = true;
    } else {
        uint32_t elapsed = _CP0_GET_COUNT() - guard_busy_no_queue_start;
        if (elapsed >= GUARD_TICKS) {
            UGS_Printf("GUARD A: No queues but still busy >2s, stopping\r\n");
            MultiAxis_StopAll();
            guard_busy_no_queue_active = false;
        }
    }
}
#endif
```

**Guard B**: "Planner empty but stepper busy" (drain timeout):
```c
#ifdef DEBUG_MOTION_BUFFER
if ((plan_cnt == 0) && stepperBusy && axisBusy) {
    if (!guard_plan_empty_stepper_busy_active) {
        guard_plan_empty_stepper_busy_start = _CP0_GET_COUNT();
        guard_plan_empty_stepper_busy_active = true;
    } else {
        uint32_t elapsed = _CP0_GET_COUNT() - guard_plan_empty_stepper_busy_start;
        if (elapsed >= GUARD_TICKS) {
            UGS_Printf("GUARD B: Planner empty but stepper busy >2s\r\n");
            MultiAxis_StopAll();
            guard_plan_empty_stepper_busy_active = false;
        }
    }
}
#endif
```

**Guard Timing**:
```c
#define GUARD_TICKS 200000000U  // 2 seconds @ 100MHz CORETIMER
```

**Why Debug-Only**:
- Development: Catch bugs early (walkabout, stepper drain timeout)
- Production: Remove overhead (bugs should be fixed by then)
- Philosophy: Debug builds find bugs, release builds deploy fixes

---

## Hardware Configuration

### PIC32MZ2048EFH100 Specifications

**Processor**:
- Core: MIPS M5150 @ 200MHz
- RAM: 512KB
- Flash: 2MB
- Peripherals: 6 OCR modules, 9 timers, 6 UARTs, DMA, etc.

**Memory Configuration**:
- Heap: 20KB (Makefile)
- Stack: 20KB (Makefile)
- Flash: 2MB total

### Clock Configuration (MCC Harmony)

**System Clocks**:
- CPU: 200MHz (from PLL)
- PBCLK1 (Peripheral Bus): 100MHz
- PBCLK2 (Peripheral Bus): 100MHz
- PBCLK3 (Timers): 50MHz
- PBCLK5 (Peripheral Bus): 100MHz

**CORETIMER**:
- Frequency: 100MHz (CPU ÷ 2)
- Used for: Guard timeouts, general delays
- **NOT 32kHz** (common misconception!)

### Timer Configuration (VERIFIED - October 18, 2025)

**Motion Control Timers** (TMR2/3/4/5):
```
PBCLK3: 50MHz
Prescaler: 1:32
Timer Clock: 50MHz ÷ 32 = 1.5625MHz
Resolution: 640ns per count
16-bit range: 0-65535 counts (42ms max period)
```

**Code Definition**:
```c
#define TMR_CLOCK_HZ 1562500UL  // 1.5625MHz
```

**Pulse Width**: 40 counts = 25.6µs (DRV8825 minimum: 1.9µs) ✅

**Step Rate Range**:
```
Min: 23.8 steps/sec (period = 65,535 counts)
Max: 31,250 steps/sec (period = 50 counts)
```

### OCR Assignments

```
OCR Module | Axis | Timer Source | Step Pin | Dir Pin | Enable Pin
------------------------------------------------------------------
OCMP1      | Y    | TMR4         | RD0      | RG9     | RG6
OCMP3      | A    | TMR5         | RD2      | RD10    | RD11
OCMP4      | X    | TMR2         | RD3      | RF0     | RF1
OCMP5      | Z    | TMR3         | RD4      | RD9     | RD5
```

### DRV8825 Stepper Driver Configuration

**Control Signals**:
- **STEP**: Pulse input (rising edge = one microstep)
- **DIR**: Direction (HIGH/LOW sets rotation)
- **ENABLE**: Active-low enable (LOW = motor energized)
- **RESET/SLEEP**: Pulled high (normal operation)

**Timing Requirements**:
- Minimum STEP pulse width: 1.9µs
- Our implementation: 25.6µs (13.5x safety margin)
- Direction setup time: 200ns minimum

**Microstepping** (MODE0/1/2 pins):
```
MODE2  MODE1  MODE0  Resolution
--------------------------------
 Low    Low    Low   Full step
High    Low    Low   Half step
 Low   High    Low   1/4 step
High   High    Low   1/8 step
 Low    Low   High   1/16 step ← DEFAULT
High    Low   High   1/32 step
```

**Current Limit**:
- Formula: `Current = VREF × 2`
- Set via VREF potentiometer on carrier board

### Pin Assignments

**Stepper Motors**:
- X-axis: RD3 (step), RF0 (dir), RF1 (enable)
- Y-axis: RD0 (step), RG9 (dir), RG6 (enable)
- Z-axis: RD4 (step), RD9 (dir), RD5 (enable)
- A-axis: RD2 (step), RD10 (dir), RD11 (enable)

**User Interface**:
- LED1: RE4 (heartbeat @ 1Hz when idle)
- LED2: RE5 (power-on indicator)
- SW1: RB12 (removed from app layer)
- SW2: RB13 (removed from app layer)

**Serial Communication**:
- UART2 TX: RG0
- UART2 RX: RG1
- Baud: 115200, 8N1

**Limit Switches** (active-low):
- X-: RA7, X+: RA9
- Y-: RA10, Y+: RA14
- Z-: RA15, Z+: (TBD)

---

## Coding Standards & Patterns

**Full Documentation**: `docs/CODING_STANDARDS.md`

### File-Level Variable Declaration Rule

**⚠️ MANDATORY**: ALL file-level variables (static or non-static) MUST be declared at top of each file under this comment:

```c
// *****************************************************************************
// Local Variables
// *****************************************************************************
```

**Rationale**:
- ISR functions need access to file-scope variables
- Variables must be declared BEFORE ISR definitions
- Improves readability and maintainability

**Example (CORRECT)**:
```c
// multiaxis_control.c

#include "definitions.h"

// *****************************************************************************
// Local Variables
// *****************************************************************************

// Coordinated move state (accessed by TMR1 ISR @ 1kHz)
static motion_coordinated_move_t coord_move;

// Per-axis state (accessed by TMR1 ISR @ 1kHz)  
static volatile scurve_state_t axis_state[NUM_AXES];

// *****************************************************************************
// Function Implementations
// *****************************************************************************

static void TMR1_MultiAxisControl(uint32_t status, uintptr_t context)
{
    // ✅ Can access coord_move and axis_state here!
}
```

**Example (WRONG)**:
```c
// ❌ Variable declared in middle of file

static void TMR1_MultiAxisControl(uint32_t status, uintptr_t context)
{
    // ❌ COMPILE ERROR: coord_move not yet declared!
    axis_id_t dominant = coord_move.dominant_axis;
}

// ❌ Too late! ISR above already tried to use it
static motion_coordinated_move_t coord_move;
```

### ISR Safety Checklist

- [ ] All ISR-accessed variables declared at file scope (top of file)
- [ ] Use `volatile` qualifier for ISR-shared state
- [ ] Document which ISR accesses each variable
- [ ] Keep ISR code minimal (no printf, no malloc, no complex operations)
- [ ] Use atomic operations for multi-byte updates

### MISRA C:2012 Compliance

**Compliance Level**: All mandatory rules

**Key Rules Followed**:
- Rule 8.4: Function/variable declarations
- Rule 10.x: Type conversions
- Rule 12.x: Expressions
- Rule 17.7: Function return values used
- Rule 21.x: Standard library usage

**Documented Deviations**:
```c
// Deviation: MISRA C:2012 Rule 17.7 (snprintf return value)
// Justification: Buffer size pre-calculated, overflow impossible
(void)snprintf(buffer, sizeof(buffer), "...", ...);
```

### XC32 Optimization Patterns

**Memory Usage** (gcode_parser.c example):
- Modal state: ~166 bytes
- Stack usage: Minimal via careful design
- Flash placement: Optimal (no special attributes needed)
- Total RAM: 553 bytes (including buffers)

**Compiler Hints**:
```c
// Force inline (zero overhead)
static inline bool __attribute__((always_inline)) IsDominantAxis(axis_id_t axis)
{
    return (segment_completed_by_axis & (1U << axis)) != 0U;
}

// Interrupt service routine
void __ISR(_TIMER_1_VECTOR, IPL7SRS) TMR1_Handler(void)
{
    // ...
}
```

---

## Testing & Validation

### PowerShell Testing Scripts

**Location**: `ps_commands/` folder

**Basic Motion Test**:
```powershell
.\motion_test.ps1 -Port COM4 -BaudRate 115200
```

**UGS Compatibility Test**:
```powershell
.\ugs_test.ps1 -Port COM4 -GCodeFile modular_test.gcode
```

**Circle Debug Test** (October 25, 2025):
```powershell
.\test_circle_debug.ps1
```

**Direct Serial Test**:
```powershell
.\test_direct_serial.ps1 -Port COM4
```

### Test G-Code Files

**Location**: `tests/` folder

**Linear Motion Tests**:
- `01_rectangle.gcode` - Simple rectangle path
- `02_diagonal_moves.gcode` - Multi-axis coordination

**Arc Motion Tests**:
- `03_circle_20segments.gcode` - Baseline (linear approximation)
- `04_arc_test.gcode` - G2/G3 interpolation tests

**Combined Tests**:
- `modular_test.gcode` - Mixed linear + arc moves

### Hardware Validation Results

**October 25, 2025 - Production Ready** ✅:
```
✅ UGS connects as "GRBL 1.1f"
✅ First circle completes successfully
✅ Second circle run doesn't hang
✅ Graphics show correct interpolation
✅ Planner shows correct calculations
✅ Infinite retry loop fixed
✅ Zero-length moves handled correctly
```

**Position Accuracy**:
- ✅ Rectangle path returns to (0.000, 0.000)
- ✅ Step accuracy: ±2 steps (±0.025mm)
- ✅ Closed shapes close exactly

### Known Issues

**⚠️ Subordinate Axis Pulses Not Running (October 26, 2025)**:
- **Symptom**: Dominant axis moves, subordinates don't
- **Evidence**: UGS graphics interpolate correctly, planner calculates correctly
- **Status**: Active investigation
- **Next Steps**: Debug Bresenham pulse generation

---

## Critical Timing Systems

### CORETIMER (100MHz)

**Configuration**:
```c
// Frequency: CPU ÷ 2 = 200MHz ÷ 2 = 100MHz
#define CORETIMER_FREQ 100000000UL

// Read current count
uint32_t now = _CP0_GET_COUNT();
```

**Uses**:
- Debug guard timeouts (2 seconds = 200,000,000 counts)
- General delays
- Precise timing measurements

**NOT Used For**:
- Motion control (use TMR1-5)
- Step pulse generation (use OCR modules)

### TMR1 (Motion Coordinator @ 1kHz)

**Configuration**:
```
Frequency: 1kHz (1ms period)
Priority: IPL7 (highest for motion)
Purpose: S-curve velocity updates, coordination
```

**Callback**: `TMR1_MultiAxisControl()`

**Responsibilities**:
- Update dominant axis S-curve velocity
- Calculate subordinate axis scaling
- Update OCR periods for all active axes
- Check motion completion

### TMR9 (Segment Preparation @ 200Hz)

**Configuration**:
```
Frequency: 200Hz (5ms period)
Priority: IPL5
Purpose: Prepare segments from planner blocks
```

**Callback**: `MotionManager_TMR9Callback()`

**Responsibilities**:
- Check if segment buffer has space
- Get next block from planner
- Calculate segment step counts
- Initialize Bresenham counters
- Add segment to buffer

### Motion Timers (TMR2/3/4/5 @ 1.5625MHz)

**Configuration**:
```
Frequency: 1.5625MHz (640ns resolution)
Prescaler: 1:32
Purpose: OCR time base for step pulse generation
```

**One timer per axis**:
- TMR2 → OCMP4 (X-axis)
- TMR3 → OCMP5 (Z-axis)
- TMR4 → OCMP1 (Y-axis)
- TMR5 → OCMP3 (A-axis)

---

## Function Pointer Dispatch

**Pattern**: Hardware abstraction for multi-axis control.

### Axis Hardware Table

```c
typedef struct {
    void (*TMR_Start)(void);
    void (*TMR_Stop)(void);
    void (*TMR_PeriodSet)(uint16_t period);
    
    void (*OCMP_Enable)(void);
    void (*OCMP_Disable)(void);
    void (*OCMP_CompareValueSet)(uint16_t value);
    void (*OCMP_CompareSecondaryValueSet)(uint16_t value);
    
    volatile uint16_t *TMRx_reg;  // Direct register access for rollover
    
} axis_hardware_t;
```

### Initialization

```c
static const axis_hardware_t axis_hw[NUM_AXES] = {
    [AXIS_X] = {
        .TMR_Start = TMR2_Start,
        .TMR_Stop = TMR2_Stop,
        .TMR_PeriodSet = TMR2_PeriodSet,
        .OCMP_Enable = OCMP4_Enable,
        .OCMP_Disable = OCMP4_Disable,
        .OCMP_CompareValueSet = OCMP4_CompareValueSet,
        .OCMP_CompareSecondaryValueSet = OCMP4_CompareSecondaryValueSet,
        .TMRx_reg = &TMR2,  // Direct register access
    },
    // ... Y, Z, A axes
};
```

### Usage Pattern

```c
// Instead of hardcoding hardware:
TMR2_Start();
OCMP4_Enable();

// Use function pointer dispatch:
axis_hw[axis].TMR_Start();
axis_hw[axis].OCMP_Enable();

// Direct register write (when needed):
*axis_hw[axis].TMRx_reg = 0xFFFF;  // Force immediate rollover
```

### Benefits

- ✅ Generic axis loops (no switch statements)
- ✅ Easy to add more axes (just extend table)
- ✅ Loosely coupled to MCC PLIB (read-only dependency)
- ✅ Zero overhead (function pointers resolved at compile time)

---

## Architecture Overview

### Data Flow (Serial → Motion)

```
UART2 RX @ 115200 baud
    ↓
Serial Ring Buffer (512 bytes)
    ↓
Command Buffer (8 commands × 256 bytes)
    ↓
G-Code Parser (modal state, 13 groups)
    ↓
parsed_move_t (mm, feedrate, axis words)
    ↓
GRBL Planner (16-block buffer, look-ahead)
    ↓
motion_block_t (steps, entry/exit velocities)
    ↓
Segment Preparation (TMR9 @ 200Hz)
    ↓
segment_t (Bresenham, direction bits)
    ↓
Segment Execution (main loop)
    ↓
Multi-Axis Control (TMR1 @ 1kHz)
    ↓
OCR Pulse Generation (hardware)
    ↓
DRV8825 Stepper Drivers
    ↓
Physical Motion
```

### Module Hierarchy

```
main.c
├── ugs_interface.c (serial protocol)
├── gcode_parser.c (GRBL v1.1f compliance)
├── command_buffer.c (line separation)
└── motion/
    ├── motion_buffer.c (arc generator)
    ├── grbl_planner.c (look-ahead planning)
    ├── grbl_stepper.c (segment preparation)
    ├── motion_manager.c (TMR9 coordinator)
    ├── multiaxis_control.c (TMR1 S-curves)
    └── motion_math.c (kinematics, settings)
```

### Critical Design Principles

⚠️ **Time-based interpolation** - NOT Bresenham step counting  
⚠️ **Hardware pulse generation** - OCR modules, no software step interrupts  
⚠️ **Coordinated motion** - All axes synchronized to dominant axis TIME  
⚠️ **Per-axis limits** - Each axis has independent velocity/accel/jerk  
⚠️ **Centralized settings** - motion_math is single source of truth  
⚠️ **Centralized types** - motion_types.h prevents duplicate definitions  
⚠️ **Ring buffer architecture** - Motion buffer bridges parser and execution  
⚠️ **Non-blocking ISR** - Arc generator state machine, not for-loop  

### Phase Development Roadmap

**Phase 1 (Complete)**: Basic infrastructure
- ✅ Multi-axis S-curve control
- ✅ OCR hardware integration
- ✅ Serial communication

**Phase 2A (Complete)**: G-code parsing
- ✅ GRBL v1.1f parser
- ✅ Modal state tracking
- ✅ UGS compatibility

**Phase 2B (Complete)**: Segment execution
- ✅ GRBL planner port
- ✅ Segment buffer
- ✅ Bresenham subordinate axes

**Phase 3 (Complete)**: Arc interpolation
- ✅ G2/G3 arc support
- ✅ TMR1 ISR state machine
- ✅ Flow control system

**Phase 4 (Future)**: Advanced features
- ⏳ Full circle arcs
- ⏳ G18/G19 planes
- ⏳ R-format arcs
- ⏳ Probing (G38.x)
- ⏳ Spindle/coolant GPIO

---

## File Organization

### Source Files

```
srcs/
├── main.c (517 lines)               - Entry point, motion execution
├── app.c (287 lines)                - System management, LED status
├── command_buffer.c (279 lines)     - Command separation
├── gcode_parser.c (1354 lines)      - GRBL v1.1f parser
├── ugs_interface.c                  - UGS serial protocol
└── motion/
    ├── multiaxis_control.c (1169 lines)  - S-curve interpolation
    ├── motion_math.c (733 lines)         - Kinematics, settings
    ├── motion_buffer.c (426 lines)       - Arc generator
    ├── grbl_planner.c                    - Look-ahead planning
    ├── grbl_stepper.c                    - Segment preparation
    └── motion_manager.c                  - TMR9 coordinator
```

### Header Files

```
incs/
├── command_buffer.h (183 lines)
├── gcode_parser.h (357 lines)
├── ugs_interface.h
└── motion/
    ├── motion_types.h (235 lines)        - Centralized type definitions
    ├── motion_buffer.h (207 lines)
    ├── multiaxis_control.h
    └── motion_math.h (398 lines)
```

### Documentation (Consolidated)

```
docs/
├── GCODE_AND_PARSING.md         - This file (replaces 15+ files)
├── LINEAR_MOTION.md             - Motion control (replaces 20+ files)
├── ARC_MOTION.md                - Arc interpolation (replaces 10+ files)
└── GENERAL_SYSTEM.md            - Build/debug/hardware (replaces 25+ files)
```

**Old Documentation** (70+ files - TO BE ARCHIVED):
- Individual session notes (SESSION_OCT25, EVENING_SESSION, etc.)
- Per-fix documentation (DEADLOCK_FIX, CONSECUTIVE_ARC_FIX, etc.)
- Architecture duplicates (PHASE2A, PHASE2B, PHASE3, etc.)
- Build system variants (BUILD_SYSTEM_COMPLETE, BUILD_SYSTEM_UPDATE, etc.)

---

## Future Development

### Immediate Priorities (October 26, 2025)

1. **⚡ LED1 Heartbeat Fix**:
   - User concern: LED stops during arc execution
   - Proposed: CORETIMER-based timing instead of loop iteration
   - Status: Needs implementation

2. **⚠️ Subordinate Axis Investigation**:
   - Symptom: Graphics interpolate, but no physical motion
   - Debug: Bresenham pulse generation
   - Verify: OCR enable/disable sequence

### Feature Enhancements

1. **Full Circle Arcs**: Single G2/G3 for 360°
2. **G18/G19 Planes**: XZ and YZ plane support
3. **R-Format Arcs**: Radius-based specification
4. **Helical Interpolation**: Arc + linear Z
5. **Probing Hardware**: G38.x probe input
6. **Spindle PWM**: M3/M4 GPIO output
7. **Coolant GPIO**: M7/M8/M9 output
8. **EEPROM Settings**: Persistent $xxx storage
9. **Homing Sequences**: Automatic G28/G30

### Performance Optimizations

1. **Adaptive Feedrate**: Slow for small segments
2. **Dynamic Junction Deviation**: Corner optimization
3. **Input Shaping**: Vibration damping
4. **Backlash Compensation**: Per-axis tables
5. **DMA Serial Reception**: Reduce CPU overhead

---

**End of General System Reference**
