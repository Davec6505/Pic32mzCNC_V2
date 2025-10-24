# Multi-Axis Motion Control - Call Hierarchy Reference

**Document Version**: 1.0  
**Date**: October 24, 2025  
**System**: PIC32MZ CNC Motion Controller V2  
**Architecture**: GRBL Phase 2B - Segment-Based Execution

---

## Table of Contents

1. [Overview](#overview)
2. [System Initialization Flow](#system-initialization-flow)
3. [Main Loop Processing](#main-loop-processing)
4. [Segment Execution Flow](#segment-execution-flow)
5. [ISR Callback Hierarchy](#isr-callback-hierarchy)
6. [Public API Functions](#public-api-functions)
7. [Internal Helper Functions](#internal-helper-functions)

---

## Overview

The PIC32MZ CNC controller uses a **layered architecture** with clear separation between:
- **Application Layer** (main.c) - G-code parsing and command buffering
- **Planning Layer** (grbl_planner.c) - Motion planning and velocity optimization
- **Execution Layer** (grbl_stepper.c, multiaxis_control.c) - Real-time segment execution
- **Hardware Layer** (MCC Harmony3 PLIB) - OCR/Timer pulse generation

### Key Architectural Principles

✅ **Segment-Based Execution** - GRBL stepper buffer feeds 2mm segments to hardware  
✅ **Dominant/Subordinate Pattern** - One axis controls timing, others bit-banged  
✅ **Hardware Pulse Generation** - OCR modules (OCMP1/3/4/5) generate step pulses  
✅ **Transition Detection** - Edge-triggered role changes (subordinate ↔ dominant)  
✅ **Atomic Updates** - Selective interrupt masking prevents race conditions  

---

## System Initialization Flow

### 1. Power-On Reset → main()

```
main() [main.c:54]
├─ SYS_Initialize(NULL)                    // Harmony3 peripheral init
│  ├─ CLK_Initialize()                     // System clock configuration
│  ├─ GPIO_Initialize()                    // Pin configurations
│  ├─ TMR2_Initialize()                    // Timer for X-axis (OCMP4)
│  ├─ TMR3_Initialize()                    // Timer for Z-axis (OCMP5)
│  ├─ TMR4_Initialize()                    // Timer for Y-axis (OCMP1)
│  ├─ TMR5_Initialize()                    // Timer for A-axis (OCMP3)
│  ├─ OCMP1_Initialize()                   // Y-axis step pulses
│  ├─ OCMP3_Initialize()                   // A-axis step pulses
│  ├─ OCMP4_Initialize()                   // Z-axis step pulses (NOTE: Swapped in HW!)
│  ├─ OCMP5_Initialize()                   // X-axis step pulses (NOTE: Swapped in HW!)
│  ├─ TMR9_Initialize()                    // Motion manager @ 10ms
│  ├─ UART2_Initialize()                   // Serial communication @ 115200
│  └─ CORETIMER_Initialize()               // System tick counter
│
├─ UGS_Initialize() [ugs_interface.c]
│  ├─ Serial_Initialize()                  // UART wrapper with ring buffer
│  └─ Serial_RegisterRxCallback()          // ISR callback setup
│
├─ GRBLPlanner_Initialize() [grbl_planner.c]
│  ├─ MotionMath_InitializeSettings()      // Load GRBL $100-$133 settings
│  └─ Clear planner ring buffer (16 blocks)
│
├─ MultiAxis_Initialize() [multiaxis_control.c:2120]
│  ├─ MotionMath_InitializeSettings()      // Already called, idempotent
│  ├─ Initialize segment_state[] arrays
│  ├─ Clear machine_position[] counters
│  ├─ Register OCR ISR callbacks:
│  │  ├─ OCMP5_CallbackRegister(OCMP5_StepCounter_X) [X-axis]
│  │  ├─ OCMP1_CallbackRegister(OCMP1_StepCounter_Y) [Y-axis]
│  │  ├─ OCMP4_CallbackRegister(OCMP4_StepCounter_Z) [Z-axis]
│  │  └─ OCMP3_CallbackRegister(OCMP3_StepCounter_A) [A-axis]
│  ├─ Set default step execution strategy:
│  │  └─ MultiAxis_SetStepStrategy(axis, Execute_Bresenham_Strategy)
│  └─ Register TMR9 motion manager callback:
│     └─ TMR9_CallbackRegister(MotionManager_TMR9_ISR)
│
├─ GCode_Initialize() [gcode_parser.c]
│  └─ Load default modal state (G90, G21, etc.)
│
└─ UGS_SendBuildInfo()                     // Announce version to host
   └─ UART TX: "[VER:1.1f.20251017:PIC32MZ CNC V2]"
```

**Total Init Time**: ~50ms (mostly peripheral configuration)

---

## Main Loop Processing

### 2. Infinite Main Loop (Polling Architecture)

```
while (true) [main.c:86]
{
    ┌─────────────────────────────────────────────────────────────┐
    │ STEP 1: Handle Real-Time Commands (?, !, ~, ^X)             │
    └─────────────────────────────────────────────────────────────┘
    rt_cmd = Serial_GetRealtimeCommand()
    if (rt_cmd != 0)
        └─ GCode_HandleControlChar(rt_cmd)
           ├─ '?' → UGS_SendStatusReport()        // Position feedback
           ├─ '!' → MotionBuffer_Pause()          // Feed hold
           ├─ '~' → MotionBuffer_Resume()         // Cycle start
           └─ ^X  → Emergency stop sequence

    ┌─────────────────────────────────────────────────────────────┐
    │ STEP 2: Read Serial Input (Non-Blocking Character Read)     │
    └─────────────────────────────────────────────────────────────┘
    c_int = Serial_Read()                         // From ring buffer
    if (c_int != -1)                              // Character available?
    {
        /* Input sanitization (Oct 23, 2025 fix) */
        if (byte >= 0x80 || non_printable_control)
            continue;                             // Drop extended/junk bytes

        /* Line buffering */
        if (c == '\n' || c == '\r')
        {
            ┌─────────────────────────────────────────────────────────┐
            │ STEP 3: Parse Complete G-Code Line                      │
            └─────────────────────────────────────────────────────────┘
            GCode_ParseLine(line, &move)
            ├─ Tokenize line (G1 X10 Y20 F1000 → tokens)
            ├─ Validate syntax and modal groups
            ├─ Apply modal state (G90/G91, G20/G21, etc.)
            └─ Fill parsed_move_t structure

            if (is_motion_command)
            {
                ┌─────────────────────────────────────────────────────┐
                │ STEP 4: Convert to Machine Coordinates              │
                └─────────────────────────────────────────────────────┘
                GRBLPlanner_GetPosition(target_mm)  // Current position
                
                for each axis:
                    if (G90)  // Absolute mode
                        target_mm[axis] = MotionMath_WorkToMachine(move.target[axis])
                    else      // G91 relative mode
                        target_mm[axis] += move.target[axis]

                ┌─────────────────────────────────────────────────────┐
                │ STEP 5: Buffer Line into GRBL Planner               │
                └─────────────────────────────────────────────────────┘
                GRBLPlanner_BufferLine(target_mm, &pl_data)
                ├─ Convert mm → steps (MotionMath_MMToSteps)
                ├─ Add to planner ring buffer (16 blocks)
                ├─ Calculate junction velocities
                └─ Trigger look-ahead planning

                UGS_SendOK()  // Flow control acknowledgment
            }
        }
    }

    ┌─────────────────────────────────────────────────────────────┐
    │ STEP 6: Check if Ready to Start New Segment                 │
    └─────────────────────────────────────────────────────────────┘
    if (!MultiAxis_IsBusy() && GRBLStepper_HasSegment())
    {
        MultiAxis_StartSegmentExecution()
        └─ Load first segment from GRBL stepper buffer
           └─ Configure dominant axis hardware (see Segment Execution)
    }
}
```

**Main Loop Frequency**: ~10kHz (100µs per iteration when idle)

**Note**: APP_Tasks() and SYS_Tasks() have been removed from the main loop for streamlined execution. These may be re-added in future if application-level state machine or additional Harmony3 polling is required.

---

## Segment Execution Flow

### 3. Segment Loading and Execution

```
MultiAxis_StartSegmentExecution() [multiaxis_control.c:2322]
│
├─ GRBLStepper_GetNextSegment() → first_seg
│  └─ Returns: st_segment_t structure with:
│     - steps[NUM_AXES]                    // Step counts per axis
│     - period                             // OCR period (timer counts)
│     - direction_bits                     // Bitmask for direction
│     - bresenham_counter[NUM_AXES]        // GRBL pre-calculated values
│     - n_step                             // Dominant axis step count
│
├─ ┌────────────────────────────────────────────────────────────┐
│  │ CRITICAL: Determine Dominant Axis (Max Steps Logic)        │
│  └────────────────────────────────────────────────────────────┘
│  max_steps = 0
│  dominant_axis = AXIS_X  // Default
│  for each axis:
│      if (first_seg->steps[axis] > max_steps)
│          max_steps = first_seg->steps[axis]
│          dominant_axis = axis
│
├─ ┌────────────────────────────────────────────────────────────┐
│  │ STEP 1: Initialize Per-Axis State                          │
│  └────────────────────────────────────────────────────────────┘
│  for each axis with motion (steps[axis] > 0):
│      state = &segment_state[axis]
│      state->current_segment = first_seg
│      state->step_count = 0
│      state->bresenham_counter = first_seg->bresenham_counter[axis]  // Oct 24 fix!
│      state->active = (axis == dominant_axis)  // ONLY dominant active!
│      state->block_steps_commanded = first_seg->block_steps[axis]
│      state->block_steps_executed = 0
│
├─ ┌────────────────────────────────────────────────────────────┐
│  │ STEP 2: Configure Dominant Axis Hardware                   │
│  └────────────────────────────────────────────────────────────┘
│  MultiAxis_EnableDriver(dominant_axis)
│  │  └─ en_clear_funcs[axis]()  // ENABLE pin active-low
│  │
│  Set Direction GPIO:
│  ├─ if (first_seg->direction_bits & (1 << dominant_axis))
│  │      DirX_Clear()  // Negative direction
│  │  else
│  │      DirX_Set()    // Positive direction
│  │
│  Configure OCR Period:
│  ├─ period = first_seg->period
│  ├─ if (period > 65485) period = 65485        // 16-bit timer limit
│  ├─ if (period <= 40) period = 50             // Pulse width safety
│  │
│  ├─ axis_hw[dominant_axis].TMR_PeriodSet(period)
│  ├─ axis_hw[dominant_axis].OCMP_CompareValueSet(period - 40)
│  ├─ axis_hw[dominant_axis].OCMP_CompareSecondaryValueSet(40)
│  │
│  Start Hardware:
│  ├─ axis_hw[dominant_axis].OCMP_Enable()
│  └─ axis_hw[dominant_axis].TMR_Start()
│
└─ ┌────────────────────────────────────────────────────────────┐
   │ STEP 3: Set Dominant Bitmask (Atomic Commit)               │
   └────────────────────────────────────────────────────────────┘
   segment_completed_by_axis = (1 << dominant_axis)
   
   ✅ NOW: Hardware running, ISRs will fire and execute segment!
```

---

## ISR Callback Hierarchy

### 4. OCR Interrupt Service Routine (Fires on Every Pulse)

**All 4 axes use identical pattern** (X-axis shown as example):

```
OCMP5_StepCounter_X(context) [ISR - multiaxis_control.c:1420]
│
├─ axis = AXIS_X
│
├─ ┌────────────────────────────────────────────────────────────┐
│  │ TRANSITION DETECTION: Check Role Change                    │
│  └────────────────────────────────────────────────────────────┘
│  if (IsDominantAxis(axis) && !axis_was_dominant_last_isr[axis])
│  {
│      /* ✅ TRANSITION: Subordinate → Dominant (ONE-TIME SETUP) */
│      
│      MultiAxis_EnableDriver(axis)
│      │  └─ en_clear_funcs[axis]()  // Enable motor driver
│      
│      state = &segment_state[axis]
│      if (state->current_segment != NULL)
│      {
│          Set Direction GPIO based on direction_bits
│          
│          Configure OCR:
│          ├─ period = state->current_segment->period  // Oct 24 fix!
│          ├─ TMR3_PeriodSet(period)
│          ├─ OCMP5_CompareValueSet(period - 40)
│          └─ OCMP5_CompareSecondaryValueSet(40)
│          
│          Enable Hardware:
│          ├─ OCMP5_Enable()
│          └─ TMR3_Start()
│      }
│      
│      axis_was_dominant_last_isr[axis] = true
│  }
│
├─ else if (IsDominantAxis(axis))
│  {
│      /* ✅ CONTINUOUS: Still Dominant (EVERY ISR) */
│      
│      ProcessSegmentStep(axis)
│      │  └─ Executes Bresenham for all axes (see below)
│      
│      Update OCR period (velocity may change):
│      ├─ state = &segment_state[axis]
│      ├─ if (state->current_segment != NULL)
│      │  {
│      │      period = state->current_segment->period  // Oct 24 fix!
│      │      TMR3_PeriodSet(period)
│      │      OCMP5_CompareValueSet(period - 40)
│      │  }
│  }
│
├─ else if (axis_was_dominant_last_isr[axis])
│  {
│      /* ✅ TRANSITION: Dominant → Subordinate (ONE-TIME TEARDOWN) */
│      
│      OCMP5_Disable()     // Stop continuous pulsing
│      TMR3_Stop()         // Wait for Bresenham trigger
│      
│      axis_was_dominant_last_isr[axis] = false
│  }
│
└─ else
   {
       /* ✅ SUBORDINATE: Pulse Completed (Bresenham-triggered) */
       
       OCMP5_Disable()     // Auto-disable after single pulse
       TMR3_Stop()         // Wait for next Bresenham trigger
   }
```

**ISR Frequency**: Variable (10Hz - 31kHz depending on velocity)  
**ISR Duration**: ~12 CPU cycles (~60ns @ 200MHz) when subordinate  
**ISR Duration**: ~150 CPU cycles (~750ns) when dominant (includes Bresenham)

---

### 5. ProcessSegmentStep() - Bresenham Coordination

```
ProcessSegmentStep(dominant_axis) [multiaxis_control.c:1003]
│
├─ Guard: Check bitmask (immediate return if not dominant)
│  if (!(segment_completed_by_axis & (1 << dominant_axis)))
│      return;
│
├─ state = &segment_state[dominant_axis]
│  if (!state->active || state->current_segment == NULL)
│      return;
│
├─ ┌────────────────────────────────────────────────────────────┐
│  │ STEP 1: Cache Segment Pointer (Oct 24 fix - prevent race!) │
│  └────────────────────────────────────────────────────────────┘
│  segment = state->current_segment  // Local cache
│
├─ ┌────────────────────────────────────────────────────────────┐
│  │ STEP 2: Execute Bresenham for ALL Axes                     │
│  └────────────────────────────────────────────────────────────┘
│  if (axis_step_executor[dominant_axis] != NULL)
│  {
│      axis_step_executor[dominant_axis](dominant_axis, segment)
│      └─ Execute_Bresenham_Strategy_Internal(dominant_axis, segment)
│         │
│         ├─ Update dominant axis position:
│         │  machine_position[dominant_axis]++  // or -- based on direction
│         │  dom_state->step_count++
│         │  dom_state->block_steps_executed++
│         │
│         └─ For each SUBORDINATE axis:
│            {
│                if (steps[sub_axis] == 0) continue;
│                
│                sub_state->bresenham_counter += steps[sub_axis]
│                
│                if (sub_state->bresenham_counter >= n_step)
│                {
│                    sub_state->bresenham_counter -= n_step
│                    
│                    ┌──────────────────────────────────────────┐
│                    │ CRITICAL: Trigger Hardware Pulse!        │
│                    └──────────────────────────────────────────┘
│                    switch (sub_axis):
│                        case AXIS_X:
│                            OCMP5_CompareValueSet(5)
│                            OCMP5_CompareSecondaryValueSet(36)
│                            TMR3 = 0xFFFF  // Force rollover
│                            OCMP5_Enable()
│                            break;
│                        // ... (similar for Y/Z/A)
│                    
│                    Update subordinate position:
│                    machine_position[sub_axis]++  // or --
│                    sub_state->step_count++
│                }
│            }
│  }
│
├─ ┌────────────────────────────────────────────────────────────┐
│  │ STEP 3: Check if Segment Complete                          │
│  └────────────────────────────────────────────────────────────┘
│  dominant_steps = segment->steps[dominant_axis]
│  if (state->step_count < dominant_steps)
│      return;  // Not done yet
│
├─ ┌────────────────────────────────────────────────────────────┐
│  │ STEP 4: Stop Dominant Hardware                             │
│  └────────────────────────────────────────────────────────────┘
│  axis_hw[dominant_axis].OCMP_Disable()
│  axis_hw[dominant_axis].TMR_Stop()
│  state->active = false
│  segment_completed_by_axis &= ~(1 << dominant_axis)
│
├─ ┌────────────────────────────────────────────────────────────┐
│  │ STEP 5: Advance to Next Segment                            │
│  └────────────────────────────────────────────────────────────┘
│  GRBLStepper_SegmentComplete()  // Advance stepper buffer tail
│  next_seg = GRBLStepper_GetNextSegment()
│
└─ if (next_seg == NULL)
   {
       /* Last segment complete - stop all axes */
       for each axis:
           axis_hw[axis].OCMP_Disable()
           axis_hw[axis].TMR_Stop()
           segment_state[axis].active = false
       segment_completed_by_axis = 0
   }
   else
   {
       ┌────────────────────────────────────────────────────────┐
       │ ATOMIC TRANSITION PATTERN (Oct 24, 2025)               │
       └────────────────────────────────────────────────────────┘
       
       /* Selective interrupt masking (OCR only) */
       saved_iec0 = DisableOCRInterrupts_Save()
       
       /* Determine new dominant axis (max steps) */
       new_dominant_axis = axis_with_most_steps(next_seg)
       
       /* Update state for new dominant */
       segment_state[new_dominant_axis].current_segment = next_seg
       segment_state[new_dominant_axis].step_count = 0
       segment_state[new_dominant_axis].active = true
       
       /* Update subordinate axes */
       for each axis != new_dominant_axis:
           if (next_seg->steps[axis] > 0)
               segment_state[axis].current_segment = next_seg
               segment_state[axis].bresenham_counter = next_seg->bresenham_counter[axis]
           else
               axis_hw[axis].OCMP_Disable()
               axis_hw[axis].TMR_Stop()
       
       /* Configure hardware for new dominant */
       MultiAxis_EnableDriver(new_dominant_axis)
       Set Direction GPIO
       Configure OCR (period, compare values)
       OCMP_Enable()
       TMR_Start()
       
       /* ATOMIC COMMIT: Update bitmask LAST */
       segment_completed_by_axis = (1 << new_dominant_axis)
       
       /* Restore OCR interrupts */
       DisableOCRInterrupts_Restore(saved_iec0)
   }
```

---

## Public API Functions

### Initialization

| Function | Purpose | Called From |
|----------|---------|-------------|
| `MultiAxis_Initialize()` | Setup hardware, register callbacks | main.c startup |
| `MultiAxis_SetStepStrategy()` | Assign execution function per axis | MultiAxis_Initialize() |

### Motion Control

| Function | Purpose | Called From |
|----------|---------|-------------|
| `MultiAxis_StartSegmentExecution()` | Load & start first segment | main.c loop |
| `MultiAxis_MoveSingleAxis()` | Legacy single-axis move | Unused (deprecated) |
| `MultiAxis_ExecuteCoordinatedMove()` | Legacy S-curve move | Unused (deprecated) |
| `MultiAxis_StopAll()` | Emergency stop all axes | GCode soft reset |

### Status Query

| Function | Purpose | Called From |
|----------|---------|-------------|
| `MultiAxis_IsBusy()` | Check if any axis moving | main.c loop |
| `MultiAxis_IsAxisBusy()` | Check single axis | Status reports |
| `MultiAxis_GetStepCount()` | Get position (steps) | UGS status reports |
| `MultiAxis_GetAxisState()` | Get detailed axis state | Debug/diagnostics |

### Driver Control

| Function | Purpose | Called From |
|----------|---------|-------------|
| `MultiAxis_EnableDriver()` | Activate motor driver | Segment start, transitions |
| `MultiAxis_DisableDriver()` | Deactivate motor driver | Emergency stop |
| `MultiAxis_IsDriverEnabled()` | Check driver state | Status query |

### Debug Functions

| Function | Purpose | Called From |
|----------|---------|-------------|
| `MultiAxis_GetDebugYStepCount()` | Get Y-axis pulse count | Debug commands |
| `MultiAxis_GetDebugSegmentCount()` | Get segment counter | Debug commands |
| `MultiAxis_ResetDebugCounters()` | Clear debug counters | Test reset |

---

## Internal Helper Functions

### Core Execution

| Function | Purpose | ISR Context |
|----------|---------|-------------|
| `Execute_Bresenham_Strategy_Internal()` | Bresenham algorithm + bit-bang | ✅ YES (dominant ISR) |
| `ProcessSegmentStep()` | Segment coordination wrapper | ✅ YES (OCR ISR) |
| `IsDominantAxis()` | Inline bitmask check | ✅ YES (OCR ISR) |

### Hardware Control

| Function | Purpose | ISR Context |
|----------|---------|-------------|
| `DisableOCRInterrupts_Save()` | Selective masking (IEC0CLR) | ✅ YES (atomic transition) |
| `DisableOCRInterrupts_Restore()` | Restore IEC0 register | ✅ YES (atomic transition) |

### OCR ISR Callbacks (Hardware-Specific)

| Function | Axis | Timer | OCR Module |
|----------|------|-------|------------|
| `OCMP5_StepCounter_X()` | X | TMR3 | OCMP5 |
| `OCMP1_StepCounter_Y()` | Y | TMR4 | OCMP1 |
| `OCMP4_StepCounter_Z()` | Z | TMR2 | OCMP4 |
| `OCMP3_StepCounter_A()` | A | TMR5 | OCMP3 |

---

## Function Call Graph (Text-Based)

```
INITIALIZATION TREE
===================
main()
├── SYS_Initialize()                           [Harmony3 Generated]
├── UGS_Initialize()
│   └── Serial_Initialize()
├── GRBLPlanner_Initialize()
│   └── MotionMath_InitializeSettings()
├── MultiAxis_Initialize()
│   ├── MotionMath_InitializeSettings()        [Idempotent]
│   ├── OCMP5_CallbackRegister()               [Hardware]
│   ├── OCMP1_CallbackRegister()               [Hardware]
│   ├── OCMP4_CallbackRegister()               [Hardware]
│   ├── OCMP3_CallbackRegister()               [Hardware]
│   ├── MultiAxis_SetStepStrategy() × 4
│   └── TMR9_CallbackRegister()                [Hardware]
└── GCode_Initialize()

RUNTIME EXECUTION TREE
======================
main() [Infinite Loop]
├── Serial_GetRealtimeCommand()
│   └── GCode_HandleControlChar()
│       ├── UGS_SendStatusReport()
│       │   ├── MultiAxis_GetStepCount() × NUM_AXES
│       │   └── MotionMath_StepsToMM() × NUM_AXES
│       ├── MotionBuffer_Pause()
│       ├── MotionBuffer_Resume()
│       └── Emergency_Stop()
│           └── MultiAxis_StopAll()
│
├── Serial_Read()                              [Ring Buffer]
│   └── [Character available]
│       └── GCode_ParseLine()
│           └── [Motion command]
│               ├── GRBLPlanner_GetPosition()
│               ├── MotionMath_WorkToMachine() × NUM_AXES
│               ├── GRBLPlanner_BufferLine()
│               │   ├── MotionMath_MMToSteps() × NUM_AXES
│               │   ├── Calculate junction velocities
│               │   └── Trigger look-ahead planning
│               └── UGS_SendOK()
│
├── MultiAxis_IsBusy()
│   └── [Check segment_state[].active flags]
│
├── MultiAxis_StartSegmentExecution()          [If not busy]
│   ├── GRBLStepper_GetNextSegment()
│   ├── [Determine dominant axis]
│   ├── MultiAxis_EnableDriver()
│   │   └── en_clear_funcs[axis]()             [GPIO]
│   ├── [Set direction GPIO]
│   ├── axis_hw[].TMR_PeriodSet()              [Hardware]
│   ├── axis_hw[].OCMP_CompareValueSet()       [Hardware]
│   ├── axis_hw[].OCMP_CompareSecondaryValueSet() [Hardware]
│   ├── axis_hw[].OCMP_Enable()                [Hardware]
│   └── axis_hw[].TMR_Start()                  [Hardware]

ISR EXECUTION TREE
==================
OCMP5_StepCounter_X() [X-Axis ISR]
├── IsDominantAxis(AXIS_X)                     [Inline bitmask check]
│
├── [TRANSITION: Subordinate → Dominant]
│   ├── MultiAxis_EnableDriver(AXIS_X)
│   ├── [Set DirX GPIO]
│   ├── TMR3_PeriodSet()                       [Hardware]
│   ├── OCMP5_CompareValueSet()                [Hardware]
│   ├── OCMP5_CompareSecondaryValueSet()       [Hardware]
│   ├── OCMP5_Enable()                         [Hardware]
│   └── TMR3_Start()                           [Hardware]
│
├── [CONTINUOUS: Dominant Processing]
│   ├── ProcessSegmentStep(AXIS_X)
│   │   ├── [Cache segment pointer]
│   │   ├── Execute_Bresenham_Strategy_Internal()
│   │   │   ├── [Update dominant position]
│   │   │   │   ├── machine_position[AXIS_X]++
│   │   │   │   ├── segment_state[].step_count++
│   │   │   │   └── segment_state[].block_steps_executed++
│   │   │   │
│   │   │   └── [For each subordinate axis]
│   │   │       ├── [Bresenham accumulation]
│   │   │       └── [If overflow]
│   │   │           ├── OCMP_CompareValueSet(5)
│   │   │           ├── OCMP_CompareSecondaryValueSet(36)
│   │   │           ├── TMR = 0xFFFF           [Force rollover]
│   │   │           ├── OCMP_Enable()
│   │   │           ├── machine_position[sub_axis]++
│   │   │           └── segment_state[].step_count++
│   │   │
│   │   ├── [Check if segment complete]
│   │   ├── [Stop dominant hardware if done]
│   │   ├── GRBLStepper_SegmentComplete()
│   │   ├── GRBLStepper_GetNextSegment()
│   │   │
│   │   └── [If next segment exists]
│   │       ├── DisableOCRInterrupts_Save()    [IEC0CLR atomic]
│   │       ├── [Determine new dominant]
│   │       ├── [Update all axis states]
│   │       ├── MultiAxis_EnableDriver()
│   │       ├── [Configure hardware]
│   │       ├── segment_completed_by_axis = (1 << new_dominant)
│   │       └── DisableOCRInterrupts_Restore() [IEC0 restore]
│   │
│   ├── TMR3_PeriodSet()                       [Update period]
│   └── OCMP5_CompareValueSet()                [Update compare]
│
├── [TRANSITION: Dominant → Subordinate]
│   ├── OCMP5_Disable()
│   └── TMR3_Stop()
│
└── [SUBORDINATE: Pulse Auto-Disable]
    ├── OCMP5_Disable()
    └── TMR3_Stop()

[IDENTICAL PATTERN FOR OCMP1_StepCounter_Y, OCMP4_StepCounter_Z, OCMP3_StepCounter_A]
```

---

## Critical Timing Characteristics

| Operation | Frequency | Duration | Context |
|-----------|-----------|----------|---------|
| Main Loop | ~10kHz | ~100µs | Polling |
| GRBL Planner | ~100Hz | ~1ms | TMR9 ISR |
| OCR ISR (Subordinate) | Variable | ~60ns | ISR |
| OCR ISR (Dominant) | Variable | ~750ns | ISR |
| Segment Transition | On-demand | ~50µs | ISR (atomic) |
| Bresenham (per pulse) | 10Hz-31kHz | ~500ns | ISR |

---

## Data Flow Summary

```
G-Code Line (UGS/Serial)
    ↓
Serial Ring Buffer (512 bytes)
    ↓
Line Buffering (main.c)
    ↓
GCode_ParseLine() → parsed_move_t
    ↓
GRBLPlanner_BufferLine() → grbl_plan_block_t (16 blocks)
    ↓
GRBLStepper Prep → st_segment_t (16 segments × 2mm each)
    ↓
MultiAxis_StartSegmentExecution() → segment_state[]
    ↓
OCR Hardware → Step Pulses (OCM=0b101 Dual Compare)
    ↓
DRV8825 Drivers → Stepper Motors
    ↓
Machine Position Updates (machine_position[] volatile)
    ↓
UGS Status Reports (? command)
```

---

## Document Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | Oct 24, 2025 | Initial creation with complete call hierarchy |

---

**See Also**:
- `docs/CALL_HIERARCHY_DIAGRAM.puml` - PlantUML graphical representation
- `docs/DOMINANT_AXIS_HANDOFF_OCT24_2025.md` - Transition detection details
- `docs/ATOMIC_TRANSITION_OCT24_2025.md` - Race condition prevention
- `.github/copilot-instructions.md` - Complete system architecture guide
