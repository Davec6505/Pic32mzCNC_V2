# Universal G-code Sender (UGS) Integration Guide# GRBL Serial Communication - Integration Guide



## Overview## Overview



The UGS interface module (`ugs_interface.c/h`) provides printf-style communication between Universal G-code Sender and the motion control system via UART2 ring buffers.The GRBL serial module (`grbl_serial.c/h`) provides printf-style communication for interfacing with Universal G-code Sender (UGS) using the UART2 ring buffer.



**Status**: ✅ **IMPLEMENTED** (October 16, 2025)**Status**: ✅ **IMPLEMENTED** (October 16, 2025)

- Complete header and implementation files with UGS naming- Complete header and implementation files

- Compiled and linked successfully (`ugs_interface.o`)- Compiled and linked successfully

- Ready for G-code parser integration- Ready for integration into main application loop



## Architecture## Quick Start



### Complete Data Flow### 1. Initialize Serial Communication



``````c

┌─────────────────────────────────────────────────────────────────────┐#include "grbl_serial.h"

│ Universal G-code Sender (PC)                                        │

│ - Sends G-code commands (G1 X10 Y20 F1500)                         │int main(void) {

│ - Sends real-time commands (?, !, ~, Ctrl-X)                       │    SYS_Initialize(NULL);  // Harmony3 peripherals

│ - Waits for "ok" before sending next command (flow control)        │    

└────────────────────────┬────────────────────────────────────────────┘    /* Initialize GRBL serial (wraps UART2_Initialize) */

                         │ UART2 @ 115200 baud    GRBL_Serial_Initialize();

                         ↓    

┌─────────────────────────────────────────────────────────────────────┐    /* Send welcome message to UGS */

│ UART2 Ring Buffers (plib_uart2.c - MCC Generated)                  │    GRBL_Serial_SendWelcome();  // "Grbl 1.1f ['$' for help]\r\n"

│                                                                     │    

│ RX Ring Buffer: UART2_ReadBuffer[256]                              │    APP_Initialize();

│   - ISR-driven: UART RX interrupt → Copies byte to ring buffer    │    

│   - Indices: rdInIndex (write), rdOutIndex (read)                  │    while (true) {

│   - Thread-safe: volatile uint32_t indices                          │        /* Process incoming G-code commands */

│                                                                     │        GRBL_ProcessSerialCommands();

│ TX Ring Buffer: UART2_WriteBuffer[256]                             │        

│   - UART2_Write() → Copies data to ring buffer                     │        /* Execute motion from buffer */

│   - ISR-driven: UART TX interrupt → Transmits from ring buffer    │        GRBL_ExecuteMotion();

│   - Indices: wrInIndex (write), wrOutIndex (read)                  │        

│   - Thread-safe: volatile uint32_t indices                          │        APP_Tasks();

└────────────────────────┬────────────────────────────────────────────┘        SYS_Tasks();

                         │    }

                         ↓}

┌─────────────────────────────────────────────────────────────────────┐```

│ UGS Interface (ugs_interface.c - This Module)                      │

│                                                                     │### 2. Send Status Reports (Real-Time Command '?')

│ RX Functions:                                                       │

│   UGS_ReadLine() → Reads until \\n → Returns complete line          │```c

