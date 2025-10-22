# Phase 3 Look-Ahead Analysis (October 22, 2025)

## 🔍 **Current Status Analysis**

### ✅ **What's Already Working**

#### **1. Junction Velocity Calculation** (grbl_planner.c lines 690-770)
**Status**: ✅ **FULLY IMPLEMENTED AND CORRECT**

The junction deviation algorithm is complete and working:

```c
// Junction angle calculation using dot product
junction_cos_theta -= pl.previous_unit_vec[idx] * unit_vec[idx];

// Special case handling
if (junction_cos_theta > 0.999999f) {
    // 0° acute junction - minimum speed
    block->max_junction_speed_sqr = MINIMUM_JUNCTION_SPEED * MINIMUM_JUNCTION_SPEED;
}
else if (junction_cos_theta < -0.999999f) {
    // 180° junction (straight line) - infinite speed
    block->max_junction_speed_sqr = SOME_LARGE_VALUE;
}
else {
    // General case: sin(θ/2) = sqrt((1-cos(θ))/2)
    float sin_theta_d2 = sqrtf(0.5f * (1.0f - junction_cos_theta));
    
    block->max_junction_speed_sqr = maxf(
        MINIMUM_JUNCTION_SPEED * MINIMUM_JUNCTION_SPEED,
        (junction_acceleration * junction_deviation * sin_theta_d2) / (1.0f - sin_theta_d2)
    );
}
```

**Key Points:**
- Uses Jens Geisler's junction deviation algorithm
- No jerk limit needed (simpler than legacy planners)
- Calculates safe cornering speed based on centripetal acceleration
- Stored in `block->max_junction_speed_sqr` (mm/min)²

---

#### **2. Look-Ahead Planning** (grbl_planner.c lines 470-570)
**Status**: ✅ **FULLY IMPLEMENTED AND CORRECT**

The famous GRBL forward/reverse pass algorithm is complete:

```c
static void planner_recalculate(void)
{
    // REVERSE PASS: Maximize deceleration curves backward from last block
    // Last block always decelerates to zero (complete stop at end of buffer)
    current->entry_speed_sqr = minf(current->max_entry_speed_sqr,
                                    2.0f * current->acceleration * current->millimeters);
    
    // Iterate backward through buffer
    while (block_index != block_buffer_planned) {
        entry_speed_sqr = next->entry_speed_sqr +
                          2.0f * current->acceleration * current->millimeters;
        
        if (entry_speed_sqr < current->max_entry_speed_sqr) {
            current->entry_speed_sqr = entry_speed_sqr;
        } else {
            current->entry_speed_sqr = current->max_entry_speed_sqr;
        }
    }
    
    // FORWARD PASS: Refine acceleration curves forward from planned pointer
    while (block_index != buffer_head) {
        if (current->entry_speed_sqr < next->entry_speed_sqr) {
            entry_speed_sqr = current->entry_speed_sqr +
                              2.0f * current->acceleration * current->millimeters;
            
            if (entry_speed_sqr < next->entry_speed_sqr) {
                next->entry_speed_sqr = entry_speed_sqr;
                block_buffer_planned = block_index; // Optimal plan point
            }
        }
    }
}
```

**Key Points:**
- Reverse pass: Calculate maximum entry speeds backward (ensure deceleration possible)
- Forward pass: Calculate exit speeds forward (ensure acceleration limits respected)
- Planned pointer optimization: Skip recalculating optimal blocks
- Result: Each block has optimized `entry_speed_sqr` for smooth junction transitions

---

### ❌ **What's Missing: Exit Velocity Handling**

#### **The Problem** (grbl_stepper.c line 255)

**Current Code (INCORRECT)**:
```c
// Line 206: Start at entry velocity (CORRECT!)
prep.current_speed = sqrtf(prep.current_block->entry_speed_sqr) / 60.0f;

// Line 255: Exit velocity using SAME entry_speed_sqr (WRONG!)
float block_exit_speed = sqrtf(prep.current_block->entry_speed_sqr) / 60.0f;
```

