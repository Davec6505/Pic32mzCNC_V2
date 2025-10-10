# PIC32MZ CNC Motion Controller V2

## Overview

This is a professional-grade CNC motion control system implemented on the **PIC32MZ2048EFH100** microcontroller. The system provides GRBL v1.1f compatible motion control with advanced features including S-curve acceleration profiles, 16-block look-ahead motion planning, G2/G3 arc interpolation, and comprehensive safety systems.

## Key Features

### 🚀 **Advanced Motion Control**
- **S-Curve Acceleration Profiles**: 7-segment jerk-limited motion with smooth velocity transitions
- **Look-Ahead Motion Planning**: GRBL-style 16-block buffer with junction speed optimization
- **Arc Interpolation**: Complete G2/G3 circular interpolation with I,J,K offset and R radius formats
- **Multi-Axis Coordination**: Synchronized 3-axis (X,Y,Z) motion with optional 4th axis support
- **Hardware Pulse Generation**: OCRx continuous pulse mode for precise step timing

### 🛡️ **Comprehensive Safety Systems**
- **Hard Limit Switches**: Real-time GPIO monitoring with immediate axis stopping
- **Soft Limit Checking**: Preventive boundary validation before motion execution
- **Emergency Stop**: Instant motion halt with proper system recovery
- **Individual Axis Control**: Independent axis stopping for selective motion control

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
OCRx Module Assignments:
├── OCMP1 → Y-axis step pulses
├── OCMP4 → X-axis step pulses  
└── OCMP5 → Z-axis step pulses

Timer Sources:
├── TMR2/TMR3 → OCRx time base
├── TMR4 → System timing
└── TMR1 → Motion control timing
```

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

### Core Components

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
│   ├── interpolation_engine.h      # Motion control API
│   ├── grbl_settings.h            # GRBL parameter definitions
│   ├── motion_profile.h           # Motion profile types
│   └── config/default/            # Hardware abstraction layer
│       └── peripheral/            # PIC32 peripheral drivers
├── srcs/                          # Source files
│   ├── app.c                      # Main application & G-code handler
│   ├── interpolation_engine.c     # Motion control implementation
│   ├── grbl_settings.c           # GRBL settings management
│   ├── motion_profile.c          # Motion profile calculations
│   └── config/default/           # Generated HAL code
├── objs/                         # Compiled object files
├── bins/                         # Final executable and hex files
└── other/                        # Linker maps and memory files
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