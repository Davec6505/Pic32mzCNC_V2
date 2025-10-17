# Universal G-code Sender (UGS) Reference Guide

## Purpose
This document provides quick reference information for Universal G-code Sender (UGS) integration with the PIC32MZ CNC Motion Controller V2. It consolidates essential connection procedures, GRBL protocol details, and troubleshooting guidance specific to this project.

**UGS Wiki**: https://github.com/winder/Universal-G-Code-Sender/wiki

---

## Quick Start - Connecting to PIC32MZ CNC Controller

### Connection Settings
- **Firmware**: Select "GRBL" from firmware combo box
- **Port**: Windows: `COM4` (or check Device Manager for correct COM port)
- **Baud Rate**: `115200` (GRBL v1.1f standard)
- **Driver**: Default `JSerialComm` (serial USB connection)

### Connection Steps
1. Power on PIC32MZ CNC controller board
2. Connect USB cable to PC
3. Open UGS Platform or UGS Classic
4. Select firmware: **GRBL**
5. Select port: **COMx** (Windows) or `/dev/ttyACMx` (Linux) or `/dev/tty.usbmodemxxxx` (Mac)
6. Select baud rate: **115200**
7. Click **Connect**
8. Wait for connection confirmation (status shows "Idle")
9. UGS will automatically send initialization commands (`?`, `$I`, `$$`, `$G`)

---

## GRBL Protocol Overview

### Connection Protocol
GRBL uses a **simple send-response** protocol for initial testing and a **character-counting** protocol for continuous operation:

#### Simple Send-Response (Phase 1 - Current Implementation)
- Host sends one command
- Wait for controller to finish motion and send "ok"
- Then send next command
- **Blocking behavior**: Motion completes before "ok" sent
- **Advantage**: Reliable, easy to implement
- **Disadvantage**: Pauses between moves (not continuous motion)

#### Character-Counting Protocol (Phase 2 - Future)
- Host tracks 128-byte RX buffer on controller
- Send multiple commands without waiting for completion
- Controller buffers commands and executes continuously
- **Advantage**: Smooth continuous motion through corners
- **Disadvantage**: More complex flow control

### Real-Time Commands (Single Byte, Immediate Response)
These bypass the command buffer and execute immediately:

| Command | Function | Description |
|---------|----------|-------------|
| `?` | Status Report | Returns machine state, position, feed rate (immediate) |
| `!` | Feed Hold | Pause motion (emergency stop without losing position) |
| `~` | Cycle Start | Resume motion after feed hold |
| `^X` | Soft Reset | Emergency stop, clear all buffers, reset machine |

**Usage in UGS**: These commands can be sent anytime, even during motion.

### System Commands ($ Commands)
GRBL system commands for configuration and status queries:

| Command | Function | Response |
|---------|----------|----------|
| `$` | Help | List all $ commands |
| `$I` | Version | "GRBL 1.1f ['$' for help]" |
| `$G` | Parser State | Current modal state (G90, G54, M5, etc.) |
| `$$` | View Settings | All GRBL settings ($100-$133) |
| `$#` | View Offsets | Work coordinate systems (G54-G59) |
| `$N` | View Startup Lines | Auto-run commands on startup |
| `$100-$133` | View Setting | Individual setting value |
| `$100=200` | Set Value | Modify setting on-the-fly |

**Important**: Settings changes take effect immediately (no reboot required).

### Status Report Format
When `?` is sent, GRBL responds with machine state:

```
<Idle|MPos:0.000,0.000,0.000,0.000|FS:0,0>
```

**Format Breakdown**:
- `<Idle|...>` - Enclosed in angle brackets
- `Idle` - Machine state (Idle, Run, Jog, Alarm, Hold, etc.)
- `MPos:` - Machine position (absolute from home)
- `WPos:` - Work position (relative to work coordinate offset) [optional]
- `FS:` - Feed rate and spindle speed (mm/min, RPM)

**Machine States**:
- **Idle**: Not running, ready for commands
- **Run**: Executing G-code program
- **Jog**: Manual jogging mode
- **Alarm**: Triggered by limit switch or error (requires `$X` unlock)
- **Hold**: Feed hold active (motion paused)
- **Home**: Homing cycle in progress
- **Check**: G-code check mode (no motion)

---

## GRBL Settings Reference (Relevant to PIC32MZ CNC)

### Steps per mm ($100-$103)
**Default**: 80 steps/mm (GT2 belt), 1280 steps/mm (Z-axis leadscrew)