│   UGS_RxHasData() → Checks if data available                       │void GRBL_SendStatusReport(void) {

│                                                                     │    /* Get current machine position (steps from multiaxis_control) */

│ TX Functions:                                                       │    float mpos_x = (float)MultiAxis_GetStepCount(AXIS_X) / 80.0f;  // 80 steps/mm

│   UGS_Printf() → vsnprintf() → tx_format_buffer[256]               │    float mpos_y = (float)MultiAxis_GetStepCount(AXIS_Y) / 80.0f;

│                → UART2_Write() → TX ring buffer                    │    float mpos_z = (float)MultiAxis_GetStepCount(AXIS_Z) / 1280.0f; // 1280 steps/mm

│   UGS_SendOK() → Sends "ok\\r\\n" for flow control                   │    

│   UGS_SendStatusReport() → Formats machine state                   │    /* Get work position (machine position - work offset) */

└────────────────────────┬────────────────────────────────────────────┘    float wpos_x = mpos_x - work_offset.x;

                         │    float wpos_y = mpos_y - work_offset.y;

                         ↓    float wpos_z = mpos_z - work_offset.z;

┌─────────────────────────────────────────────────────────────────────┐    

│ Main Application Loop (To Be Implemented)                          │    /* Determine state */

│                                                                     │    const char* state;

│ 1. Read G-code: UGS_ReadLine() → line buffer                       │    if (MultiAxis_IsBusy()) {

│ 2. Parse G-code: GCode_ParseLine() → parsed_move_t                 │        state = "Run";

│ 3. Queue motion: MotionBuffer_Add(&move)                           │    } else if (alarm_active) {

│ 4. Send response: UGS_SendOK() if accepted                         │        state = "Alarm";

└────────────────────────┬────────────────────────────────────────────┘    } else {

                         │        state = "Idle";

                         ↓    }

┌─────────────────────────────────────────────────────────────────────┐    

│ Motion Buffer (motion_buffer.c - Ring Buffer with Look-Ahead)      │    /* Send formatted status report */

│                                                                     │    GRBL_Serial_SendStatusReport(

│ Ring Buffer: motion_block_t motion_buffer[16]                      │        state,

│   - Indices: wrInIndex (write), rdOutIndex (read)                  │        mpos_x, mpos_y, mpos_z,

│   - Pattern: Same as UART2 ring buffers                            │        wpos_x, wpos_y, wpos_z

│   - MotionBuffer_Add() → Convert mm to steps, queue block          │    );

│   - MotionBuffer_GetNext() → Dequeue planned block                 │    /* Output: "<Idle|MPos:10.000,20.000,5.000|WPos:10.000,20.000,5.000>\r\n" */

│   - MotionBuffer_HasData() → Check if blocks pending               │}

└────────────────────────┬────────────────────────────────────────────┘```

                         │

                         ↓### 3. Process Incoming Commands

┌─────────────────────────────────────────────────────────────────────┐

│ Multi-Axis Control (multiaxis_control.c - S-Curve Motion)          │```c

│                                                                     │void GRBL_ProcessSerialCommands(void) {

│ TMR1 @ 1kHz → Updates S-curve velocity profiles                    │    static char rx_line_buffer[128];

│ MultiAxis_ExecuteCoordinatedMove() → Time-synchronized motion      │    

│ Per-axis OCR hardware → Generates step pulses                      │    /* Check if complete line available */

└─────────────────────────────────────────────────────────────────────┘    if (GRBL_Serial_ReadLine(rx_line_buffer, sizeof(rx_line_buffer)) > 0) {

```        

        /* Check for system commands ($ commands) */

## UART2_Write Interface Details        if (rx_line_buffer[0] == '$') {

            if (rx_line_buffer[1] == '$') {

### How UART2_Write Works                /* $$ - View all settings */

                GRBL_SendAllSettings();

From `plib_uart2.c` (MCC-generated):                GRBL_Serial_SendOK();

            } else if (rx_line_buffer[1] == 'H') {

```c                /* $H - Run homing cycle */

// TX Ring Buffer (256 bytes)                APP_RunHomingCycle();

#define UART2_WRITE_BUFFER_SIZE (256U)                GRBL_Serial_SendOK();

volatile static uint8_t UART2_WriteBuffer[UART2_WRITE_BUFFER_SIZE];            } else if (rx_line_buffer[1] == 'X') {

                /* $X - Clear alarm state */

// Ring buffer indices (thread-safe with volatile)                APP_ClearAlarmState();

typedef struct {                GRBL_Serial_SendOK();

    volatile uint32_t wrInIndex;   // Write pointer (ISR updates)            }

    volatile uint32_t wrOutIndex;  // Read pointer (ISR updates)            return;

    // ... other fields        }

} UART_RING_BUFFER_OBJECT;        

        /* Parse G-code line */

size_t UART2_Write(uint8_t* pWrBuffer, const size_t size) {        parsed_move_t move;

    size_t nBytesWritten = 0;        if (GCode_ParseLine(rx_line_buffer, &move)) {

                

    // Copy bytes to ring buffer (while space available)            /* Try to add to motion buffer */

    while (nBytesWritten < size) {            if (MotionBuffer_Add(&move)) {

        if (UART2_TxPushByte(pWrBuffer[nBytesWritten])) {                /* Success - send "ok" (flow control) */

            nBytesWritten++;                GRBL_Serial_SendOK();

        } else {            } else {

            break;  // Buffer full                /* Buffer full - DON'T send "ok" yet */

        }                /* UGS will wait for "ok" before sending next command */

    }            }

            } else {

    // Enable TX interrupt (ISR will transmit from ring buffer)            /* Parse error */

    if (UART2_WritePendingBytesGet() > 0U) {            GRBL_Serial_SendError(1, "Invalid G-code");

        UART2_TX_INT_ENABLE();        }

    }    }

    }

    return nBytesWritten;  // How many bytes queued```

}

