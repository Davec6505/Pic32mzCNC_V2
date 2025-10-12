# CNC Position Feedback Testing Guide

## Overview
This test suite verifies the position feedback system with slow, observable motion commands.

## Test Configuration
- **Default Velocity**: Reduced to 50 steps/sec for clear observation
- **Enhanced Status**: Shows position, velocity, and step counts
- **Debug Command**: `DEBUG` for detailed motion information

## Running the Extended Test

### Method 1: Automated Test Sequence
```powershell
.\extended_feedback_test.ps1
```

This runs a comprehensive 20-command sequence with:
- System initialization and settings check
- Origin positioning 
- Slow incremental moves (X, Y, Z axes)
- Diagonal movements
- Return to origin
- Automatic status reporting after each motion

### Method 2: Manual Testing
```powershell
.\serial_test.ps1
```

Then manually send these commands (wait for completion between each):

#### Basic Test Sequence
1. `$$` - Check system settings
2. `?` - Initial status report
3. `G0 X0 Y0 Z0` - Move to origin
4. `F50` - Set very slow feed rate (50 mm/min)
5. `G1 X10` - Move X axis slowly
6. `?` - Check position (should show X=10.000)
7. `G1 Y5` - Move Y axis slowly  
8. `?` - Check position (should show Y=5.000)
9. `G1 Z2` - Move Z axis slowly
10. `?` - Check position (should show Z=2.000)
11. `DEBUG` - Detailed motion information

#### Advanced Test Commands
- `G1 X20 Y10` - Diagonal move
- `G1 X0 Y0` - Return to XY origin
- `G1 Z0` - Return Z to origin
- `DEBUG` - Final detailed status

## Expected Output

### Enhanced Status Report Format
```
<Idle|MPos:10.000,5.000,2.000|FS:0.0,50|Steps:500,250,100>
```

**Format Breakdown:**
- `Idle` - Current state (Idle/Run/Hold/etc.)
- `MPos:X,Y,Z` - Machine position in mm
- `FS:velocity,feedrate` - Current velocity and feed rate
- `Steps:X,Y,Z` - Accumulated step counts per axis

### Debug Command Output
```
[DEBUG] InterpPos:10.000,5.000,2.000 AxisPos:500,250,100 | Steps:500,250,100 | Vel:0.0 Active:1,1,1
```

**Debug Breakdown:**
- `InterpPos` - Interpolation engine position (mm)
- `AxisPos` - Direct axis position (steps)  
- `Steps` - Step counters
- `Vel` - Current velocity
- `Active` - Axis active states (0/1)

## Verification Points

### Position Accuracy
✅ **Machine Position** matches commanded moves
✅ **Step Counts** increment correctly (50 steps/mm assumed)
✅ **Interpolation Position** matches axis positions

### Motion Feedback
✅ **Status during motion** shows "Run" state
✅ **Status after motion** shows "Idle" state
✅ **Velocity feedback** shows current motion speed
✅ **Step counting** tracks each individual step pulse

### Real-Time Performance
✅ **OCR callbacks** update positions without delays
✅ **1kHz trajectory updates** maintain smooth motion
✅ **Position synchronization** between modules

## Troubleshooting

### No Position Updates
- Check OCR callback functions are enabled
- Verify step pulse generation is active
- Confirm position feedback calls are working

### Incorrect Step Counts
- Verify steps-per-mm configuration
- Check direction signals
- Ensure step pulse width is correct

### Motion Too Fast to Observe
- Reduce feed rate further: `F25` or `F10`
- Increase delay in test script
- Use smaller move distances

### Status Not Updating
- Send `?` command manually
- Check UART communication
- Verify status report function

## Performance Notes
- **OCR Callbacks**: Optimized for speed using direct access
- **Real-Time Sections**: No function call overhead in critical paths
- **Non-Critical Code**: Uses getter/setter functions for readability
- **Test Configuration**: Temporarily slow for observation

## Restoring Normal Speed
After testing, restore normal operating speeds:

In `app.h` and `motion_gcode_parser.h`:
```c
#define DEFAULT_MAX_VELOCITY 1000.0f // Restore normal speed
```

## Test Results Documentation
Record your observations:
- [ ] Position feedback accuracy
- [ ] Step count consistency  
- [ ] Real-time performance
- [ ] Status report completeness
- [ ] Motion smoothness
- [ ] Debug command functionality