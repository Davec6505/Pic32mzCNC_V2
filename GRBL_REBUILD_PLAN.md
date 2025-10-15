# GRBL Rebuild Plan - PIC32MZ CNC Controller V2

**Date**: October 15, 2025  
**Branch**: `grbl-rebuild`  
**Goal**: Rebuild system using authentic GRBL v1.1f architecture with PIC32MZ OCR continuous pulse mode hardware

## Executive Summary

The modular architecture (master branch) works for manual commands but has fundamental issues with file streaming due to competing execution paths and interpolation engine vectorization. This rebuild will use GRBL's proven planner/stepper architecture adapted for PIC32MZ hardware.

## What We Keep

### ✅ Hardware Layer (WORKING - Keep 100%)
1. **OCR Dual-Compare Continuous Pulse Mode**
   - `OCMP1` (Y-axis) → `TMR4` timebase
   - `OCMP3` (A-axis) → `TMR5` timebase
   - `OCMP4` (X-axis) → `TMR2` timebase
   - `OCMP5` (Z-axis) → `TMR3` timebase
   - Proven pattern: `TMRx_PeriodSet()`, `OCMPx_CompareValueSet(period-40)`, `OCMPx_CompareSecondaryValueSet(40)`

2. **DRV8825 Driver Interface**
   - Direction pins: Set BEFORE step pulses
   - Step pulse width: 40 timer counts (40µs @ 1MHz)
   - Timer restart: `TMRx_Start()` for each move

3. **GPIO Configuration**
   - Limit switches (active low)
   - Direction control pins
   - LED heartbeat (100ms toggle)

### ✅ GRBL Protocol Layer (WORKING - Keep 100%)
- `srcs/grbl_serial.c` - Serial communication, character counting protocol
- `srcs/grbl_settings.c` - Settings storage ($100-$132)
- `srcs/gcode_helpers.c` - Position reporting
- `incs/grbl_config.h` - GRBL constants

### ✅ Build System & Tools (WORKING - Keep 100%)
- Cross-platform Makefile (Windows/Linux)
- PowerShell test scripts (`motion_test.ps1`, `ugs_test.ps1`)
- XC32 v4.60 compiler configuration
- MPLAB Harmony peripheral libraries

## What We Replace

### ❌ Motion Planning System (REPLACE with GRBL planner.c)

**Current Issues**:
- Dual execution paths (state machine + TMR1)
- Interpolation engine vectorizes blocks
- Custom buffer management doesn't match GRBL streaming protocol

**GRBL Solution**:
Replace these files with authentic GRBL code:
- `srcs/motion_planner.c` → `srcs/grbl_planner.c`
- `srcs/motion_buffer.c` → Integrate into `grbl_planner.c`
- `srcs/interpolation_engine.c` → `srcs/grbl_stepper.c`
- `srcs/motion_profile.c` → Part of `grbl_planner.c`

### ❌ G-code Parser (REPLACE with GRBL gcode.c)

**Current Issues**:
- Custom parser doesn't fully implement GRBL modal groups
- Arc handling incomplete

**GRBL Solution**:
- `srcs/motion_gcode_parser.c` → `srcs/grbl_gcode.c`
- Full GRBL v1.1f G-code parser with modal state tracking

## GRBL Architecture Adaptation

### Core GRBL Files to Port

```
grbl/                          PIC32MZ Adaptation
├── planner.c                  → srcs/grbl_planner.c
│   ├── 16-block ring buffer      ✅ Keep as-is
│   ├── Velocity planning         ✅ Keep junction deviation logic
│   └── Block optimization        ✅ Keep look-ahead planner
│
├── stepper.c                  → srcs/grbl_stepper.c (MAJOR ADAPTATION)
│   ├── Step segment buffer       ❌ REPLACE with OCR hardware
│   ├── Bresenham algorithm       ❌ REPLACE with OCR period calculation
│   └── ISR step execution        ❌ REPLACE with OCR callbacks
│
├── gcode.c                    → srcs/grbl_gcode.c
│   ├── Modal group parsing       ✅ Keep parser logic
│   ├── Target position calc      ✅ Keep coordinate transforms
│   └── Arc center calculation    ✅ Keep G2/G3 implementation
│
├── protocol.c                 → srcs/grbl_protocol.c
│   ├── Serial line parsing       ✅ Already working in grbl_serial.c
│   └── Real-time commands        ✅ Already working
│
└── limits.c                   → srcs/grbl_limits.c
    ├── Hard limit detection      ✅ Keep GPIO interrupt logic
    └── Homing cycle              ✅ Implement later
```