```### 4. Execute Motion from Buffer



### Key Properties```c

void GRBL_ExecuteMotion(void) {

1. **Non-Blocking**: Returns immediately (copies to buffer only)    /* Check if motion controller is ready */

2. **ISR-Driven**: UART TX interrupt transmits bytes automatically    if (!MultiAxis_IsBusy()) {

3. **Ring Buffer Pattern**: Same wrInIndex/wrOutIndex as motion_buffer        

4. **Thread-Safe**: Volatile indices prevent optimization issues        /* Try to get next motion block */

5. **Partial Writes**: Returns bytes queued (may be less than requested)        motion_block_t block;

        if (MotionBuffer_GetNext(&block)) {

## Quick Start            

            /* Execute coordinated move */

### 1. Initialize UGS Interface            MultiAxis_ExecuteCoordinatedMove(block.steps);

            

```c            /* If buffer was full, now there's space */

#include "ugs_interface.h"            /* Next loop iteration will send "ok" for queued command */

#include "motion_buffer.h"        }

#include "multiaxis_control.h"    }

}

int main(void) {```

    SYS_Initialize(NULL);  // Harmony3 peripherals

    ### 5. Send Settings Display ($$ Command)

    /* Initialize UGS serial interface */

    UGS_Initialize();  // Wraps UART2_Initialize()```c

    void GRBL_SendAllSettings(void) {

    /* Initialize motion subsystems */    /* Get settings from motion_math */

    MultiAxis_Initialize();    

    MotionBuffer_Initialize();    /* Steps per mm ($100-$103) */

        for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++) {

    /* Send welcome message to UGS */        float steps_per_mm = MotionMath_GetStepsPerMM(axis);

    UGS_SendWelcome();  // "Grbl 1.1f ['$' for help]\\r\\n"        GRBL_Serial_SendSetting(100 + (uint8_t)axis, steps_per_mm);

        }

    APP_Initialize();    

        /* Max rates ($110-$113) */

    while (true) {    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++) {

        /* Process incoming G-code from UGS */        float max_rate = MotionMath_GetMaxRate(axis);

        UGS_ProcessCommands();        GRBL_Serial_SendSetting(110 + (uint8_t)axis, max_rate);

            }

        /* Execute motion from buffer */    

        UGS_ExecuteMotion();    /* Acceleration ($120-$123) */

            for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++) {

        APP_Tasks();        float accel = MotionMath_GetAcceleration(axis);

        SYS_Tasks();        GRBL_Serial_SendSetting(120 + (uint8_t)axis, accel);

    }    }

}    

```    /* Max travel ($130-$133) */

    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++) {

### 2. Process G-code Commands (Using motion_buffer)        float max_travel = MotionMath_GetMaxTravel(axis);

        GRBL_Serial_SendSetting(130 + (uint8_t)axis, max_travel);

```c    }

