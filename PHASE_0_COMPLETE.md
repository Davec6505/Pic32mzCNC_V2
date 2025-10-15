# GRBL Rebuild - Phase 0 Complete ✅

**Branch:** grbl-rebuild  
**Date:** October 15, 2025  
**Status:** Clean slate ready for GRBL source integration

---

## ✅ What We Accomplished

### 1. Downloaded GRBL v1.1f Source Code
- ✅ Downloaded 40 official GRBL files from GitHub
- ✅ Stored in `grbl-source/` directory
- ✅ All core files ready: planner, stepper, gcode, protocol, etc.
- ✅ License file (GPL v3) included

### 2. Cleaned Up Old Custom Code
- ✅ Removed 32 custom files (14 .c files, 18 .h files)
- ✅ Deleted old architecture:
  - ❌ motion_planner.c/h
  - ❌ motion_buffer.c/h
  - ❌ motion_gcode_parser.c/h
  - ❌ interpolation_engine.c/h
  - ❌ motion_profile.c/h
  - ❌ speed_control.c/h
  - ❌ gcode_parser.c/h
  - ❌ grbl_serial.c/h (old partial implementation)
  - ❌ grbl_settings.c/h (old partial implementation)
  - ❌ All test/example files

### 3. Preserved Critical Files
✅ **Hardware Layer** (will be adapted):
- `srcs/app.c` - OCR, GPIO, timer configuration
- `incs/app.h` - Hardware interface definitions
- `srcs/main.c` - Entry point

✅ **Harmony Configuration** (complete):
- `srcs/config/default/*` - All initialization code
- `srcs/config/default/peripheral/*` - All PLIB implementations
- `incs/config/default/*` - All PLIB headers
- `srcs/startup/startup.S` - PIC32MZ startup code

---

## 📊 Current File Structure

```
Pic32mzCNC_V2/
├── grbl-source/          ← GRBL v1.1f source (40 files)
│   ├── planner.c/h
│   ├── stepper.c/h
│   ├── gcode.c/h
│   ├── protocol.c/h
│   ├── motion_control.c/h
│   ├── limits.c/h
│   ├── settings.c/h
│   ├── report.c/h
│   ├── serial.c/h
│   ├── system.c/h
│   ├── nuts_bolts.c/h
│   └── ... (other GRBL files)
│
├── srcs/
│   ├── app.c             ← Hardware abstraction (KEEP, ADAPT)
│   ├── main.c            ← Entry point (KEEP, ADAPT)
│   ├── config/           ← Harmony files (PRESERVE)
│   └── startup/          ← Startup code (PRESERVE)
│
├── incs/
│   ├── app.h             ← Hardware interface (KEEP, ADAPT)
│   └── config/           ← Harmony headers (PRESERVE)
│
├── download_grbl.ps1     ← Script to download GRBL source
├── cleanup_old_code.ps1  ← Script to clean old files
└── GRBL_REBUILD_PLAN.md  ← Implementation strategy
```

---

## 🎯 Next Steps - Phase 1: Copy GRBL Core Files

### Step 1.1: Copy GRBL Planner
```powershell
Copy-Item grbl-source/planner.c srcs/
Copy-Item grbl-source/planner.h incs/
```

### Step 1.2: Copy GRBL G-code Parser
```powershell
Copy-Item grbl-source/gcode.c srcs/
Copy-Item grbl-source/gcode.h incs/
```

### Step 1.3: Copy GRBL Protocol
```powershell
Copy-Item grbl-source/protocol.c srcs/
Copy-Item grbl-source/protocol.h incs/
```

### Step 1.4: Copy GRBL Motion Control
```powershell
Copy-Item grbl-source/motion_control.c srcs/
Copy-Item grbl-source/motion_control.h incs/
```