### Key Adaptation: stepper.c → OCR Hardware

**GRBL stepper.c ISR (AVR)**:
```c
ISR(TIMER1_COMPA_vect) {
    // Bresenham step decision
    st.counter_x += st.step_event_count;
    if (st.counter_x > st.exec_block->step_event_count) {
        STEP_PORT |= (1<<X_STEP_BIT);  // Pulse X axis
        st.counter_x -= st.exec_block->step_event_count;
    }
    // ...repeat for Y, Z axes
}
```

**Our PIC32MZ OCR Adaptation**:
```c
// INSTEAD: Calculate OCR period for each axis based on step rate
// Called from grbl_planner when block starts execution
void Stepper_SetAxisRate(uint8_t axis, float steps_per_sec) {
    uint32_t period = (uint32_t)(1000000.0f / steps_per_sec);
    
    switch(axis) {
        case X_AXIS:
            TMR2_PeriodSet(period);
            OCMP4_CompareValueSet(period - 40);
            OCMP4_CompareSecondaryValueSet(40);
            TMR2_Start();
            break;
        // ...repeat for Y, Z
    }
}

// OCR interrupt counts steps (replaces Bresenham accumulator)
void OCMP4_Callback(uintptr_t context) {
    st_block.step_count[X_AXIS]++;
    if (st_block.step_count[X_AXIS] >= st_block.steps[X_AXIS]) {
        TMR2_Stop();  // Axis motion complete
        OCMP4_Disable();
    }
}
```

### Data Flow Comparison

**GRBL (AVR)**:
```
Serial RX → protocol.c → gcode.c → planner.c (16 blocks)
                                        ↓
                            stepper.c (segment buffer)
                                        ↓
                            TIMER1 ISR (Bresenham step logic)
                                        ↓
                            GPIO bit manipulation (step pulses)
```

**Our System (PIC32MZ)**:
```
Serial RX → grbl_serial.c → grbl_gcode.c → grbl_planner.c (16 blocks)
                                                  ↓
                                      grbl_stepper.c (OCR management)
                                                  ↓
                                      OCR Hardware (automatic pulse generation)
                                                  ↓
                                      OCR Callbacks (step counting)
```

## Implementation Phases

### Phase 1: Foundation (Week 1)
**Goal**: Get GRBL planner integrated with existing hardware

1. **Port grbl/planner.c**
   - Copy GRBL v1.1f `planner.c` → `srcs/grbl_planner.c`
   - Keep all junction deviation, acceleration planning logic
   - Replace AVR-specific types with PIC32 equivalents
   - Keep 16-block ring buffer

2. **Minimal stepper.c Adapter**
   - Create `srcs/grbl_stepper.c` 
   - Implement `st_prep_buffer()` to calculate OCR periods
   - Implement `st_wake_up()`, `st_go_idle()` interface
   - **Don't implement Bresenham** - use OCR hardware instead

3. **Testing**
   - Feed simple G1 moves through planner
   - Verify block optimization works
   - Check junction velocity calculations

**Success Criteria**: Planner fills 16-block buffer correctly

### Phase 2: G-code Parser (Week 2)
**Goal**: Full GRBL G-code compatibility

1. **Port grbl/gcode.c**
   - Copy GRBL v1.1f `gcode.c` → `srcs/grbl_gcode.c`
   - Keep modal state machine
   - Keep arc center calculations
   - Adapt coordinate system transforms

2. **Testing**
   - Parse full G-code programs
   - Verify modal state transitions
   - Test G2/G3 arc commands

**Success Criteria**: UGS can send full G-code programs without errors

### Phase 3: Stepper Execution (Week 3)
**Goal**: OCR-based step generation with proper sequencing

1. **Implement Segment Preparation**
   ```c
   void st_prep_buffer() {
       // Get current planner block
       plan_block_t *pl_block = plan_get_current_block();
       
       // Calculate step rates for each axis
       for (axis = 0; axis < N_AXIS; axis++) {
           float steps_per_sec = pl_block->steps[axis] / pl_block->millimeters 
                               * current_speed;
           Stepper_SetAxisRate(axis, steps_per_sec);
       }
   }
   ```

2. **OCR Callback Step Counting**
   - Each OCR interrupt increments axis step counter
   - When target reached, stop that axis's timer
   - When all axes complete, advance to next planner block

3. **Testing**
   - Single-axis moves
   - Multi-axis coordinated motion
   - Acceleration/deceleration profiles

**Success Criteria**: Smooth coordinated motion matching GRBL behavior

### Phase 4: Acceleration Profiles (Week 4)
**Goal**: Proper velocity ramping using planner data