void UGS_ProcessCommands(void) {    

    static char rx_line_buffer[128];    /* Junction deviation ($11) */

    parsed_move_t move;    float junction_dev = MotionMath_GetJunctionDeviation();

        GRBL_Serial_SendSetting(11, junction_dev);

    /* Read complete line from UART2 RX ring buffer */    

    if (UGS_ReadLine(rx_line_buffer, sizeof(rx_line_buffer)) > 0) {    /* Output:

             * $100=80.000\r\n

        /* Handle system commands ($ commands) */     * $101=80.000\r\n

        if (rx_line_buffer[0] == '$') {     * $102=1280.000\r\n

            UGS_HandleSystemCommand(rx_line_buffer);     * ...

            return;     */

        }}

        ```

        /* Parse G-code line → parsed_move_t */

        if (GCode_ParseLine(rx_line_buffer, &move)) {### 6. Handle Real-Time Commands (Interrupt Priority)

            

            /* Add to motion_buffer (converts mm to steps) */```c

            if (MotionBuffer_Add(&move)) {/* Real-time commands are processed BEFORE parsing G-code lines */

                /* ✅ Success - send "ok" (UGS flow control) */void GRBL_ProcessRealTimeCommands(void) {

                UGS_SendOK();    uint8_t byte;

            }    

            /* ❌ Buffer full - DON'T send "ok" */    /* Check for single-character commands */

            /* UGS will wait, we'll retry next loop */    while (UART2_Read(&byte, 1) > 0) {

                    

        } else {        switch (byte) {

            /* Parse error */            case GRBL_CMD_STATUS_REPORT:  /* '?' (0x3F) */

            UGS_SendError(1, "Invalid G-code");                GRBL_SendStatusReport();

        }                break;

    }                

}            case GRBL_CMD_FEED_HOLD:  /* '!' (0x21) */

```                MotionBuffer_Pause();  /* Stop retrieving blocks */

                MultiAxis_EmergencyStop();  /* Stop current move */

### 3. Execute Motion from motion_buffer                break;

                

```c            case GRBL_CMD_CYCLE_START:  /* '~' (0x7E) */

void UGS_ExecuteMotion(void) {                MotionBuffer_Resume();  /* Resume motion */

    motion_block_t block;                break;

                    

    /* Check if motion controller ready and buffer has data */            case GRBL_CMD_SOFT_RESET:  /* Ctrl-X (0x18) */

    if (!MultiAxis_IsBusy() && MotionBuffer_HasData()) {                MotionBuffer_Clear();  /* Clear all queued moves */

                        MultiAxis_EmergencyStop();

        /* Get next planned block from ring buffer */                APP_SoftReset();

        if (MotionBuffer_GetNext(&block)) {                break;

                            

            /* Execute time-synchronized coordinated move */            case '\r':

            MultiAxis_ExecuteCoordinatedMove(block.steps);            case '\n':

                            /* Line terminator - buffer complete line for parsing */

            /* Motion started - status reports will show "Run" */                /* (handled by GRBL_Serial_ReadLine) */

        }                break;

    }                

}            default:

```                /* Regular character - buffer for G-code parsing */

                /* (handled by GRBL_Serial_ReadLine) */

### 4. Send Status Reports (Real-Time Command '?')                break;

        }

```c    }

