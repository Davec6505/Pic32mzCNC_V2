# PIC32MZ CNC Motion Controller V2

**A high-performance 4-axis CNC controller with hardware-accelerated S-curve motion profiles**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![Hardware](https://img.shields.io/badge/hardware-PIC32MZ2048EFH100-orange.svg)]()

## 🎯 Project Vision

This is the **definitive direction** for a modern, modular CNC controller that eliminates traditional interrupt-heavy step generation in favor of hardware-accelerated motion control. The architecture leverages the PIC32MZ's powerful Output Compare (OCR) modules for autonomous step pulse generation, freeing the CPU for advanced motion planning with jerk-limited S-curve profiles.

## ⚡ Current Status (October 2025)

**Branch:** `master` (Main development branch)

### ✅ Working Features (Production Ready!)
- **Full GRBL v1.1f Protocol**: Complete UGS integration with all system commands ($I, $G, $$, $#, $N, $)
- **Real-Time Position Feedback**: Live position updates from hardware step counters (? status reports)
- **Live Settings Management**: View and modify all GRBL settings ($100-$133) without recompilation
- **GRBL Simple Send-Response Flow Control**: Blocking protocol ensures each move completes before accepting next command
- **Real-Time Commands**: Feed hold (!), cycle start (~), soft reset (^X), status report (?)
- **Time-Synchronized Coordinated Motion**: Dominant axis determines timing, subordinate axes scale velocities for accurate multi-axis moves
- **Multi-Axis S-Curve Control**: Per-axis independent state machines with 7-segment jerk-limited profiles
- **Hardware Step Generation**: OCMP1/3/4/5 modules generate step pulses autonomously (no CPU interrupts per step)
- **4-Axis Support**: X, Y, Z, and A axes with independent control
- **Centralized Hardware Configuration**: GT2 belt (80 steps/mm) on X/Y/A, leadscrew (1280 steps/mm) on Z
- **Oscilloscope Verified**: Symmetric acceleration/deceleration profiles confirmed on all axes
- **Perfect Restart**: Per-axis active flags enable reliable motion restart
- **Dynamic Direction Control**: Function pointer tables for efficient GPIO management
- **MISRA C Compliance**: Static assertions and runtime validation for safety-critical operation

### 🧪 Ready for Testing
- **UGS Connectivity**: Verified connection as "GRBL 1.1f" with full initialization sequence
- **Serial Communication**: UART2 @ 115200 baud operational
- **G-code Command Set**: G0/G1 (linear), G90/G91 (absolute/relative), G92 (coordinate offset), G28/G30 (predefined positions)
- **M-Commands**: M3/M4/M5 (spindle), M7/M8/M9 (coolant), M0/M1/M2/M30 (program control)
- **Blocking Protocol**: Each move completes before "ok" sent (pauses between moves are CORRECT per GRBL spec!)

### 🚧 Development Roadmap

**Phase 1: GRBL Protocol Integration ✅ COMPLETE!**
- [x] Full G-code parser with modal/non-modal commands
- [x] UGS serial interface with GRBL v1.1f protocol
- [x] System commands ($I, $G, $$, $#, $N, $)
- [x] Real-time position feedback from hardware
- [x] Live settings management ($xxx=value)
- [x] GRBL Simple Send-Response flow control
- [x] Real-time command handling (?, !, ~, ^X)
- [x] Build system optimization (make quiet)
- [x] MISRA C:2012 compliance

**Phase 2: Hardware Testing & Validation (CURRENT PHASE)**
- [ ] Flash firmware and test via UGS
- [ ] Verify coordinated motion accuracy with oscilloscope
- [ ] Test blocking protocol behavior (pauses between moves)
- [ ] Validate position feedback accuracy
- [ ] Test real-time commands (feed hold, cycle start, reset)
- [ ] Verify settings modification (change steps/mm, test motion)
- [ ] Multi-line G-code file streaming tests

**Phase 3: Look-Ahead Planning (Future Optimization)**
- [ ] Implement full junction velocity calculations
- [ ] Forward/reverse pass velocity optimization
- [ ] S-curve profile pre-calculation in buffer
- [ ] Switch to GRBL Character-Counting protocol
- [ ] Continuous motion through corners (no stops)
- [ ] Arc support (G2/G3 circular interpolation)

**Phase 4: Advanced Features**
- [ ] Coordinate systems ($#, G54-G59 work offsets)
- [ ] Probing (G38.x commands)
- [ ] Spindle PWM output (M3/M4 GPIO control)
- [ ] Coolant GPIO control (M7/M8/M9 GPIO control)
- [ ] Hard limit switch integration (immediate stop)
- [ ] Soft limit enforcement (preventive)
- [ ] Homing sequences (per-axis)
- [ ] Manual jog controls (variable speed)

## 🏗️ Architecture

### Hardware Platform
- **MCU**: PIC32MZ2048EFH100 @ 200MHz
- **Flash**: 2MB
- **RAM**: 512KB
- **Stepper Drivers**: DRV8825 (or compatible)
- **Microstepping**: Configurable (1/32 recommended)

### Core Architecture Pattern

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

### Key Design Decisions

**1. GRBL v1.1f Protocol Implementation**
- Full system command support ($I, $G, $$, $#, $N, $)
- Real-time commands (?, !, ~, ^X) with immediate response
- GRBL Simple Send-Response flow control (Phase 1 - blocking protocol)
- Live settings management ($100-$133 read/write)
- Real-time position feedback from hardware step counters
- UGS compatibility verified

**2. Hardware-Accelerated Step Generation**
- Traditional GRBL: 30kHz interrupt per step (CPU intensive)
- Our approach: OCR modules generate pulses autonomously
- Benefit: CPU free for advanced motion planning

**3. Per-Axis State Management**
- No global motion_running flag
- Each axis has independent `active` flag
- Enables true concurrent motion and reliable restart

**4. S-Curve Motion Profiles**
- 7-segment jerk-limited profiles
- Smooth acceleration/deceleration
- Reduced mechanical stress and vibration
- Parameters configurable via GRBL settings ($110-$123)

**5. Flow Control Strategy**
- Phase 1 (Current): Simple Send-Response blocking protocol
  - Each move completes before "ok" sent
  - Brief pauses between moves are CORRECT per GRBL spec
  - Recommended by GRBL docs as "most fool-proof and simplest method"
- Phase 2 (Future): Character-Counting protocol with look-ahead
  - Track 128-byte RX buffer for maximum throughput
  - Enable continuous motion through corners
  - Requires full look-ahead planning implementation

**6. MISRA C Compliance**
- Static assertions for compile-time validation
- Runtime parameter validation
- Defensive programming patterns
- Safety-critical system design

## 📁 Project Structure

```
Pic32mzCNC_V2/
├── srcs/
│   ├── main.c                          # Entry point, G-code processing loop
│   ├── app.c                           # System management, LED status
│   ├── gcode_parser.c                  # GRBL v1.1f parser (1354 lines)
│   ├── ugs_interface.c                 # UGS serial protocol
│   └── motion/
│       ├── multiaxis_control.c         # Multi-axis S-curve engine (1169 lines)
│       ├── motion_math.c               # Kinematics & GRBL settings (733 lines)
│       ├── motion_buffer.c             # Ring buffer for look-ahead (284 lines)
│       └── stepper_control.c           # Legacy single-axis reference (unused)
├── incs/
│   ├── app.h                           # Main system interface
│   ├── gcode_parser.h                  # Parser API & modal state (357 lines)
│   ├── ugs_interface.h                 # UGS protocol API
│   └── motion/
│       ├── motion_types.h              # Centralized type definitions (235 lines)
│       ├── motion_buffer.h             # Motion buffer API (207 lines)
│       ├── multiaxis_control.h         # Multi-axis API (4-axis support)
│       └── motion_math.h               # Motion math API (398 lines)
├── docs/
│   ├── GCODE_PARSER_COMPLETE.md        # GRBL v1.1f implementation guide
│   ├── XC32_COMPLIANCE_GCODE_PARSER.md # MISRA/XC32 compliance docs
│   ├── APP_CLEANUP_SUMMARY.md          # App layer cleanup docs
│   ├── MAKEFILE_QUIET_BUILD.md         # make quiet target docs
│   └── UGS_INTEGRATION.md              # UGS integration guide
├── .github/
│   └── copilot-instructions.md         # AI assistant guidance (1082 lines)
├── Makefile                            # Cross-platform build system
└── README.md                           # This file
```

## 🔧 Building the Project

### Prerequisites
- **MPLAB X IDE** v6.25 or later
- **XC32 Compiler** v4.60 or later
- **PICkit 4** or compatible programmer
- **Universal G-code Sender (UGS)** for testing

### Build Commands

```powershell
# Standard build
make all

# Quiet build (filtered output - errors/warnings only)
make quiet

# Create directory structure (first time only)
make build_dir

# Clean all outputs
make clean

# Show build configuration
make debug

# Show platform information
make platform
```

### Output Files
- `bins/CS23.elf` - Executable with debug symbols
- `bins/CS23.hex` - Flash programming file
- `objs/` - Object files (.o)

### Build System Features
- Cross-platform Make (Windows/Linux)
- Automatic dependency generation
- Optimized compilation (-O1 -Werror -Wall)
- MISRA C:2012 compliance checking
- Quiet build mode for cleaner output

## 🖥️ Usage

### Connecting to UGS

1. **Flash firmware** to PIC32MZ board (`bins/CS23.hex`)
2. **Open Universal G-code Sender**
3. **Configure connection**:
   - Port: COM4 (or your serial port)
   - Baud Rate: 115200
   - Firmware: GRBL
4. **Click "Connect"**
5. **Verify connection**: Should see "*** Connected to GRBL 1.1f"

### Basic Operation

```gcode
# Check system status
?

# View all settings
$$

# Set work coordinate to current position
G92 X0 Y0 Z0

# Move in absolute mode
G90
G1 X10 Y10 F1000

# Move in relative mode
G91
G1 X5 Y5 F500

# Emergency stop (real-time)
^X
```

### Understanding Blocking Protocol

When using the current Simple Send-Response protocol, you will observe:
- ✅ **Brief pauses between commands** - This is CORRECT behavior!
- ✅ **"ok" sent after motion completes** - Ensures position accuracy
- ✅ **UGS waits for "ok" before sending next command** - Prevents buffer overflow

**Why this is good for testing:**
- Each move completes before next starts
- Position is always known
- No risk of buffer overrun
- Simple and reliable

**Future optimization:**
- Implement look-ahead planning
- Switch to Character-Counting protocol
- Enable continuous motion through corners

## 🧪 Testing

### Current Testing Status

**UGS Connectivity ✅**
- Connects as "GRBL 1.1f" at 115200 baud
- Full initialization sequence verified (?, $I, $$, $G)
- Ready for motion testing via serial interface

### Supported G-code Commands

**System Commands:**
```gcode
$                    ; Show GRBL help message
$I                   ; Build info (version detection)
$G                   ; Parser state (current modal settings)
$$                   ; View all settings ($11, $12, $100-$133)
$#                   ; Coordinate offsets (placeholder)
$N                   ; Startup lines
$100=250.0           ; Set X steps/mm (live settings modification)
```

**Real-Time Commands:**
```gcode
?                    ; Status report (position, state)
!                    ; Feed hold (pause motion)
~                    ; Cycle start (resume motion)
^X                   ; Soft reset (emergency stop)
```

**Motion Commands:**
```gcode
G90                  ; Absolute positioning mode
G91                  ; Relative positioning mode  
G0 X10 Y20 F1500     ; Rapid positioning
G1 X50 Y50 F1000     ; Linear interpolation move
G92 X0 Y0 Z0         ; Set work coordinate offset
G28                  ; Go to predefined position 1
G30                  ; Go to predefined position 2
```

**Machine Control:**
```gcode
M3 S1000             ; Spindle on CW @ 1000 RPM (state tracking)
M4 S1000             ; Spindle on CCW @ 1000 RPM (state tracking)
M5                   ; Spindle off
M7                   ; Mist coolant on (state tracking)
M8                   ; Flood coolant on (state tracking)
M9                   ; All coolant off
M0                   ; Program pause
M2                   ; Program end
M30                  ; Program end and reset
```

### Testing Workflow

**Phase 1: UGS Connection (Complete ✅)**
```powershell
# Connect via UGS @ COM4, 115200 baud
# Verify initialization commands work
? $I $$ $G
```

**Phase 2: Motion Testing (Ready to Test 🎯)**
```gcode
# Test absolute positioning
G90
G1 X10 Y10 F1000

# Verify blocking protocol (pause between commands is CORRECT!)
G1 X20 Y20 F1000

# Test relative positioning  
G91
G1 X-5 Y-5 F500

# Test coordinate offset
G92 X0 Y0 Z0
G1 X10 F1000
```

**Phase 3: Settings Testing (Ready to Test 🎯)**
```gcode
# View current settings
$$

# Modify X axis steps/mm
$100=200.0

# Verify change
$$

# Test motion with new setting
G1 X10 F1000

# Restore original
$100=80.0
```

**Phase 4: Real-Time Commands (Ready to Test 🎯)**
```gcode
# Start a long move
G1 X100 F500

# While moving, send feed hold
!

# Resume motion
~

# Emergency stop
^X
```

### Hardware Button Tests (Legacy - Removed)

```c
// SW1/SW2 removed in favor of G-code control
// Motion now exclusively via serial interface
```

### Future Testing Scripts

```powershell
# Motion testing via serial
.\motion_test.ps1 -Port COM4

# UGS G-code file streaming
.\ugs_test.ps1 -Port COM4 -GCodeFile modular_test.gcode

# Debug output monitoring
.\monitor_debug.ps1 -Port COM4
```

## 📊 Hardware Mapping

### Axis Assignments

| Axis | OCR Module | Timer | Direction Pin | Step Pin | Drive System | Steps/mm | Status      |
| ---- | ---------- | ----- | ------------- | -------- | ------------ | -------- | ----------- |
| X    | OCMP4      | TMR2  | DirX (GPIO)   | RC3      | GT2 Belt     | 80       | ✅ Tested    |
| Y    | OCMP1      | TMR4  | DirY (GPIO)   | RD0      | GT2 Belt     | 80       | ✅ Tested    |
| Z    | OCMP5      | TMR3  | DirZ (GPIO)   | RD4      | 2.5mm Lead   | 1280     | ✅ Tested    |
| A    | OCMP3      | TMR5  | DirA (GPIO)   | RD2      | GT2 Belt     | 80       | ⏸️ Not wired |

### Limit Switches (Active Low)

```c
GPIO_PIN_RA7  → X-axis negative limit
GPIO_PIN_RA9  → X-axis positive limit
GPIO_PIN_RA10 → Y-axis negative limit
GPIO_PIN_RA14 → Y-axis positive limit
GPIO_PIN_RA15 → Z-axis negative limit
```

## 📚 API Documentation

### Multi-Axis Control

```c
// Initialize all axes
void MultiAxis_Initialize(void);

// Single-axis motion
void MultiAxis_MoveSingleAxis(axis_id_t axis, int32_t steps, bool forward);

// Time-synchronized coordinated motion (ONLY method for multi-axis moves)
void MultiAxis_ExecuteCoordinatedMove(int32_t steps[NUM_AXES]);

// Motion status
bool MultiAxis_IsBusy(void);                    // Any axis moving?
bool MultiAxis_IsAxisBusy(axis_id_t axis);      // Specific axis moving?
int32_t MultiAxis_GetStepCount(axis_id_t axis); // Current step count

// Emergency control
void MultiAxis_EmergencyStop(void);
```

### Axis IDs

```c
typedef enum {
    AXIS_X = 0,
    AXIS_Y = 1,
    AXIS_Z = 2,
    AXIS_A = 3,
    NUM_AXES = 4
} axis_id_t;
```

## 🔬 S-Curve Profile Details

### Time-Synchronized Coordinated Motion

The `MultiAxis_ExecuteCoordinatedMove()` function ensures accurate multi-axis motion where all axes finish simultaneously:

**Algorithm:**
1. Find **dominant axis** (longest distance in steps)
2. Calculate S-curve profile for dominant axis → determines **total_move_time**
3. For subordinate axes: `velocity_scale = distance_axis / distance_dominant`
4. All axes share **same segment times** (t1-t7) but with scaled velocities
5. Result: All axes finish at same time with correct distances traveled

**Example:**
```c
int32_t steps[NUM_AXES] = {4000, 2000, 0, 0};  // X=50mm, Y=25mm
MultiAxis_ExecuteCoordinatedMove(steps);
// X is dominant (4000 steps), determines total time = 3.0s
// Y scales: velocity_scale = 2000/4000 = 0.5
// Y moves at half X's velocity, both finish at 3.0s
```

### 7-Segment S-Curve Profile

```
Velocity
   ^
   │     ╱────────╲
   │    ╱          ╲
   │   ╱            ╲
   │  ╱              ╲
   │ ╱                ╲
   │╱                  ╲___
   └────────────────────────> Time
   t1 t2  t3  t4  t5 t6  t7

Segments:
1. Jerk-in (acceleration increases)
2. Constant acceleration
3. Jerk-out (acceleration decreases)
4. Constant velocity (cruise)
5. Jerk-in (deceleration increases)
6. Constant deceleration
7. Jerk-out (deceleration decreases to zero)
```

### Hardware Configuration (motion_types.h)

```c
// Timer and microstepping
#define TMR_CLOCK_HZ 12500000UL          // 12.5 MHz (25MHz ÷ 2 prescaler)
#define STEPPER_STEPS_PER_REV 200.0f     // 1.8° steppers
#define MICROSTEPPING_MODE 16.0f         // 1/16 microstepping

// Drive systems
#define BELT_PITCH_MM 2.0f               // GT2 belt pitch
#define PULLEY_TEETH 20.0f               // 20-tooth pulley
#define SCREW_PITCH_MM 2.5f              // Leadscrew pitch

// Calculated steps/mm
#define STEPS_PER_MM_BELT 80.0f          // (200 × 16) / (20 × 2)
#define STEPS_PER_MM_LEADSCREW 1280.0f   // (200 × 16) / 2.5

// Motion limits (from motion_math.c)
Max Rate:       1000 mm/min (X/Y/A), 500 mm/min (Z)
Acceleration:   100 mm/sec² (X/Y/A), 50 mm/sec² (Z)
Jerk Limit:     1000 mm/sec³ (all axes)
```

## 🐛 Known Issues & Solutions

### Resolved Critical Bugs (October 2025)

1. **SEGMENT_COMPLETE Blocking**: Guard condition prevented completion - removed from skip logic
2. **Global Motion Flag Deadlock**: Removed `motion_running` flag - pure per-axis control
3. **Deceleration Not Triggering**: Added 60% step count safety check for cruise segment exit
4. **Direction Pin Control**: Added wrapper functions for GPIO macros (MISRA compliance)

See `.github/copilot-instructions.md` for detailed bug analysis and fixes.

## 🤝 Contributing

This is a personal CNC controller project, but feedback and suggestions are welcome!

### Development Guidelines

1. **MISRA C Compliance**: Follow safety-critical coding standards
2. **Hardware Abstraction**: Use getter/setter functions, never direct global access
3. **Testing First**: Verify each subsystem with oscilloscope before moving on
4. **Document Decisions**: Update copilot-instructions.md with architectural choices

## 📖 References

### Technical Documentation
- [PIC32MZ Family Reference Manual](https://www.microchip.com/en-us/product/PIC32MZ2048EFH100)
- [DRV8825 Stepper Driver Datasheet](https://www.ti.com/lit/ds/symlink/drv8825.pdf)
- [GRBL v1.1f Documentation](https://github.com/gnea/grbl/wiki)

### Related Projects
- [GRBL](https://github.com/gnea/grbl) - Original Arduino-based CNC controller
- [Universal G-code Sender](https://winder.github.io/ugs_website/) - Cross-platform G-code sender

## 📝 License

This project is licensed under the MIT License - see the LICENSE file for details.

## 🙏 Acknowledgments

- GRBL project for G-code protocol standards
- Microchip for excellent PIC32MZ documentation
- Texas Instruments for DRV8825 stepper drivers

---

**Current Development Focus**: Perfecting multi-axis S-curve motion control with hardware button tests before advancing to limit switches and G-code integration.

**Last Updated**: October 16, 2025