```gcode
$100=80.0    ; X-axis steps/mm (GT2 belt: 20 teeth × 2mm pitch × 1/16 microstep)
$101=80.0    ; Y-axis steps/mm
$102=1280.0  ; Z-axis steps/mm (2.5mm leadscrew: 200 steps × 1/16 × 1.6 ratio)
$103=80.0    ; A-axis steps/mm (rotary axis)
```

### Max Rate ($110-$113) - mm/min
**Default**: Conservative for testing (5000 XY, 2000 Z)

```gcode
$110=5000.0  ; X-axis max rate (mm/min)
$111=5000.0  ; Y-axis max rate
$112=2000.0  ; Z-axis max rate (slower for precision)
$113=5000.0  ; A-axis max rate
```

### Acceleration ($120-$123) - mm/sec²
**Default**: Conservative for testing (500 XY, 200 Z)

```gcode
$120=500.0   ; X-axis acceleration (mm/sec²)
$121=500.0   ; Y-axis acceleration
$122=200.0   ; Z-axis acceleration (slower for safety)
$123=500.0   ; A-axis acceleration
```

### Max Travel ($130-$133) - mm
**Default**: Machine work envelope limits

```gcode
$130=300.0   ; X-axis max travel (mm)
$131=300.0   ; Y-axis max travel (mm)
$132=100.0   ; Z-axis max travel (mm)
$133=360.0   ; A-axis max travel (degrees for rotary axis)
```

### Other Important Settings
```gcode
$11=0.010    ; Junction deviation (mm) - cornering speed optimization
$12=0.002    ; Arc tolerance (mm) - G2/G3 accuracy
$13=0        ; Report inches (0=mm, 1=inches)
$20=0        ; Soft limits (0=disabled, 1=enabled)
$21=0        ; Hard limits (0=disabled, 1=enabled)
$22=0        ; Homing cycle (0=disabled, 1=enabled)
$23=0        ; Homing direction invert mask (binary)
$24=25.0     ; Homing feed rate (mm/min)
$25=500.0    ; Homing seek rate (mm/min)
$27=1.0      ; Homing pull-off distance (mm)
```

---

## UGS Platform User Interface

### Main Toolbar (Top)
- **Firmware Combo**: Select controller type (GRBL, TinyG, g2core, etc.)
- **Port Selector**: Choose serial port (auto-refresh in newer versions)
- **Baud Rate**: Connection speed (115200 for GRBL v1.1f)
- **Connect/Disconnect**: Establish/close connection

### Digital Read-Out (DRO) Panel
Shows real-time machine status:
- **Work Coordinates (WPos)**: Position relative to work zero
- **Machine Coordinates (MPos)**: Absolute position from home
- **Zero Buttons**: Set work coordinate offset for each axis (G92)
- **Machine State**: Current status (Idle, Run, Alarm, etc.)
- **Feed Rate**: Current speed (mm/min)
- **Spindle Speed**: Current RPM
- **GCode State**: Active modal commands (G90, G54, M5, etc.)

### Visualizer Window
3D preview of loaded G-code:
- **Rotate**: Left mouse button drag
- **Pan**: Shift + left mouse button
- **Zoom**: Mouse wheel
- **Tool Position**: Yellow cone shows current position
- **Units Toggle**: Switch between mm/inches

### Machine Actions
Common operations available in toolbar:
- **Reset Zero**: Set current position as work zero (G92 X0 Y0 Z0)
- **Return to Zero**: Move to work coordinate origin (G90 G0 X0 Y0 Z0)
- **Soft Reset**: Emergency stop and reset controller (^X)
- **Unlock**: Clear alarm state ($X)
- **Home**: Run homing cycle ($H)

### Overrides
Real-time adjustments during program execution:
- **Feed Rate Override**: Adjust speed (50%-200%)
- **Spindle Speed Override**: Adjust RPM
- **Rapid Override**: Adjust G0 moves

---

## Testing Commands for PIC32MZ CNC

### Initial Connection Test
```gcode
?               ; Query status (should return <Idle|MPos:...>)
$I              ; Get version (should return "GRBL 1.1f [...]")
$$              ; View all settings (should show $100-$133)
$G              ; Get parser state (should show G90, G54, etc.)
```

### Simple Motion Test (Absolute Mode)
```gcode
G90             ; Absolute positioning mode
G0 X10 Y10      ; Rapid move to (10,10)
?               ; Check position during move
G1 X20 F1000    ; Linear move to X=20 @ 1000mm/min
G0 X0 Y0        ; Return to origin
```

