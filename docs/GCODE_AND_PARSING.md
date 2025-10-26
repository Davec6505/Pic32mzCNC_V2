# G-Code & Parsing - Complete Reference

**Last Updated**: October 26, 2025  
**Status**: Consolidated documentation (replaces 15+ individual files)

---

## Table of Contents

1. [GRBL v1.1f Parser Implementation](#grbl-v11f-parser-implementation)
2. [Serial Buffer Architecture](#serial-buffer-architecture)
3. [Serial Wrapper & Real-Time Commands](#serial-wrapper--real-time-commands)
4. [UGS Interface & Protocol](#ugs-interface--protocol)
5. [Command Buffer Architecture](#command-buffer-architecture)
6. [Input Sanitization & Plan Logging](#input-sanitization--plan-logging)
7. [DMA Serial Reception (Future)](#dma-serial-reception-future)
8. [Coordinate System Implementation](#coordinate-system-implementation)

---

## GRBL v1.1f Parser Implementation

**Source Files**: `srcs/gcode_parser.c` (1354 lines), `incs/gcode_parser.h` (357 lines)

### Parser Architecture

The G-code parser implements **full GRBL v1.1f compliance** with 13 modal groups:

```c
// Modal state structure (~166 bytes)
typedef struct {
    uint8_t motion_mode;           // G0, G1, G2, G3, G38.x
    uint8_t coordinate_system;     // G54-G59
    uint8_t plane_select;          // G17, G18, G19
    uint8_t distance_mode;         // G90 (absolute), G91 (relative)
    uint8_t feedrate_mode;         // G93, G94, G95
    uint8_t units_mode;            // G20 (inches), G21 (mm)
    uint8_t cutter_compensation;   // G40, G41, G42
    uint8_t tool_length_offset;    // G43, G49
    uint8_t program_mode;          // M0, M1, M2, M30
    uint8_t spindle_state;         // M3, M4, M5
    uint8_t coolant_state;         // M7, M8, M9
    float feedrate;                // Current feedrate (mm/min)
    int32_t spindle_speed;         // Current RPM
    // ... work coordinate offsets, tool offsets
} parser_modal_state_t;
```

### 13 Modal Groups

**Group 1: Motion Commands**
- `G0` - Rapid positioning
- `G1` - Linear interpolation
- `G2` - Clockwise arc
- `G3` - Counter-clockwise arc
- `G38.2` - Straight probe
- `G38.3`, `G38.4`, `G38.5` - Probe variants
- `G80` - Cancel motion mode

**Group 2: Plane Selection**
- `G17` - XY plane (default)
- `G18` - XZ plane
- `G19` - YZ plane

**Group 3: Distance Mode**
- `G90` - Absolute positioning (default)
- `G91` - Incremental (relative) positioning

**Group 5: Feed Rate Mode**
- `G93` - Inverse time mode
- `G94` - Units per minute (default)

**Group 6: Units**
- `G20` - Inches
- `G21` - Millimeters (default)

**Group 7: Cutter Radius Compensation**
- `G40` - Cancel (default)
- `G41` - Left
- `G42` - Right

**Group 8: Tool Length Offset**
- `G43` - Enable
- `G49` - Cancel (default)

**Group 10: Return Mode**
- `G98` - Initial point return
- `G99` - R-point return

**Group 12: Work Coordinate System**
- `G54` - Work coordinate 1 (default)
- `G55` - Work coordinate 2
- `G56` - Work coordinate 3
- `G57` - Work coordinate 4
- `G58` - Work coordinate 5
- `G59` - Work coordinate 6

**Group 13: Path Control Mode**
- `G61` - Exact path mode
- `G61.1` - Exact stop mode
- `G64` - Continuous mode (default)

### Non-Modal Commands

**Coordinate System**
- `G28` - Go to predefined position
- `G28.1` - Set predefined position
- `G30` - Go to predefined position (2nd)
- `G30.1` - Set predefined position (2nd)
- `G92` - Set work coordinate offset
- `G92.1` - Clear work coordinate offset
- `G92.2` - Suspend work coordinate offset
- `G92.3` - Restore work coordinate offset

**Program Control**
- `G4` - Dwell (P parameter in seconds)
- `G10` - Programmable data input
- `G53` - Move in machine coordinates

**M-Commands (Program Control)**
- `M0` - Program stop
- `M1` - Optional stop
- `M2` - Program end
- `M30` - Program end and rewind

**M-Commands (Spindle)**
- `M3` - Spindle on CW
- `M4` - Spindle on CCW
- `M5` - Spindle stop

**M-Commands (Coolant)**
- `M7` - Mist coolant on
- `M8` - Flood coolant on
- `M9` - All coolant off

### Parser API

```c
void GCode_Initialize(void);                          // Initialize with modal defaults
bool GCode_BufferLine(char *buffer, size_t size);    // Buffer incoming serial line
bool GCode_ParseLine(const char *line, parsed_move_t *move);  // Parse G-code → move
bool GCode_IsControlChar(char c);                    // Check for ?, !, ~, ^X
const char* GCode_GetLastError(void);                // Get error message
void GCode_ClearError(void);                         // Clear error state
const parser_modal_state_t* GCode_GetModalState(void);  // Get current modal state
```

### Parsing Flow

```
Serial Line → GCode_BufferLine() → Line complete?
                                         ↓ YES
                            GCode_ParseLine(line, &move)
                                         ↓
                            Extract words (G, X, Y, Z, F, S)
                                         ↓
                            Validate modal groups (max 1 per group)
                                         ↓
                            Update modal state
                                         ↓
                            Build parsed_move_t structure
                                         ↓
                            Return to main loop
                                         ↓
                            MotionBuffer_Add(&move)
```

### Error Handling

**Error Codes** (GRBL-compatible):
- `error:1` - Invalid G-code (unrecognized command)
- `error:2` - Bad number format
- `error:3` - Invalid $ statement
- `error:20` - Unsupported command
- `error:21` - Modal group violation
- `error:22` - Undefined feed rate
- `error:23` - Invalid target (exceeds machine limits)
- `error:24` - Arc radius error
- `error:33` - Invalid gcode ID

---

## Serial Buffer Architecture

**Critical Fix (October 18, 2025)**: Buffer increased from 256 to **512 bytes** to prevent overflow during burst G-code streaming.

### Problem Identified

During coordinate system testing, **persistent serial data corruption**:
- Commands fragmented: `"G92.1"` → `"9.1"`, `"G92 X0 Y0"` → `"G2 X0"`
- Pattern: Always losing characters at **start** of commands
- Root cause: 256-byte buffer too small for UGS/PowerShell burst streaming

### Solution Applied

**Comparison with mikroC version** (working):
```c
// mikroC Implementation (500-byte ring buffer)
typedef struct {
    char temp_buffer[500];  // ← Large ring buffer
    int head, tail, diff;
    char has_data: 1;
} Serial;
```

**Harmony/MCC Implementation (UPDATED)**:
```c
// plib_uart2.c - ORIGINAL 256 bytes (TOO SMALL)
#define UART2_READ_BUFFER_SIZE (256U)

// plib_uart2.c - UPDATED October 18, 2025
/* Increased buffer sizes to match mikroC implementation (500 bytes)
 * Previous 256 bytes caused overflow with burst commands from UGS/PowerShell.
 * Larger buffer provides safety margin for G-code streaming.
 */
#define UART2_READ_BUFFER_SIZE      (512U)  // ← DOUBLED from 256
#define UART2_WRITE_BUFFER_SIZE     (512U)  // ← DOUBLED from 256
```

### Why This Works

1. **Burst Absorption**: 512-byte buffer holds ~5 typical G-code commands in flight
2. **PowerShell Timing**: Fast `WriteLine()` calls won't overflow between ISR reads
3. **GRBL Protocol**: "ok" response controls flow, buffer prevents corruption during burst
4. **CPU Speed**: 200MHz PIC32MZ processes data fast, needs buffer for bursty arrivals
5. **Proven Pattern**: mikroC version handled identical hardware with 500 bytes perfectly

### Memory Impact

```
Before: 256 bytes RX + 256 bytes TX = 512 bytes total
After:  512 bytes RX + 512 bytes TX = 1024 bytes total
Change: +512 bytes (0.025% of 2MB RAM - negligible)
```

---

## Serial Wrapper & Real-Time Commands

**Critical Pattern (October 19, 2025)**: **ISR flag-based** real-time command handling (NOT direct execution in ISR).

### Architecture

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
│        Add to ring buffer (512 bytes)                           │
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

### Critical Design Decision

**❌ WRONG APPROACH (Caused Crash)**:
```c
// DON'T call GCode_HandleControlChar() from ISR!
void Serial_RxCallback(uintptr_t context) {
    if (GCode_IsControlChar(data)) {
        GCode_HandleControlChar(data);  // ❌ CRASH! Blocking UART calls in ISR
    }
}
```

**✅ CORRECT APPROACH (Flag-Based)**:
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

### Real-Time Commands

**Control Characters**:
- `?` (0x3F) - Status report request
- `!` (0x21) - Feed hold (pause motion)
- `~` (0x7E) - Cycle start (resume motion)
- `^X` (0x18) - Soft reset (emergency stop)

**Implementation** (gcode_parser.c):
```c
void GCode_HandleControlChar(char c) {
    switch (c) {
        case GCODE_CTRL_STATUS_REPORT:  // '?' (0x3F)
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

### Serial Processing Robustness Fix (October 19, 2025)

**Fixed**: `"error:1 - Invalid G-code: G"` during fast command streaming

**Problem**: Static variables in `ProcessSerialRx()` not properly reset between calls
- `line_pos` reset inside conditional, caused incomplete line processing
- No `line_complete` flag to track state across function calls
- Race condition when serial data arrived in multiple chunks

**Solution**: Proper state tracking with three static variables:
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

---

## UGS Interface & Protocol

**Files**: `srcs/ugs_interface.c`, `incs/ugs_interface.h`

### GRBL v1.1f Protocol Compliance

**Version String**: `"GRBL 1.1f.20251017:PIC32MZ CNC V2"`

### Initialization Sequence

When UGS connects, it sends:
1. `?` - Status report request
2. `$I` - Build info request
3. `$$` - Settings list request
4. `$G` - Parser state request

**Our Responses**:
```
<Idle|MPos:0.000,0.000,0.000|WPos:0.000,0.000,0.000>
[VER:1.1f.20251017:PIC32MZ CNC V2]
[OPT:V,16,512]
$0=10 (Step pulse time, microseconds)
$1=255 (Step idle delay, milliseconds)
...
$133=100.000 (A-axis maximum travel, degrees)
[GC:G0 G54 G17 G21 G90 G94 M5 M9 T0 F0 S0]
```

### API Functions

```c
void UGS_Initialize(void);                           // Initialize UART2 @ 115200 baud
void UGS_SendOK(void);                               // Send "ok\r\n" for flow control
void UGS_SendError(uint8_t code, const char *msg);   // Send "error:X (message)\r\n"
void UGS_Print(const char *str);                     // Send arbitrary string
void UGS_Printf(const char *fmt, ...);               // Formatted output
bool UGS_RxHasData(void);                            // Check if data available
void UGS_SendStatusReport(...);                      // Send <Idle|MPos:...|WPos:...>
void UGS_SendBuildInfo(void);                        // Send [VER:...] and [OPT:...]
void UGS_SendSettings(void);                         // Send all $xxx settings
void UGS_SendParserState(void);                      // Send [GC:G0 G54...]
```

### Flow Control Pattern

**Character-Counting Protocol**:
```c
// In main loop after parsing G-code:
if (GCode_ParseLine(line, &parsed_move)) {
    if (MotionBuffer_Add(&parsed_move)) {
        UGS_SendOK();  // ✅ Buffer accepted move - UGS can send next
    } else {
        // ❌ Buffer full - DON'T send "ok"
        // UGS will wait, retry on next loop iteration
    }
}
```

**Real-Time Commands** (bypass flow control):
- Processed immediately without waiting for "ok"
- Don't increment line counter
- Can be sent at any time (even during motion)

### Status Report Format

```
<State|MPos:x,y,z|WPos:x,y,z|Bf:blocks,bytes|FS:feed,spindle>
```

**Example**:
```
<Idle|MPos:0.000,0.000,5.003|WPos:0.000,0.000,5.003|Bf:15,128|FS:0,0>
```

**Fields**:
- `State`: Idle, Run, Hold, Door, Home, Alarm, Check
- `MPos`: Machine position (absolute from power-on)
- `WPos`: Work position (relative to work coordinate system)
- `Bf`: Buffer state (blocks available, bytes used)
- `FS`: Feed rate (mm/min), Spindle speed (RPM)

---

## Command Buffer Architecture

**Files**: `srcs/command_buffer.c` (279 lines), `incs/command_buffer.h` (183 lines)

### Purpose

Separates **individual G-code commands** from continuous serial stream before parsing.

### Architecture

```
Serial Stream: "G90\nG1 X10\nG1 Y20\n"
        ↓
Command Buffer (circular queue, 8 commands × 256 bytes each)
        ↓
Individual Commands: "G90", "G1 X10", "G1 Y20"
        ↓
G-code Parser
```

### Buffer Design

```c
#define CMD_BUFFER_SIZE 8         // Must be power of 2
#define MAX_COMMAND_LENGTH 256    // Max chars per command

typedef struct {
    char commands[CMD_BUFFER_SIZE][MAX_COMMAND_LENGTH];
    volatile uint8_t head;        // Next write index
    volatile uint8_t tail;        // Next read index
    volatile bool overflow;       // Buffer full flag
} command_buffer_t;
```

### API

```c
void CommandBuffer_Initialize(void);
bool CommandBuffer_AddLine(const char *line);     // Add complete line
bool CommandBuffer_GetNext(char *buffer, size_t size);  // Retrieve next command
bool CommandBuffer_IsEmpty(void);
bool CommandBuffer_IsFull(void);
uint8_t CommandBuffer_GetCount(void);
void CommandBuffer_Clear(void);
```

### Usage Pattern

```c
// In serial RX processing:
if (line_complete) {
    if (CommandBuffer_AddLine(line_buffer)) {
        // ✅ Command buffered
    } else {
        // ❌ Buffer full - wait for main loop to drain
    }
}

// In main loop:
char command[256];
if (CommandBuffer_GetNext(command, sizeof(command))) {
    parsed_move_t move;
    if (GCode_ParseLine(command, &move)) {
        MotionBuffer_Add(&move);
    }
}
```

---

## Input Sanitization & Plan Logging

**Added**: October 23, 2025

### Input Filter

To prevent rare stray extended bytes from influencing parsing, the main loop now sanitizes input bytes before line buffering.

**Pattern in `main.c`**:
```c
// Drop extended/non-printable controls (keep CR/LF/TAB/space)
unsigned char uc = (unsigned char)c;
if ((uc >= 0x80U) || ((uc < 0x20U) && (c != '\n') && (c != '\r') && (c != '\t') && (c != ' ')))
{
    continue; // ignore this byte
}
```

**What's Filtered**:
- Bytes >= 0x80 (extended ASCII, UTF-8 continuation bytes)
- Control characters < 0x20 EXCEPT:
  - `\n` (0x0A) - Line feed
  - `\r` (0x0D) - Carriage return
  - `\t` (0x09) - Tab
  - ` ` (0x20) - Space

**Benefits**:
- Eliminates "walkabout" triggered by random high-bit bytes mid-stream
- Prevents parser confusion from unexpected control characters
- Non-invasive: Motion execution unaffected

### Plan Logging (Debug Only)

Optional plan logging (enabled with `DEBUG_MOTION_BUFFER`) prints each planned target just before queuing to GRBL planner.

```c
#ifdef DEBUG_MOTION_BUFFER
UGS_Printf("PLAN: G%d X%.3f Y%.3f Z%.3f F%.1f\r\n",
           (int)move.motion_mode,
           target_mm[AXIS_X], target_mm[AXIS_Y], target_mm[AXIS_Z],
           pl_data.feed_rate);
#endif
```

**Output Example**:
```
PLAN: G1 X10.000 Y20.000 Z5.000 F1500.0
PLAN: G1 X20.000 Y30.000 Z5.000 F1500.0
```

**Benefits**:
- Provides clear breadcrumbs if unexpected motion observed
- One-line trace per buffered move (low overhead)
- Off by default (enable with `make all DEBUG_MOTION_BUFFER=1`)

---

## DMA Serial Reception (Future)

**Status**: **NOT IMPLEMENTED** - Reference from mikroC version

### mikroC Pattern (For Future Reference)

**DMA0 Configuration**:
```c
// Serial_Dma.h - 500-byte ring buffer
typedef struct {
    char temp_buffer[500];  // Large ring buffer
    int head, tail, diff;
    char has_data: 1;
} Serial;

// DMA0 ISR copies from rxBuf[200] → temp_buffer[500]
// Pattern matching: DMA triggers on '\n' or '?' character
```

**Benefits** (when implemented):
- Reduces CPU overhead (DMA handles byte-by-byte copying)
- Automatic pattern matching (trigger on newline)
- Larger buffer for burst absorption
- Frees CPU for motion control

**Current Alternative**:
- UART RX interrupt with 512-byte ring buffer (sufficient for now)
- Can migrate to DMA if CPU load becomes issue

---

## Coordinate System Implementation

**Files**: `srcs/gcode_parser.c` (G92, G54-G59 support)

### Work Coordinate Systems

**Supported Systems**:
- **G54-G59**: 6 work coordinate systems (default: G54)
- **G92**: Temporary work coordinate offset
- **G53**: Move in machine coordinates (bypasses offsets)

### Coordinate Offset Storage

```c
// In parser_modal_state_t:
typedef struct {
    float work_offset[NUM_AXES];       // G92 offset
    float coord_system_offset[6][NUM_AXES];  // G54-G59 offsets
    uint8_t coordinate_system;         // Active system (0-5 for G54-G59)
} parser_modal_state_t;
```

### Position Calculation

```
Machine Position = Work Position + Work Offset + Coordinate System Offset

Example:
- Work position (G-code): X10 Y20
- G54 offset: X5 Y0
- G92 offset: X0 Y0
- Machine position: X15 Y20
```

### G92 Behavior

**G92 Commands**:
- `G92 X0 Y0` - Set work offset such that current position becomes (0, 0)
- `G92.1` - Clear work offset (reset to zero)
- `G92.2` - Suspend work offset (temporarily disable)
- `G92.3` - Restore work offset (re-enable after suspend)

**Implementation**:
```c
case GCODE_NON_MODAL_SET_COORDINATE_OFFSET:  // G92
    // Calculate offset: offset = machine_pos - target_work_pos
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++) {
        if (move->axis_words[axis]) {
            int32_t current_steps = MultiAxis_GetStepCount(axis);
            float current_mm = MotionMath_StepsToMM(current_steps, axis);
            modal_state.work_offset[axis] = current_mm - move->target[axis];
        }
    }
    break;
```

### Coordinate System Offsets (G54-G59)

**Usage**:
```gcode
G10 L2 P1 X10 Y20 Z5   ; Set G54 offset to (10, 20, 5)
G10 L2 P2 X0 Y0 Z0     ; Set G55 offset to (0, 0, 0)
G54                    ; Activate G54 coordinate system
G1 X0 Y0               ; Move to (0, 0) in G54 = (10, 20) in machine coords
G55                    ; Switch to G55
G1 X0 Y0               ; Move to (0, 0) in G55 = (0, 0) in machine coords
```

**Not Yet Implemented**: G10 command for setting coordinate system offsets (placeholder in parser)

---

## Testing & Validation

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
✅ Serial robustness: No more "error:1 - Invalid G-code: G" errors
✅ Single-axis motion: G1 Y10 moves Y-only (no diagonal drift)
```

### PowerShell Testing Scripts

**Basic Motion Test**:
```powershell
.\motion_test.ps1 -Port COM4 -BaudRate 115200
```

**UGS Compatibility Test**:
```powershell
.\ugs_test.ps1 -Port COM4 -GCodeFile modular_test.gcode
```

**Real-Time Debugging**:
```powershell
.\monitor_debug.ps1 -Port COM4
```

---

## Known Issues & Limitations

### Current Limitations

1. **G10 Command**: Not implemented (coordinate system offset setting)
2. **G38.x Probing**: State tracking only, no hardware integration
3. **Tool Change**: M6 parsed but not executed
4. **Spindle PWM**: M3/M4 state tracked, GPIO output pending
5. **Coolant GPIO**: M7/M8/M9 state tracked, GPIO output pending

### Future Enhancements

1. **Arc Support Completion**: R-format, full circles, G18/G19 planes
2. **Probing Hardware**: G38.x probe input integration
3. **DMA Serial Reception**: Reduce CPU overhead
4. **Error Recovery**: Resume from error conditions
5. **EEPROM Settings**: Persistent storage of $xxx settings

---

## File Cross-References

**Parser Implementation**:
- `srcs/gcode_parser.c` - Main parser (1354 lines)
- `incs/gcode_parser.h` - API and types (357 lines)

**Serial Communication**:
- `srcs/ugs_interface.c` - UGS protocol
- `incs/ugs_interface.h` - API
- `config/default/peripheral/uart/plib_uart2.c` - Hardware UART (512-byte buffers)

**Command Buffering**:
- `srcs/command_buffer.c` - Circular queue (279 lines)
- `incs/command_buffer.h` - API (183 lines)

**Main Loop Integration**:
- `srcs/main.c` - Serial RX processing, command execution

---

**End of G-Code & Parsing Reference**