1. **Implement Velocity Updates**
   - Call `st_prep_buffer()` from TMR1 @ 1kHz
   - Recalculate OCR periods based on acceleration phase
   - Smoothly transition between blocks

2. **Junction Handling**
   - Use planner's entry/exit speeds
   - Update OCR rates at block boundaries

**Success Criteria**: No stuttering at junctions, smooth motion

### Phase 5: Polish & Production (Week 5)
**Goal**: Production-ready firmware

1. **Remove Debug Code**
   - Strip all `[DEBUG]`, `[ADD]`, `[EXEC]` messages
   - Keep only GRBL protocol responses

2. **Performance Tuning**
   - Optimize OCR period calculations
   - Minimize ISR latency

3. **Documentation**
   - Update copilot-instructions.md
   - Create GRBL_ARCHITECTURE.md
   - Document OCR adaptation strategy

## File Structure (After Rebuild)

```
Pic32mzCNC_V2/
├── srcs/
│   ├── main.c                    (Keep - initialization)
│   ├── app.c                     (Keep - hardware abstraction)
│   │
│   ├── grbl_planner.c            (NEW - GRBL planner.c port)
│   ├── grbl_stepper.c            (NEW - OCR adaptation of stepper.c)
│   ├── grbl_gcode.c              (NEW - GRBL gcode.c port)
│   ├── grbl_protocol.c           (NEW - GRBL protocol.c port)
│   ├── grbl_limits.c             (NEW - GRBL limits.c port)
│   │
│   ├── grbl_serial.c             (Keep - already working)
│   ├── grbl_settings.c           (Keep - already working)
│   ├── gcode_helpers.c           (Keep - status reports)
│   │
│   └── [DELETE]
│       ├── motion_planner.c
│       ├── motion_buffer.c
│       ├── interpolation_engine.c
│       ├── motion_gcode_parser.c
│       └── motion_profile.c
│
├── incs/
│   ├── grbl_planner.h            (NEW)
│   ├── grbl_stepper.h            (NEW)
│   ├── grbl_gcode.h              (NEW)
│   ├── grbl_protocol.h           (NEW)
│   └── grbl_config.h             (Update with new constants)
│
└── docs/
    ├── GRBL_ARCHITECTURE.md      (NEW - architecture documentation)
    └── OCR_ADAPTATION.md         (NEW - stepper.c → OCR mapping)
```

## Success Metrics

### Functional Requirements
- ✅ Manual commands work (single-axis sequential motion)
- ✅ File streaming works (coordinated multi-axis motion)
- ✅ Junction planning smooth (no stuttering)
- ✅ Acceleration profiles accurate (no lost steps)
- ✅ UGS compatibility (full GRBL v1.1f protocol)

### Performance Targets
- **Look-ahead buffer**: 16 blocks minimum
- **Step rate**: 30 kHz maximum (PIC32MZ @ 200MHz)
- **Planning rate**: >1000 blocks/second
- **Serial throughput**: 115200 baud character counting protocol

### Quality Gates
- **Phase 1**: Planner fills buffer without errors
- **Phase 2**: UGS accepts full G-code programs
- **Phase 3**: Motion executes without diagonal drift
- **Phase 4**: Smooth acceleration through junctions
- **Phase 5**: Production-ready (no debug output)

## Risk Mitigation

### Risk 1: OCR Hardware Limitations
**Risk**: OCR may not provide fine enough resolution for slow speeds  
**Mitigation**: Implement pulse count limiting (stop after N pulses instead of time-based)

### Risk 2: Multi-Axis Synchronization
**Risk**: Independent timers may drift during coordinated motion  
**Mitigation**: Use single "master" timer with axis-specific pulse counters

### Risk 3: GRBL Code Portability
**Risk**: AVR-specific code may not port cleanly to PIC32  
**Mitigation**: Start with core algorithms, adapt progressively

### Risk 4: Performance Bottlenecks
**Risk**: PIC32 may not keep up with GRBL planning speed  
**Mitigation**: PIC32MZ @ 200MHz is 12.5x faster than Arduino Mega @ 16MHz

## Next Steps

1. ✅ **Commit current master branch** (COMPLETE)
2. ✅ **Create grbl-rebuild branch** (COMPLETE)
3. ⏭️ **Download GRBL v1.1f source code**
4. ⏭️ **Begin Phase 1: Port planner.c**
5. ⏭️ **Create grbl_stepper.c OCR adapter**

---

**Branch**: `grbl-rebuild`  
**Previous work preserved in**: `master` branch  
**Start date**: October 15, 2025  
**Expected completion**: November 15, 2025 (1 month)
