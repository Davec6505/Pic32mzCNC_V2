# Coordinate System Implementation Plan
## Date: October 17, 2025

## Problem Identified

**Root Cause**: Motion buffer converts parsed moves to absolute step positions without accounting for:
1. Current machine position
2. G92 coordinate offset (non-persistent)
3. Work coordinate system offsets G54-G59 (persistent)
4. Incremental vs absolute mode (G90/G91)

**Result**: Commands like `G92 X0 Y0 Z0` followed by `G1 X10` produce zero motion because:
- Parser sets G92 offset correctly
- Motion buffer ignores the offset
- Calculates: target=10mm → 800 steps (absolute)
- Should calculate: delta from current position!

## GRBL v1.1f Coordinate System Specification

### Machine Coordinates (MPos)
- Absolute position relative to machine home (0,0,0 at homing switches)
- Never affected by offsets
- Reported in status reports: `<Idle|MPos:10.000,5.000,0.000|...>`

### Work Coordinates (WPos)
- Position relative to current work coordinate system
- Formula: **WPos = MPos - G54_offset - G92_offset**
- Reported in status reports: `<Idle|WPos:0.000,0.000,0.000|...>`

### Coordinate Systems (Persistent in EEPROM)
- **G54-G59**: 6 work coordinate systems
- **G28/G30**: 2 predefined positions
- Stored as offsets from machine coordinates
- Survive reset/power cycle

### Temporary Offset (Non-Persistent)
- **G92**: Temporary coordinate offset
- Cleared on reset/power cycle
- Added to work coordinate system offset

### Commands
- **G10 L2 Px**: Set work coordinate system offset (P1=G54, P2=G55, etc.)
- **G10 L20 Px**: Set work coordinate system to current position
- **G28.1**: Save current position as G28 predefined position
- **G30.1**: Save current position as G30 predefined position
- **G92**: Set work coordinate offset (makes current position appear as commanded values)
- **G92.1**: Clear G92 offset
- **G53**: Move in machine coordinates (one-shot, doesn't change modal state)

## Implementation Strategy (MISRA C Compliant - Single Source of Truth)

### Critical Design Constraints

1. **NO duplicate state** - Follow existing architecture
2. **Position in steps**: ALREADY EXISTS in `multiaxis_control.c` via `axis_state[].step_count` (volatile, updated by OCR ISR)
3. **GRBL settings**: ALREADY EXISTS in `motion_math.c` via global `motion_settings`
4. **Coordinate offsets**: ADD to `motion_math.c` (follows same pattern as `motion_settings`)

### Phase 1: Add Coordinate Offset Storage (motion_math.c)

**Add to motion_math.c (after motion_settings declaration)**:
```c
// *****************************************************************************
// Global Coordinate System Instance (GRBL v1.1f)
// *****************************************************************************

/**
 * @brief Work coordinate system offsets (G54-G59) - PERSISTENT
 * 
 * These are stored in EEPROM and survive reset/power cycle.
 * Each WCS is an offset from machine coordinates.
 * 
 * Formula: WPos = MPos - work_offsets[active_wcs] - g92_offset
 */
float work_offsets[6][NUM_AXES] = {{0.0f}};  // 6 WCS (G54-G59) × 4 axes

/**
 * @brief G28/G30 predefined positions - PERSISTENT
 * 
 * Stored in machine coordinates, not offsets.
 */
float predefined_positions[2][NUM_AXES] = {{0.0f}};  // 2 positions × 4 axes

/**
 * @brief G92 temporary coordinate offset - NON-PERSISTENT
 * 
 * Cleared on reset/power cycle.
 */
float g92_offset[NUM_AXES] = {0.0f};  // 4 axes

/**
 * @brief Active work coordinate system (0=G54, 1=G55, ..., 5=G59)
 */
uint8_t active_wcs = 0U;  // Default: G54
```

### Phase 2: Position Conversion Functions (motion_math.h/c)

**Add to motion_math.h** (extern declarations for coordinate offset globals):
```c
// Coordinate System Global Variables (defined in motion_math.c)
extern float work_offsets[6][NUM_AXES];          // G54-G59 offsets
extern float predefined_positions[2][NUM_AXES];  // G28/G30 positions
extern float g92_offset[NUM_AXES];               // G92 temporary offset
extern uint8_t active_wcs;                       // Active WCS (0-5)
```

**Add to motion_math.h** (function declarations):
```c
```c
/**
 * @brief Convert work coordinates to machine coordinates
 * 
 * Formula: MPos = WPos + current_wcs_offset + g92_offset
 * 
 * @param work_pos Work position in mm
 * @param axis Axis ID
 * @return Machine position in mm
 */
float MotionMath_WorkToMachine(float work_pos, axis_id_t axis);

/**
 * @brief Convert machine coordinates to work coordinates
 * 
 * Formula: WPos = MPos - current_wcs_offset - g92_offset
 * 
 * @param machine_pos Machine position in mm
 * @param axis Axis ID
 * @return Work position in mm
 */
float MotionMath_MachineToWork(float machine_pos, axis_id_t axis);

/**
 * @brief Get current work position for status reports
 * 
 * @param axis Axis ID
 * @return Current work position in mm
 */
float MotionMath_GetWorkPosition(axis_id_t axis);

/**
 * @brief Get current machine position for status reports
 * 
 * @param axis Axis ID
 * @return Current machine position in mm
 */
float MotionMath_GetMachinePosition(axis_id_t axis);
```

### Phase 3: Fix Motion Buffer Planning (motion_buffer.c)

**CRITICAL**: Use existing `MultiAxis_GetStepCount()` for current position!

**Update motion_buffer.c `plan_buffer_line()`**:
```c
static void plan_buffer_line(motion_block_t *block, const parsed_move_t *move)
{
    memset(block, 0, sizeof(motion_block_t));
    
    // Calculate target position in machine coordinates
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++) {
        if (move->axis_words[axis]) {
            // Get current position in steps (from multiaxis_control.c)
            int32_t current_pos_steps = (int32_t)MultiAxis_GetStepCount(axis);
            
            // Convert work coordinates to machine coordinates
            float target_mm_machine = MotionMath_WorkToMachine(move->target[axis], axis);
            int32_t target_pos_steps = MotionMath_MMToSteps(target_mm_machine, axis);
            
            // Calculate DELTA (this is what we actually move!)
            block->steps[axis] = target_pos_steps - current_pos_steps;
            block->axis_active[axis] = true;
        } else {
            block->steps[axis] = 0;
            block->axis_active[axis] = false;
        }
    }
    
    // Rest of planning (feedrate, junction velocity, etc.)...
}
```

### Phase 4: G-code Parser Integration

**Update gcode_parser.c**:
```c
// G92 handler - set coordinate offset
static bool GCode_HandleG92_CoordinateOffset(const gcode_line_t *tokenized_line)
{
    // Get current machine position
    // Calculate offset to make current position appear as commanded values
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++) {
        if (axis_specified[axis]) {
            float current_work_pos = MotionMath_GetWorkPosition(axis);
            modal_state.g92_offset[axis] = current_work_pos - axis_values[axis];
        }
    }
    // Notify motion system of offset change
    MotionMath_SetG92Offset(modal_state.g92_offset);
}

