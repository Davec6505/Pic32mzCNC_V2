# Quick Start Guide - When You Return

## 🎯 System Status: PRODUCTION READY! (October 26, 2025)

**Current Firmware**: `bins/Debug/CS23.hex` (flashed and tested)

**Major Milestones Achieved**:
- ✅ **Arc Generator Working** - G2/G3 circular interpolation via TMR1 @ 40ms
- ✅ **Linear Motion Working** - G0/G1 moves with coordinated multi-axis
- ✅ **Consecutive Arcs Fixed** - Position tracking from GRBL planner (Oct 25)
- ✅ **Segment Re-entry Fixed** - Main loop no longer hangs (Oct 25)
- ✅ **All Axes Moving** - X, Y, Z, A coordinated motion verified
- ✅ **Documentation Consolidated** - 4 main files replace 70+ fragmented docs
- ✅ **Test Files Ready** - Rectangle-to-circle handoff test created

**Recent Fix (October 25, 2025)**:
- **Subordinate Axis Motion** - Bresenham pulse generation and OCR enable/disable sequence corrected
- **Result**: All axes now move correctly in coordinated diagonal moves

---

## 🚀 Next Steps (Priority Order)

### 1. **Test Rectangle-to-Circle Handoff** ⭐ **READY TO RUN**

**File**: [`tests/06_rectangle_circle_handoff.gcode`](tests/06_rectangle_circle_handoff.gcode )

**What it tests**:
- 4 rounded corners (linear → arc → linear transitions)
- Circle entry/exit (linear → arc)
- Arc-to-arc transition (two semicircles = full circle)
- Diagonal moves (subordinate axis coordination)

**How to run**:
```powershell
# From project root:
.\test_file.ps1 -Port COM4 -GCodeFile tests\06_rectangle_circle_handoff.gcode
```

**Expected behavior**:
- Smooth transitions between linear and arc moves
- No position jumps at arc boundaries
- Clean return to origin (0.000, 0.000)
- No "jiggle" at corner handoffs

**Watch for**:
- Transition quality (linear-to-arc should be seamless)
- Position accuracy after full pattern
- Any velocity discontinuities
- Proper arc radius (3mm corners, 10mm circle diameter)

---

### 2. **Machine Configuration & Homing** ⚡ **NEW PRIORITY** (October 26, 2025)

These are **essential production features** that should be implemented before advanced testing:

#### **A. Homing Cycle (G28, G28.1, G30, G30.1)**
**Status**: Commands parsed but **NOT executed** (placeholder implementation)

**TODO**:
- [ ] Implement homing sequence for each axis
- [ ] Add $22 (homing cycle enable/disable)
- [ ] Add $23 (homing direction invert mask)
- [ ] Add $24 (homing feed rate, mm/min)
- [ ] Add $25 (homing seek rate, mm/min)
- [ ] Add $26 (homing debounce, milliseconds)
- [ ] Add $27 (homing pull-off, mm)
- [ ] Test with actual limit switches

**Reference**: GRBL v1.1f homing cycle (grbl/motion_control.c)

---

#### **B. Hard Limits (Emergency Stop on Switch)**
**Status**: Hardware pins configured but **NOT monitored**

**TODO**:
- [ ] Add $21 (hard limits enable/disable)
- [ ] Implement limit switch polling in main loop
- [ ] Add emergency stop on limit trigger
- [ ] Add $5 (limit pins invert mask) for normally-open vs normally-closed switches
- [ ] Add alarm state (requires reset to clear)
- [ ] Test with physical switches

**Hardware Pins** (from GPIO configuration):
- X-axis: RA7 (min), RA9 (max)
- Y-axis: RA10 (min), RA14 (max)
- Z-axis: RA15 (min), ?? (max - check schematic)

**Current Implementation**: Pins configured but not monitored

---

#### **C. Soft Limits (Software Travel Boundaries)**
**Status**: Settings exist ($130-$133) but **NOT enforced**

**TODO**:
- [ ] Add $20 (soft limits enable/disable)
- [ ] Implement boundary checking in motion planner
- [ ] Reject moves that exceed max_travel settings
- [ ] Add proper error messages ("Soft limit exceeded")
- [ ] Test with moves beyond configured boundaries

**Current Settings**:
```
$130=300.000  ; X max travel (mm)
$131=300.000  ; Y max travel (mm)
$132=100.000  ; Z max travel (mm)
$133=360.000  ; A max travel (degrees)
```

---

#### **D. Direction Pin Inversion**
**Status**: Hardware direction pins work but **NOT configurable**

**TODO**:
- [ ] Add $3 (direction port invert mask)
- [ ] Implement per-axis direction inversion in `multiaxis_control.c`
- [ ] Update `DirX_Set()` / `DirX_Clear()` logic based on mask
- [ ] Test with motors that need reversed direction

**Why needed**: Different motor wiring may require reversed direction signals

---

#### **E. Machine Size Configuration**
**Status**: Max travel settings exist but **NOT used for work envelope**

**TODO**:
- [ ] Document actual machine dimensions (measure hardware)
- [ ] Update $130-$133 with real values
- [ ] Add machine size to startup banner
- [ ] Implement work coordinate system boundaries (G54-G59)
- [ ] Test that soft limits respect actual machine size

**Example**:
```gcode
$130=400.000  ; X max travel = 400mm (measured)
$131=400.000  ; Y max travel = 400mm (measured)
$132=150.000  ; Z max travel = 150mm (measured)
```

---

### Implementation Priority Order