void UGS_ProcessRealTimeCommands(void) {}

    uint8_t byte;```

    

    /* Check for single-character commands */## Complete Main Loop Integration

    while (UART2_Read(&byte, 1) > 0) {

        ```c

        switch (byte) {int main(void) {

            case UGS_CMD_STATUS_REPORT:  /* '?' */    SYS_Initialize(NULL);

                UGS_SendStatusReport(    GRBL_Serial_Initialize();

                    MultiAxis_IsBusy() ? "Run" : "Idle",    GRBL_Serial_SendWelcome();

                    mpos_x, mpos_y, mpos_z,    

                    wpos_x, wpos_y, wpos_z    /* Initialize motion subsystems */

                );    APP_Initialize();

                break;    MultiAxis_Initialize();

                    MotionBuffer_Initialize();

            case UGS_CMD_FEED_HOLD:  /* '!' */    

                MotionBuffer_Pause();  // Stop retrieving blocks    while (true) {

                MultiAxis_EmergencyStop();        /* Priority 1: Real-time commands (highest priority) */

                break;        GRBL_ProcessRealTimeCommands();

                        

            case UGS_CMD_CYCLE_START:  /* '~' */        /* Priority 2: Process G-code lines (when buffer has space) */

                MotionBuffer_Resume();  // Resume motion        GRBL_ProcessSerialCommands();

                break;        

                        /* Priority 3: Execute motion (when controller ready) */

            case UGS_CMD_SOFT_RESET:  /* Ctrl-X */        GRBL_ExecuteMotion();

                MotionBuffer_Clear();        

                MultiAxis_EmergencyStop();        /* Priority 4: Application tasks (buttons, LEDs) */

                NVIC_SystemReset();        APP_Tasks();

                break;        

        }        /* Priority 5: System tasks (lowest priority) */

    }        SYS_Tasks();

}    }

```}

```

### 5. Display Settings ($$  Command)

## GRBL Protocol Flow Control

```c

void UGS_HandleSystemCommand(const char* cmd) {The GRBL protocol uses **synchronous flow control** via the "ok" message:

    if (strcmp(cmd, "$$") == 0) {

        /* Send all GRBL settings from motion_math */### Flow Control Pattern

        for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++) {

            float steps_per_mm = MotionMath_GetStepsPerMM(axis);```

            UGS_SendSetting(100 + (uint8_t)axis, steps_per_mm);UGS: G1 X10 Y20 F1500\n         → Send G-code command

            CNC: [parse, add to buffer]     → Process command

            float max_rate = MotionMath_GetMaxRate(axis);CNC: ok\n                        → Confirm command accepted

            UGS_SendSetting(110 + (uint8_t)axis, max_rate);UGS: [wait for ok]               → Sender waits before next command

            UGS: G1 X15 Y25 F1500\n         → Send next command after receiving ok

            float accel = MotionMath_GetAcceleration(axis);```

            UGS_SendSetting(120 + (uint8_t)axis, accel);

            ### Critical Rule

            float max_travel = MotionMath_GetMaxTravel(axis);

            UGS_SendSetting(130 + (uint8_t)axis, max_travel);**ONLY send "ok" when:**

        }1. Command parsed successfully AND

        UGS_SendOK();2. Motion buffer accepted the move (not full)

        

    } else if (strcmp(cmd, "$H") == 0) {**DO NOT send "ok" if:**

        /* Run homing cycle */- Buffer is full (wait until space available)

        APP_RunHomingCycle();- Parse error occurred (send error instead)

        UGS_SendOK();- Alarm state active (send alarm instead)

        

    } else if (strcmp(cmd, "$X") == 0) {### Example Implementation

        /* Clear alarm state */

        APP_ClearAlarmState();```c

        UGS_SendOK();void GRBL_ProcessSerialCommands(void) {

    }    static char rx_line_buffer[128];

}    static char pending_line[128];

```    static bool have_pending = false;

    

## Integration with motion_buffer    /* Try to process pending command first */

    if (have_pending) {

### Flow Control Pattern        parsed_move_t move;

        if (GCode_ParseLine(pending_line, &move)) {

The motion_buffer uses the **same ring buffer pattern** as UART2:            if (MotionBuffer_Add(&move)) {

                /* Success! */

```c                GRBL_Serial_SendOK();

// motion_buffer.c                have_pending = false;

static motion_block_t motion_buffer[MOTION_BUFFER_SIZE];  // 16 blocks            }

static volatile uint32_t wrInIndex = 0U;   // Write pointer            /* else: still full, try again next loop */

static volatile uint32_t rdOutIndex = 0U;  // Read pointer        }

    }

