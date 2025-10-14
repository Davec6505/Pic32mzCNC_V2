# PIC32MZ CNC Motion Controller V2

## Overview

This is a professional-grade CNC motion control system implemented on the **PIC32MZ2048EFH100** microcontroller. The system provides GRBL v1.1f compatible motion control with advanced features including S-curve acceleration profiles, 16-block look-ahead motion planning, G2/G3 arc interpolation, and comprehensive safety systems.

**🆕 NEW: Modular Architecture** - Recently restructured with a clean modular design for improved maintainability and code organization while preserving all real-time performance characteristics.

## Key Features

### 🚀 **Advanced Motion Control**
- **S-Curve Acceleration Profiles**: 7-segment jerk-limited motion with smooth velocity transitions
- **Look-Ahead Motion Planning**: GRBL-style 16-block buffer with junction speed optimization
- **Arc Interpolation**: Complete G2/G3 circular interpolation with I,J,K offset and R radius formats
- **Multi-Axis Coordination**: Synchronized 3-axis (X,Y,Z) motion with optional 4th axis support
- **Hardware Pulse Generation**: OCRx continuous pulse mode for precise step timing
- **Real-time Trajectory Control**: 1kHz Core Timer updates with OCR period calculations

### 🛡️ **Comprehensive Safety Systems**
- **Hard Limit Switches**: Real-time GPIO monitoring with immediate axis stopping
- **Soft Limit Checking**: Preventive boundary validation before motion execution
- **Emergency Stop**: Instant motion halt with proper system recovery
- **Individual Axis Control**: Independent axis stopping for selective motion control
- **Position Feedback**: Closed-loop monitoring via OCR interrupt callbacks

### 🔧 **GRBL v1.1f Compatibility**
- **Universal G-code Sender (UGS) Support**: Full compatibility with popular CNC software
- **Standard G-code Commands**: G0/G1 linear moves, G2/G3 arc moves
- **Motion Settings**: Complete $100-$132 parameter support
- **Real-time Control**: Feed hold, cycle start, and override functions

## Hardware Architecture

### Microcontroller: PIC32MZ2048EFH100
- **Core**: 32-bit MIPS M5150 at 200MHz
- **Memory**: 2MB Flash, 512KB RAM
- **Package**: 100-pin TQFP
- **Development Environment**: MPLAB X IDE v6.25 + XC32 v4.60

### Hardware Pulse Generation
```
OCRx Module Assignments (VERIFIED per MCC - October 2025):
├── OCMP1 → Y-axis step pulses (TMR4 time base)
├── OCMP4 → X-axis step pulses (TMR2 time base)
└── OCMP5 → Z-axis step pulses (TMR3 time base)

Timer Sources:
├── TMR2 → OCMP4 time base (X-axis)
├── TMR3 → OCMP5 time base (Z-axis)
├── TMR4 → OCMP1 time base (Y-axis)
├── TMR5 → OCMP3 time base (A-axis, future)
└── TMR1 → 1kHz motion control timing
```

### OCR Dual-Compare Mode (PRODUCTION-PROVEN PATTERN)

The system uses OCR dual-compare mode to generate precise step pulses for DRV8825 stepper drivers:

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
 * 
 * Example with period=300:
 *   TMR2PR = 300          // Timer rolls over at 300
 *   OC4R = 260            // Pulse rises at count 260 (300-40)
 *   OC4RS = 40            // Pulse falls at count 40
 *   Result: Pin HIGH from count 40 to 260, LOW from 260 to 300, then repeat
 *   Effective pulse width = 40 counts (meets DRV8825 1.9µs minimum)
 */
TMRx_PeriodSet(period);                                    // Set timer rollover
OCMPx_CompareValueSet(period - OCMP_PULSE_WIDTH);         // Rising edge (variable)
OCMPx_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);         // Falling edge (fixed at 40)
OCMPx_Enable();
TMRx_Start();                                              // CRITICAL: Must restart timer for each move
```

**CRITICAL: Motion Execution Pattern**
```c
// 1. Set direction GPIO BEFORE enabling step pulses
if (direction_forward) {
    DirX_Set();    // GPIO high for forward
} else {
    DirX_Clear();  // GPIO low for reverse
}

// 2. Configure OCR registers
TMR2_PeriodSet(period);
OCMP4_CompareValueSet(period - 40);
OCMP4_CompareSecondaryValueSet(40);