### Relative Motion Test
```gcode
G91             ; Relative positioning mode
G1 X5 F500      ; Move 5mm positive X
G1 Y-3 F500     ; Move 3mm negative Y
G90             ; Back to absolute mode
```

### Work Coordinate Offset Test
```gcode
G92 X0 Y0 Z0    ; Set current position as (0,0,0)
?               ; Verify WPos now shows (0,0,0)
G1 X10 F1000    ; Move 10mm in X
G92 X5          ; Change work offset (actual position now reads X=5)
```

### Multi-Axis Coordinated Move
```gcode
G90             ; Absolute mode
G1 X50 Y50 Z5 F1500  ; Coordinated move (all axes finish together)
```

### Slow Z-Axis Test (Verify Timer Prescaler Fix)
```gcode
G90             ; Absolute mode
G1 Z1 F60       ; Move 1mm @ 60mm/min (1mm/sec = 1,280 steps/sec)
; Expected period: 1,562,500 / 1,280 = 1,221 counts (780µs) ✓ FITS!
; Should move CORRECTLY, not 2-3x too fast
```

---

## Troubleshooting

### Connection Issues

**Problem**: UGS can't find COM port
- **Solution**: Check Device Manager (Windows) or `ls /dev/tty*` (Linux/Mac)
- Verify USB cable is data-capable (not charge-only)
- Install CH340 drivers if using clone Arduino boards

**Problem**: "Port in use" error
- **Solution**: Close all other serial terminals/Arduino IDE
- Restart UGS
- Unplug/replug USB cable

**Problem**: Connection established but no response
- **Solution**: Verify baud rate is 115200
- Try soft reset (^X)
- Check power to PIC32MZ board (LED2 should be ON)

### Motion Issues

**Problem**: Steppers run too fast (2-3x expected speed)
- **Root Cause**: Timer prescaler set to 1:2 causing 16-bit overflow
- **Solution**: Update MCC prescalers (TMR2/3/4/5) to **1:16** (see TIMER_PRESCALER_ANALYSIS.md)
- Verify TMR_CLOCK_HZ = 1562500UL in motion_types.h
- Test with slow Z-axis: `G1 Z1 F60`

**Problem**: Motion pauses between moves (not continuous)
- **Expected Behavior**: Phase 1 uses Simple Send-Response protocol (blocking)
- Motion completes before "ok" sent - this is CORRECT for current implementation
- **Future**: Switch to Character-Counting protocol for continuous motion (Phase 2)

**Problem**: Stepper motors not moving
- **Solution**: Check DRV8825 ENABLE pins (should be LOW or disconnected)
- Verify STEP/DIR pins connected correctly
- Check stepper motor wiring (coil pairs)
- Measure VREF on DRV8825 for current limit

**Problem**: Position drift or incorrect distances
- **Solution**: Verify steps/mm settings ($100-$103)
- Check mechanical backlash/belt tension
- Verify microstepping mode (MODE0/1/2 pins on DRV8825)
- Use oscilloscope to confirm OCR pulse accuracy

### Alarm States

**Problem**: GRBL reports "ALARM:1" (Hard limit triggered)
- **Solution**: Send `$X` to unlock
- Check limit switch wiring (active LOW)
- Verify soft limits disabled ($20=0) for testing
- Move away from limit manually, then unlock

**Problem**: GRBL reports "ALARM:2" (Soft limit exceeded)
- **Solution**: Send `$X` to unlock
- Disable soft limits: `$20=0`
- Reset work coordinate: `G92 X0 Y0 Z0`

**Problem**: Controller stuck in "Hold" state
- **Solution**: Send `~` (cycle start) to resume
- Or send `^X` (soft reset) to fully clear

### Real-Time Command Testing

**Test Feed Hold**:
```gcode
G1 X100 F500    ; Start long move
!               ; Send feed hold (should pause immediately)
?               ; Verify state is "Hold"
~               ; Cycle start (should resume motion)
```

**Test Soft Reset**:
```gcode
G1 X100 F500    ; Start long move
^X              ; Soft reset (should stop immediately, clear buffers)
?               ; Verify state is "Idle"
```

---

## Hardware-Specific Notes (PIC32MZ CNC V2)