// G54-G59 handler - select work coordinate system
static bool GCode_HandleWorkCoordinateSystem(uint8_t wcs_number)
{
    // wcs_number: 0=G54, 1=G55, etc.
    MotionMath_SetActiveWCS(wcs_number);
    modal_state.active_wcs = wcs_number;
}

// G10 L2 handler - set work coordinate offset
static bool GCode_HandleG10_L2(const gcode_line_t *tokenized_line)
{
    // P value specifies which WCS (P1=G54, P2=G55, etc.)
    // Axis values specify the offset
    uint8_t wcs = p_value - 1;  // Convert P1-P6 to 0-5
    MotionMath_SetWorkOffset(wcs, axis_values);
}
```

### Phase 5: Status Report Updates

**Update gcode_parser.c status report**:
```c
case GCODE_CTRL_STATUS_REPORT: {
    // Get positions from motion system
    float mpos_x = MotionMath_GetMachinePosition(AXIS_X);
    float mpos_y = MotionMath_GetMachinePosition(AXIS_Y);
    float mpos_z = MotionMath_GetMachinePosition(AXIS_Z);
    
    float wpos_x = MotionMath_GetWorkPosition(AXIS_X);
    float wpos_y = MotionMath_GetWorkPosition(AXIS_Y);
    float wpos_z = MotionMath_GetWorkPosition(AXIS_Z);
    
    const char *state = MultiAxis_IsBusy() ? "Run" : "Idle";
    
    // Send status with both MPos and WPos
    snprintf(buffer, sizeof(buffer),
             "<%s|MPos:%.3f,%.3f,%.3f|WPos:%.3f,%.3f,%.3f>\r\n",
             state, mpos_x, mpos_y, mpos_z, wpos_x, wpos_y, wpos_z);
    UGS_Print(buffer);
}
```

### Phase 6: $# Command Implementation

**Add to main.c**:
```c
else if (line_buffer[1] == '#') {
    // $# - View coordinate parameters
    MotionMath_PrintCoordinateParameters();
    UGS_SendOK();
}
```

**Add to motion_math.c**:
```c
void MotionMath_PrintCoordinateParameters(void)
{
    // Print G54-G59 offsets
    for (uint8_t i = 0; i < 6; i++) {
        UGS_Printf("[G%d:%.3f,%.3f,%.3f]\r\n", 
                   54 + i,
                   work_offsets[i][AXIS_X],
                   work_offsets[i][AXIS_Y],
                   work_offsets[i][AXIS_Z]);
    }
    
    // Print G28/G30 positions
    UGS_Printf("[G28:%.3f,%.3f,%.3f]\r\n",
               predefined_positions[0][AXIS_X],
               predefined_positions[0][AXIS_Y],
               predefined_positions[0][AXIS_Z]);
    
    UGS_Printf("[G30:%.3f,%.3f,%.3f]\r\n",
               predefined_positions[1][AXIS_X],
               predefined_positions[1][AXIS_Y],
               predefined_positions[1][AXIS_Z]);
    
    // Print G92 offset
    UGS_Printf("[G92:%.3f,%.3f,%.3f]\r\n",
               g92_offset[AXIS_X],
               g92_offset[AXIS_Y],
               g92_offset[AXIS_Z]);
    
    // Print tool length offset (TLO) - not implemented yet
    UGS_Printf("[TLO:0.000]\r\n");
    
    // Print probe result (PRB) - not implemented yet
    UGS_Printf("[PRB:0.000,0.000,0.000:0]\r\n");
}
```

## Testing Plan

### Test 1: G92 Basic Functionality
```gcode
G90          ; Absolute mode
G0 X10 Y10   ; Move to (10,10) machine coords
G92 X0 Y0    ; Set current position as work zero
?            ; Should show WPos:0.000,0.000 MPos:10.000,10.000
G1 X5 Y5     ; Move to (5,5) in work coords
?            ; Should show WPos:5.000,5.000 MPos:15.000,15.000
G92.1        ; Clear G92 offset
?            ; Should show WPos:15.000,15.000 MPos:15.000,15.000
```

### Test 2: Work Coordinate Systems
```gcode
G90
G0 X0 Y0                    ; Move to origin
G10 L20 P1 X0 Y0            ; Set G54 to current position
G0 X50 Y50                  ; Move away
G10 L20 P2 X0 Y0            ; Set G55 to (50,50)
G54                         ; Select G54
?                           ; Should show WPos near 50,50
G55                         ; Select G55
?                           ; Should show WPos near 0,0
```

### Test 3: Square Pattern (Original Problem)
```gcode
G90
G0 X0 Y0
G92 X0 Y0      ; Set work zero
G1 Y10 F1000   ; Should move to Y=10
?              ; Verify position
G1 X10         ; Should move to X=10, Y=10
?              ; Verify position
G1 Y0          ; Should move to X=10, Y=0
?              ; Verify position
G1 X0          ; Should return to origin
?              ; Should show WPos:0.000,0.000
```

## Implementation Order

1. **Add machine_state_t to motion_types.h** ✅
2. **Initialize machine state in multiaxis_control.c** ✅
3. **Add position tracking functions to motion_math.c** ✅
4. **Fix plan_buffer_line() in motion_buffer.c** ✅
5. **Update G92 handler in gcode_parser.c** ✅
6. **Add G54-G59 handlers** ✅
7. **Add G10 L2/L20 handlers** ✅
8. **Update status report** ✅
9. **Implement $# command** ✅
10. **Test with square pattern** ✅

## Files to Modify

1. `incs/motion/motion_types.h` - Add machine_state_t
2. `srcs/motion/multiaxis_control.c` - Initialize and update machine state
3. `srcs/motion/motion_math.c` - Add coordinate conversion functions
4. `incs/motion/motion_math.h` - Add function declarations
5. `srcs/motion/motion_buffer.c` - Fix plan_buffer_line()
6. `srcs/gcode_parser.c` - Update G92, add G54-G59, G10, status report
7. `srcs/main.c` - Add $# command handler

## Estimated Effort

- Code changes: ~500 lines
- Testing: 2-3 hours
- Documentation: 1 hour
- **Total**: 4-5 hours development time

## Success Criteria

✅ G92 X0 Y0 Z0 followed by G1 Y10 produces actual 10mm motion  
✅ Status reports show both MPos and WPos correctly  
✅ Work coordinate systems G54-G59 function properly  
✅ $# command displays all offsets  
✅ Square pattern test produces accurate 10mm×10mm square  
✅ Position tracking persists across multiple commands  

## Notes

- This is a FUNDAMENTAL architectural fix required for GRBL compliance
- All future motion commands depend on this working correctly
- Without this, the CNC cannot function properly
- Implementation must be done carefully to avoid breaking existing functionality