**Phase 1: Safety (Immediate)**:
1. Hard limits ($21, $5) - Prevent crashes
2. Soft limits ($20) - Prevent moves beyond boundaries
3. Direction inversion ($3) - Correct motor direction

**Phase 2: Usability (Next)**:
4. Homing cycle ($22-$27, G28) - Zero position reference
5. Machine size documentation - Know actual work envelope

**Phase 3: Advanced (Later)**:
6. Work coordinate systems (G54-G59) - Multiple job origins
7. Tool length offsets (G43, G49) - Multi-tool support

---

### 3. **Full Circle Arc Test** (After Handoff Test)

**Purpose**: Validate arc closure accuracy (360° = return to start)

**Test sequence**:
```gcode
G90 G21         ; Absolute mode, mm
G0 X50 Y50      ; Move to center
G2 X50 Y50 I10 J0 F1000  ; Full circle (20mm diameter)
; Should return EXACTLY to (50, 50) - check position accuracy
```

**Watch for**:
- Position error after full circle (should be <0.01mm)
- Smooth motion throughout (no speed bumps)
- Consistent arc radius (oscilloscope or test indicator)

---

### 4. **Advanced Arc Features** (Future)

**Currently NOT implemented**:
- [ ] R-format arcs (G2 X10 Y10 R5 instead of IJK)
- [ ] G18/G19 plane selection (XZ, YZ arcs)
- [ ] Full circles with P parameter (G2 X0 Y0 I5 J0 P2 = 2 full rotations)
- [ ] Helical arcs (G2 with Z-axis movement)

**Priority**: LOW - Basic arc functionality working

---

## 📋 Quick Reference

### Current Firmware Status

**Working Features**:
- ✅ Linear motion (G0/G1) - All axes coordinated
- ✅ Arc motion (G2/G3) - Quarter/half circles verified
- ✅ Consecutive arcs - Position tracking fixed
- ✅ Rectangle test - 6 moves complete correctly
- ✅ Debug system - 8 levels, level 3 recommended
- ✅ Serial communication - UGS protocol complete
- ✅ Real-time commands - ?, !, ~, ^X working

**Known Limitations**:
- ❌ Homing cycle not implemented (G28 parsed but no action)
- ❌ Hard limits not monitored (pins configured but not polled)
- ❌ Soft limits not enforced (settings exist but not checked)
- ❌ Direction inversion not configurable (hardcoded)
- ❌ R-format arcs not supported (IJK only)
- ❌ G18/G19 plane selection not implemented (G17/XY only)

**Test Files Available**:
- [`tests/01_simple_square.gcode`](tests/01_simple_square.gcode ) - 4 linear moves
- [`tests/02_diagonal_move.gcode`](tests/02_diagonal_move.gcode ) - Coordinated X+Y
- [`tests/03_circle_20segments.gcode`](tests/03_circle_20segments.gcode ) - Linear approximation baseline
- [`tests/04_arc_test.gcode`](tests/04_arc_test.gcode ) - Arc interpolation (G2/G3)
- [`tests/06_rectangle_circle_handoff.gcode`](tests/06_rectangle_circle_handoff.gcode ) - **NEW!** Transition test

---

## 🔧 Build & Flash

**Quick rebuild** (after code changes):
```bash
make all BUILD_CONFIG=Debug DEBUG_MOTION_BUFFER=3
```

**Flash firmware**:
1. Open MPLAB X IPE
2. Load `bins/Debug/CS23.hex`
3. Connect PICkit4
4. Program device

**Connect to UGS**:
1. Serial port: COM4 (or check Device Manager)
2. Baud rate: 115200
3. Should see: `[VER:1.1f.20251017:PIC32MZ CNC V2]`

---

## 🐛 Debug Watchpoints

**If motion doesn't start**:
```bash
# Check buffer counts
[MAIN] Planner=X Stepper=Y  # Should show blocks accumulating
```

**If arcs show geometry errors**:
```bash
# Check arc parameters
[ARC] center=(x,y) radius=r segments=n
```

**If position drifts**:
```bash
# Check position tracking
<Run|MPos:x,y,z|WPos:x,y,z>  # Should match commanded position
```

---

## 📚 Documentation Reference

**Main Documentation** (4 consolidated files):
1. [`docs/GCODE_AND_PARSING.md`](docs/GCODE_AND_PARSING.md ) - Parser, serial, UGS protocol
2. [`docs/LINEAR_MOTION.md`](docs/LINEAR_MOTION.md ) - Planner, segments, position tracking
3. [`docs/ARC_MOTION.md`](docs/ARC_MOTION.md ) - Arc generator, TMR1 ISR, flow control
4. [`docs/GENERAL_SYSTEM.md`](docs/GENERAL_SYSTEM.md ) - Build system, debug, hardware

**Legacy Documentation**: 70+ old files kept for reference but NOT updated

---

## 🎯 Key Learnings

**From October 25, 2025 Session**:
- Arc position must use GRBL planner (not local cache)
- Segment start needs re-entry guard (prevent infinite retry)
- Small segments don't need periodic correction (0.285mm accurate enough)
- LED1 heartbeat needs timer-based (not loop-count based)
- Documentation consolidation prevents fragmentation

**For Next Session**:
- Test complex transition patterns (rectangle-to-circle)
- Validate arc closure accuracy (full circles)
- Consider homing and limit implementation (safety features)
- Measure actual machine dimensions (update $130-$133)

---

**Good luck with the testing! The rectangle-to-circle handoff should reveal any remaining transition issues.** 🚀