// 3. Enable OCR and start timer
OCMP4_Enable();
TMR2_Start();  // CRITICAL: Always restart timer for each move
```

**Key Register Values:**
- **TMRxPR**: Timer period register (controls pulse frequency)
- **OCxR**: Primary compare (rising edge - varies: period-40)
- **OCxRS**: Secondary compare (falling edge - fixed at 40)
- **Pulse Width**: Always 40 counts (meeting DRV8825 1.9µs minimum)
- **Maximum Period**: 65485 counts (16-bit timer limit with safety margin)

### GPIO Pin Assignments
```
Limit Switches (Active Low):
├── RA7  → X-axis negative limit
├── RA9  → X-axis positive limit
├── RA10 → Y-axis negative limit
├── RA14 → Y-axis positive limit
└── RA15 → Z-axis negative limit

Step/Direction Outputs:
├── Step pins controlled via OCRx pulse generation
└── Direction pins controlled via GPIO
```

## Software Architecture

### 🆕 **Modular Architecture (2024 Restructuring)**

The motion control system has been restructured into clean, modular components with proper separation of concerns:

```
Motion Control Architecture:
┌─────────────────────────────────────────────────────────────────────┐
│                          Core Timer (1kHz)                          │
│                    MotionPlanner_UpdateTrajectory()                 │
└───────────────────────────────┬─────────────────────────────────────┘
                                │
                ┌───────────────▼───────────────┐
                │        Motion Planner        │
                │      (motion_planner.c)      │
                │ • Real-time trajectory calc  │
                │ • OCR period updates         │
                │ • Velocity profiling         │
                └───────────────┬───────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        │                       │                       │
┌───────▼────────┐    ┌─────────▼──────────┐   ┌───────▼─────────┐
│ Motion Buffer  │    │  G-code Parser     │   │ Hardware Layer  │
│(motion_buffer.c)│    │(motion_gcode_      │   │    (app.c)      │
│• 16-cmd buffer │    │ parser.c)          │   │• OCR interrupts │
│• Look-ahead    │    │• G0/G1/G2/G3 parse │   │• Position feedback│
│• Thread-safe   │    │• Machine state     │   │• Limit switches │
└────────────────┘    └────────────────────┘   └─────────────────┘
```

#### **Module Responsibilities:**

**1. Motion Buffer (`motion_buffer.c/.h`)**
- 16-command circular buffer management
- Thread-safe operations for real-time access
- Buffer status monitoring and statistics
- Encapsulated buffer storage (no global access)

**2. Motion Planner (`motion_planner.c/.h`)**
- Real-time trajectory calculations at 1kHz
- OCR period calculations for step rate control
- Acceleration/deceleration profile generation
- Position feedback integration from OCR interrupts
- Hardware abstraction for stepper control

**3. G-code Parser (`motion_gcode_parser.c/.h`)**
- G-code command parsing (G0/G1/G2/G3)
- Machine state management (coordinates, feedrate, etc.)
- Motion block generation and validation
- Parameter extraction and validation

**4. Application Layer (`app.c`)**
- Hardware initialization and configuration
- OCR interrupt service routines
- Position feedback and limit switch monitoring
- UART communication and user interface
- System state management

### Core Hardware Integration

```
Real-time Control Flow:
Core Timer (1kHz) ──── MotionPlanner_UpdateTrajectory()
     │                           │
     │                           ▼
     │                 Calculate Velocities
     │                           │
     │                           ▼
     │                 CalculateOCRPeriod()
     │                           │
     │                           ▼
     └────────────────► UpdateAxisOCRPeriods()
                                 │
                                 ▼
                        OCMP1/4/5 Hardware
                                 │
                                 ▼
                          Step Pulse Generation
                                 │
                                 ▼
                        OCR Interrupt Callbacks
                                 │
                                 ▼
                     MotionPlanner_UpdateAxisPosition()
```

### Legacy Components (Preserved)

#### 1. **Interpolation Engine** (`interpolation_engine.c/.h`)
The heart of the motion control system providing:

```c
// Core motion planning functions
bool INTERP_PlanLinearMove(position_t start, position_t end, float feed_rate);
bool INTERP_PlanArcMove(position_t start, position_t end, float i, float j, float k, 
                       bool clockwise, float feed_rate);
bool INTERP_PlanSCurveMove(position_t start, position_t end, float feed_rate);

// Look-ahead planner
void INTERP_ProcessPlannerBuffer(void);
void INTERP_OptimizeJunctionSpeeds(void);