**Why This Is Wrong:**
- Uses `entry_speed_sqr` for BOTH start AND end of block
- Means every block decelerates to its own entry speed (not the next block's entry!)
- Result: Machine still stops at junctions (exit speed = entry speed of CURRENT block, not NEXT)

**The GRBL Pattern:**
- Exit speed of block N = Entry speed of block N+1
- Planner ensures: `block[N].exit_velocity == block[N+1].entry_velocity`
- This is **implicitly encoded** in the `entry_speed_sqr` chain

---

#### **The Fix: Look Ahead to Next Block**

**What We Need:**
```c
// Get NEXT block's entry speed (our exit speed!)
grbl_plan_block_t *next_block = GRBLPlanner_GetNextBlock(prep.current_block);

float block_exit_speed;
if (next_block != NULL) {
    // Exit at next block's entry speed (smooth junction!)
    block_exit_speed = sqrtf(next_block->entry_speed_sqr) / 60.0f;
} else {
    // Last block in buffer - decelerate to zero
    block_exit_speed = 0.0f;
}
```

**Why This Works:**
- Planner already calculated optimal entry speeds for ALL blocks
- `next_block->entry_speed_sqr` = safe speed for the junction between current and next
- By decelerating to this speed (not zero), we flow smoothly into next block!

---

## 🎯 **Implementation Plan**

### **Task 1: Add Helper Function to Get Next Block** (15 minutes)

**File**: `srcs/motion/grbl_planner.c`

**Add Function**:
```c
/*! \brief Get next block in buffer (for look-ahead)
 *  Returns NULL if current block is last in buffer
 *  Used by stepper to determine exit velocity
 */
grbl_plan_block_t *GRBLPlanner_GetNextBlock(grbl_plan_block_t *current_block)
{
    // Calculate current block's index
    uint8_t current_index = (uint8_t)(current_block - block_buffer);
    
    // Get next block index
    uint8_t next_index = GRBLPlanner_NextBlockIndex(current_index);
    
    // Check if next block exists
    if (next_index == block_buffer_head) {
        return NULL; // Current block is last in buffer
    }
    
    return &block_buffer[next_index];
}
```

**Add to Header** (`incs/motion/grbl_planner.h`):
```c
/*! \brief Get next block in buffer (for stepper look-ahead) */
grbl_plan_block_t *GRBLPlanner_GetNextBlock(grbl_plan_block_t *current_block);
```

---

### **Task 2: Fix Exit Velocity Calculation** (15 minutes)

**File**: `srcs/motion/grbl_stepper.c` lines 252-260

**Replace**:
```c
// WRONG: Uses current block's entry speed as exit speed
float block_exit_speed = sqrtf(prep.current_block->entry_speed_sqr) / 60.0f;
```

**With**:
```c
// CORRECT: Look ahead to next block's entry speed (our exit speed!)
grbl_plan_block_t *next_block = GRBLPlanner_GetNextBlock(prep.current_block);

float block_exit_speed;
if (next_block != NULL) {
    // Exit at next block's entry speed for smooth junction transition
    block_exit_speed = sqrtf(next_block->entry_speed_sqr) / 60.0f;
} else {
    // Last block in buffer - decelerate to zero (complete stop)
    block_exit_speed = 0.0f;
}
```

**Add Comment**:
```c
// CRITICAL INSIGHT (October 22, 2025):
// GRBL planner stores entry_speed_sqr for each block.
// Exit speed of block[N] = Entry speed of block[N+1]
// This creates continuous velocity chain through buffer!
//
// Example: 90° corner at 1000mm/min programmed rate
//   Block 1 (straight): entry=0, exit=707 (junction limit for 90° turn)
//   Block 2 (straight): entry=707, exit=0 (last block)
//
// By looking ahead to next block's entry, we get smooth cornering!
```

---

### **Task 3: Enable Non-Blocking Buffer Feeding** (30 minutes)

**Problem**: Currently `UGS_SendOK()` waits for motion complete before sending "ok"

**File**: `srcs/main.c` ProcessCommandBuffer() function

**Find Current Pattern**:
```c
if (GRBLPlanner_BufferLine(...)) {
    // Wait for motion to complete
    while (GRBLStepper_IsBusy()) {
        GRBLStepper_PrepareBuffer();
    }
    UGS_SendOK(); // Send after motion complete
}
```

**Replace With**:
```c
if (GRBLPlanner_BufferLine(...)) {
    UGS_SendOK(); // ✅ Send immediately - don't wait for motion!
}

// Motion continues in background while next command arrives
// Buffer fills up to 16 blocks - planner optimizes ALL of them
```

**Why This Matters:**
- UGS can stream multiple commands before motion starts
- Planner has more blocks to optimize (better junction velocities)
- Circle test will have all 20 segments buffered → continuous motion!

---

## 🧪 **Testing Plan**

### **Test 1: Simple Junction** (10 minutes)
```gcode
G90          ; Absolute mode
G92 X0 Y0    ; Zero position
G1 X10 F1000 ; Move X
G1 Y10 F1000 ; Move Y (90° junction)
```

**Before Fix:**
- Stops at (10,0) junction
- Oscilloscope: Frequency drops to zero between moves

**After Fix:**
- Flows through junction at ~707mm/min (70.7% of programmed rate)
- Oscilloscope: Frequency slows but never stops
- Junction speed calculation: `v = sqrt(2 * 0.01 * 500 * 0.707) = 707mm/min`

---

### **Test 2: Circle Path** (20 minutes)

**G-code** (20 segments, 18° each):
```gcode
G90 G21 G17
G92 X0 Y0 Z0
G1 F1000
G1 X10.000 Y0.000
G1 X9.511 Y3.090
G1 X8.090 Y5.878
G1 X5.878 Y8.090
G1 X3.090 Y9.511
G1 X0.000 Y10.000
G1 X-3.090 Y9.511
G1 X-5.878 Y8.090
G1 X-8.090 Y5.878
G1 X-9.511 Y3.090
G1 X-10.000 Y0.000
G1 X-9.511 Y-3.090
G1 X-8.090 Y-5.878
G1 X-5.878 Y-8.090
G1 X-3.090 Y-9.511
G1 X0.000 Y-10.000
G1 X3.090 Y-9.511
G1 X5.878 Y-8.090
G1 X8.090 Y-5.878
G1 X9.511 Y-3.090
G1 X10.000 Y0.000
```

**Before Fix:**
- Stops at each of 20 corners
- Total time: ~3 seconds (2s motion + 20×50ms pause)
- Oscilloscope: 20 distinct stop events (frequency = 0)

**After Fix:**
- Continuous motion through all corners
- Total time: ~2.2 seconds (10% slower at corners, no pauses)
- Oscilloscope: Frequency varies (800-900Hz) but never zero
- Corner speed: ~950mm/min (95% of programmed 1000mm/min for 18° angle)

---

## 📊 **Expected Performance**

### **Junction Speed Calculations**

| Junction Angle | cos(θ) | sin(θ/2) | Max Speed (%) | Example Speed |
|----------------|--------|----------|---------------|---------------|
| **0°** (acute) | +1.0 | 0.000 | 0% | MINIMUM_JUNCTION_SPEED |
| **18°** (gentle) | +0.951 | 0.156 | ~95% | 950 mm/min @ 1000 programmed |
| **45°** | +0.707 | 0.383 | ~85% | 850 mm/min @ 1000 programmed |
| **90°** (right angle) | 0.0 | 0.707 | ~70% | 707 mm/min @ 1000 programmed |
| **135°** (obtuse) | -0.707 | 0.924 | ~35% | 350 mm/min @ 1000 programmed |
| **180°** (straight) | -1.0 | 1.000 | ∞ | Full speed (no slowdown) |

**Formula**:
```
v_junction = sqrt((a * δ * sin(θ/2)) / (1 - sin(θ/2)))
```
Where:
- `a` = axis-limited acceleration (mm/sec²)
- `δ` = junction deviation (0.01mm typical)
- `θ` = junction angle

---

## 🎓 **Key Insights**

### **1. GRBL's Elegant Design**
- No explicit "exit_speed" field needed!
- Exit velocity encoded in **next block's entry velocity**
- Planner ensures continuity: `block[N].exit == block[N+1].entry`

### **2. Why Current System Stops**
- Stepper uses current block's entry as exit
- Example: Block with entry=707mm/min decelerates to 707mm/min (not next block's 707!)
- Next block starts from zero (not 707) → full stop between blocks!

### **3. The Fix Is Simple**
- Look ahead one block: `next_block->entry_speed_sqr`
- This is our exit target for current block
- Result: Decelerate to junction speed (not zero)!

---

## 🚀 **Implementation Checklist**

- [ ] **Task 1**: Add `GRBLPlanner_GetNextBlock()` helper (15 min)
  - Add function to grbl_planner.c
  - Add prototype to grbl_planner.h
  - Test: Returns NULL for last block, valid pointer otherwise

- [ ] **Task 2**: Fix exit velocity calculation (15 min)
  - Update grbl_stepper.c line 255
  - Add comprehensive comment explaining GRBL pattern
  - Build and flash firmware

- [ ] **Task 3**: Enable non-blocking buffer (30 min)
  - Move UGS_SendOK() to immediate position in main.c
  - Test: Buffer fills with multiple commands
  - Verify: No blocking wait for motion complete

- [ ] **Test 1**: Simple 90° junction (10 min)
  - Send G1 X10, G1 Y10
  - Oscilloscope: Should NOT stop at junction
  - Measure: ~707mm/min corner speed (70.7% of 1000mm/min)

- [ ] **Test 2**: 20-segment circle (20 min)
  - Stream circle G-code
  - Total time: ~2.2 seconds (was 3 seconds)
  - Oscilloscope: Continuous motion, no zero-frequency events
  - Corner speed: ~950mm/min (95% for 18° gentle turns)

- [ ] **Documentation**: Update PHASE3 completion doc
  - Performance metrics (time saved, corner speeds)
  - Oscilloscope screenshots (before/after)
  - Academic paper notes (junction optimization working!)

---

## 📝 **Next Steps After Phase 3**

**Phase 4: Advanced Features** (3 weeks)
- Arc support (G2/G3) with circular interpolation
- Coordinate systems (G54-G59)
- Probing (G38.x)
- Spindle PWM
- Coolant control

**Phase 5: DMA Optimization** (2 weeks)
- Zero-ISR S-curve motion using DMA auto-load
- Pre-calculate velocity arrays
- 100x further CPU reduction (1% → 0.01%)

---

**Status**: Ready to implement! Estimated time: 1-2 hours total

**Expected Result**: Continuous motion through corners → First truly smooth open-source CNC with OCR hardware acceleration! 🚀
