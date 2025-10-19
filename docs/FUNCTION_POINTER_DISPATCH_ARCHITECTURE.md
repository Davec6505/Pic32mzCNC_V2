# Function Pointer Dispatch Architecture

**Date**: October 19, 2025  
**Author**: Dave + GitHub Copilot  
**Status**: ✅ **IMPLEMENTED - Ready for Testing**  
**Build**: bins/CS23.hex (220,657 bytes, 8:35 PM)

---

## 🎯 Overview

Implemented **strategy pattern** using function pointers to allow dynamic dispatch between different step execution methods (Bresenham, arc interpolation, etc.). This provides:

1. **Flexibility**: Switch execution strategies at runtime (G1 linear vs G2/G3 arcs)
2. **Minimal Overhead**: 1 extra CPU cycle per ISR call (~5ns @ 200MHz)
3. **Extensibility**: Easy to add new interpolation methods (splines, helical, etc.)
4. **Clean Architecture**: Separates execution strategy from hardware control

---

## 🏗️ Architecture

### Function Pointer Type Definition

```c
// incs/motion/multiaxis_control.h (line 23)
typedef void (*step_execution_func_t)(axis_id_t axis, const st_segment_t* seg);
```

**Signature**:
- `axis`: Axis being executed (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
- `seg`: Current segment from GRBL stepper buffer (contains steps[], n_step, direction_bits)

### Per-Axis Strategy Registration

```c
// srcs/motion/multiaxis_control.c (line 458)
static step_execution_func_t axis_step_executor[NUM_AXES];
```

Each axis has its own function pointer, allowing:
- X-axis: Bresenham (G1 linear move)
- Y-axis: Arc interpolation (G2 circular move)
- Z-axis: Bresenham (G1 linear move)
- A-axis: Custom strategy (future: helical interpolation)

### Public API

```c
// Set execution strategy for an axis
void MultiAxis_SetStepStrategy(axis_id_t axis, step_execution_func_t strategy);

// Example usage:
MultiAxis_SetStepStrategy(AXIS_X, Execute_Bresenham_Strategy);
MultiAxis_SetStepStrategy(AXIS_Y, Execute_ArcInterpolation_Strategy);
```

---

## 💡 Execution Strategies

### 1. Bresenham Strategy (Linear Interpolation)

**Purpose**: Bit-bang subordinate axes for G0/G1 linear moves.

**Implementation**: `Execute_Bresenham_Strategy_Internal()` (line 517)

**Algorithm**:
```
For each step on dominant axis:
  For each subordinate axis:
    bresenham_counter[axis] += steps[axis]
    if (bresenham_counter[axis] >= n_step)
      bresenham_counter[axis] -= n_step
      Toggle step pin (GPIO bit-bang)
```

**CPU Cost**: ~10-20 cycles per subordinate axis per step
- Integer add: 1 cycle
- Comparison: 1 cycle
- Conditional subtract: 1-2 cycles
- GPIO toggle (if step needed): 2-3 cycles

**Example** (X dominant @ 25,000 steps, Y subordinate @ 12,500 steps):
- X uses OCMP5 hardware (zero CPU for 25,000 pulses)
- Y bit-banged every 2nd X step (12,500 GPIO toggles)
- Total CPU: 12,500 × 15 cycles = 187,500 cycles = 0.94ms @ 200MHz

### 2. Arc Interpolation Strategy (Circular Interpolation)

**Purpose**: Real-time arc trajectory calculation for G2/G3 moves.

**Implementation**: `Execute_ArcInterpolation_Strategy_Internal()` (line 573)

**Status**: Placeholder - currently falls back to Bresenham

**Future Algorithm**:
```
For each step on dominant axis:
  Calculate X/Y from:
    - Center point (I, J offsets)
    - Radius
    - Current angle (incremented by arc step size)
  
  Bit-bang X/Y to reach calculated position
```

**Required**:
- Trigonometry (sin/cos lookup tables or CORDIC)
- DDA (Digital Differential Analyzer) for smooth arc
- Arc error handling (radius tolerance)

---

## ⚡ CPU Overhead Analysis

### Function Pointer Call (PIC32MZ MIPS32 Architecture)

**Direct Function Call**:
```assembly
jal function_name       ; 1 cycle (jump and link)
nop                     ; 1 cycle (branch delay slot)
; Total: 2 cycles
```

**Function Pointer Call**:
```assembly
lw  t0, axis_step_executor[axis]  ; 1 cycle (load pointer)
jalr t0                            ; 1 cycle (jump to register)
nop                                ; 1 cycle (branch delay slot)
; Total: 3 cycles
```

**Overhead**: 1 extra cycle per ISR call = **5ns @ 200MHz** (negligible!)

### Context Switch (Mode Change)

**Switching from Linear to Arc**:
```c
// One-time operation when G2/G3 command parsed:
MultiAxis_SetStepStrategy(AXIS_X, Execute_ArcInterpolation_Strategy);  // 1 cycle
```

**Per-step overhead**: **ZERO!** Function pointer already loaded.

---

## 🔧 Implementation Details

### Initialization (Default Strategy)

```c
// multiaxis_control.c - MultiAxis_Initialize() (line 1454)
void MultiAxis_Initialize(void)
{
    // ... (motion math, position tracking)
    
    // Initialize step execution strategies (default: Bresenham for all axes)
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        axis_step_executor[axis] = Execute_Bresenham_Strategy;
    }
    
    // ... (register OCR callbacks, start TMR9)
}
```

### OCR Callback Integration (Future)

When hybrid OCR/bit-bang approach is implemented:

```c
void OCMP5_StepCounter_X(uint32_t status, uintptr_t context)
{
    // Dominant axis (X) handling hardware pulse
    
    volatile axis_segment_state_t* state = &segment_state[AXIS_X];
    state->step_count++;
    
    // Update machine position
    if (state->current_segment->direction_bits & (1 << AXIS_X))
        machine_position[AXIS_X]--;
    else
        machine_position[AXIS_X]++;
    
    // **DISPATCH**: Call strategy function for this axis
    axis_step_executor[AXIS_X](AXIS_X, state->current_segment);
    
    // Check if segment complete, auto-advance, etc...
}
```

**Key Point**: Strategy function receives segment data, decides how to handle subordinate axes.

### Public vs Internal Strategies

**Public API** (in header, can be called from other modules):
```c
void Execute_Bresenham_Strategy(axis_id_t axis, const st_segment_t* seg);
void Execute_ArcInterpolation_Strategy(axis_id_t axis, const st_segment_t* seg);
```

**Internal Implementations** (static, only within multiaxis_control.c):
```c
static void Execute_Bresenham_Strategy_Internal(axis_id_t axis, const st_segment_t* seg);
static void Execute_ArcInterpolation_Strategy_Internal(axis_id_t axis, const st_segment_t* seg);
```

**Why both?**
- Public wrappers allow external modules (gcode_parser, motion_manager) to set strategies
- Internal implementations keep logic encapsulated within multiaxis_control.c
- Avoids exposing internal state to external callers

---

## 📊 Benefits vs Trade-offs

### ✅ Benefits

1. **Dynamic Dispatch**: Switch between linear and arc modes at runtime
2. **Extensibility**: Add new strategies without modifying OCR callbacks
3. **Clean Architecture**: Strategy pattern separates "what to execute" from "how to execute"
4. **Minimal Overhead**: 1 cycle per ISR call (~5ns @ 200MHz) is negligible
5. **Future-Proof**: Ready for G2/G3 arcs, splines, helical interpolation, etc.

### ⚠️ Trade-offs

1. **Indirection**: One extra memory access to load function pointer
2. **Complexity**: Slightly more code than direct function calls
3. **Debugging**: Function pointer calls harder to trace than direct calls

---

## 🧪 Testing Plan

### Phase 1: Verify Bresenham Strategy

1. **Single-Axis Move**: Send `G0 X10` via UGS
   - Verify OCMP5 (X-axis) generates hardware pulses
   - Verify position feedback: `<Idle|MPos:10.000,0.000,0.000>`

2. **Multi-Axis Move**: Send `G1 X10 Y5 F1000` via UGS
   - Verify X uses hardware (OCMP5), Y bit-banged in X's ISR
   - Verify diagonal motion (not stairstepping)
   - Verify position: `<Idle|MPos:10.000,5.000,0.000>`

### Phase 2: Implement Arc Strategy

1. **Create Arc Strategy**: Implement trigonometry/DDA for G2/G3
2. **Set Strategy**: In `ProcessCommandBuffer()`, detect G2/G3 and call:
   ```c
   MultiAxis_SetStepStrategy(AXIS_X, Execute_ArcInterpolation_Strategy);
   MultiAxis_SetStepStrategy(AXIS_Y, Execute_ArcInterpolation_Strategy);
   ```
3. **Test Circles**: Send `G2 X10 Y0 I5 J0` (full circle)
4. **Restore Linear**: After arc complete, restore Bresenham:
   ```c
   MultiAxis_SetStepStrategy(AXIS_X, Execute_Bresenham_Strategy);
   MultiAxis_SetStepStrategy(AXIS_Y, Execute_Bresenham_Strategy);
   ```

### Phase 3: Performance Profiling

1. **Measure ISR Time**: Use GPIO toggle to measure execution time
2. **Compare Direct vs Dispatch**: Verify 1-cycle overhead
3. **Stress Test**: Send 100-move sequence with mixed G1/G2/G3

---

## 🚀 Next Steps

1. ✅ **COMPLETE**: Function pointer architecture implemented
2. ⏸️ **PENDING**: Implement hybrid OCR/bit-bang (use dispatch in dominant ISR)
3. ⏸️ **PENDING**: Hardware test with oscilloscope (verify bit-bang timing)
4. ⏸️ **FUTURE**: Implement arc interpolation strategy for G2/G3

---

## 📝 Code Locations

| Component                         | File                              | Lines     |
| --------------------------------- | --------------------------------- | --------- |
| Function pointer typedef          | `incs/motion/multiaxis_control.h` | 23        |
| Public API                        | `incs/motion/multiaxis_control.h` | 29-35     |
| Strategy implementations          | `incs/motion/multiaxis_control.h` | 207-227   |
| Function pointer array            | `srcs/motion/multiaxis_control.c` | 458       |
| Forward declarations              | `srcs/motion/multiaxis_control.c` | 461-462   |
| Bresenham strategy (internal)     | `srcs/motion/multiaxis_control.c` | 517-571   |
| Arc strategy (placeholder)        | `srcs/motion/multiaxis_control.c` | 573-589   |
| Public wrappers                   | `srcs/motion/multiaxis_control.c` | 591-601   |
| Set strategy API                  | `srcs/motion/multiaxis_control.c` | 603-618   |
| Initialization (default strategy) | `srcs/motion/multiaxis_control.c` | 1454-1458 |

---

## 🏆 Design Credits

**Dave's Insight**: "Why run all axes in continuous pulse mode? Let the dominant axis use OCR hardware, subordinate axes bit-bang!"

**Strategy Pattern**: Use function pointers for dynamic dispatch (Bresenham vs Arc vs Spline).

**Result**: Elegant, extensible architecture with minimal overhead (1 cycle per ISR call).

---

**Status**: ✅ Architecture implemented and compiled. Ready for hardware testing after hybrid OCR/bit-bang implementation.