// Safety and control
void INTERP_EmergencyStop(void);
void INTERP_StopSingleAxis(axis_id_t axis, const char* reason);
bool INTERP_CheckSoftLimits(position_t target_position);
```

#### 2. **Motion Planning Architecture**

```
Motion Pipeline:
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   G-code Input  │ → │  Motion Planner  │ → │ Step Generation │
│   (G0/G1/G2)   │    │  (Look-ahead)    │    │   (OCRx PWM)    │
└─────────────────┘    └──────────────────┘    └─────────────────┘
```

**Look-Ahead Planner Buffer:**
```c
typedef struct {
    planner_block_t blocks[16];     // 16-block GRBL-compatible buffer
    uint8_t head, tail, count;      // Buffer management
    float junction_deviation;       // $11 setting
    float minimum_planner_speed;    // Minimum planning speed
    bool recalculate_needed;        // Optimization flag
} planner_buffer_t;
```

#### 3. **S-Curve Motion Profiles**

The system implements professional 7-segment S-curve profiles:

```
Velocity Profile:
    │     ╭─────────╮
    │   ╱─┘         └─╲
    │ ╱─┘             └─╲
    │╱─┘                 └─╲
────┼─────────────────────────╲──── Time
    │                         └─╲
    │                           └╲
    
Segments: Jerk+ → Accel → Jerk- → Constant → Jerk+ → Decel → Jerk-
```

#### 4. **Arc Interpolation**

Complete circular interpolation support:

```c
// I,J,K offset format: G2 X10 Y10 I5 J0
// R radius format: G2 X10 Y10 R5
// Helical interpolation: G2 X10 Y10 Z5 I5 J0

typedef struct {
    position_t start, end, center;
    float radius;
    float start_angle, end_angle, sweep_angle;
    bool clockwise;
    float feed_rate;
} arc_parameters_t;
```

### Safety System Implementation

#### Hard Limit Monitoring
```c
void APP_ProcessLimitSwitches(void) {
    // Check every 5ms for debouncing
    bool x_neg_limit = !GPIO_PinRead(GPIO_PIN_RA7);
    bool x_pos_limit = !GPIO_PinRead(GPIO_PIN_RA9);
    
    if(x_neg_limit || x_pos_limit) {
        INTERP_HandleHardLimit(AXIS_X, x_neg_limit, x_pos_limit);
        // Immediate OCRx disable stops pulse generation
        appData.state = APP_STATE_MOTION_ERROR;
    }
}
```

#### Soft Limit Prevention
```c
bool APP_AddLinearMove(float *target, float feedrate) {
    // Check soft limits before adding to buffer
    position_t target_position = {target[0], target[1], target[2], 0.0f};
    if(!INTERP_CheckSoftLimits(target_position)) {
        return false;  // Reject move
    }
    // Add to motion buffer...
}
```

## File Structure

```
Pic32mzCNC_V2/
├── incs/                           # Header files
│   ├── app.h                       # Main application interface
│   ├── motion_buffer.h             # 🆕 Motion buffer management API
│   ├── motion_gcode_parser.h       # 🆕 G-code parsing API
│   ├── motion_planner.h            # 🆕 Motion planning API
│   ├── interpolation_engine.h      # Legacy motion control API
│   ├── grbl_settings.h            # GRBL parameter definitions
│   ├── motion_profile.h           # Motion profile types
│   ├── speed_control.h            # Speed control algorithms
│   └── config/default/            # Hardware abstraction layer
│       └── peripheral/            # PIC32 peripheral drivers
├── srcs/                          # Source files
│   ├── app.c                      # Main application & hardware layer
│   ├── motion_buffer.c            # 🆕 Motion buffer implementation
│   ├── motion_gcode_parser.c      # 🆕 G-code parsing implementation
│   ├── motion_planner.c           # 🆕 Motion planning implementation
│   ├── interpolation_engine.c     # Legacy motion control implementation
│   ├── grbl_settings.c           # GRBL settings management
│   ├── motion_profile.c          # Motion profile calculations
│   ├── speed_control.c           # Speed control implementation
│   └── config/default/           # Generated HAL code
├── objs/                         # Compiled object files
├── bins/                         # Final executable and hex files
└── other/                        # Linker maps and memory files
```

### 🆕 **New Modular API Functions**

#### Motion Buffer API
```c
// Buffer management
void MotionBuffer_Initialize(void);
bool MotionBuffer_Add(motion_block_t *block);
motion_block_t *MotionBuffer_GetNext(void);
void MotionBuffer_Complete(void);
bool MotionBuffer_HasSpace(void);
bool MotionBuffer_IsEmpty(void);
void MotionBuffer_Clear(void);
motion_buffer_status_t MotionBuffer_GetStatus(void);
```

#### Motion Planner API
```c
// Real-time trajectory control
void MotionPlanner_Initialize(void);
void MotionPlanner_UpdateTrajectory(void);  // Called at 1kHz
void MotionPlanner_ProcessBuffer(void);
void MotionPlanner_UpdateAxisPosition(uint8_t axis, int32_t position);

