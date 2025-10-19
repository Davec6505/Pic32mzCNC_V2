# PIC32MZ CNC Motion Controller V2 - AI Coding Guide

## ⚠️ CRITICAL SERIAL WRAPPER ARCHITECTURE (October 19, 2025)

**MCC PLIB_UART2 WRAPPER WITH GRBL REAL-TIME COMMANDS** ✅ **COMPLETE AND WORKING!**

### Final Working Architecture

After multiple iterations, the system now uses a **callback-based UART wrapper** with **ISR flag-based real-time command handling**:

```
┌─────────────────────────────────────────────────────────────────┐
│ Hardware UART2 (MCC plib_uart2)                                 │
│ - 115200 baud, 8N1                                              │
│ - RX interrupt enabled (Priority 5)                             │
│ - TX/Error interrupts disabled                                  │
│ - No blocking mode (callback-based)                             │
└────────────────────┬────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────────┐
│ Serial Wrapper (serial_wrapper.c/h)                             │
│                                                                 │
│ Serial_RxCallback(ISR context):                                 │
│   1. Read byte from UART                                        │
│   2. if (GCode_IsControlChar(byte)):                            │
│        realtime_cmd = byte  // Set flag for main loop          │
│   3. else:                                                      │
│        Add to ring buffer (256 bytes)                           │
│   4. Re-enable UART read                                        │
│                                                                 │
│ Serial_GetRealtimeCommand():                                    │
│   - Returns and clears realtime_cmd flag                        │
│   - Called by main loop every iteration                         │
└────────────────────┬────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────────┐
│ Main Loop (main.c)                                              │
│                                                                 │
│ while(true) {                                                   │
│   ProcessSerialRx();  // Read from ring buffer                 │
│                                                                 │
│   uint8_t cmd = Serial_GetRealtimeCommand();                   │
│   if (cmd != 0) {                                               │
│     GCode_HandleControlChar(cmd);  // Handle in main context!  │
│   }                                                             │
│                                                                 │
│   ProcessCommandBuffer();                                       │
│   ExecuteMotion();                                              │
│   APP_Tasks();                                                  │
│   SYS_Tasks();                                                  │
│ }                                                               │
└─────────────────────────────────────────────────────────────────┘
```

### Critical Design Decisions

**❌ WRONG APPROACH (Caused Crash):**
```c
// DON'T call GCode_HandleControlChar() from ISR!
void Serial_RxCallback(uintptr_t context) {
    if (GCode_IsControlChar(data)) {
        GCode_HandleControlChar(data);  // ❌ CRASH! Blocking UART calls in ISR
    }
}
```

**✅ CORRECT APPROACH (Flag-Based):**
```c
// ISR only sets flag
void Serial_RxCallback(uintptr_t context) {
    if (GCode_IsControlChar(data)) {
        realtime_cmd = data;  // ✅ Safe: Just set flag
    }
}

// Main loop handles in safe context
void main(void) {
    while(true) {
        uint8_t cmd = Serial_GetRealtimeCommand();
        if (cmd != 0) {
            GCode_HandleControlChar(cmd);  // ✅ Safe: Not in ISR
        }
    }
}
```

### Why ISR Can't Call Blocking Functions

1. **UGS_SendStatusReport()** calls `Serial_Write()` which blocks waiting for TX complete
2. **ISR Priority**: UART RX ISR at Priority 5 cannot safely call UART TX functions
3. **Deadlock Risk**: ISR waiting for UART TX while UART TX ISR is blocked
4. **GRBL Pattern**: Original GRBL sets flags in ISR, main loop checks flags

### Implementation Files

**serial_wrapper.c** (160 lines):
```c
// ISR-safe flag
static volatile uint8_t realtime_cmd = 0;

void Serial_RxCallback(uintptr_t context) {
    uint8_t data = uart_rx_byte;
    
    if (GCode_IsControlChar(data)) {
        realtime_cmd = data;  // Flag for main loop
    } else {
        // Add to ring buffer
        rx_buffer.data[rx_buffer.head] = data;
        rx_buffer.head = (rx_buffer.head + 1) & 0xFF;
    }
    
    UART2_Read(&uart_rx_byte, 1);  // Re-enable
}

uint8_t Serial_GetRealtimeCommand(void) {
    uint8_t cmd = realtime_cmd;
    realtime_cmd = 0;
    return cmd;
}
```

**gcode_parser.c** - `GCode_HandleControlChar()`:
```c
void GCode_HandleControlChar(char c) {
    switch (c) {
        case GCODE_CTRL_STATUS_REPORT:  // '?' (0x3F)
            // Get positions and send status report
            UGS_SendStatusReport(...);  // Safe in main loop context
            break;
            
        case GCODE_CTRL_FEED_HOLD:  // '!' (0x21)
            MotionBuffer_Pause();
            UGS_Print(">> Feed Hold\r\n");
            break;
            
        case GCODE_CTRL_CYCLE_START:  // '~' (0x7E)
            MotionBuffer_Resume();
            UGS_Print(">> Cycle Start\r\n");
            break;
            
        case GCODE_CTRL_SOFT_RESET:  // Ctrl-X (0x18)
            MultiAxis_StopAll();
            MotionBuffer_Clear();
            GCode_ResetModalState();
            break;
    }
}
```

### UGS Test Results (October 19, 2025) ✅

```
*** Connected to GRBL 1.1f
[VER:1.1f.20251017:PIC32MZ CNC V2]
[OPT:V,16,512]

✅ Status queries: <Idle|MPos:0.000,0.000,0.000|WPos:0.000,0.000,0.000>
✅ Settings query: $$ returns all 18 settings
✅ Real-time commands: ?, !, ~, Ctrl-X all working
✅ Motion execution: G0 Z5 moved Z-axis to 5.003mm
✅ Position feedback: Real-time updates during motion
✅ Feed hold: ! pauses motion immediately
✅ Modal commands: G90, G21, G17, G94, M3, M5 all functional
```

### Serial Processing Robustness Fix (October 19, 2025)

**Fixed: "error:1 - Invalid G-code: G"** during fast command streaming:

**Problem**: Static variables in `ProcessSerialRx()` not properly reset between calls
- `line_pos` reset inside conditional, caused incomplete line processing
- No `line_complete` flag to track state across function calls
- Race condition when serial data arrived in multiple chunks

**Solution**: Added proper state tracking with three static variables:
```c
static char line_buffer[256] = {0};
static size_t line_pos = 0;         // Track position across calls
static bool line_complete = false;  // Flag for complete line
```

**Key Changes**:
1. Only read new data if line not already complete
2. Added buffer overflow protection (discard and send error)
3. Reset all three state variables after processing each line
4. Prevents partial line processing during burst streaming

**Result**: ✅ Robust serial processing under high-speed command streaming

### Key Takeaways

1. ✅ **Never call blocking functions from ISR** - Set flags instead
2. ✅ **Main loop handles flags** - Safe context for UART writes
3. ✅ **Ring buffer for regular data** - Control chars never enter buffer
4. ✅ **Flag-based is fast enough** - Main loop runs at ~1kHz (1ms response)
5. ✅ **Proven GRBL pattern** - Original GRBL uses same approach

---

## ⚠️ CRITICAL SERIAL BUFFER FIX (October 18, 2025)

**UART RING BUFFER SIZE INCREASED - RESOLVES SERIAL DATA CORRUPTION** ✅ **COMPLETE!**:

### Problem Identified
During coordinate system testing (G92/G91 commands), **persistent serial data corruption** occurred:
- Symptoms: Commands fragmented during transmission
  - "G92.1" → received as "9.1" or "G-102.1"
  - "G92 X0 Y0" → received as "G2 X0" (missing '9')
  - "G90" → received as "0"
  - "G1 Y10 F1000" → received as "GG F1" or "Y00"
- Pattern: Always losing characters at **start** of commands
- Attempted fixes failed: GRBL flow control (wait for "ok"), 50ms delays, real-time command handling

### Root Cause Analysis
Comparison with **mikroC version** (Pic32mzCNC/ folder) revealed the solution:

**mikroC Implementation (Working):**
```c
// Serial_Dma.h - 500-byte ring buffer
typedef struct {
    char temp_buffer[500];  // ← Large ring buffer
    int head;
    int tail;
    int diff;
    char has_data: 1;
} Serial;

// DMA0 ISR copies from rxBuf[200] → temp_buffer[500]
// Main loop: Sample_Gocde_Line() calls Get_Difference() non-blocking
// Pattern matching: DMA triggers on '\n' or '?' character
```

**Harmony/MCC Implementation (Had Issues):**
```c
// plib_uart2.c - ORIGINAL 256-byte ring buffer
#define UART2_READ_BUFFER_SIZE (256U)  // ← TOO SMALL for burst commands!
volatile static uint8_t UART2_ReadBuffer[UART2_READ_BUFFER_SIZE];
```

**Key Differences:**
1. **Buffer Size**: mikroC used **500 bytes**, Harmony used **256 bytes**
2. **DMA vs Interrupt**: mikroC used DMA with auto-enable, Harmony uses RX interrupt
3. **Flow Pattern**: mikroC's larger buffer absorbed command bursts from UGS/PowerShell
4. **Safety Margin**: 500 bytes provides 2x safety vs typical G-code line length (~100 chars)