bool MotionBuffer_Add(const parsed_move_t* move) {    

    // Check if buffer full (same logic as UART2)    /* Read new command if no pending */

    uint32_t nextWr = (wrInIndex + 1U) % MOTION_BUFFER_SIZE;    if (!have_pending && GRBL_Serial_ReadLine(rx_line_buffer, sizeof(rx_line_buffer)) > 0) {

    if (nextWr == rdOutIndex) {        

        return false;  // Buffer full        parsed_move_t move;

    }        if (GCode_ParseLine(rx_line_buffer, &move)) {

                if (MotionBuffer_Add(&move)) {

    // Convert mm to steps using motion_math                /* Success! */

    motion_block_t* block = &motion_buffer[wrInIndex];                GRBL_Serial_SendOK();

    block->steps[AXIS_X] = MotionMath_MMToSteps(move->target[AXIS_X], AXIS_X);            } else {

    block->steps[AXIS_Y] = MotionMath_MMToSteps(move->target[AXIS_Y], AXIS_Y);                /* Buffer full - save for retry */

    // ...                strncpy(pending_line, rx_line_buffer, sizeof(pending_line));

                    have_pending = true;

    // Advance write pointer            }

    wrInIndex = nextWr;        } else {

    return true;            /* Parse error */

}            GRBL_Serial_SendError(1, "Invalid G-code");

        }

bool MotionBuffer_GetNext(motion_block_t* block) {    }

    // Check if buffer empty (same logic as UART2)}

    if (wrInIndex == rdOutIndex) {```

        return false;  // Empty

    }## Performance Characteristics

    

    // Copy block### UART2 Ring Buffer

    *block = motion_buffer[rdOutIndex];- **TX Buffer**: 256 bytes

    - **RX Buffer**: 256 bytes

    // Advance read pointer- **Baud Rate**: 115200 (typically)

    rdOutIndex = (rdOutIndex + 1U) % MOTION_BUFFER_SIZE;- **Throughput**: ~11.5 KB/sec theoretical max

    return true;

}### Printf Performance

- **Format Buffer**: 256 bytes (static, no malloc)

bool MotionBuffer_HasData(void) {- **vsnprintf**: Standard C library (optimized)

    uint32_t wrIdx = wrInIndex;   // Snapshot (thread-safe)- **UART2_Write**: Interrupt-driven, non-blocking

    uint32_t rdIdx = rdOutIndex;- **Typical Message**: 50-100 bytes (status report)

    return (wrIdx != rdIdx);

}### Thread Safety

```- ✅ **Ring buffer indices**: Volatile uint32_t

- ✅ **UART ISR-driven**: TX/RX handled in interrupt context

### Critical Flow Control Rule- ✅ **Printf buffer**: Static (file scope), not reentrant

- ⚠️ **Not ISR-safe**: Don't call printf from interrupts

**ONLY send "ok" when motion_buffer accepts the move:**

## Testing with UGS

```c

// ✅ CORRECT - UGS flow control### 1. Configure UGS Serial Settings

if (MotionBuffer_Add(&move)) {```

    UGS_SendOK();  // Buffer accepted - UGS sends next commandPort: COM4 (or your serial port)

}Baud: 115200

// If false: DON'T send "ok" - UGS waits - retry next loop```



// ❌ INCORRECT - Breaks flow control### 2. Expected Output on Connect

if (GCode_ParseLine(line, &move)) {```

    MotionBuffer_Add(&move);  // Might fail!Grbl 1.1f ['$' for help]

    UGS_SendOK();  // ❌ Sent even if buffer full!```

}

```### 3. Test Commands

```

## Complete Main Loop Example?                  → <Idle|MPos:0.000,0.000,0.000|WPos:0.000,0.000,0.000>

$$                 → $100=80.000 ... (all settings)

```c$H                 → [run homing] ok

int main(void) {G1 X10 Y20 F1500   → ok

    SYS_Initialize(NULL);?                  → <Run|MPos:5.123,10.246,0.000|WPos:5.123,10.246,0.000>

    UGS_Initialize();!                  → [feed hold - motion stops]

    MultiAxis_Initialize();~                  → [cycle start - motion resumes]

    MotionBuffer_Initialize();```

    UGS_SendWelcome();

    APP_Initialize();## Next Steps

    

    while (true) {1. **Implement G-code Parser** (`gcode_parser.c/h`)

        /* Priority 1: Real-time commands (highest) */   - Parse G0/G1/G2/G3 commands

        UGS_ProcessRealTimeCommands();   - Extract X/Y/Z/F parameters

           - Generate `parsed_move_t` structures

        /* Priority 2: G-code lines (when buffer has space) */

        UGS_ProcessCommands();2. **Integrate with Motion Buffer**

           - Call `MotionBuffer_Add()` for each parsed move

        /* Priority 3: Execute motion (when controller ready) */   - Implement flow control (only send "ok" when buffer accepts)

        UGS_ExecuteMotion();

        3. **Add Real-Time Command Handler**

        /* Priority 4: Application tasks */   - Process '?', '!', '~', Ctrl-X in main loop

        APP_Tasks();   - Priority over G-code parsing

        

        /* Priority 5: System tasks */4. **Test with UGS**

        SYS_Tasks();   - Verify status reports update correctly

    }   - Test feed hold/cycle start

}   - Verify flow control (sender waits for "ok")

```

## References

## Testing with Universal G-code Sender

- **GRBL v1.1f Protocol**: https://github.com/gnea/grbl/wiki/Grbl-v1.1-Interface

### 1. Configure UGS- **UART Ring Buffer**: `incs/config/default/peripheral/uart/plib_uart2.h`

- **Motion Buffer**: `srcs/motion/motion_buffer.c`

```- **Motion Math**: `srcs/motion/motion_math.c`

Port: COM4 (or your serial port)
Baud: 115200
Firmware: GRBL
```

### 2. Expected Output on Connect

```
Grbl 1.1f ['$' for help]
```

### 3. Test Commands

```
$$                      → $100=80.000 $101=80.000 ... (all settings)
?                       → <Idle|MPos:0.000,0.000,0.000|WPos:0.000,0.000,0.000>
G1 X10 Y20 F1500        → ok
?                       → <Run|MPos:5.123,10.246,0.000|WPos:5.123,10.246,0.000>
!                       → [feed hold - motion pauses]
~                       → [cycle start - motion resumes]
$H                      → [run homing cycle] ok
$X                      → [clear alarm] ok
```

## Performance Characteristics

### UART2 Ring Buffers

- **TX Buffer**: 256 bytes (non-blocking)
- **RX Buffer**: 256 bytes (ISR-driven)
- **Baud Rate**: 115200 (typical)
- **Throughput**: ~11.5 KB/sec theoretical max

### motion_buffer Ring Buffer

- **Capacity**: 16 motion blocks (configurable)
- **Block Size**: ~120 bytes (motion_block_t)
- **Pattern**: Same wrInIndex/rdOutIndex as UART2
- **Look-Ahead**: Ready for velocity optimization

### Printf Performance

- **Format Buffer**: 256 bytes (static, no malloc)
- **vsnprintf**: Standard C library (optimized by XC32)
- **UART2_Write**: Copies to ring buffer (non-blocking)
- **ISR**: Transmits automatically (interrupt-driven)

## Next Steps

1. **✅ DONE**: UGS interface module (renamed from GRBL)
2. **✅ DONE**: motion_buffer ring buffer (matches UART2 pattern)
3. **TODO**: G-code parser (`gcode_parser.c/h`)
4. **TODO**: Main loop integration (UGS_ProcessCommands, UGS_ExecuteMotion)
5. **TODO**: Real-time command handler
6. **TODO**: Test with UGS and verify flow control

## References

- **GRBL v1.1f Protocol**: https://github.com/gnea/grbl/wiki/Grbl-v1.1-Interface
- **UART2 Ring Buffer**: `incs/config/default/peripheral/uart/plib_uart2.h`
- **Motion Buffer**: `srcs/motion/motion_buffer.c` (16-block ring buffer)
- **Motion Math**: `srcs/motion/motion_math.c` (GRBL settings, conversions)
- **Multi-Axis Control**: `srcs/motion/multiaxis_control.c` (S-curve motion)