### Timer Configuration (CRITICAL - October 2025)
**TIMER PRESCALER FIX APPLIED**:
- Old: 1:2 prescaler (12.5MHz) - CAUSED 16-BIT OVERFLOW
- New: 1:16 prescaler (1.5625MHz) - PREVENTS OVERFLOW
- Resolution: 640ns per count
- Pulse Width: 40 counts = 25.6µs (exceeds DRV8825 1.9µs minimum)
- Step Rate Range: 23.8 to 31,250 steps/sec (all speeds fit in 16-bit timer)

**MCC Action Required**:
- Open MPLAB X → MCC
- Update TMR2 prescaler to 1:16 (X-axis)
- Update TMR3 prescaler to 1:16 (Z-axis)
- Update TMR4 prescaler to 1:16 (Y-axis)
- Update TMR5 prescaler to 1:16 (A-axis)
- Regenerate code
- Build: `make all`
- Flash: `bins/CS23.hex`

### Motion Architecture
- **TMR1 @ 1kHz**: Main motion control loop (S-curve interpolation)
- **OCR Modules**: Hardware pulse generation (no software interrupts)
  - OCMP4 + TMR2: X-axis step pulses
  - OCMP1 + TMR4: Y-axis step pulses
  - OCMP5 + TMR3: Z-axis step pulses
  - OCMP3 + TMR5: A-axis step pulses
- **LED1**: Heartbeat @ 1Hz (idle) or solid (motion active)
- **LED2**: Power-on indicator

### DRV8825 Stepper Drivers
- **STEP Pin**: Active HIGH pulse (40 counts @ 1.5625MHz = 25.6µs)
- **DIR Pin**: Must be set BEFORE first step pulse
- **ENABLE**: Active LOW (can leave disconnected for always-enabled)
- **Current Limit**: Set via VREF potentiometer (I = VREF × 2)
- **Microstepping**: Configure MODE0/1/2 pins (default 1/16 step)

### Known Working Commands
```gcode
$                    ; Show GRBL settings
?                    ; Status report (real-time)
!                    ; Feed hold (pause)
~                    ; Cycle start (resume)
^X                   ; Soft reset
G90                  ; Absolute mode
G91                  ; Relative mode  
G0 X10 Y20 F1500     ; Rapid positioning
G1 X50 Y50 F1000     ; Linear move
G92 X0 Y0 Z0         ; Set work coordinate offset
G28                  ; Go to predefined home
M3 S1000             ; Spindle on CW @ 1000 RPM (state tracking only)
M5                   ; Spindle off (state tracking only)
```

---

## Development Workflow

### Serial Testing with PowerShell
```powershell
# Basic motion testing
.\motion_test.ps1 -Port COM4 -BaudRate 115200

# UGS compatibility testing  
.\ugs_test.ps1 -Port COM4 -GCodeFile modular_test.gcode

# Real-time debugging
.\monitor_debug.ps1 -Port COM4
```

### Build and Flash
```bash
# From project root directory:
make all                    # Clean build with hex generation
make build_dir             # Create directory structure (run first)
make clean                 # Clean all outputs  
```

### Hardware Testing Checklist
1. ✅ Flash firmware (`bins/CS23.hex`)
2. ✅ Connect via UGS @ 115200 baud
3. ✅ Send `?` (status query)
4. ✅ Send `$I` (version check - should return "GRBL 1.1f")
5. ✅ Send `$$` (view all settings)
6. ✅ Send `$G` (parser state)
7. ✅ Test G-code moves: `G90`, `G1 X10 Y10 F1500`
8. ✅ Verify position feedback in UGS status window
9. ✅ Test real-time commands: `!`, `~`, `^X`
10. ✅ Test slow Z-axis: `G1 Z1 F60` (should move correctly, not 2-3x fast!)
11. ✅ Use oscilloscope to verify OCR periods (15,625 counts @ 100 steps/sec)

---

## References

- **UGS Wiki**: https://github.com/winder/Universal-G-Code-Sender/wiki
- **GRBL Wiki**: https://github.com/grbl/grbl/wiki
- **Timer Prescaler Analysis**: See `docs/TIMER_PRESCALER_ANALYSIS.md`
- **G-code Parser Implementation**: See `docs/GCODE_PARSER_COMPLETE.md`
- **PlantUML Architecture Diagrams**: See `docs/plantuml/README.md`

---

## Version History

- **October 17, 2025**: Initial creation - UGS integration complete, timer prescaler fix applied
- **Phase 1 Complete**: Simple Send-Response protocol, system commands, real-time position feedback
- **Phase 2 Planned**: Character-Counting protocol for continuous motion, look-ahead planning

---

**End of UGS Reference Guide**