### Step 1.5: Copy GRBL Support Files
```powershell
Copy-Item grbl-source/nuts_bolts.c srcs/
Copy-Item grbl-source/nuts_bolts.h incs/
Copy-Item grbl-source/settings.c srcs/
Copy-Item grbl-source/settings.h incs/
Copy-Item grbl-source/defaults.h incs/
Copy-Item grbl-source/config.h incs/
Copy-Item grbl-source/report.c srcs/
Copy-Item grbl-source/report.h incs/
Copy-Item grbl-source/limits.c srcs/
Copy-Item grbl-source/limits.h incs/
Copy-Item grbl-source/system.c srcs/
Copy-Item grbl-source/system.h incs/
```

### Step 1.6: Create PIC32MZ-Specific Files
**DO NOT COPY** these AVR-specific files, create PIC32 versions:
- `serial.c/h` → Create `pic32_serial.c/h` using UART2 PLIB
- `stepper.c/h` → Create `pic32_stepper.c/h` using OCR hardware
- `eeprom.c/h` → Create `pic32_nvm.c/h` using NVM PLIB
- `cpu_map.h` → Create `pic32_config.h` with GPIO pin assignments
- `grbl.h` → Modify to include PIC32 headers instead of AVR

---

## 🔧 Key Adaptations Required

### 1. Serial Communication
**GRBL AVR:** Uses AVR UART with interrupts  
**PIC32MZ:** Use existing UART2 PLIB (already configured)

### 2. Step Generation
**GRBL AVR:** Timer ISR with Bresenham algorithm  
**PIC32MZ:** OCR dual-compare continuous pulse mode (hardware generates pulses)

**Critical Innovation:**
```c
// GRBL calculates: "How many steps at what rate?"
// We convert to: "What OCR period for this step rate?"

uint32_t period = MotionPlanner_CalculateOCRPeriod(step_rate);
TMR2_PeriodSet(period);
OCMP4_CompareValueSet(period - 40);      // Rising edge
OCMP4_CompareSecondaryValueSet(40);       // Falling edge (1.9µs pulse)
OCMP4_Enable();
TMR2_Start();
```

### 3. EEPROM Storage
**GRBL AVR:** Uses AVR EEPROM  
**PIC32MZ:** Use NVM (flash memory) for settings persistence

### 4. System Timing
**GRBL AVR:** 16MHz AVR, ~62.5ns tick  
**PIC32MZ:** 200MHz, 5ns tick (40x faster!)

---

## 📝 Git Commits

### Commit 1: GRBL_REBUILD_PLAN.md
```
Create GRBL rebuild plan
- 5-phase implementation strategy
- What to keep vs. replace
- OCR stepper adaptation approach
```

### Commit 2: Clean Slate (This Commit)
```
Clean slate: Remove all old custom code, preserve Harmony config
- Deleted 32 old custom files
- Preserved: app.c, main.c, app.h
- Downloaded: GRBL v1.1f source (40 files)
- Ready for Phase 1: Copy GRBL core files
```

---

## ✅ Success Criteria

- [x] All old custom motion code removed
- [x] Harmony configuration preserved
- [x] GRBL v1.1f source downloaded (40 files)
- [x] Clean git commit on grbl-rebuild branch
- [x] Hardware abstraction layer preserved (app.c/h)
- [x] Entry point preserved (main.c)
- [ ] **Next:** Copy GRBL core files to project
- [ ] **Next:** Create OCR stepper adapter
- [ ] **Next:** Build and test

---

## 🚀 Why This Approach Works

1. **Proven Planning Algorithm:** GRBL's planner has been tested by thousands of users
2. **Character-Counting Protocol:** Solves the file streaming issue (UGS bulk sends)
3. **Look-Ahead Buffer:** 16-block buffer with junction optimization
4. **Hardware Advantage:** OCR offloads step generation, more CPU for planning
5. **Clean Architecture:** GRBL provides interfaces, we adapt hardware layer only

---

## 📚 References

- **GRBL Repository:** https://github.com/gnea/grbl
- **GRBL Wiki:** https://github.com/gnea/grbl/wiki
- **GRBL v1.1 Documentation:** https://github.com/gnea/grbl/wiki/Grbl-v1.1-Commands
- **PIC32MZ Datasheet:** Section 18 (Output Compare)
- **DRV8825 Datasheet:** Timing requirements

---

**Ready to proceed with Phase 1: Copy GRBL core files! 🎉**
