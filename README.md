# PIC32MZ CNC Motion Controller V2

**A high-performance 4-axis CNC controller with hardware-accelerated S-curve motion profiles**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![Hardware](https://img.shields.io/badge/hardware-PIC32MZ2048EFH100-orange.svg)]()

## 🎯 Project Vision

This is the **definitive direction** for a modern, modular CNC controller that eliminates traditional interrupt-heavy step generation in favor of hardware-accelerated motion control. The architecture leverages the PIC32MZ's powerful Output Compare (OCR) modules for autonomous step pulse generation, freeing the CPU for advanced motion planning with jerk-limited S-curve profiles.

## ⚡ Current Status (October 2025)

**Branch:** `master` (Main development branch)

### ✅ Working Features
- **Time-Synchronized Coordinated Motion**: Dominant axis determines timing, subordinate axes scale velocities for accurate multi-axis moves
- **Multi-Axis S-Curve Control**: Per-axis independent state machines with 7-segment jerk-limited profiles
- **Hardware Step Generation**: OCMP1/3/4/5 modules generate step pulses autonomously (no CPU interrupts per step)
- **4-Axis Support**: X, Y, Z, and A axes with independent control
- **Centralized Hardware Configuration**: GT2 belt (80 steps/mm) on X/Y/A, leadscrew (1280 steps/mm) on Z
- **Oscilloscope Verified**: Symmetric acceleration/deceleration profiles confirmed on all axes
- **Perfect Restart**: Per-axis active flags enable reliable motion restart
- **Dynamic Direction Control**: Function pointer tables for efficient GPIO management
- **MISRA C Compliance**: Static assertions and runtime validation for safety-critical operation

### 🧪 Currently Testing
- **X/Y/Z Axes**: All mechanically operational with proper S-curve motion
- **Coordinated Motion**: SW1/SW2 buttons trigger 50mm X+Y coordinated moves (forward/reverse)
- **Distance Accuracy**: Timer clock (12.5MHz) and steps/mm calibrated correctly

### 🚧 Development Roadmap

This project follows a **methodical validation approach** - each subsystem is perfected before moving to the next:

#### Phase 1: Motion Perfection (Current Phase - Nearly Complete!)
**Objective**: Achieve flawless stepper motor control with S-curve profiles

- [x] Single-axis S-curve implementation
- [x] Multi-axis hardware abstraction
- [x] Per-axis state management
- [x] Oscilloscope verification of motion profiles
- [x] Wire X/Y/Z axis hardware (all tested mechanically)
- [x] Hardware configuration centralized (motion_types.h)
- [x] Timer clock calibrated (12.5MHz)
- [x] Steps/mm calculated (80 belt, 1280 leadscrew)
- [x] SW1/SW2 button tests for coordinated motion:
  - [x] Multi-axis coordinated moves (50mm X+Y forward/reverse)
  - [x] Time-synchronized coordination (dominant axis determines timing)
  - [x] Distance accuracy verified
  - [x] Direction reversal reliability
- [ ] Circular interpolation (G2/G3 arc motion)

**Exit Criteria**: All axes move with symmetric S-curves, coordinated motion verified ✅, zero issues with restarts or direction changes ✅

#### Phase 2: Limit Switches & Jogging
**Objective**: Safe manual control and homing

- [ ] Hard limit switch integration (immediate stop)
- [ ] Soft limit enforcement (preventive)
- [ ] Homing sequences (per-axis)
- [ ] Manual jog controls (variable speed)
- [ ] Pick-and-place mode (Z-axis limit masking)
- [ ] Emergency stop validation

**Exit Criteria**: Safe operation with all limit switches, reliable homing, smooth jogging

#### Phase 3: Serial Communication & G-Code
**Objective**: GRBL v1.1f compatibility with UGS integration

- [ ] UART2 G-code parser (DMA-based)
- [ ] Motion buffer (16-block look-ahead)
- [ ] G-code command set (G0/G1/G2/G3)
- [ ] Status reports ($X, ?, ~)
- [ ] GRBL settings ($100-$132)
- [ ] Universal G-code Sender (UGS) testing
- [ ] Real-time command handling (feed hold, cycle start, reset)

**Exit Criteria**: Full GRBL compatibility, UGS control panel working, production-ready

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

**1. Hardware-Accelerated Step Generation**
- Traditional GRBL: 30kHz interrupt per step (CPU intensive)
- Our approach: OCR modules generate pulses autonomously
- Benefit: CPU free for advanced motion planning

**2. Per-Axis State Management**
- No global motion_running flag
- Each axis has independent `active` flag
- Enables true concurrent motion and reliable restart

**3. S-Curve Motion Profiles**
- 7-segment jerk-limited profiles
- Smooth acceleration/deceleration
- Reduced mechanical stress and vibration
- Parameters: `max_velocity=5000 steps/sec`, `max_accel=10000 steps/sec²`, `max_jerk=50000 steps/sec³`

**4. MISRA C Compliance**
- Static assertions for compile-time validation
- Runtime parameter validation
- Defensive programming patterns
- Safety-critical system design

## 📁 Project Structure

```
Pic32mzCNC_V2/
├── srcs/
│   ├── main.c                          # Entry point, TMR1 setup
│   ├── app.c                           # Button-based testing (SW1/SW2)
│   └── motion/
│       ├── multiaxis_control.c         # Multi-axis S-curve engine (773 lines)
│       └── stepper_control.c           # Legacy single-axis reference
├── incs/
│   ├── app.h                           # Main system interface
│   └── motion/
│       └── multiaxis_control.h         # Multi-axis API (4-axis support)
├── .github/
│   └── copilot-instructions.md         # AI assistant guidance
├── Makefile                            # Cross-platform build system
└── README.md                           # This file
```

## 🔧 Building the Project

### Prerequisites
- **MPLAB X IDE** v6.25 or later
- **XC32 Compiler** v4.60 or later
- **PICkit 4** or compatible programmer

### Build Commands

```powershell
# Clean build
make all

# Create directory structure (first time only)
make build_dir

# Clean all outputs
make clean

# Show build configuration
make debug
```

### Output Files
- `bins/CS23.elf` - Executable with debug symbols
- `bins/CS23.hex` - Flash programming file

## 🧪 Testing

### Hardware Button Tests (Current)

```c
// SW1 - Coordinated X+Y 50mm forward
// Tests: Time-synchronized coordinated motion, dominant axis timing

// SW2 - Coordinated X+Y 50mm reverse  
// Tests: Return to start position, bidirectional accuracy
```

### Future Testing Scripts

```powershell
# Motion testing (Phase 1)
.\motion_test.ps1 -Port COM4

# Limit switch testing (Phase 2)
.\limit_test.ps1 -Port COM4

# G-code testing (Phase 3)
.\ugs_test.ps1 -Port COM4 -GCodeFile test.gcode
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

// Coordinated multi-axis motion (simple - independent axes)
void MultiAxis_MoveCoordinated(int32_t steps[NUM_AXES]);

// Time-synchronized coordinated motion (RECOMMENDED)
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