// Motion calculations
void MotionPlanner_CalculateDistance(motion_block_t *block);
void MotionPlanner_OptimizeVelocityProfile(motion_block_t *block);
void MotionPlanner_ExecuteBlock(motion_block_t *block);

// Control functions
void MotionPlanner_EmergencyStop(void);
void MotionPlanner_SetAcceleration(float acceleration);
float MotionPlanner_GetAcceleration(void);
```

#### G-code Parser API
```c
// Parsing functions
void MotionGCodeParser_Initialize(void);
bool MotionGCodeParser_ParseMove(const char *command, motion_block_t *block);
bool MotionGCodeParser_ParseArc(const char *command, motion_block_t *block);
bool MotionGCodeParser_ParseDwell(const char *command, motion_block_t *block);

// State management
void MotionGCodeParser_UpdateSpindleState(const char *command);
void MotionGCodeParser_UpdateCoolantState(const char *command);
void MotionGCodeParser_SetPosition(float x, float y, float z);
void MotionGCodeParser_GetPosition(float *x, float *y, float *z);
```

## Motion Control Flow

### 1. **G-code Reception**
```c
void APP_ExecuteGcodeCommand(const char *command) {
    // Parse G-code (G0, G1, G2, G3)
    // Extract coordinates and parameters
    // Call appropriate INTERP_Plan*() function
}
```

### 2. **Motion Planning**
```c
// Linear move planning
INTERP_PlanLinearMove(start_pos, end_pos, feedrate);
    ↓
// Add to look-ahead buffer
Add to planner_buffer[head]
    ↓
// Optimize junction speeds
INTERP_ProcessPlannerBuffer();
    ↓
// Execute motion
Generate OCRx pulse patterns
```

### 3. **Real-time Execution**
```c
void INTERP_Tasks(void) {  // Called from Timer1 ISR
    // Update current motion block
    // Calculate instantaneous velocities
    // Update OCRx periods for each axis
    // Handle motion state transitions
}
```

## GRBL Settings Integration

The system supports standard GRBL settings:

```c
// Motion settings ($100-$132)
$100 = X steps/mm          // X-axis steps per millimeter
$101 = Y steps/mm          // Y-axis steps per millimeter  
$102 = Z steps/mm          // Z-axis steps per millimeter
$110 = X max rate          // X-axis maximum rate (mm/min)
$111 = Y max rate          // Y-axis maximum rate (mm/min)
$112 = Z max rate          // Z-axis maximum rate (mm/min)
$120 = X acceleration      // X-axis acceleration (mm/sec²)
$121 = Y acceleration      // Y-axis acceleration (mm/sec²)
$122 = Z acceleration      // Z-axis acceleration (mm/sec²)
$130 = X max travel        // X-axis max travel (mm)
$131 = Y max travel        // Y-axis max travel (mm)
$132 = Z max travel        // Z-axis max travel (mm)

// Junction control
$11 = Junction deviation   // Junction deviation (mm)
$12 = Arc tolerance        // Arc tolerance (mm)
```

## Building the Project

### Prerequisites
- **MPLAB X IDE v6.25** or later
- **XC32 Compiler v4.60** or later
- **PIC32MZ-EF DFP v1.4.168** or later

### Build Commands
```bash
# Clean and build
make clean && make

# Build only
make

# Clean only  
make clean
```

### Output Files
```
bins/CS23       # ELF executable
bins/CS23.hex   # Intel HEX for programming
```

## Usage Examples

### Basic Linear Move
```c
// Move to X=100mm, Y=50mm at 1000mm/min
float target[] = {100.0f, 50.0f, 0.0f};
APP_AddLinearMove(target, 1000.0f);
```

### Arc Interpolation
```c
// G2 clockwise arc from current position to X=20, Y=0 with center at X=10, Y=0
position_t start = INTERP_GetCurrentPosition();
position_t end = {20.0f, 0.0f, 0.0f, 0.0f};
INTERP_PlanArcMove(start, end, 10.0f, 0.0f, 0.0f, true, 500.0f);
```

### Emergency Stop
```c
// Immediate stop all motion
APP_EmergencyStop();