### Solution Applied
**Increased UART buffer sizes to 512 bytes (closest power-of-2 to mikroC's 500):**

```c
// plib_uart2.c (UPDATED - October 18, 2025)
/* Increased buffer sizes to match mikroC implementation (500 bytes)
 * Previous 256 bytes caused overflow with burst commands from UGS/PowerShell.
 * Larger buffer provides safety margin for G-code streaming.
 */
#define UART2_READ_BUFFER_SIZE      (512U)  // ← DOUBLED from 256
#define UART2_WRITE_BUFFER_SIZE     (512U)  // ← DOUBLED from 256
volatile static uint8_t UART2_ReadBuffer[UART2_READ_BUFFER_SIZE];
volatile static uint8_t UART2_WriteBuffer[UART2_WRITE_BUFFER_SIZE];
```

**Also increased inter-command delay in test scripts:**
```powershell
# test_coordinates.ps1 - 100ms delay after "ok" (was 50ms)
# Matches mikroC timing, allows UART ISR + parser + motion buffer processing
Start-Sleep -Milliseconds 100
```

### Why This Works
1. **Burst Absorption**: 512-byte buffer can hold ~5 typical G-code commands in flight
2. **PowerShell Timing**: Even fast WriteLine() calls won't overflow buffer between ISR reads
3. **GRBL Protocol**: "ok" response still controls flow, but buffer prevents corruption during burst
4. **CPU Speed**: 200MHz PIC32MZ processes data fast, but needs buffer for bursty serial arrivals
5. **Proven Pattern**: mikroC version handled identical hardware with 500-byte buffer perfectly

### Memory Impact
```
Before: 256 bytes RX + 256 bytes TX = 512 bytes total
After:  512 bytes RX + 512 bytes TX = 1024 bytes total
Change: +512 bytes (0.025% of 2MB RAM - negligible)
```

### Testing Verification
After rebuild with 512-byte buffers:
- ✅ Commands arrive intact (no fragmentation)
- ✅ G92/G91 coordinate tests can proceed
- ✅ UGS connection stable during streaming
- ✅ No "ok" timeout warnings
- ✅ Motion accuracy tests ready

### Future Considerations
- **If corruption returns**: Check interrupt priorities (ensure UART ISR not preempted)
- **For even faster streaming**: Consider DMA like mikroC (but 512-byte ISR-driven should suffice)
- **Buffer monitoring**: Could add debug code to track max buffer usage (head-tail difference)
- **Alternative solution**: Use UGS instead of PowerShell for testing (known-good GRBL client)

### Documentation Updates
- `ugs_interface.c`: Updated comments from 256→512 bytes
- `UGS_SendBuildInfo()`: Reports "[OPT:V,16,512]" to UGS (was 256)
- This section: Complete analysis for future reference

**Status**: ✅ Serial buffer sized correctly! Coordinate system testing can proceed!

---

## ⚠️ CRITICAL HARDWARE FIX APPLIED (October 17, 2025)

**TIMER PRESCALER FIX - RESOLVES STEPPER SPEED ISSUE** ✅ **COMPLETE!**:
- **Problem Found**: 1:2 prescaler (12.5MHz) caused 16-bit timer overflow at slow speeds
  - Example: 100 steps/sec requires 125,000 counts → **OVERFLOWS 16-bit limit (65,535)!**
  - Hardware saturated at max value, causing steppers to run **2-3x too fast**
- **Solution Applied**: Changed to **1:16 prescaler** (1.5625MHz timer clock)
  - New range: 23.8 to 31,250 steps/sec (fits in 16-bit timer ✓)
  - Pulse width: 40 counts × 640ns = **25.6µs** (exceeds DRV8825's 1.9µs minimum)
  - Period example: 100 steps/sec = 15,625 counts ✓ (no overflow!)
- **Code Changes**: ✅ Updated `TMR_CLOCK_HZ` from 12500000UL to **1562500UL**
- **MCC Configuration**: ✅ **COMPLETE!** TMR2/3/4/5 prescalers updated to 1:16 in MCC GUI
- **Documentation**: See `docs/TIMER_PRESCALER_ANALYSIS.md` for full analysis
- **Status**: ✅ Ready for rebuild and hardware testing!

## ⚠️ CURRENT STATUS: Direct Hardware Testing Phase (October 18, 2025)

**Latest Progress**: Built and flashed **test_ocr_direct.c** to verify OCR period scaling works correctly with known step counts, bypassing coordinate system bugs.

**Current Testing Focus** 🎯:
- **✅ COORDINATED MOVE BUG FIXED!** - October 18, 2025
- **Test Results**: All 3 tests completed successfully!
  - Test 1: Y-axis 800 steps ✓ (10.000mm exact, ±1 step rounding)
  - Test 2: Coordinated X/Y diagonal ✓ (both axes completed, no hang!)
  - Test 3: Return to origin ✓ (negative moves work, completed)
- **Bug Fixed**: Subordinate axis now stops when dominant completes
  - Problem: Subordinate copied SEGMENT_COMPLETE but never handled it
  - Solution: Check dominant's `active` flag (not segment state)
  - Location: multiaxis_control.c lines 837-851
- **Position Tracking Note**: `step_count` is per-move (resets each move)
  - This is correct for relative motion controller
  - Absolute position tracking is handled by G-code parser (future)
- **Status**: ✅ OCR period scaling verified! Time-based S-curve works!

**Current Working System** ✅:
- **Full GRBL v1.1f protocol**: All system commands ($I, $G, $$, $#, $N, $), real-time commands (?, !, ~, ^X)
- **Serial G-code control**: UART2 @ 115200 baud, UGS connects successfully as "GRBL 1.1f"
- **Real-time position feedback**: Actual step counts from hardware → mm conversion in status reports
- **Live settings management**: $$ views all 18 settings, $xxx=value modifies settings on-the-fly
- **GRBL Character-Counting Protocol**: Non-blocking "ok" response enables continuous motion ✨**PHASE 2 ACTIVE**
- **Full G-code parser**: Modal/non-modal commands, 13 modal groups, M-command support
- **Multi-axis S-curve control**: TMR1 @ 1kHz drives 7-segment jerk-limited profiles per axis
- **Hardware pulse generation**: OCR modules (OCMP1/4/5/3) generate step pulses independently
- **All axes ready**: X, Y, Z, A all enabled and configured
- **Time-synchronized coordination**: Dominant (longest) axis determines total time, subordinate axes scale velocities
- **Hardware configuration**: GT2 belt (80 steps/mm) on X/Y/A, 2.5mm leadscrew (1280 steps/mm) on Z
- **Timer clock VERIFIED**: 1.5625MHz (50MHz PBCLK3 with 1:32 prescaler) ✨**VERIFIED - October 18, 2025!**
- **Motion buffer infrastructure**: 16-block ring buffer for look-ahead planning (ready for optimization)
- **PlantUML documentation**: 9 architecture diagrams for visual reference
- **Direct hardware test**: $T command for OCR verification ✨**WORKING - October 18, 2025**

**Recent Additions (October 18, 2025)** ✅:
- 🧪 **DIRECT HARDWARE TEST** - test_ocr_direct.c bypasses coordinate systems to test OCR scaling
- 🔧 **BUILD FIXES** - Resolved duplicate DEBUG_MOTION_BUFFER definitions, format specifiers
- ⚡ **TEST INTEGRATION** - Test functions called from main loop, triggered by 'T' command
- 🏗️ **BUILD SUCCESS** - All compilation errors fixed, firmware flashed and ready
- ✅ **CRITICAL: Timer prescaler fix COMPLETE** - Both code AND MCC updated to 1:16 (ready to test!)
- ✅ **PlantUML documentation system**: 9 diagrams (system overview, data flow, timer architecture, etc.)
- ✅ **System commands**: $I (version), $G (parser state), $$ (all settings), $N (startup lines), $ (help)
- ✅ **Settings management**: $100-$133 read/write with MotionMath integration
- ✅ **Real-time position feedback**: MultiAxis_GetStepCount() → MotionMath_StepsToMM() in ? status reports
- ✅ **Machine state tracking**: "Idle" vs "Run" based on MultiAxis_IsBusy()
- ✅ **UGS connectivity verified**: Full initialization sequence working (?, $I, $$, $G)
- ✅ **G-code parser**: Full GRBL v1.1f compliance with modal/non-modal commands (1354 lines)
- ✅ **UGS interface**: Serial communication with flow control and real-time commands
- ✅ **Non-modal commands**: G4, G28, G28.1, G30, G30.1, G92, G92.1 implemented
- ✅ **Modal groups**: 13 groups (motion, plane, distance, feedrate, units, coordinate systems, etc.)
- ✅ **M-commands**: Spindle control (M3/M4/M5), coolant (M7/M8/M9), program control (M0/M1/M2/M30)
- ✅ **MISRA C:2012 compliance**: All mandatory rules, documented deviations, snprintf() Rule 17.7 compliant
- ✅ **XC32 optimization**: 553 bytes RAM, optimal memory placement, no special attributes needed
- ✅ **App layer cleanup**: Removed SW1/SW2 debug buttons, motion now via G-code only
- ✅ **Makefile improvements**: Added `make quiet` target for filtered build output

**Current File Structure**:
```
srcs/
  ├── main.c                        // Entry point, three-stage pipeline, motion execution ✨UPDATED
  ├── app.c                         // System management, LED status (SW1/SW2 removed)
  ├── command_buffer.c              // Command separation algorithm (279 lines) ✨NEW
  ├── gcode_parser.c                // GRBL v1.1f parser (1354 lines)
  ├── ugs_interface.c               // UGS serial protocol
  └── motion/
      ├── multiaxis_control.c       // Time-based S-curve interpolation (1169 lines) ✨UPDATED
      ├── motion_math.c             // Kinematics & GRBL settings (733 lines) ✨UPDATED
      ├── motion_buffer.c           // Ring buffer for look-ahead planning (284 lines)
      └── stepper_control.c         // Legacy single-axis reference (unused)

incs/
  ├── command_buffer.h              // Command buffer API (183 lines) ✨NEW
  ├── gcode_parser.h                // Parser API, modal state (357 lines) ✨NEW
  ├── ugs_interface.h               // UGS protocol API ✨NEW
  └── motion/
      ├── motion_types.h            // Centralized type definitions (235 lines) ✨UPDATED
      ├── motion_buffer.h           // Ring buffer API (207 lines)
      ├── multiaxis_control.h       // Multi-axis API
      └── motion_math.h             // Unit conversions, look-ahead support (398 lines)

docs/
  ├── COMMAND_BUFFER_ARCHITECTURE.md // Command separation architecture (450 lines) ✨NEW
  ├── COMMAND_BUFFER_TESTING.md     // Testing guide (550 lines) ✨NEW
  ├── BUILD_SUCCESS_COMMAND_BUFFER.md // Build verification (420 lines) ✨NEW
  ├── PHASE2_NON_BLOCKING_PROTOCOL.md // Non-blocking protocol guide (320 lines) ✨NEW
  ├── GCODE_PARSER_COMPLETE.md      // Full GRBL v1.1f implementation guide ✨NEW
  ├── XC32_COMPLIANCE_GCODE_PARSER.md // MISRA/XC32 compliance documentation ✨NEW
  ├── APP_CLEANUP_SUMMARY.md        // SW1/SW2 removal documentation ✨NEW
  ├── MAKEFILE_QUIET_BUILD.md       // make quiet target documentation ✨NEW
  ├── TIMER_PRESCALER_ANALYSIS.md   // Prescaler fix analysis (1:2 → 1:16) ✨NEW
  └── plantuml/                      // Architecture visualization (9 diagrams) ✨NEW
      ├── README.md                  // PlantUML setup and viewing guide
      ├── QUICK_REFERENCE.md         // PlantUML syntax cheat sheet
      ├── TEMPLATE_NEW_PROJECT.puml  // Reusable template
      ├── 01_system_overview.puml    // Hardware/firmware/application layers
      ├── 02_data_flow.puml          // Serial → Parser → Buffer → Control
      ├── 03_module_dependencies.puml // Module relationships
      ├── 04_motion_buffer.puml      // Ring buffer architecture ✨FIXED
      ├── 07_coordinated_move_sequence.puml // Motion execution sequence
      └── 12_timer_architecture.puml // TMR1 + OCR timing ✨UPDATED
```

**Design Philosophy**:
- **Serial G-code control** (UGS → parser → motion buffer → execution)
- **Time-based interpolation** (NOT Bresenham step counting)
- **Hardware-accelerated pulse generation** (OCR modules, no software interrupts)
- **Per-axis motion limits** (Z can be slower than XY)
- **Centralized settings** (motion_math.c for GRBL $100-$133)
- **Centralized types** (motion_types.h - single source of truth)
- **MISRA C:2012 compliant** (safety-critical embedded code standards)
- **XC32 optimized** (minimal RAM footprint, optimal flash placement)
- **Visual documentation** (PlantUML diagrams for architecture understanding)

**Motion Control Data Flow** (Production):
```
UART2 (115200 baud)
    ↓
UGS_Interface (ugs_interface.c) - Serial protocol, flow control
    ↓
G-code Parser (gcode_parser.c) - Parse commands → parsed_move_t
    ↓
Motion Buffer (motion_buffer.c) - Ring buffer, look-ahead planning
    ↓
Multi-Axis Control (multiaxis_control.c) - Time-synchronized S-curves
    ↓
Hardware OCR/TMR Modules - Step pulse generation
```

**TODO - NEXT PRIORITY**: 
🎯 **✅ SERIAL BUFFER FIX - COMPLETE! (October 18, 2025)** 🎉:
- ✅ **Root Cause Identified**: 256-byte UART buffer too small for burst commands
  - Compared with mikroC version (Pic32mzCNC/ folder)
  - mikroC used 500-byte ring buffer with DMA auto-fill
  - Harmony/MCC used 256-byte interrupt-driven buffer
- ✅ **Solution Applied**: Increased UART buffers to 512 bytes
  - Changed UART2_READ_BUFFER_SIZE from 256→512
  - Changed UART2_WRITE_BUFFER_SIZE from 256→512
  - Added 100ms inter-command delay in test scripts
- ✅ **Memory Impact**: +512 bytes (0.025% of 2MB RAM - negligible)
- ✅ **Firmware Rebuilt**: New hex file ready for testing
- **Status**: Serial communication FIXED! Ready for coordinate system testing!

🎯 **Coordinate System Fixes (Next Priority)** ⚡:
- **G92 Bug**: Sets offset to current position instead of resetting work coordinates
  - Location: motion_math.c line 895 `MotionMath_WorkToMachine()`
  - Symptom: Subsequent moves calculate wrong step counts (658 instead of 800)
- **G91 Bug**: Relative mode only commands 400 steps instead of 800
  - Symptom: Position reports 53.85mm instead of 5mm
- **Position Feedback Bug**: Incorrect mm conversion or offset application
- **Plan**: Fix after verifying OCR hardware works correctly with test_ocr_direct.c

🎯 **Hardware Motion Testing (Ready - After Coordinate Fixes)** ⏸️:
- ✅ UGS connectivity verified - connects as "GRBL 1.1f"
- ✅ System commands working - $I, $G, $$, $#, $N, $
- ✅ Settings management - $100-$133 read/write operational
- ✅ Real-time position feedback - ? command shows actual positions
- ✅ **Non-blocking protocol active** - GRBL Character-Counting enables continuous motion!
- ✅ **Timer prescaler fix applied** - Prevents 16-bit overflow at slow speeds
- ⏸️ **Blocked**: Need to fix coordinate bugs before full motion testing
  - Test slow Z-axis: G1 Z1 F60 (should move correctly, not 2-3x too fast!)
  - Send multiple G-code moves: G90, G1 X10 Y10 F1000, G1 X20 Y20 F1000, G1 X30 Y30 F1000
  - **Verify non-blocking behavior**: "ok" sent immediately, motion continues in background!
  - **Verify continuous motion**: No stops between moves (buffer fills with commands)
  - Observe position values update during motion in UGS status window
  - Test real-time commands: ! (feed hold), ~ (cycle start), ^X (reset)
  - Verify settings changes: $100=200, send move, verify new steps/mm applied
  - Use oscilloscope to confirm smooth cornering with multiple moves queued
  - Test buffer full condition: Send 20+ rapid moves, verify UGS retries when buffer full

🎯 **Look-Ahead Planning Implementation (Ready for Phase 3!)**
- Motion buffer now accepts commands non-blocking (Phase 2 complete ✅)
- Next step: Implement full look-ahead planning in MotionBuffer_RecalculateAll()
  - Forward pass: Calculate maximum exit velocities for each block
  - Reverse pass: Ensure acceleration limits respected
  - Junction velocity optimization for smooth cornering
  - S-curve profile generation with entry/exit velocities
- Test with complex G-code: Circles, spirals, text engraving paths
- Measure corner speeds with oscilloscope (should NOT slow to zero!)

🎯 **Future Development (Phase 3+)**
- Add arc support (G2/G3 circular interpolation)
  - Arc engine with center-format and radius-format
  - Integration with look-ahead planner
- Implement coordinate systems ($# command - work offsets G54-G59)
- Add probing support (G38.x commands)
- Spindle PWM output (M3/M4 state tracking complete, GPIO pending)
- Coolant GPIO control (M7/M8/M9 state tracking complete, GPIO pending)

**Known Working Commands**:
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
M3 S1000             ; Spindle on CW @ 1000 RPM
M5                   ; Spindle off
```

## Architecture Overview

This is a **modular embedded CNC controller** for the PIC32MZ2048EFH100 microcontroller with **hardware-accelerated multi-axis S-curve motion profiles**. The system uses independent OCR (Output Compare) modules for pulse generation, eliminating the need for GRBL's traditional 30kHz step interrupt.

### Core Architecture Pattern (Current - October 2025)
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

### Critical Data Flow
- **Real-time control**: TMR1 @ 1kHz updates S-curve velocities for ALL active axes
- **Step generation**: Independent OCR hardware modules (OCMP1/3/4/5) generate pulses
- **Position feedback**: OCR callbacks increment `step_count` (volatile, per-axis)
- **Synchronization**: All axes share segment timing from dominant axis (for coordinated moves)
- **Safety**: Per-axis active flags, step count validation, velocity clamping

## Key Files & Responsibilities

| File | Purpose | Critical Patterns |
|------|---------|------------------|
| `srcs/main.c` | **G-code processing & motion execution** | ProcessGCode() → ExecuteMotion() → APP_Tasks() → SYS_Tasks() loop |
| `srcs/app.c` | **System management, LED status** | Simplified (SW1/SW2 removed), LED1 heartbeat, LED2 power-on, error state framework |
| `srcs/gcode_parser.c` | **GRBL v1.1f G-code parser** | 1354 lines: Modal/non-modal commands, 13 modal groups, MISRA C:2012 compliant |
| `incs/gcode_parser.h` | **Parser API & modal state** | 357 lines: parser_modal_state_t (~166 bytes), 13 modal groups, work coordinate systems |
| `srcs/ugs_interface.c` | **UGS serial protocol** | UART2 ring buffer, flow control, real-time commands (?, !, ~, ^X) |
| `incs/ugs_interface.h` | **UGS API** | SendOK(), SendError(), Print() for GRBL protocol |
| `incs/motion/motion_types.h` | **Centralized type definitions** | 235 lines: Single source of truth - axis_id_t, position_t, motion_block_t, parsed_move_t |
| `srcs/motion/motion_buffer.c` | **Ring buffer for look-ahead planning** | 284 lines: 16-block FIFO, converts parsed_move_t to motion_block_t |
| `incs/motion/motion_buffer.h` | **Motion buffer API** | 207 lines: Add/GetNext/Recalculate, Pause/Resume/Clear, flow control |
| `srcs/motion/multiaxis_control.c` | **Time-based S-curve interpolation** | 1169 lines: 7-segment profiles, TMR1 @ 1kHz, per-axis limits from motion_math |
| `incs/motion/multiaxis_control.h` | **Multi-axis API** | 4-axis support (X/Y/Z/A), coordinated/single-axis moves, driver enable control |
| `srcs/motion/motion_math.c` | **Kinematics & settings library** | 733 lines: Unit conversions, GRBL settings, look-ahead helpers, time-based calculations |
| `incs/motion/motion_math.h` | **Motion math API** | 398 lines: Settings management, velocity calculations, junction planning, S-curve timing |
| `srcs/motion/stepper_control.c` | **Legacy single-axis reference** | UNUSED - kept for reference only |
| `docs/GCODE_PARSER_COMPLETE.md` | **GRBL v1.1f implementation guide** | 500+ lines: Command reference, modal groups, testing recommendations |
| `docs/XC32_COMPLIANCE_GCODE_PARSER.md` | **MISRA/XC32 compliance documentation** | Memory usage, compiler optimization, MISRA C:2012 rules |
| `docs/APP_CLEANUP_SUMMARY.md` | **App layer cleanup documentation** | SW1/SW2 removal, architecture changes |
| `docs/MAKEFILE_QUIET_BUILD.md` | **Quiet build documentation** | make quiet target for filtered output |
| `docs/CODING_STANDARDS.md` | **Coding standards & file organization** | ⚠️ MANDATORY: Variable declaration rules, ISR safety patterns |

## Coding Standards (MANDATORY)

⚠️ **CRITICAL**: See `docs/CODING_STANDARDS.md` for complete standards.

### File-Level Variable Declaration Rule

**ALL file-level variables (static or non-static) MUST be declared at the top of each file under this comment:**

```c
// *****************************************************************************
// Local Variables
// *****************************************************************************
```

**Rationale:**
- ISR functions need access to file-scope variables
- Variables must be declared BEFORE ISR definitions
- Improves readability and maintainability

**Example (CORRECT):**
```c
// multiaxis_control.c

#include "definitions.h"

// *****************************************************************************
// Local Variables
// *****************************************************************************

// Coordinated move state (accessed by TMR1 ISR @ 1kHz)
static motion_coordinated_move_t coord_move;

// Per-axis state (accessed by TMR1 ISR @ 1kHz)  
static volatile scurve_state_t axis_state[NUM_AXES];

// *****************************************************************************
// Function Implementations
// *****************************************************************************

static void TMR1_MultiAxisControl(uint32_t status, uintptr_t context)
{
    // ✅ Can access coord_move and axis_state here!
    axis_id_t dominant = coord_move.dominant_axis;
}
```

**Example (WRONG - DO NOT DO THIS):**
```c
// ❌ Variable declared in middle of file

static void TMR1_MultiAxisControl(uint32_t status, uintptr_t context)
{
    // ❌ COMPILE ERROR: coord_move not yet declared!
    axis_id_t dominant = coord_move.dominant_axis;
}

// ❌ Too late! ISR above already tried to use it
static motion_coordinated_move_t coord_move;
```

**Real Bug Example:**
```
multiaxis_control.c:607:31: error: 'coord_move' undeclared (first use in this function)
```
This error occurred because `coord_move` was declared at line 1040, but the ISR at line 607 tried to access it.

### ISR Safety Checklist
- [ ] All ISR-accessed variables declared at file scope (top of file)
- [ ] Use `volatile` qualifier for ISR-shared state
- [ ] Document which ISR accesses each variable
- [ ] Keep ISR code minimal (no printf, no malloc, no complex operations)

## Development Workflow

### Build System (Cross-Platform Make)
```bash
# From project root directory:
make all                    # Clean build with hex generation
make build_dir             # Create directory structure (run first)
make clean                 # Clean all outputs  
make platform             # Show build system info
make debug                # Show detailed build configuration
```

**Critical**: Build system expects specific directory structure:
- `srcs/` - Source files (.c, .S)  
- `incs/` - Headers (.h)
- `objs/` - Object files (auto-generated)
- `bins/` - Final executables (.elf, .hex)

### Testing with PowerShell Scripts
The project uses **PowerShell scripts for hardware-in-the-loop testing**:

```powershell
# Basic motion testing
.\motion_test.ps1 -Port COM4 -BaudRate 115200

# UGS compatibility testing  
.\ugs_test.ps1 -Port COM4 -GCodeFile modular_test.gcode

# Real-time debugging
.\monitor_debug.ps1 -Port COM4
```

**Pattern**: All test scripts use `Send-GCodeCommand` function with timeout/retry logic for reliable serial communication.

## Code Patterns & Conventions

### Modular API Design
**Current (October 2025)**: Production system with G-code parser integration:

```c
// G-code Parser API (gcode_parser.h)
void GCode_Initialize(void);                          // Initialize parser with modal defaults
bool GCode_BufferLine(char *buffer, size_t size);    // Buffer incoming serial line
bool GCode_ParseLine(const char *line, parsed_move_t *move);  // Parse G-code → move structure
bool GCode_IsControlChar(char c);                    // Check for real-time commands (?, !, ~, ^X)
const char* GCode_GetLastError(void);                // Get error message
void GCode_ClearError(void);                         // Clear error state
const parser_modal_state_t* GCode_GetModalState(void);  // Get current modal state

// UGS Interface API (ugs_interface.h)
void UGS_Initialize(void);                           // Initialize UART2 @ 115200 baud
void UGS_SendOK(void);                               // Send "ok\r\n" for flow control
void UGS_SendError(uint8_t code, const char *msg);   // Send "error:X (message)\r\n"
void UGS_Print(const char *str);                     // Send arbitrary string
bool UGS_RxHasData(void);                            // Check if data available

// Motion Buffer API (motion_buffer.h)
void MotionBuffer_Initialize(void);                  // Initialize ring buffer
bool MotionBuffer_Add(const parsed_move_t *move);    // Add move to buffer (converts mm→steps)
bool MotionBuffer_GetNext(motion_block_t *block);    // Dequeue next planned move
bool MotionBuffer_HasData(void);                     // Check if moves pending
void MotionBuffer_Pause(void);                       // Feed hold (!)
void MotionBuffer_Resume(void);                      // Cycle start (~)
void MotionBuffer_Clear(void);                       // Emergency stop/soft reset

// Multi-Axis Control API - Time-based S-curve profiles
void MultiAxis_Initialize(void);                     // Calls MotionMath_InitializeSettings()
void MultiAxis_ExecuteCoordinatedMove(int32_t steps[NUM_AXES]);  // Time-synchronized motion
bool MultiAxis_IsBusy(void);                         // Checks all axes
void MultiAxis_EmergencyStop(void);                  // Immediate stop all axes

// Motion Math API - Kinematics & Settings
void MotionMath_InitializeSettings(void);            // Load GRBL defaults
float MotionMath_GetMaxVelocityStepsPerSec(axis_id_t axis);
float MotionMath_GetAccelStepsPerSec2(axis_id_t axis);
float MotionMath_GetJerkStepsPerSec3(axis_id_t axis);
int32_t MotionMath_MMToSteps(float mm, axis_id_t axis);  // Unit conversion
float MotionMath_StepsToMM(int32_t steps, axis_id_t axis);
```

### Motion Math Settings Pattern
**GRBL v1.1f Compatibility**: Centralized settings in motion_math module

```c
// Default settings (loaded by MotionMath_InitializeSettings)
motion_settings.steps_per_mm[AXIS_X] = 250.0f;    // $100: Steps per mm
motion_settings.max_rate[AXIS_X] = 5000.0f;       // $110: Max rate (mm/min)
motion_settings.acceleration[AXIS_X] = 500.0f;    // $120: Acceleration (mm/sec²)
motion_settings.max_travel[AXIS_X] = 300.0f;      // $130: Max travel (mm)
motion_settings.junction_deviation = 0.01f;       // $11: Junction deviation
motion_settings.jerk_limit = 5000.0f;             // Jerk limit (mm/sec³)

// CORRECT: Use motion_math for conversions
int32_t steps = MotionMath_MMToSteps(10.0f, AXIS_X);  // 10mm → 2500 steps
float max_vel = MotionMath_GetMaxVelocityStepsPerSec(AXIS_X);  // 5000mm/min → steps/sec

// INCORRECT: Don't hardcode motion parameters
static float max_velocity = 5000.0f;  // ❌ Use motion_math instead!
```

### OCR Hardware Integration
**PIC32MZ-specific**: Hardware pulse generation using Output Compare modules:

```c
// OCR assignments (fixed in hardware):
// OCMP1 → Y-axis step pulses  
// OCMP3 → A-axis step pulses (4th axis)
// OCMP4 → X-axis step pulses
// OCMP5 → Z-axis step pulses

// Timer sources (VERIFIED per MCC configuration - October 2025):
// TMR1 → 1kHz motion control timing (MotionPlanner_UpdateTrajectory)
// TMR2 → OCMP4 time base for X-axis step pulse generation
// TMR3 → OCMP5 time base for Z-axis step pulse generation
// TMR4 → OCMP1 time base for Y-axis step pulse generation
// TMR5 → OCMP3 time base for A-axis step pulse generation
```

**PIC32MZ Timer Source Options (Table 18-1):**
```
Output Compare Module | Available Timer Sources | ACTUAL Assignment
------------------------------------------------------------------
OC1 (Y-axis)         | Timer4 or Timer5        | TMR4 (per MCC)
OC2 (unused)         | Timer4 or Timer5        | N/A
OC3 (A-axis)         | Timer4 or Timer5        | TMR5 (per MCC)
OC4 (X-axis)         | Timer2 or Timer3        | TMR2 (per MCC)
OC5 (Z-axis)         | Timer2 or Timer3        | TMR3 (per MCC)

**OCR Dual-Compare Architecture - VERIFIED WORKING PATTERN (Oct 2025):**

This is the **PRODUCTION-PROVEN CONFIGURATION** tested with hardware oscilloscope verification:

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
 * The hardware generates rising edge at OCxR and falling edge at OCxRS.
````
 * 
 * Example with period=300:
 *   TMR2PR = 300          // Timer rolls over at 300
 *   OC4R = 260            // Pulse rises at count 260 (300-40)
 *   OC4RS = 40            // Pulse falls at count 40
 *   Result: Pin HIGH from count 40 to 260, LOW from 260 to 300, then repeat
 *   Effective pulse width = 40 counts (meets DRV8825 1.9µs minimum)
 * 
 * Timing diagram:
 *   Count: 0....40.......260.......300 (rollover)
 *   Pin:   LOW  HIGH     LOW       LOW
 *          └────────────┘
 *          40 counts ON
 */
TMRx_PeriodSet(period);                                    // Set timer rollover
OCMPx_CompareValueSet(period - OCMP_PULSE_WIDTH);         // Rising edge (variable)
OCMPx_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);         // Falling edge (fixed at 40)
OCMPx_Enable();
TMRx_Start();                                              // CRITICAL: Must restart timer for each move
```

**CRITICAL MOTION EXECUTION PATTERN (October 2025):**

This sequence is **MANDATORY** for reliable bidirectional motion:

```c
// 1. Set direction GPIO BEFORE enabling step pulses (DRV8825 requirement)
if (direction_forward) {
    DirX_Set();    // GPIO high for forward
} else {
    DirX_Clear();  // GPIO low for reverse
}

// 2. Configure OCR registers with proven pattern
TMR2_PeriodSet(period);
OCMP4_CompareValueSet(period - 40);  // Rising edge
OCMP4_CompareSecondaryValueSet(40);  // Falling edge

// 3. Enable OCR module
OCMP4_Enable();

// 4. **ALWAYS** restart timer for each move (even if already running)
TMR2_Start();  // Critical! Timers are stopped when motion completes
```

**Common Mistakes to Avoid:**
- ❌ **Don't forget `TMRx_Start()`** - Timer stops when motion completes, must restart for next move
- ❌ **Don't swap OCxR/OCxRS values** - Use exact pattern above (period-40 / 40)
- ❌ **Don't set direction after step pulses start** - DRV8825 needs direction stable before first pulse
- ❌ **Don't use wrong timer** - X=TMR2, Y=TMR4, Z=TMR3 per MCC configuration

**Key Register Values:**
- **TMRxPR**: Timer period register (controls pulse frequency)
- **OCxR**: Primary compare (rising edge - varies: period-40)
- **OCxRS**: Secondary compare (falling edge - fixed at 40)
- **Pulse Width**: Always 40 counts (meeting DRV8825 1.9µs minimum)
- **Maximum Period**: 65485 counts (16-bit timer limit with safety margin)

### DRV8825 Stepper Driver Interface
**Hardware**: Pololu DRV8825 carrier boards (or compatible) for bipolar stepper motors

**Control signals** (microcontroller → driver):
- **STEP**: Pulse input - each rising edge = one microstep (pulled low by 100kΩ)
- **DIR**: Direction control - HIGH/LOW sets rotation direction (pulled low by 100kΩ)
  - **CRITICAL**: Must be set BEFORE first step pulse and remain stable during motion
  - **Our implementation**: Set via `DirX_Set()`/`DirX_Clear()` before `OCMPx_Enable()`
- **ENABLE**: Active-low enable (can leave disconnected for always-enabled)
- **RESET/SLEEP**: Pulled high by 1MΩ/10kΩ respectively (normal operation)

**Timing requirements** (DRV8825 datasheet):
- **Minimum STEP pulse width**: 1.9µs HIGH + 1.9µs LOW
- **Our implementation**: 40 timer counts @ 1MHz = 40µs (safe margin above minimum)
- **Why 40 counts**: Ensures reliable detection across all microstepping modes
- **Direction setup time**: 200ns minimum (our GPIO write provides this)

**Microstepping configuration** (MODE0/1/2 pins with 100kΩ pull-downs):
```
MODE2  MODE1  MODE0  Resolution
--------------------------------
 Low    Low    Low   Full step
High    Low    Low   Half step
 Low   High    Low   1/4 step
High   High    Low   1/8 step
 Low    Low   High   1/16 step
High    Low   High   1/32 step
```

**Power and current limiting**:
- **VMOT**: 8.2V - 45V motor supply (100µF decoupling capacitor required)
- **Current limit**: Set via VREF potentiometer using formula `Current = VREF × 2`
- **Current sense resistors**: 0.100Ω (DRV8825) vs 0.330Ω (A4988)
- **CRITICAL**: Never connect/disconnect motors while powered - will destroy driver

**Fault protection**:
- **FAULT pin**: Pulls low on over-current, over-temperature, or under-voltage
- **Protection resistor**: 1.5kΩ in series allows safe connection to logic supply
- Our system can monitor this pin for real-time error detection

### Timer Prescaler Configuration (VERIFIED - October 18, 2025)

**CURRENT CONFIGURATION** ✅ **CORRECT AND VERIFIED**:

**Hardware Configuration**:
```
Peripheral Clock (PBCLK3): 50 MHz
Timer Prescaler: 1:32 (TMR2/3/4/5)
Timer Clock: 50 MHz ÷ 32 = 1.5625 MHz
Resolution: 640ns per count
```

**Code Configuration**:
```c
// In motion_types.h:
#define TMR_CLOCK_HZ 1562500UL  // 1.5625 MHz (50 MHz PBCLK3 ÷ 32 prescaler)

// Timer characteristics:
Timer Clock: 1.5625 MHz
Resolution: 640ns per count
Pulse Width: 40 counts = 25.6µs (exceeds DRV8825 1.9µs minimum ✓)

// Step rate range (fits in 16-bit timer):
Min: 23.8 steps/sec (period = 65,535 counts = 41.94ms)
Max: 31,250 steps/sec (period = 50 counts = 32µs)

// Example calculations:
100 steps/sec:   period = 15,625 counts (10ms) ✓ FITS!
1,000 steps/sec: period = 1,563 counts (1ms) ✓ FITS!
5,000 steps/sec: period = 313 counts (200µs) ✓ FITS!
```

**MCC Configuration** ✅ **VERIFIED IN HARDWARE**:
- TMR2 (X-axis): Prescaler = **1:32** ✓
- TMR3 (Z-axis): Prescaler = **1:32** ✓
- TMR4 (Y-axis): Prescaler = **1:32** ✓
- TMR5 (A-axis): Prescaler = **1:32** ✓
- PBCLK3 (Timer peripheral clock): **50 MHz** ✓

**Hardware Test Results** (October 18, 2025):
- ✅ Test 1: Y-axis 800 steps = 10.000mm (exact!)
- ✅ Test 2: Coordinated X/Y diagonal completed (no hang)
- ✅ Test 3: Negative moves completed successfully
- ✅ Step accuracy: ±2 steps (±0.025mm) - within tolerance
- ✅ All motion tests complete without hanging

**Benefits of 1:32 Prescaler**:
- ✅ Supports slow Z-axis moves (down to 24 steps/sec)
- ✅ Still fast enough for rapids (up to 31,250 steps/sec)
- ✅ All GRBL settings ($110-$113 max rates) fit within range
- ✅ 13.5x safety margin on pulse width (25.6µs vs 1.9µs minimum)
- ✅ No timer overflow issues

**CRITICAL**: Do NOT change prescaler values - current configuration is verified and working!

### Fault protection**:
- **FAULT pin**: Pulls low on over-current, over-temperature, or under-voltage
- **Protection resistor**: 1.5kΩ in series allows safe connection to logic supply
- Our system can monitor this pin for real-time error detection

### Time-Based Interpolation Architecture
**CRITICAL**: This system uses **time-based velocity interpolation**, NOT Bresenham step counting!

```c
// How it works (TMR1 @ 1kHz):
TMR1_MultiAxisControl() {
    // Get per-axis motion limits from motion_math
    float max_velocity = MotionMath_GetMaxVelocityStepsPerSec(axis);
    float max_accel = MotionMath_GetAccelStepsPerSec2(axis);
    float max_jerk = MotionMath_GetJerkStepsPerSec3(axis);
    
    // Update S-curve velocity profile every 1ms
    velocity = calculate_scurve_velocity(time_elapsed, max_accel, max_jerk);
    
    // Convert velocity to OCR period for hardware pulse generation
    OCR_period = 1MHz / velocity_steps_sec;
    
    // All axes finish at SAME TIME (coordinated via dominant axis)
}
```

**Key Differences from Bresenham**:
- ✅ **Velocity-driven**: OCR hardware generates pulses at calculated rates
- ✅ **Time-synchronized**: All axes finish simultaneously (coordinated moves)
- ✅ **Per-axis limits**: Z can have different acceleration than XY
- ❌ **NOT step counting**: No error accumulation or step ratios

### GRBL Settings Pattern
Full GRBL v1.1f compliance (ready for G-code parser):

```c
// Standard GRBL settings ($100-$133):
$100-$103 = Steps per mm (X,Y,Z,A)
$110-$113 = Max rates (mm/min)  
$120-$123 = Acceleration (mm/sec²)
$130-$133 = Max travel (mm)
$11 = Junction deviation (for look-ahead)
$12 = Arc tolerance (for G2/G3)
```

### S-Curve Motion Profiles
**Advanced feature**: 7-segment jerk-limited motion profiles:

```c
// S-curve profile structure
typedef struct {
    float total_time, accel_time, const_time, decel_time;
    float peak_velocity, acceleration, distance;
    bool use_scurve;
    position_t start_pos, end_pos;
} scurve_motion_profile_t;
```

### Error Handling & Safety
**Critical safety patterns**:

```c  
// Hard limit handling (immediate response)
void APP_ProcessLimitSwitches(void) {
    bool x_limit = !GPIO_PinRead(GPIO_PIN_RA7);  // Active low
    if (x_limit) {
        INTERP_HandleHardLimit(AXIS_X, true, false);
        appData.state = APP_STATE_MOTION_ERROR;  // Immediate stop
    }
}

// Soft limit validation (preventive)
if (!INTERP_CheckSoftLimits(target_position)) {
    return false;  // Reject unsafe move
}
```

## Hardware-Specific Considerations

### PIC32MZ Memory Management
- **Heap**: 20KB configured in Makefile
- **Stack**: 20KB configured in Makefile
- **Flash**: 2MB total, use efficiently for look-ahead buffers

### Pin Assignments (Active Low Logic)
```c
// Limit switches (check app.h for current assignments):
GPIO_PIN_RA7  → X-axis negative limit
GPIO_PIN_RA9  → X-axis positive limit  
GPIO_PIN_RA10 → Y-axis negative limit
GPIO_PIN_RA14 → Y-axis positive limit
GPIO_PIN_RA15 → Z-axis negative limit
```

### Pick-and-Place Mode
**Special feature**: Z-axis limit masking for spring-loaded operations:

```c
APP_SetPickAndPlaceMode(true);   // Mask Z minimum limit
// ... perform pick/place operations ...
APP_SetPickAndPlaceMode(false);  // Restore normal limits
```

## Integration Points

### Hardware Testing Current Capabilities
- **Serial G-code control**: Send commands via UGS or serial terminal @ 115200 baud
- **LED1**: Heartbeat @ 1Hz when idle, solid during motion (driven by TMR1 callback)
- **LED2**: Power-on indicator
- **All axes enabled**: X, Y, Z, A all initialized and ready
- **Verified motion**: Oscilloscope confirms smooth S-curve velocity profiles
- **Real-time commands**: ?, !, ~, ^X supported (status, hold, resume, reset)

### Current Production Features
- **Universal G-code Sender (UGS)**: GRBL v1.1f protocol compatible ✅
- **Serial Protocol**: Real-time commands (feed hold, cycle start, reset) ✅
- **Modal state tracking**: G90/G91, work coordinate systems, M-commands ✅
- **Motion buffer**: Ring buffer with look-ahead planning framework ✅

### Future Integration Points
- **Look-ahead planning**: Full implementation in motion buffer (currently placeholder)
- **Arc Support**: G2/G3 circular interpolation (requires arc engine)
- **Probing**: G38.x probe commands (requires hardware integration)
- **Spindle PWM**: M3/M4 with PWM output (state tracking implemented, GPIO pending)
- **Coolant control**: M7/M8/M9 GPIO output (state tracking implemented, GPIO pending)

### Cross-Platform Build  
- **Windows**: PowerShell-based testing, MPLAB X IDE v6.25
- **Linux**: Make-based build system, XC32 v4.60 compiler
- **Paths**: Auto-detected OS with proper path separators
- **Quiet build**: `make quiet` for filtered output (errors/warnings only)

### Version Control
- **Git workflow**: Use raw git commands (not GitKraken)
- Standard git add/commit/push workflow for version control

## Common Tasks

### Testing Current System
1. **Flash firmware** to PIC32MZ board (`bins/CS23.hex`)
2. **Connect via UGS** or serial terminal @ 115200 baud
3. **Send G-code commands**: `G90`, `G1 X10 Y10 F1500`, etc.
4. **Observe LED1** for heartbeat (1Hz idle) or solid (motion active)
5. **Observe LED2** for power-on status
6. **Use oscilloscope** to verify S-curve velocity profiles on step/dir pins

### Adding New Motion Commands
1. **Via G-code (PRODUCTION METHOD)**:
   ```gcode
   G90              ; Absolute mode
   G1 X10 Y20 F1500 ; Linear move to (10,20) @ 1500mm/min
   G92 X0 Y0        ; Set current position as (0,0)
   ```

2. **For coordinated moves (programmatic)**:
   ```c
   int32_t steps[NUM_AXES] = {800, 400, 0, 0};  // X=10mm, Y=5mm (80 steps/mm)
   MultiAxis_ExecuteCoordinatedMove(steps);  // Time-synchronized - ensures straight line motion
   ```

3. **For G-code parsing (in main.c)**:
   ```c
   parsed_move_t move;
   if (GCode_ParseLine("G1 X10 F1500", &move)) {
       MotionBuffer_Add(&move);  // Adds to ring buffer → converts mm to steps
   }
   ```

4. **Monitor completion**:
   ```c
   while (MultiAxis_IsBusy()) { }  // Wait for all axes
   ```

### Debugging Motion Issues
1. Check TMR1 @ 1kHz callback: `TMR1_MultiAxisControl()` in `multiaxis_control.c`
2. Check OCR interrupt callbacks: `OCMP4_StepCounter_X()`, `OCMP1_StepCounter_Y()`, `OCMP5_StepCounter_Z()`
3. Verify S-curve profile calculations with oscilloscope (expect symmetric velocity ramps)
4. Monitor per-axis `active` flags and `step_count` values
5. Verify LED1 heartbeat confirms TMR1 is running @ 1Hz
6. Check G-code parser state: `GCode_GetModalState()` for current modes
7. Monitor motion buffer: `MotionBuffer_GetCount()` for pending moves

### Hardware Testing
1. **Always** use conservative velocities for initial testing (max_velocity = 5000 mm/min)
2. Verify OCR period calculations with oscilloscope (expect symmetric S-curve)
3. Test emergency stop functionality: Send `^X` (Ctrl-X) via serial
4. Test feed hold/resume: Send `!` to pause, `~` to resume
5. All axes configured and enabled (X, Y, Z, A)

### Testing Current System
1. **Flash firmware** to PIC32MZ board (`bins/CS23.hex`)
2. **Press SW1** to trigger X-axis single move (5000 steps forward)
3. **Press SW2** to trigger coordinated 3-axis move (X/Y/Z)
4. **Observe LED1** for heartbeat (1Hz idle) or solid (motion active)
5. **Observe LED2** for power-on and axis processing activity
6. **Use oscilloscope** to verify S-curve velocity profiles on step/dir pins

## Motion Math Integration (October 16, 2025)

### Architecture Overview
The motion system now uses a **two-layer architecture**:

1. **Motion Math Layer** (`motion_math.c/h`):
   - Centralized GRBL settings storage
   - Unit conversions (mm ↔ steps, mm/min ↔ steps/sec)
   - Look-ahead planning helpers (junction velocity, S-curve timing)
   - Pure functions (no side effects, easy to test)

2. **Motion Control Layer** (`multiaxis_control.c/h`):
   - Time-based S-curve interpolation (7 segments)
   - Per-axis state machines (TMR1 @ 1kHz)
   - Hardware OCR pulse generation
   - Gets motion limits from motion_math

### Integration Points

**Initialization**:
```c
void MultiAxis_Initialize(void) {
    MotionMath_InitializeSettings();  // Load GRBL defaults
    // ... register callbacks, start TMR1
}
```

**Per-Axis Motion Limits**:
```c
// In calculate_scurve_profile() and TMR1_MultiAxisControl():
float max_velocity = MotionMath_GetMaxVelocityStepsPerSec(axis);
float max_accel = MotionMath_GetAccelStepsPerSec2(axis);
float max_jerk = MotionMath_GetJerkStepsPerSec3(axis);
```

**Default Settings** (Conservative for Testing):
```c
Steps/mm:     250 (all axes) - GT2 belt with 1/16 microstepping
Max Rate:     5000 mm/min (X/Y/A), 2000 mm/min (Z)
Acceleration: 500 mm/sec² (X/Y/A), 200 mm/sec² (Z)
Max Travel:   300mm (X/Y), 100mm (Z), 360° (A)
Junction Dev: 0.01mm - Tight corners for accuracy
Jerk Limit:   5000 mm/sec³ - Smooth S-curve transitions
```

## Centralized Type System (October 16, 2025)

### Architecture Philosophy

**Problem Solved**: Previous architecture had duplicate type definitions across multiple headers (`axis_id_t` in both `multiaxis_control.h` and `motion_math.h`, causing compilation errors and maintenance headaches).

**Solution**: Single `motion_types.h` header as the **authoritative source** for all motion-related data structures.

### Type System Organization

**File**: `incs/motion/motion_types.h` (235 lines)

All motion modules include this header:
```c
#include "motion_types.h"  // Gets ALL motion types
```

**Critical Rule**: 🚫 **NEVER define types elsewhere!** Always use motion_types.h

### Core Type Categories

#### **1. Axis Definitions**
```c
typedef enum {
    AXIS_X = 0,
    AXIS_Y = 1,
    AXIS_Z = 2,
    AXIS_A = 3,
    NUM_AXES = 4
} axis_id_t;
```
Used by: ALL motion modules for axis identification

#### **2. Position Tracking**
```c
typedef struct {
    int32_t x, y, z, a;
} position_t;
```
Used by: Tracking current/target positions in steps

#### **3. Motion Block (Ring Buffer Entry)** ⭐ **CRITICAL**
```c
typedef struct {
    int32_t steps[NUM_AXES];        // Target position (absolute steps)
    float feedrate;                 // Requested feedrate (mm/min)
    float entry_velocity;           // From look-ahead planner (mm/min)
    float exit_velocity;            // From look-ahead planner (mm/min)
    float max_entry_velocity;       // Junction limit (mm/min)
    bool recalculate_flag;          // Needs replanning
    bool axis_active[NUM_AXES];     // Which axes move
    scurve_motion_profile_t profile; // Pre-calculated S-curve
} motion_block_t;
```
Used by: Motion buffer ring buffer - **this is what feeds multiaxis_control**

#### **4. Parsed G-Code Move** ⭐ **CRITICAL**
```c
typedef struct {
    float target[NUM_AXES];         // Target position (mm or degrees)
    float feedrate;                 // Feed rate (mm/min)
    bool absolute_mode;             // G90 (true) or G91 (false)
    bool axis_words[NUM_AXES];      // Which axes specified
    uint8_t motion_mode;            // G0, G1, G2, G3, etc.
} parsed_move_t;
```
Used by: G-code parser output → input to motion buffer

#### **5. S-Curve Motion Profile**
```c
typedef struct {
    float total_time, accel_time, const_time, decel_time;
    float peak_velocity, acceleration, distance;
    bool use_scurve;
    position_t start_pos, end_pos;
} scurve_motion_profile_t;
```
Used by: multiaxis_control for 7-segment jerk-limited motion

#### **6. Other Important Types**
- `coordinated_move_t` - Multi-axis move request (steps + active flags)
- `velocity_profile_t` - Look-ahead velocity optimization data
- `scurve_timing_t` - Detailed 7-segment timing calculations
- `motion_settings_t` - GRBL v1.1f settings structure ($100-$133)
- `motion_coordinated_move_t` - Coordination analysis (dominant axis, ratios)

### Benefits of Centralization

1. ✅ **No Duplicate Definitions** - Compiler catches redefinitions immediately
2. ✅ **Single Source of Truth** - Update type once, all modules see it
3. ✅ **Clear Dependencies** - `#include "motion_types.h"` documents what's used
4. ✅ **Easy Maintenance** - Add new fields without hunting through multiple files
5. ✅ **Scalability** - Add new types (spindle_state_t, coolant_state_t) in one place

### Migration Pattern

**BEFORE (Duplicate Types)**:
```c
// multiaxis_control.h
typedef enum { AXIS_X, AXIS_Y, AXIS_Z, AXIS_A } axis_id_t;

// motion_math.h  
typedef enum { AXIS_X, AXIS_Y, AXIS_Z, AXIS_A } axis_id_t;  // ❌ Duplicate!
```

**AFTER (Centralized)**:
```c
// motion_types.h
typedef enum { AXIS_X, AXIS_Y, AXIS_Z, AXIS_A } axis_id_t;

// multiaxis_control.h
#include "motion_types.h"  // ✅ Import types

// motion_math.h
#include "motion_types.h"  // ✅ Import types
```

---

## Motion Buffer & Ring Buffer Architecture (October 16, 2025)

### Overview

The motion buffer implements a **circular FIFO queue** of planned motion blocks that feeds the real-time motion controller. It provides look-ahead planning to optimize junction velocities and minimize total move time.

### Ring Buffer Design

**File**: `srcs/motion/motion_buffer.c` (284 lines)  
**API**: `incs/motion/motion_buffer.h` (207 lines)

#### **Configuration**
```c
#define MOTION_BUFFER_SIZE 16               // Must be power of 2
#define LOOKAHEAD_PLANNING_THRESHOLD 4      // Trigger replanning at this count
```

#### **Buffer State**
```c
static motion_block_t motion_buffer[MOTION_BUFFER_SIZE];  // Circular array
static volatile uint8_t buffer_head = 0;  // Next write index
static volatile uint8_t buffer_tail = 0;  // Next read index
```

#### **Ring Buffer Properties**
- **Empty**: `head == tail`
- **Full**: `(head + 1) % SIZE == tail`
- **Count**: `(head - tail + SIZE) % SIZE`
- **Modulo Arithmetic**: Efficient wraparound using `% MOTION_BUFFER_SIZE`

### Complete Data Flow (Serial to Motion)

```
┌─────────────────────────────────────────────────────────────────┐
│ Stage 1: Serial Reception (Future)                             │
│ Serial RX Interrupt → Serial Ring Buffer (raw bytes)           │
└────────────────────┬────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────────┐
│ Stage 2: Command Parsing (Future - G-code Parser)              │
│ Extract lines → Parse tokens → Generate parsed_move_t          │
│                                                                 │
│ Example: "G1 X10 Y20 F1500" →                                  │
│   parsed_move_t {                                               │
│     target = {10.0, 20.0, 0.0, 0.0},  // mm                    │
│     feedrate = 1500.0,                // mm/min                │
│     axis_words = {true, true, false, false}                    │
│   }                                                             │
└────────────────────┬────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────────┐
│ Stage 3: Motion Planning (motion_buffer.c) ⭐ IMPLEMENTED       │
│ MotionBuffer_Add(parsed_move_t*) {                             │
│   1. Convert mm to steps (MotionMath_MMToSteps)                │
│   2. Calculate max entry velocity (junction angle)             │
│   3. Add to ring buffer → motion_block_t                        │
│   4. Trigger replanning if threshold reached                   │
│ }                                                               │
│                                                                 │
│ Ring Buffer: [motion_block_t, motion_block_t, ...]             │
│              ↑tail (read)          ↑head (write)                │
└────────────────────┬────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────────┐
│ Stage 4: Look-Ahead Planning (motion_buffer.c) ⭐ PLACEHOLDER   │
│ MotionBuffer_RecalculateAll() {                                 │
│   Forward Pass: Maximize exit velocities                       │
│   Reverse Pass: Ensure accel limits respected                  │
│   Calculate S-curve profiles for each block                    │
│ }                                                               │
│                                                                 │
│ Output: Optimized motion_block_t with:                          │
│   - entry_velocity (safe cornering speed)                      │
│   - exit_velocity (limited by next junction)                   │
│   - scurve_motion_profile_t (ready for execution)              │
└────────────────────┬────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────────┐
│ Stage 5: Motion Execution (multiaxis_control.c) ✅ WORKING      │
│ Main Loop:                                                      │
│   if (!MultiAxis_IsBusy() && !MotionBuffer_IsEmpty()) {        │
│     motion_block_t block;                                       │
│     MotionBuffer_GetNext(&block);  // Dequeue from buffer      │
│     MultiAxis_MoveCoordinated(block.steps);  // Execute!       │
│   }                                                             │
│                                                                 │
│ TMR1 @ 1kHz → S-Curve Interpolation → OCR Pulse Generation     │
└─────────────────────────────────────────────────────────────────┘
```

### Motion Buffer API

#### **Core Functions**

**Initialization**:
```c
void MotionBuffer_Initialize(void);  // Clear buffer, reset pointers
```

**Adding Moves** (from G-code parser):
```c
bool MotionBuffer_Add(const parsed_move_t* move);
// Returns: true if added, false if buffer full
// Side effects:
//   - Converts mm to steps using motion_math
//   - Calculates max_entry_velocity
//   - Triggers replanning if threshold reached
```

**Retrieving Planned Moves** (for execution):
```c
bool MotionBuffer_GetNext(motion_block_t* block);
// Returns: true if block available, false if empty
// Side effects:
//   - Advances tail pointer
//   - Copies block with pre-calculated S-curve profile
```

**Buffer Queries**:
```c
bool MotionBuffer_IsEmpty(void);     // Check if no moves pending
bool MotionBuffer_IsFull(void);      // Check if can accept more moves
uint8_t MotionBuffer_GetCount(void); // Get number of blocks in buffer
```

**Flow Control** (for GRBL protocol):
```c
void MotionBuffer_Pause(void);       // Feed hold (!) - stop retrieving blocks
void MotionBuffer_Resume(void);      // Cycle start (~) - resume motion
void MotionBuffer_Clear(void);       // Emergency stop - discard all blocks
```

**Look-Ahead Planning**:
```c
void MotionBuffer_RecalculateAll(void);  // Optimize all block velocities
float MotionBuffer_CalculateJunctionVelocity(
    const motion_block_t* block1,
    const motion_block_t* block2
);  // Calculate safe cornering speed between two moves
```

### GRBL Serial Protocol Integration

**Flow Control Pattern** (when G-code parser is added):
```c
// In G-code parser main loop:
if (GCode_ParseLine(line, &parsed_move)) {
    if (MotionBuffer_Add(&parsed_move)) {
        Serial_SendOK();  // ✅ Buffer accepted move - UGS can send next
    } else {
        // ❌ Buffer full - DON'T send "ok"
        // UGS will wait, retry on next loop iteration
    }
}
```

**Real-Time Commands** (bypass buffer):
```c
// Check for real-time commands BEFORE parsing:
if (serial_char == '?') {
    GRBL_SendStatusReport();  // Immediate response
    return;
}
if (serial_char == '!') {
    MotionBuffer_Pause();  // Feed hold - stop motion
    return;
}
if (serial_char == '~') {
    MotionBuffer_Resume();  // Cycle start - resume motion
    return;
}
```

### Look-Ahead Planning Algorithm (TODO)

**Current Status**: Placeholder implementation in `recalculate_trapezoids()`

**Full Algorithm** (GRBL-style):
1. **Forward Pass**: Iterate tail → head
   - Calculate maximum exit velocity for each block
   - Limited by: feedrate, acceleration, next junction angle
   
2. **Reverse Pass**: Iterate head → tail
   - Ensure entry velocities respect acceleration limits
   - Adjust exit velocities if entry velocity reduced
   
3. **S-Curve Generation**: For each block
   - Call `MotionMath_CalculateSCurveTiming()` with entry/exit velocities
   - Store profile in `motion_block_t.profile`

### Memory Usage

**Total**: ~2KB for motion buffer
```c
sizeof(motion_block_t) ≈ 120 bytes
16 blocks × 120 bytes = 1920 bytes
Plus state variables: ~100 bytes
Total: ~2KB
```

### Ring Buffer Benefits

1. ✅ **O(1) Operations** - Add and remove are constant time (modulo arithmetic)
2. ✅ **Memory Efficient** - Fixed size, no dynamic allocation
3. ✅ **Cache Friendly** - Contiguous array, good locality
4. ✅ **FIFO Ordering** - Oldest moves executed first (natural for G-code)
5. ✅ **Flow Control Ready** - Buffer full condition for GRBL protocol
6. ✅ **Look-Ahead Ready** - Can iterate forward/backward for velocity planning

### Integration with Motion Math

The motion buffer is the **bridge** between G-code parsing and motion execution:

```c
// Step 1: G-code parser creates parsed_move_t (in mm)
parsed_move_t move = {
    .target = {10.0, 20.0, 0.0, 0.0},  // X=10mm, Y=20mm
    .feedrate = 1500.0,                 // 1500 mm/min
    .axis_words = {true, true, false, false}
};

// Step 2: Motion buffer converts using motion_math
int32_t steps[NUM_AXES];
steps[AXIS_X] = MotionMath_MMToSteps(10.0, AXIS_X);  // 10mm → 2500 steps
steps[AXIS_Y] = MotionMath_MMToSteps(20.0, AXIS_Y);  // 20mm → 5000 steps

// Step 3: Add to ring buffer with calculated velocities
motion_block_t block = {
    .steps = {2500, 5000, 0, 0},
    .feedrate = 1500.0,
    .entry_velocity = calculated_junction_velocity,
    .exit_velocity = next_junction_velocity,
    // ... pre-calculated S-curve profile
};

// Step 4: Execution retrieves from buffer
if (MotionBuffer_GetNext(&block)) {
    MultiAxis_MoveCoordinated(block.steps);  // Execute with S-curve
}
```

---

## Motion Math Integration (October 16, 2025)

### Architecture Overview
The motion system now uses a **two-layer architecture**:

1. **Motion Math Layer** (`motion_math.c/h`):
   - Centralized GRBL settings storage
   - Unit conversions (mm ↔ steps, mm/min ↔ steps/sec)
   - Look-ahead planning helpers (junction velocity, S-curve timing)
   - Pure functions (no side effects, easy to test)

2. **Motion Control Layer** (`multiaxis_control.c/h`):
   - Time-based S-curve interpolation (7 segments)
   - Per-axis state machines (TMR1 @ 1kHz)
   - Hardware OCR pulse generation
   - Gets motion limits from motion_math

### Integration Points

**Initialization**:
```c
void MultiAxis_Initialize(void) {
    MotionMath_InitializeSettings();  // Load GRBL defaults
    // ... register callbacks, start TMR1
}
```

**Per-Axis Motion Limits**:
```c
// In calculate_scurve_profile() and TMR1_MultiAxisControl():
float max_velocity = MotionMath_GetMaxVelocityStepsPerSec(axis);
float max_accel = MotionMath_GetAccelStepsPerSec2(axis);
float max_jerk = MotionMath_GetJerkStepsPerSec3(axis);
```

**Default Settings** (Conservative for Testing):
```c
Steps/mm:     250 (all axes) - GT2 belt with 1/16 microstepping
Max Rate:     5000 mm/min (X/Y/A), 2000 mm/min (Z)
Acceleration: 500 mm/sec² (X/Y/A), 200 mm/sec² (Z)
Max Travel:   300mm (X/Y), 100mm (Z), 360° (A)
Junction Dev: 0.01mm - Tight corners for accuracy
Jerk Limit:   5000 mm/sec³ - Smooth S-curve transitions
```

### Benefits of This Architecture

1. ✅ **Per-axis tuning**: Z can be slower/more precise than XY
2. ✅ **GRBL compatibility**: Settings use standard $100-$133 format
3. ✅ **Testability**: Motion math is pure functions (easy unit tests)
4. ✅ **Separation of concerns**: Math library vs real-time control
5. ✅ **Ready for G-code**: Conversion functions already in place
6. ✅ **Look-ahead ready**: Junction velocity helpers for future planner
7. ✅ **Centralized types**: motion_types.h prevents duplicate definitions
8. ✅ **Ring buffer ready**: Motion buffer bridges parser and execution

### Critical Design Principles
Total: ~2KB
```

### Critical Design Principles

⚠️ **Time-based interpolation** - NOT Bresenham step counting  
⚠️ **Hardware pulse generation** - OCR modules, no software step interrupts  
⚠️ **Coordinated motion** - All axes synchronized to dominant axis TIME  
⚠️ **Per-axis limits** - Each axis has independent velocity/accel/jerk  
⚠️ **Centralized settings** - motion_math is single source of truth  
⚠️ **Centralized types** - motion_types.h is single source for all type definitions  
⚠️ **Ring buffer architecture** - Motion buffer bridges parser and execution  

Remember: This is a **safety-critical real-time system**. Always validate motion commands and maintain proper error handling in interrupt contexts.