// Or stop single axis
INTERP_StopSingleAxis(AXIS_X, "Limit switch triggered");
```

## Performance Characteristics

### Timing Performance
- **Motion Update Rate**: 1kHz (Timer1 interrupt)
- **Look-ahead Planning**: 16-block buffer with continuous optimization
- **Step Pulse Generation**: Hardware OCRx for microsecond precision
- **Limit Switch Response**: <5ms detection + immediate hardware stop

### Motion Quality
- **Acceleration Smoothness**: S-curve profiles eliminate jerk
- **Path Accuracy**: Arc segmentation with configurable tolerance
- **Speed Optimization**: Junction speed calculation for continuous motion
- **Coordinate Precision**: 32-bit floating point with 1 micron tolerance

## Safety Features

### Hard Limits
- ✅ Real-time GPIO monitoring
- ✅ Immediate axis stopping  
- ✅ Hardware-level pulse disable
- ✅ Alarm state management
- ✅ Recovery procedures

### Soft Limits
- ✅ Pre-move validation
- ✅ Configurable boundaries
- ✅ Move rejection before execution
- ✅ GRBL $130-$132 compliance

### Emergency Systems
- ✅ Emergency stop function
- ✅ Individual axis control
- ✅ Motion buffer clearing
- ✅ System state recovery
- ✅ Error reporting via UART

## Recent Updates (October 2025)

### OCR Dual-Compare Continuous Pulse Mode - FULLY OPERATIONAL ✅

Successfully implemented and verified hardware-based step pulse generation with the following fixes:

#### Bug Fix #1: Status Report Steps/mm Mismatch
- **Issue**: Motion appeared to stop at 62.5% of target (6.25mm instead of 10mm)
- **Cause**: `gcode_helpers.c` used hardcoded 400 steps/mm while execution used 250 steps/mm
- **Solution**: Updated `GCodeHelpers_GetCurrentPositionFromSteps()` to use 250 steps/mm fallback
- **Result**: Status reports now accurately reflect actual machine position

#### Bug Fix #2: Missing Timer Restart on Subsequent Moves
- **Issue**: First move worked, second move failed (no motion)
- **Cause**: OCR callbacks stopped timers (TMR2/3/4) on completion but weren't restarted
- **Solution**: Added `TMR2_Start()`, `TMR3_Start()`, `TMR4_Start()` in motion execution
- **Result**: Multi-move sequences now work reliably

#### Bug Fix #3: Missing Direction Pin Control
- **Issue**: Reverse motion didn't execute
- **Cause**: Direction GPIO pins (DirX/Y/Z) never set before step pulses
- **Solution**: Added direction pin control before OCR enable for all axes
- **Result**: Bidirectional motion fully functional

### Verified Working Pattern
```c
// 1. Set direction FIRST (DRV8825 requirement)
cnc_axes[axis].direction_forward ? DirX_Set() : DirX_Clear();

// 2. Configure OCR dual-compare registers
TMR2_PeriodSet(period);
OCMP4_CompareValueSet(period - 40);      // Rising edge
OCMP4_CompareSecondaryValueSet(40);       // Falling edge

// 3. Enable and start
OCMP4_Enable();
TMR2_Start();  // CRITICAL: Always restart timer
```

**Test Results**: G0 X10 → 10.000mm ✅ | G0 X1 → 1.000mm (reverse) ✅

## Integration with UGS

This motion controller is designed for seamless integration with **Universal G-code Sender (UGS)**:

1. **GRBL Protocol Compatibility**: Standard command/response format
2. **Real-time Commands**: Feed hold, cycle start, reset support  
3. **Status Reporting**: Position, state, and alarm reporting
4. **Settings Management**: $$ command support for parameter access
5. **Arc Support**: Native G2/G3 processing without linearization

## Future Development

### Planned Enhancements
- [ ] 4th Axis (A) rotation support
- [ ] Spindle speed control (PWM)
- [ ] Tool length compensation
- [ ] Coordinate system offsets (G54-G59)
- [ ] Probe functionality (G38.x)
- [ ] Advanced feed override

### Optimization Opportunities
- [ ] Adaptive feed rate control
- [ ] Vibration compensation
- [ ] Thermal drift compensation
- [ ] Energy-efficient motion profiles

## License

This project is provided for educational and development purposes. Please refer to the license file for specific terms and conditions.

## Contributing

Contributions are welcome! Please follow the existing code style and include appropriate documentation for any new features.

---

**Developed with:** MPLAB X IDE, XC32 Compiler, PIC32MZ2048EFH100  
**Compatible with:** GRBL v1.1f, Universal G-code Sender, CNC.js  
**Target Applications:** CNC Mills, 3D Printers, Laser Engravers, Pick & Place Machines