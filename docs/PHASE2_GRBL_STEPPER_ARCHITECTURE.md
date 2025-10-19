# Phase 2: GRBL Stepper Integration - Comprehensive Architecture Guide

**Date**: October 19, 2025  
**Status**: Planning Phase  
**Purpose**: Complete architectural understanding before implementation

---

## Dave's Simple Understanding (The Real-World View)

> *"This is exactly the issues I had with V1 code - I just did not know how to achieve this. I found myself contemplating over and over, pulling my hair out. I am glad you came along."*

### The Problem You Were Trying to Solve (In Plain English)

**Your V1 Challenge**:
- You had working motion code (AVR AppNote 446 S-curves)
- But the machine **STOPPED between every move**
- You **knew** it should flow smoothly from one move to the next
- You could **see** the problem but couldn't figure out how to fix it
- Academic papers made it sound impossibly complex

**The Simple Truth** (What You Now Understand):

1. **Don't pre-calculate the entire move** → Calculate small chunks continuously
2. **Don't store full trajectories** → Store just strategic data (blocks) + small tactical window (segments)
3. **Don't start/stop each move at zero velocity** → Carry velocity forward from one move to the next
4. **Don't wait to see the next move** → Buffer multiple moves ahead so you can plan junctions

**That's it.** That's the whole concept. Everything else is just implementation details.

---

### Your Breakthrough Moment

**Your Question**:
> "If we are moving X100 F1000 and next move is X200 at F500, we don't stop as we are calculating continuously, we adjust small segments down to F500, is that it?"

**The Answer**: **YES!** You discovered the core insight:

```
Old Way (Your V1):
Move 1: Calculate X0→X100 @ 1000mm/min (stores: 0→1000→0)
        Execute entire trajectory
        END AT ZERO VELOCITY ← Machine stops!
        
Move 2: Calculate X100→X200 @ 500mm/min (stores: 0→500→0)
        Execute entire trajectory
        END AT ZERO VELOCITY

Problem: Can't see Move 2 when calculating Move 1, so must stop between them!
```

```
New Way (GRBL Phase 2):
Planner sees BOTH moves ahead:
- Move 1: X0→X100 @ 1000mm/min
- Move 2: X100→X200 @ 500mm/min

Planner says: "Move 1 should END at 500mm/min so Move 2 can START at 500mm/min!"

Segment buffer executes:
Move 1 ending:    ...→800→700→600→500mm/min (smooth decel to junction)
Move 2 beginning: 500→500→500mm/min (NO STOP! Already at target speed!)

Result: Machine NEVER STOPS between moves!
```

---

### Why Was This So Hard to Find?

**The Frustration** (Every CNC Hobbyist Feels This):

1. **Academic Papers**: Full of equations, Bézier curves, cubic splines, matrix algebra
   - **Reality**: You just need to ramp velocity up/down smoothly between moves!

2. **Industrial Documentation**: Assumes you have $10,000 motion controllers
   - **Reality**: You're building DIY CNC with $50 microcontroller!

3. **GRBL Source Code**: 10,000+ lines of optimized C, minimal comments
   - **Reality**: Core concept is simple, implementation is where complexity lives!

4. **Forum Posts**: "Just implement trapezoidal velocity profiling with junction deviation!"
   - **Reality**: What does that MEAN in plain English?!

**The Truth**: The concept is simple. The math is simple. The implementation is fiddly.

---

### The Simple Theory (Dave's Mental Model)

**Think of it like driving a car**:

```
Scenario 1: Stop signs everywhere (Your V1)
├─ Accelerate to 60mph
├─ See stop sign ahead
├─ STOP completely (0mph)
├─ Wait
├─ Accelerate to 30mph
├─ See next stop sign
└─ STOP completely (0mph)

Result: Jerky, slow, wastes fuel, annoying!
```

```
Scenario 2: Smooth traffic flow (GRBL Phase 2)
├─ Accelerate to 60mph
├─ See slow zone ahead (30mph)
├─ Start slowing down BEFORE the zone (60→50→40→30mph)
├─ Enter slow zone at 30mph (NO STOP!)
├─ Continue at 30mph
└─ Eventually stop when journey ends

Result: Smooth, fast, efficient, comfortable!
```

**That's literally it.** GRBL is just "smooth traffic flow" for CNC motion.

---

### The Three-Part System (Simplified)

**Part 1: The Planner** (The GPS/Navigator)
- **Job**: Look ahead at upcoming moves
- **Output**: "Move 1 should end at 500mm/min, Move 2 should start at 500mm/min"
- **When**: Runs in background when G-code arrives
- **Analogy**: GPS saying "slow down, there's a curve ahead"

**Part 2: The Segment Buffer** (The Cruise Control)
- **Job**: Break current move into small chunks (2mm), adjust velocity smoothly
- **Output**: "Next 2mm: run at 625mm/min, then 600mm/min, then 575mm/min..."
- **When**: Runs continuously (100 times per second)
- **Analogy**: Cruise control adjusting throttle smoothly as you approach curve

**Part 3: The Hardware (OCR)** (The Engine)
- **Job**: Actually generate step pulses at requested rate
- **Output**: Physical step pulses to stepper drivers
- **When**: Runs continuously in hardware (zero CPU)
- **Analogy**: Engine responding to throttle input

---

### Why GRBL's Approach is Genius

**Memory Efficiency**:
```
Your V1 (storing full trajectory):
- 1 move × 428 bytes = 428 bytes
- 16 moves × 428 bytes = 6,848 bytes ← Doesn't fit in ATmega328 (2KB RAM)!

GRBL (storing strategic data + small window):
- 16 blocks × 88 bytes = 1,408 bytes (strategic planning)
- 6 segments × 40 bytes = 240 bytes (tactical execution)
- Total = 1,648 bytes ← Fits easily!
```

**CPU Efficiency**:
```
Your V1 (burst calculation):
- CPU idle → BURST (50ms calculating) → CPU idle → Motion executes
- Problem: 50ms freeze when calculating next move!

GRBL (continuous small calculations):
- Every 10ms: Calculate one 2mm segment (0.5ms)
- Spread over time: 0.5ms / 10ms = 5% average CPU load
- No freezes, smooth background operation
```

**Flexibility**:
```
Your V1 (locked trajectory):
- Pre-calculated trajectory is FIXED
- Can't adjust mid-move (feed override? Nope!)
- Can't pause smoothly (emergency stop? Full brake!)

GRBL (adaptive):
- Calculate next segment based on current conditions
- Feed override? Just scale velocity for next segment!
- Pause? Stop generating new segments, coast to stop on current one!
```

---

### Your Journey (Documented for Future Reference)

**Stage 1: The Problem**
- V1 code worked but stopped between moves
- Knew it should be smooth but couldn't figure out how
- Tried various approaches, got frustrated

**Stage 2: The Questions**
- "What and why are we doing with steppers on Phase 2?"
- "How do continuous pulses fit into this?"
- "Do segments fit into each of the 16 blocks?"
- "Does each segment relate to the S-curve 7 segments?"

**Stage 3: The Breakthrough**
- "Ah Ha! It's the same as GRBL TMR1 ISR doing Bresenham, except we use continuous pulse drive!"
- "If moving X100 F1000 then X200 F500, we don't stop - we adjust small segments down to F500!"

**Stage 4: The Understanding**
- Don't pre-calculate everything → Calculate continuously
- Don't store full trajectories → Store strategic data + small window
- Don't stop between moves → Carry velocity forward
- Simple concept, implementation is just engineering details

**Stage 5: Relief**
- "This is exactly the issues I had with V1 code - I just did not know how to achieve this!"
- Frustration resolved by understanding the SIMPLE truth behind complex jargon

---

### Why This Information is Hard to Find

**The Real Answer**:

1. **Most experts don't remember being confused** - They learned years ago, forgot how hard it was
2. **Academic focus on math** - Papers prove optimality, not explain intuition
3. **Industrial secrecy** - Commercial controllers don't share implementation details
4. **GRBL is open source BUT** - Code is highly optimized, minimal documentation
5. **Forums assume knowledge** - "Just use trapezoidal profiling!" (assumes you know what that means)

**The Missing Piece**: Someone to translate between academic math and practical implementation!

---

### The Simple Concept (One Sentence)

**GRBL Motion Planning**:
> *"Look at multiple moves ahead, figure out how fast you can safely go through each junction, then break the current move into tiny chunks that smoothly transition from one speed to the next, all while hardware generates actual step pulses so CPU stays free."*

**That's it.** Everything else is just code to make that sentence happen reliably.

---

## Where Acceleration & Velocity Calculations Happen (Dave's Question)

> *"Where does max acc and velocity calculation take place in the planner buffer?"*

**Short Answer**: **In the PLANNER**, not the segment buffer! The segment buffer just executes what the planner calculated.

---

### The Division of Labor (Clear Separation)

```
┌─────────────────────────────────────────────────────────────┐
│ GRBL PLANNER (grbl_planner.c)                               │
│ - Strategic planning layer                                   │
│ - Runs ONCE when G-code command added to buffer             │
│                                                              │
│ CALCULATES:                                                  │
│   ✅ Maximum velocity for this move (from feedrate)         │
│   ✅ Maximum acceleration (from settings $120-$123)         │
│   ✅ Junction velocity with previous move (angle, deviation)│
│   ✅ Junction velocity with next move (same)                │
│   ✅ Entry velocity (safe speed to enter this move)         │
│   ✅ Exit velocity (safe speed to exit this move)           │
│   ✅ Nominal velocity (cruise speed for this move)          │
│                                                              │
│ STORES in planner block:                                     │
│   - steps[NUM_AXES] (target position)                       │
│   - millimeters (move distance)                             │
│   - entry_speed_sqr, exit_speed_sqr (velocities)            │
│   - acceleration (mm/sec²)                                   │
│   - max_entry_speed_sqr (junction limit)                    │
│                                                              │
│ OUTPUT: 16 blocks with optimized entry/exit velocities      │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│ SEGMENT BUFFER (grbl_stepper.c)                             │
│ - Tactical execution layer                                   │
│ - Runs CONTINUOUSLY (100Hz) while move executes             │
│                                                              │
│ USES (from planner block):                                   │
│   ✅ entry_speed_sqr, exit_speed_sqr (start/end velocities)│
│   ✅ acceleration (ramp rate)                                │
│   ✅ millimeters (total distance)                            │
│                                                              │
│ CALCULATES:                                                  │
│   ✅ Current velocity (interpolate between entry/exit)      │
│   ✅ Steps for next 2mm segment (Bresenham)                 │
│   ✅ Step rate (convert velocity to timer period)           │
│                                                              │
│ DOES NOT CALCULATE:                                          │
│   ❌ Max velocity (uses planner's value)                    │
│   ❌ Max acceleration (uses planner's value)                │
│   ❌ Junction velocities (uses planner's values)            │
│                                                              │
│ OUTPUT: 6 segments with step count & rate for hardware      │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│ OCR HARDWARE (OCMP1/3/4/5)                                  │
│ - Pulse generation layer                                     │
│ - Runs in hardware (zero CPU)                               │
│                                                              │
│ USES (from segment):                                         │
│   ✅ n_steps (how many steps for this segment)              │
│   ✅ period (timer period for step rate)                    │
│   ✅ direction_bits (which way to move)                     │
│                                                              │
│ GENERATES: Physical step pulses at requested rate           │
└─────────────────────────────────────────────────────────────┘
```

---

### Summary: Who Calculates What

| Component          | Calculates                                                             | Uses                                          | Frequency            |
| ------------------ | ---------------------------------------------------------------------- | --------------------------------------------- | -------------------- |
| **GRBL Planner**   | Max velocity, max acceleration, junction velocities, entry/exit speeds | G-code commands, $120-$123 settings, geometry | Once per command     |
| **Segment Buffer** | Current velocity (interpolate), steps per segment, step rate           | Planner's entry/exit speeds, acceleration     | Continuously (100Hz) |
| **OCR Hardware**   | Nothing (just executes)                                                | Segment's n_steps, period, direction          | Hardware (zero CPU)  |

**The Key Insight**:
- Planner does the **HARD math** (junctions, optimization) in **background** (low priority)
- Segment buffer does **SIMPLE math** (interpolation) **continuously** (medium priority)
- OCR hardware does **NO math**, just generates pulses in **hardware** (zero CPU)

**This separation allows**:
- ✅ Planner can take 10-50ms to optimize (no rush, background task)
- ✅ Segment buffer runs predictably every 10ms (no complex calculations)
- ✅ Hardware handles timing (zero jitter, zero CPU)

---



## Table of Contents

1. [Overview](#overview)
2. [Why Phase 2 Matters](#why-phase-2-matters)
3. [GRBL Bit-Bang vs OCR Hardware](#grbl-bit-bang-vs-ocr-hardware)
4. [Segment Buffer Architecture](#segment-buffer-architecture)
5. [Continuous Pulse Generation](#continuous-pulse-generation)
6. [ISR Responsibilities](#isr-responsibilities)
7. [Implementation Strategy](#implementation-strategy)
8. [Performance Analysis](#performance-analysis)
9. [Next Steps](#next-steps)

---

## Overview

### What Phase 2 Accomplishes

Phase 2 **replaces the custom time-based S-curve interpolator** (Phase 1) with **GRBL's proven segment-based stepper algorithm**, while retaining your PIC32MZ's hardware acceleration advantage.

**Key Changes:**
- ✅ Replace `TMR1 @ 1kHz` S-curve updates with GRBL segment buffer
- ✅ Port GRBL's Bresenham algorithm for perfect step synchronization
- ✅ Leverage OCR hardware for automatic pulse generation (GRBL uses bit-banging)
- ✅ Maintain GRBL compatibility while achieving higher performance

---

## Why Phase 2 Matters

### Phase 1 Limitations (Current System)

**Architecture:**
```
TMR1 @ 1kHz → Calculate velocity every 1ms
              ↓
         Update OCR periods for each axis
              ↓
         Hardware generates step pulses
```

**Problems:**
1. ❌ **1ms resolution is coarse** - Velocity updates only happen 1000 times per second
2. ❌ **Step synchronization gaps** - Between 1ms updates, axes can drift out of sync
3. ❌ **Jerk at segment boundaries** - Velocity changes happen in discrete jumps
4. ❌ **No proven track record** - Custom algorithm, not battle-tested like GRBL

### Phase 2 Improvements (GRBL Segment Buffer)

**Architecture:**
```
GRBL Planner (16 blocks)
    ↓
Segment Buffer (6 segments) ← NEW!
    ↓
Bresenham Step Generation ← NEW!
    ↓
OCR Hardware (your existing dual-compare)
```

**Benefits:**
1. ✅ **Sub-millisecond precision** - Segment updates at ~0.5-2ms intervals (distance-based)
2. ✅ **Perfect step synchronization** - Bresenham guarantees axes finish together
3. ✅ **Smooth acceleration** - Gradual velocity changes between segments
4. ✅ **Proven reliability** - GRBL's 10+ years of development and testing
5. ✅ **Higher speed capability** - Not limited by 1ms update rate
6. ✅ **Better junction handling** - Optimized velocity transitions at corners

---

## GRBL Bit-Bang vs OCR Hardware

This is **THE KEY INSIGHT** to understanding Phase 2's advantage!

### GRBL's Original Design (ATmega328 - Bit-Banging)

**Why GRBL Bit-Bangs:**
- ATmega328 doesn't have 4× independent PWM timers
- Must manually set/clear GPIO pins in software
- 30kHz ISR is the "pulse generator"

**GRBL's 30kHz Stepper ISR:**
```c
ISR(TIMER1_COMPA_vect) {  // Fires 30,000 times per second!
    
    // 1. Bresenham algorithm for each axis
    for (axis = 0; axis < N_AXIS; axis++) {
        st.counter[axis] += st.delta[axis];  // Accumulate error
        
        if (st.counter[axis] > st.event_count) {
            // This axis needs a step!
            STEP_PORT |= (1 << axis);  // ✅ Manual GPIO bit-bang HIGH
            st.counter[axis] -= st.event_count;
        }
    }
    
    // 2. Wait minimum pulse width (software delay)
    delay_microseconds(2);  
    
    // 3. Reset step pins
    STEP_PORT = 0;  // ✅ Manual GPIO bit-bang LOW
    
    // 4. Load next segment if needed
    if (--st.step_count == 0) {
        st_prep_segment();  // Get next segment from buffer
    }
}
```

**ISR Job Description:**
- ✅ Run Bresenham algorithm (every call)
- ✅ Manually toggle STEP pins HIGH
- ✅ Wait for pulse width (blocking delay)
- ✅ Manually toggle STEP pins LOW
- ✅ Manage segment transitions
- ✅ Update direction pins

**Performance Cost:**
- Fires: **30,000 times per second** (constant)
- ISR duration: ~100 CPU cycles
- Total overhead: **3,000,000 CPU cycles per second**
- Max step rate: Limited to ~30 kHz

---

### Your PIC32MZ Design (Hardware Acceleration)

**Why You Don't Bit-Bang:**
- PIC32MZ has 4× independent OCR modules with dual-compare PWM
- OCR hardware automatically generates pulses
- ISR just loads period - hardware handles timing

**Your Segment-Based Approach (Phase 2):**
```c
// Segment Complete ISR (fires ~500-2000× per second - per segment!)
void SegmentCompleteISR() {
    // 1. Get next segment (pre-calculated by st_prep_buffer)
    segment_t *seg = segment_buffer_pop();
    
    if (seg != NULL) {
        // 2. Configure OCR for this segment's velocity
        uint32_t period = seg->period;  // ✅ Pre-calculated in main loop!
        
        TMR2_PeriodSet(period);
        OCMP4_CompareValueSet(period - 40);  // X-axis
        OCMP1_CompareValueSet(period - 40);  // Y-axis
        OCMP5_CompareValueSet(period - 40);  // Z-axis
        
        // 3. Set direction bits BEFORE enabling pulses
        if (seg->direction_bits & X_DIRECTION_BIT) {
            DirX_Set();
        } else {
            DirX_Clear();
        }
        
        // 4. Hardware automatically generates pulses!
        // ✅ OCR generates 40-count pulses at 'period' frequency
        // ✅ No more ISR calls until segment complete
        // ✅ Perfect pulse width (25.6µs) guaranteed by hardware
        // ✅ Zero CPU overhead for pulse generation
        
        steps_remaining = seg->n_step;  // Track when segment done
    }
}

// Simple step counter (NOT bit-banging - just counting!)
void OCMP4_StepCallback_X() {  // Fires once PER STEP
    steps_remaining--;
    
    if (steps_remaining == 0) {
        SegmentCompleteISR();  // Load next segment
    }
}
```

**ISR Job Description:**
- ✅ Load OCR period (one instruction)
- ✅ Set direction GPIO (one instruction)
- ✅ Update segment pointer (one instruction)
- ✅ Hardware does everything else automatically!

**Performance Advantage:**
- Fires: **~2,000 times per second** (only when segment completes)
- ISR duration: ~50 CPU cycles
- Total overhead: **100,000 CPU cycles per second**
- CPU savings: **97% less than GRBL!**
- Max step rate: Limited by OCR hardware (~100 kHz+)

---

## Segment Buffer Architecture

### What Is a Segment?

A **segment** is a small subdivision of a motion block, typically 1-2mm long:

```
Block: G1 X100 Y100 F1000 (141mm diagonal)
                ↓
Segments: [2mm] [2mm] [2mm] ... [2mm] (70 segments)
          v=10   v=20   v=30  ... v=5  (varying velocities)
```

**Key Properties:**
- Fixed distance (e.g., 2mm each)
- Variable velocity (from acceleration profile)
- Pre-calculated step counts
- Pre-calculated OCR periods

### Segment Buffer Structure

```c
// Segment structure (adapted from GRBL stepper.c):
typedef struct {
    uint32_t n_step;              // Steps to execute in this segment
    uint32_t period;              // OCR period (pre-calculated)
    uint16_t step_event_count;    // Bresenham counter
    uint8_t direction_bits;       // Direction for each axis
    float mm_per_step;            // Distance per step
    float rate;                   // Target velocity (mm/min)
} segment_t;

#define SEGMENT_BUFFER_SIZE 6
segment_t segment_buffer[SEGMENT_BUFFER_SIZE];  // Circular buffer
```

**Buffer Size Rationale:**
- 6 segments × 2mm = **12mm look-ahead**
- Enough for smooth motion transitions
- Not too large (memory efficient)
- Matches GRBL's proven buffer size

### How Blocks Become Segments

**Example: 10mm Move with Acceleration**

```
GRBL Planner Block:
  Distance: 10mm
  Entry speed: 0 mm/min (stopped)
  Exit speed: 0 mm/min (stop at end)
  Max speed: 1500 mm/min (cruise)
  Acceleration: 500 mm/sec²

Segment Buffer Prep (st_prep_buffer):
  Segment 1: 2mm, v=300 mm/min  (accel phase)
  Segment 2: 2mm, v=600 mm/min  (accel phase)
  Segment 3: 2mm, v=900 mm/min  (accel phase)
  Segment 4: 2mm, v=900 mm/min  (cruise phase)
  Segment 5: 2mm, v=600 mm/min  (decel phase)
  Segment 6: 2mm, v=300 mm/min  (decel phase)
  
  [Block complete - move to next block]
```

### Data Flow: Planner → Segment Buffer → OCR

```
┌─────────────────────────────────────────────────────────────┐
│ GRBL Planner (16 blocks)                                    │
│ • Junction velocity optimization                            │
│ • Acceleration profile calculation                          │
│ • Entry/exit speed constraints                              │
└────────────────────┬────────────────────────────────────────┘
                     ↓ GRBLPlanner_GetCurrentBlock()
┌─────────────────────────────────────────────────────────────┐
│ Segment Prep (st_prep_buffer - Main Loop or TMR9)          │
│ • Break block into 2mm segments                             │
│ • Calculate velocity for each segment                       │
│ • Convert velocity → OCR period                             │
│ • Calculate Bresenham ratios                                │
│ • Pre-calculate all segment parameters                      │
└────────────────────┬────────────────────────────────────────┘
                     ↓ segment_buffer_push()
┌─────────────────────────────────────────────────────────────┐
│ Segment Buffer (6 segments)                                 │
│ [Seg 1] [Seg 2] [Seg 3] [Seg 4] [Seg 5] [Seg 6]           │
│  ready   ready   ready   empty   empty   empty             │
└────────────────────┬────────────────────────────────────────┘
                     ↓ segment_buffer_pop()
┌─────────────────────────────────────────────────────────────┐
│ Segment Execution ISR (SegmentCompleteISR)                  │
│ • Load pre-calculated OCR period                            │
│ • Set direction GPIO                                        │
│ • Update step counter                                       │
└────────────────────┬────────────────────────────────────────┘
                     ↓ Hardware OCR modules
┌─────────────────────────────────────────────────────────────┐
│ OCR Hardware (OCMP1/3/4/5 + TMR2/3/4/5)                    │
│ • Automatically generate step pulses                        │
│ • Perfect 40-count pulse width (25.6µs)                    │
│ • Zero CPU overhead during pulse generation                │
└─────────────────────────────────────────────────────────────┘
```

---

## Continuous Pulse Generation

### The Key Question: How Do Pulses Continue Between Updates?

**Answer:** OCR hardware auto-repeat mode!

### OCR Dual-Compare Pattern (Your Current Hardware)

```c
// Configure OCR dual-compare mode (done ONCE per segment):
TMR2_PeriodSet(period);                        // Timer rollover
OCMP4_CompareValueSet(period - 40);            // Rising edge
OCMP4_CompareSecondaryValueSet(40);            // Falling edge
OCMP4_Enable();                                // Enable output
TMR2_Start();                                  // Start timer

// Now hardware generates pulses AUTOMATICALLY:
// 
// TMR2 counts: 0 → 40 → (period-40) → period → rollover → repeat
//              ↓    ↓         ↓          ↓
// STEP pin:  LOW  HIGH      LOW        LOW (rollover)
//
// Pulse repeats EVERY time TMR2 overflows!
```

**Critical Insight:** Once OCR is configured, the **PIC32MZ hardware generates pulses automatically** until you:
- Stop the timer (`TMR2_Stop()`)
- Disable OCR (`OCMP4_Disable()`)
- Change the period (for velocity change)

### Phase 1 vs Phase 2: Pulse Generation Timeline

**Phase 1 (Time-Based Updates):**
```
Timeline:
t=0ms:   TMR1 ISR → Set period=1000 (100 steps/sec)
         OCR generates: ↑↓↑↓↑↓↑↓↑↓↑↓... (100 pulses per second)

t=1ms:   TMR1 ISR → Set period=800 (125 steps/sec)
         OCR generates: ↑↓↑↓↑↓↑↓↑↓↑↓... (125 pulses per second)
         
t=2ms:   TMR1 ISR → Set period=600 (166 steps/sec)
         OCR generates: ↑↓↑↓↑↓↑↓↑↓↑↓... (166 pulses per second)
         
Problem: Velocity changes only every 1ms (coarse)
         Sudden velocity jumps at each update
```

**Phase 2 (Segment-Based Updates):**
```
Timeline:
Segment 1 (2mm):  Set period=1000, generate 400 pulses
                  ↑↓↑↓↑↓↑↓... (400 pulses total @ 100 steps/sec)
                  Duration: ~4 seconds
                  ↓
                  Segment complete ISR

Segment 2 (2mm):  Set period=800, generate 400 pulses
                  ↑↓↑↓↑↓↑↓... (400 pulses total @ 125 steps/sec)
                  Duration: ~3.2 seconds
                  ↓
                  Segment complete ISR

Segment 3 (2mm):  Set period=600, generate 400 pulses
                  ↑↓↑↓↑↓↑↓... (400 pulses total @ 166 steps/sec)
                  Duration: ~2.4 seconds
                  
Improvement: Velocity changes based on distance traveled (segments)
             Segments are smaller than 1ms updates would achieve
             More frequent updates = smoother acceleration
```

### Example: 10mm Move @ 1000mm/min

**Phase 1 (Time-Based):**
```
Updates: Every 1ms (1000 updates total over 10mm)
Problem: If moving slowly (50 steps/sec), updates happen MORE OFTEN than steps
         If moving fast (5000 steps/sec), many steps happen BETWEEN updates
         No connection between steps executed and velocity updates
```

**Phase 2 (Distance-Based):**
```
Segments: 5× 2mm segments (5 updates total over 10mm)
Benefit: Updates tied to actual motion distance
         Each segment = fixed distance regardless of speed
         Natural, smooth acceleration profiles
         Physics-based control (distance → velocity)
```

---

## ISR Responsibilities

### Critical Design Principle: Separation of Concerns

**Key Insight:** Segment **calculation** happens in background (low priority), segment **execution** happens in ISR (high priority with pre-calculated data).

### Level 1: Segment Preparation (NON-ISR Context)

```c
// In main loop or TMR9 @ 100Hz (Priority 1 - lowest):
void st_prep_buffer(void) {
    // This runs in MAIN CONTEXT or LOW-PRIORITY ISR, NOT high-priority ISR!
    
    // Get current block from planner
    grbl_plan_block_t *block = GRBLPlanner_GetCurrentBlock();
    
    if (block == NULL) {
        return;  // No blocks to execute
    }
    
    // Calculate segments for this block
    while (segment_buffer_has_space() && /* block not complete */) {
        segment_t seg;
        
        // ✅ HEAVY CALCULATIONS (why it's NOT in high-priority ISR):
        // - Calculate velocity at this point in acceleration profile
        // - Apply junction velocity constraints
        // - Calculate Bresenham step ratios
        // - Determine segment length
        // - Convert velocity to OCR period
        
        seg.n_step = calculate_steps_for_segment();
        seg.rate = calculate_velocity_at_distance();
        seg.period = (uint32_t)(TMR_CLOCK_HZ / steps_per_sec);
        seg.step_event_count = /* Bresenham calculation */
        
        // Add to segment buffer (thread-safe queue)
        segment_buffer_push(&seg);
    }
}
```

**Characteristics:**
- ✅ Runs in **main loop context** or **low-priority ISR (TMR9 @ Priority 1)**
- ✅ Can do **heavy floating-point math** (no time pressure)
- ✅ Fills segment buffer **ahead of execution** (look-ahead)
- ✅ Can be preempted by UART/OCR ISRs (not time-critical)
- ✅ Runs at ~100Hz (fills buffer faster than consumption)

---

### Level 2: Segment Execution (HIGH-PRIORITY ISR)

```c
// OCR callback ISR (Priority 3 - fires when segment completes):
void SegmentCompleteISR(void) {
    // This runs in HIGH-PRIORITY ISR CONTEXT - must be FAST!
    
    // Get next segment from buffer
    segment_t *next = segment_buffer_pop();  // Thread-safe, O(1)
    
    if (next != NULL) {
        // ✅ SIMPLE OPERATIONS ONLY (ISR-safe):
        uint32_t period = next->period_precalculated;  // Already computed!
        
        // Update OCR for new velocity
        TMR2_PeriodSet(period);
        OCMP4_CompareValueSet(period - 40);
        
        // Set direction (already pre-calculated)
        if (next->direction_bits & X_DIRECTION_BIT) {
            DirX_Set();
        } else {
            DirX_Clear();
        }
        
        // Update step counter
        current_segment.steps_remaining = next->n_step;
        
    } else {
        // No more segments - stop motion
        OCMP4_Disable();
        TMR2_Stop();
    }
}

// Step counter callback (Priority 3 - fires every step):
void OCMP4_StepCallback_X(uint32_t status, uintptr_t context) {
    // Fires once PER STEP - must be EXTREMELY fast!
    
    current_segment.steps_remaining--;
    
    if (current_segment.steps_remaining == 0) {
        // Segment complete - load next one
        SegmentCompleteISR();
    }
}
```

**Characteristics:**
- ✅ Runs in **ISR context** (Priority 3 - time-critical)
- ✅ Only **simple operations** (no floating-point math)
- ✅ Loads **pre-calculated** segment data
- ✅ Can be preempted only by UART (Priority 5)
- ✅ Runs at ~500-2000Hz (segment rate) + per-step callbacks

---

### ISR Responsibility Breakdown

| Task                    | Where It Happens   | Priority | Frequency              | Math Complexity        |
| ----------------------- | ------------------ | -------- | ---------------------- | ---------------------- |
| **Segment Calculation** | Main loop or TMR9  | LOW (1)  | ~100Hz                 | HIGH (float, division) |
| **Segment Loading**     | SegmentCompleteISR | HIGH (3) | ~500-2kHz              | LOW (memory loads)     |
| **Step Counting**       | OCMP callbacks     | HIGH (3) | Per step (up to 30kHz) | NONE (decrement)       |
| **Pulse Generation**    | OCR Hardware       | HIGHEST  | Per step               | NONE (automatic)       |

**Critical Separation:**
```
Heavy Math (slow)     →  Background context (TMR9 or main loop)
Simple Loads (fast)   →  High-priority ISR (segment complete)
Pulse Generation      →  Hardware (zero CPU)
```

---

## Implementation Strategy

### Recommended Approach for Phase 2

#### **Step 1: Port GRBL Stepper Core**

**Create `grbl_stepper.c/h`:**

```c
// grbl_stepper.h
#ifndef GRBL_STEPPER_H
#define GRBL_STEPPER_H

#include <stdint.h>
#include <stdbool.h>
#include "motion_types.h"

// Segment buffer configuration
#define SEGMENT_BUFFER_SIZE 6
#define SEGMENT_LENGTH_MM 2.0f  // Target segment length

// Segment structure
typedef struct {
    uint32_t n_step;              // Steps to execute
    uint32_t period;              // OCR period (pre-calculated)
    uint16_t step_event_count;    // Bresenham counter
    uint8_t direction_bits;       // Direction for each axis (bit mask)
    float mm_per_step;            // Distance per step
    float rate;                   // Velocity (mm/min)
} segment_t;

// Public API
void GRBLStepper_Initialize(void);
void GRBLStepper_PrepBuffer(void);      // Call from TMR9 or main loop
bool GRBLStepper_IsBusy(void);
void GRBLStepper_Reset(void);

// Segment buffer management
bool segment_buffer_push(segment_t *seg);
segment_t* segment_buffer_pop(void);
bool segment_buffer_has_space(void);
uint8_t segment_buffer_count(void);

#endif // GRBL_STEPPER_H
```

**Key Functions:**

```c
// grbl_stepper.c

// Segment buffer (circular queue)
static segment_t segment_buffer[SEGMENT_BUFFER_SIZE];
static volatile uint8_t seg_head = 0;
static volatile uint8_t seg_tail = 0;

// Current segment execution state
static volatile struct {
    uint32_t steps_remaining;
    uint8_t direction_bits;
} current_segment;

// Prep buffer state machine
static struct {
    grbl_plan_block_t *current_block;
    float current_position;       // Position within block (mm)
    float current_velocity;       // Current velocity (mm/min)
    uint32_t steps_remaining;     // Steps left in block
} prep_state;

// Initialize stepper system
void GRBLStepper_Initialize(void) {
    seg_head = 0;
    seg_tail = 0;
    current_segment.steps_remaining = 0;
    prep_state.current_block = NULL;
    
    // Register OCR callbacks for segment execution
    // (Implementation depends on your OCR wrapper)
}

// Prep buffer - converts planner blocks into segments
void GRBLStepper_PrepBuffer(void) {
    // Get current block if needed
    if (prep_state.current_block == NULL) {
        prep_state.current_block = GRBLPlanner_GetCurrentBlock();
        if (prep_state.current_block == NULL) {
            return;  // No blocks available
        }
        
        // Initialize for new block
        prep_state.steps_remaining = prep_state.current_block->step_event_count;
        prep_state.current_position = 0.0f;
        prep_state.current_velocity = prep_state.current_block->entry_speed;
    }
    
    // Fill segment buffer
    while (segment_buffer_has_space() && prep_state.steps_remaining > 0) {
        segment_t seg;
        
        // Calculate segment parameters
        calculate_segment(&seg);
        
        // Add to buffer
        if (!segment_buffer_push(&seg)) {
            break;  // Buffer full
        }
        
        // Update state
        prep_state.steps_remaining -= seg.n_step;
        prep_state.current_position += SEGMENT_LENGTH_MM;
        
        // Check if block complete
        if (prep_state.steps_remaining == 0) {
            GRBLPlanner_DiscardCurrentBlock();
            prep_state.current_block = NULL;
            break;  // Get next block on next call
        }
    }
}

// Calculate segment parameters (velocity, steps, period)
static void calculate_segment(segment_t *seg) {
    grbl_plan_block_t *block = prep_state.current_block;
    
    // Calculate distance remaining
    float distance_remaining = (float)prep_state.steps_remaining * block->millimeters / 
                              block->step_event_count;
    
    // Determine segment length (typically 2mm, or remaining distance if less)
    float segment_mm = (distance_remaining < SEGMENT_LENGTH_MM) ? 
                       distance_remaining : SEGMENT_LENGTH_MM;
    
    // Calculate velocity at current position (acceleration profile)
    float velocity = calculate_velocity_at_position(
        prep_state.current_position,
        block->millimeters,
        block->entry_speed,
        block->cruise_speed,
        block->exit_speed,
        block->acceleration
    );
    
    // Convert to steps
    seg->n_step = (uint32_t)(segment_mm * block->step_event_count / block->millimeters);
    
    // Convert velocity to OCR period
    float steps_per_sec = velocity * block->step_event_count / block->millimeters / 60.0f;
    seg->period = (uint32_t)(TMR_CLOCK_HZ / steps_per_sec);
    
    // Clamp period to safe range
    if (seg->period > 65485) seg->period = 65485;
    if (seg->period < 100) seg->period = 100;
    
    // Copy direction bits
    seg->direction_bits = block->direction_bits;
    
    // Store metadata
    seg->rate = velocity;
    seg->mm_per_step = block->millimeters / block->step_event_count;
}
```

---

#### **Step 2: Integrate with Motion Manager**

**Modify `motion_manager.c`:**

```c
// Replace Phase 1 execution with Phase 2 segment prep
void TMR9_MotionManagerISR(uint32_t status, uintptr_t context) {
    // Check if segment buffer needs filling
    if (segment_buffer_space_available() > 2) {
        GRBLStepper_PrepBuffer();  // Calculate new segments
    }
    
    // Position tracking (keep existing code)
    if (!MultiAxis_IsBusy()) {
        // Update machine_position when motion complete
        // ... existing code ...
    }
}
```

---

#### **Step 3: Wire Segment Execution to OCR**

**Segment Complete Handler:**

```c
// Called when current segment's steps complete
void SegmentCompleteISR(void) {
    segment_t *seg = segment_buffer_pop();
    
    if (seg != NULL) {
        // Load OCR periods for new velocity
        uint32_t period = seg->period;
        
        TMR2_PeriodSet(period);  // X-axis timer
        TMR4_PeriodSet(period);  // Y-axis timer
        TMR3_PeriodSet(period);  // Z-axis timer
        TMR5_PeriodSet(period);  // A-axis timer
        
        OCMP4_CompareValueSet(period - 40);  // X-axis
        OCMP1_CompareValueSet(period - 40);  // Y-axis
        OCMP5_CompareValueSet(period - 40);  // Z-axis
        OCMP3_CompareValueSet(period - 40);  // A-axis
        
        // Set direction GPIO BEFORE enabling pulses
        if (seg->direction_bits & (1 << X_AXIS)) {
            DirX_Set();
        } else {
            DirX_Clear();
        }
        // ... repeat for Y, Z, A axes
        
        // Update step counter
        current_segment.steps_remaining = seg->n_step;
        current_segment.direction_bits = seg->direction_bits;
        
    } else {
        // No more segments - stop motion
        MultiAxis_StopAll();
    }
}
```

**Step Counter Callbacks (existing OCR ISRs):**

```c
// Modify existing OCMP4_StepCounter_X callback:
void OCMP4_StepCounter_X(uint32_t status, uintptr_t context) {
    // Existing position tracking
    if (current_segment.direction_bits & (1 << X_AXIS)) {
        machine_position[X_AXIS]--;
    } else {
        machine_position[X_AXIS]++;
    }
    
    // NEW: Segment step counter
    current_segment.steps_remaining--;
    
    if (current_segment.steps_remaining == 0) {
        SegmentCompleteISR();  // Load next segment
    }
}
```

---

### Implementation Phases

**Phase 2A: Core Stepper Port (Day 1)**
1. Create `grbl_stepper.c/h` with segment buffer
2. Port segment prep logic from GRBL
3. Implement segment calculation functions
4. Add Bresenham step distribution
5. Build and verify compilation

**Phase 2B: OCR Integration (Day 1-2)**
1. Connect segment execution to OCR callbacks
2. Implement SegmentCompleteISR
3. Modify existing step counter callbacks
4. Test with single-axis moves
5. Verify pulse generation with oscilloscope

**Phase 2C: Multi-Axis Testing (Day 2)**
1. Test coordinated moves (X+Y diagonal)
2. Test acceleration/deceleration profiles
3. Verify position tracking accuracy
4. Test junction velocity transitions
5. Oscilloscope verification of smooth profiles

**Phase 2D: Optimization & Tuning (Day 2-3)**
1. Tune segment buffer size (test 4, 6, 8 segments)
2. Optimize segment length (test 1mm, 2mm, 3mm)
3. Profile CPU usage (ensure <10% overhead)
4. Stress test with complex G-code (circles, spirals)
5. Final hardware verification

---

## Performance Analysis

### CPU Overhead Comparison

**GRBL (ATmega328 @ 16MHz):**
```
ISR Frequency:     30,000 Hz (constant)
ISR Duration:      ~100 CPU cycles
Total Overhead:    3,000,000 cycles/sec
Percentage:        18.75% of 16MHz CPU
Max Step Rate:     ~30 kHz (limited by ISR)
```

**Phase 1 (Current - Time-Based):**
```
ISR Frequency:     1,000 Hz (TMR1 @ 1kHz)
ISR Duration:      ~500 CPU cycles (S-curve math)
Total Overhead:    500,000 cycles/sec
Percentage:        0.25% of 200MHz CPU
Max Step Rate:     Limited by 1ms update granularity
```

**Phase 2 (Segment-Based):**
```
Prep Frequency:    100 Hz (TMR9 or main loop)
Prep Duration:     ~5,000 CPU cycles (heavy math OK)
Seg ISR Frequency: ~2,000 Hz (segment complete)
Seg ISR Duration:  ~50 CPU cycles (simple loads)
Step ISR Frequency: ~10,000 Hz (step callbacks)
Step ISR Duration: ~20 CPU cycles (decrement)

Total Overhead:    500,000 + 100,000 + 200,000 = 800,000 cycles/sec
Percentage:        0.4% of 200MHz CPU
Max Step Rate:     Limited by OCR hardware (~100 kHz+)

Advantage:         
  - 97% less overhead than GRBL bit-bang
  - Smoother motion than Phase 1 (distance-based)
  - Higher max speed than both
```

### Memory Usage

```
Segment Buffer:    6 segments × 32 bytes = 192 bytes
Prep State:        ~64 bytes
Total:             ~256 bytes (negligible on 512KB RAM)
```

### Performance Metrics (Expected)

| Metric            | Phase 1         | Phase 2          | GRBL Original  |
| ----------------- | --------------- | ---------------- | -------------- |
| CPU Overhead      | 0.25%           | 0.4%             | 18.75%         |
| Update Rate       | 1kHz (time)     | ~2kHz (distance) | 30kHz (steps)  |
| Max Step Rate     | ~10kHz          | ~100kHz          | ~30kHz         |
| Pulse Jitter      | ±0ns (hardware) | ±0ns (hardware)  | ±500ns (ISR)   |
| Motion Smoothness | ⭐⭐ (1ms jumps)  | ⭐⭐⭐⭐⭐ (smooth)   | ⭐⭐⭐⭐⭐ (smooth) |
| Code Maturity     | Custom (new)    | GRBL (proven)    | GRBL (proven)  |

---

## Next Steps

### Before Implementation (Questions to Resolve)

1. **Segment Length Tuning**
   - Should we use 1mm, 2mm, or 3mm segments?
   - How does segment length affect smoothness vs CPU?

2. **Buffer Size Optimization**
   - Is 6 segments enough? Test with 4, 8, 12?
   - Trade-off: Look-ahead distance vs memory

3. **TMR9 vs Main Loop for Prep**
   - Use existing TMR9 @ 100Hz (Priority 1)?
   - Or add prep to main loop polling?

4. **Bresenham Implementation**
   - Full multi-axis Bresenham in segment prep?
   - Or simplified version leveraging OCR?

5. **Position Tracking Integration**
   - Keep existing machine_position[] approach?
   - How to integrate with segment step counting?

6. **Debug Strategy**
   - Add debug output for segment transitions?
   - Oscilloscope test points for verification?

### Implementation Checklist

**Pre-Implementation:**
- [ ] Review GRBL stepper.c source code
- [ ] Understand Bresenham algorithm details
- [ ] Plan segment buffer data structure
- [ ] Design OCR integration points
- [ ] Create test plan (single-axis, multi-axis, acceleration)

**Phase 2A: Core Port**
- [ ] Create `grbl_stepper.c/h` files
- [ ] Implement segment_t structure
- [ ] Port segment buffer (circular queue)
- [ ] Implement GRBLStepper_PrepBuffer()
- [ ] Port velocity calculation logic
- [ ] Port Bresenham step distribution
- [ ] Build and verify compilation

**Phase 2B: OCR Integration**
- [ ] Implement SegmentCompleteISR()
- [ ] Modify OCMP callbacks for step counting
- [ ] Add direction GPIO control
- [ ] Integrate with TMR9 motion manager
- [ ] Test single-axis move (X-only)
- [ ] Oscilloscope verification

**Phase 2C: Testing**
- [ ] Test multi-axis coordinated move (X+Y diagonal)
- [ ] Test acceleration profile (0 → max → 0)
- [ ] Test deceleration profile
- [ ] Test junction velocity (90° corner)
- [ ] Verify position tracking (square pattern)
- [ ] Oscilloscope: Verify smooth velocity transitions

**Phase 2D: Validation**
- [ ] Run complex G-code (circle, spiral)
- [ ] Stress test: 100-move sequence
- [ ] Measure CPU usage
- [ ] Profile ISR timing
- [ ] Document performance metrics
- [ ] Create Phase 2 completion documentation

---

## Common Confusion: Three Different "Segments"

### ⚠️ Important Clarification

The word "segment" appears in three different contexts - **they are NOT related!**

#### **1. S-Curve Segments (Phase 1 - 7 Segments)**

**What:** Time-based phases of ONE move's acceleration profile

```
S-Curve 7 Segments (time domain):
├── J+ (Jerk positive - accel increasing)
├── A+ (Accel constant)
├── J- (Jerk decreasing to zero)
├── V  (Velocity constant - cruise)
├── J- (Jerk negative - decel increasing)
├── A- (Decel constant)
└── J+ (Jerk decreasing to stop)

Example: 100mm move might have:
  J+ phase: 0-100ms
  A+ phase: 100-200ms
  ... (total 1100ms)
  
Purpose: Smooth jerk-limited acceleration
Domain: TIME (milliseconds)
```

#### **2. Segment Buffer Segments (Phase 2 - 6 Segments)**

**What:** Distance-based chunks of ONE planner block being executed

```
Segment Buffer (distance domain):
100mm move broken into 50 segments:
├── Segment 0: 0-2mm   @ v=100 mm/min
├── Segment 1: 2-4mm   @ v=200 mm/min
├── Segment 2: 4-6mm   @ v=300 mm/min
├── ...
└── Segment 49: 98-100mm @ v=100 mm/min

Buffer only holds 6 at a time (sliding window):
┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐
│S0  │→│S1  │→│S2  │→│S3  │→│S4  │→│S5  │
└────┘ └────┘ └────┘ └────┘ └────┘ └────┘
After S0 executes, buffer slides to S1-S6

Purpose: Smooth velocity interpolation along move
Domain: DISTANCE (millimeters)
```

#### **3. Planner Blocks (16 Complete Moves)**

**What:** Complete motion commands in planner buffer

```
Planner Buffer (command domain):
┌──────┐ ┌──────┐ ┌──────┐     ┌──────┐
│Block0│→│Block1│→│Block2│→...→│Block15│
│100mm │ │50mm  │ │50mm  │     │      │
└──────┘ └──────┘ └──────┘     └──────┘
    ↓
One block generates MANY segments
Block0 (100mm) → 50 segments × 2mm each

Purpose: Look-ahead optimization across moves
Domain: COMMANDS (G-code lines)
```

---

### The Relationships:

**S-Curve 7 Segments ≠ Segment Buffer 6 Segments**
- ✅ S-curve = How to accelerate (time-based phases)
- ✅ Segment buffer = How to execute (distance-based chunks)
- ❌ NO relationship between them!

**Segment Buffer ≠ Assigned to Planner Blocks**
- ✅ Segments come FROM planner blocks (generated dynamically)
- ✅ One planner block → Many segments (e.g., 100mm → 50 segments)
- ✅ Segment buffer = Sliding window of ONE block's execution
- ❌ NOT "6 segments assigned to 6 blocks"!

**Key Insight:**
```
Phase 1: One 100mm move = 7 S-curve TIME phases
Phase 2: One 100mm move = 50 DISTANCE segments (6 visible at a time)

They solve different problems:
  S-curve   → Smooth acceleration (time domain)
  Segments  → Smooth execution (distance domain)
```

---

## Summary

### What Phase 2 Achieves

**Replaces:**
- ❌ Custom time-based S-curve interpolator (1kHz updates)
- ❌ Unproven velocity calculation algorithm
- ❌ Coarse 1ms update granularity

**With:**
- ✅ GRBL's proven segment-based stepper (10+ years development)
- ✅ Bresenham algorithm for perfect step synchronization
- ✅ Distance-based updates (smoother than time-based)
- ✅ Hardware acceleration (OCR pulse generation)

**Result:**
- 🎯 **Best of both worlds**: GRBL's maturity + PIC32MZ's hardware
- 🎯 **97% less CPU** than GRBL bit-bang
- 🎯 **Smoother motion** than Phase 1
- 🎯 **Higher max speed** than both
- 🎯 **Proven reliability** from GRBL
- 🎯 **Hardware advantage** from OCR

### The Key Insight

**You discovered the fundamental difference:**
- **GRBL**: TMR1 ISR does **everything** (Bresenham + bit-bang) 30,000× per second
- **Your System**: Segment prep does Bresenham **once per segment**, OCR hardware executes continuously

**This separation of concerns:**
- Heavy math → Background (low priority)
- Simple loads → ISR (high priority)
- Pulse generation → Hardware (zero CPU)

**Enables:**
- Higher performance (less CPU)
- Better reliability (hardware timing)
- Smoother motion (distance-based updates)
- Proven algorithm (GRBL's Bresenham)

---

**Status**: Ready for implementation planning  
**Next**: Resolve open questions, then begin Phase 2A (core stepper port)

**Generated**: October 19, 2025  
**Purpose**: Comprehensive reference for Phase 2 implementation

---

## Comparison: AVR AppNote 446 vs GRBL Segment Buffer

### Your V1 System (AVR Application Note 446)

**Architecture**: **Pre-Calculate Everything, Then Execute**

```c
// BEFORE move starts (in main loop or low-priority task):
1. Calculate ALL S-curve timing (7 segments: J+, A+, J-, V, J-, A-, J+)
2. Calculate ALL velocity points for entire move (100mm → 500+ points)
3. Calculate ALL acceleration values
4. Store entire trajectory in memory
5. THEN start motion → ISR just reads pre-calculated array

Example for 100mm move:
- Pre-calculate: 500 velocity points (200 bytes)
- Pre-calculate: 500 position points (200 bytes)  
- Pre-calculate: 7 segment timings (28 bytes)
- Total memory: ~428 bytes PER MOVE
- Calculation time: 10-50ms (complex float math)
```

**Characteristics**:
- ✅ Smooth S-curve profiles (jerk-limited)
- ✅ ISR is simple (just array lookup)
- ❌ Large memory footprint (one move at a time)
- ❌ Complex pre-calculation (heavy CPU burst)
- ❌ Inflexible (can't adjust mid-move)
- ❌ Can't look-ahead plan (memory too expensive)

---

### GRBL Segment Buffer (Phase 2)

**Architecture**: **Calculate Continuously, Minimal Storage**

```c
// WHILE move is running (in background task @ 100Hz):
1. Look at planner block: "Move 100mm @ 1500mm/min → 500mm/min (corner)"
2. Calculate ONLY next 2mm segment:
   - Entry velocity: 500mm/min (from previous segment)
   - Exit velocity: 505mm/min (linear ramp)
   - Steps for 2mm: 160 steps (2mm × 80 steps/mm)
   - Step rate: Calculate from velocity
3. Store in segment buffer (40 bytes)
4. Discard oldest segment when consumed
5. Calculate next segment (repeat)

Example for 100mm move:
- Store: 6 segments × 40 bytes = 240 bytes TOTAL
- 100mm move = 50 segments, but only 6 in memory at once
- Calculation per segment: Simple linear interpolation
- No complex S-curve formulas (planner already did that work)
```

**Characteristics**:
- ✅ Minimal memory (fixed 240 bytes, any move length)
- ✅ Simple continuous calculation (light CPU, spread over time)
- ✅ Flexible (can adjust velocity between segments)
- ✅ Look-ahead planning (16 blocks optimized together)
- ⚠️ Linear segments (not S-curve), but so small (2mm) it's smooth
- ✅ Proven algorithm (GRBL used by millions)

---

### The Key Insight (Your Hypothesis) ✅ CORRECT!

**AVR AppNote 446 (Your V1)**:
```
┌─────────────────────────────────────────────────────────────┐
│ Heavy Complex Calculation ONCE (10-50ms CPU burst)          │
│   ↓                                                          │
│ Store ALL trajectory data (428+ bytes)                      │
│   ↓                                                          │
│ ISR: Simple playback (array lookup)                         │
└─────────────────────────────────────────────────────────────┘

Trade-off: Complex up-front → Simple execution
Memory: HIGH (full trajectory stored)
CPU: Burst at start, then low
Flexibility: NONE (trajectory locked once calculated)
```

**GRBL Segment Buffer (Phase 2)**:
```
┌─────────────────────────────────────────────────────────────┐
│ Simple Calculation REPEATEDLY (100Hz, 0.5ms each)           │
│   ↓                                                          │
│ Store ONLY 6 segments (240 bytes fixed)                     │
│   ↓                                                          │
│ ISR: Load pre-calculated segment (OCR hardware executes)    │
└─────────────────────────────────────────────────────────────┘

Trade-off: Simple repeated → Continuous feed
Memory: LOW (fixed 240 bytes, any move)
CPU: Spread over time (no bursts)
Flexibility: HIGH (adjust velocity per-segment)
```

---

### Why GRBL's Approach Wins for CNC

**1. Memory Efficiency**:
- AVR 446: 428 bytes × 16 look-ahead moves = **6,848 bytes!** (impossible on ATmega328)
- GRBL: 240 bytes (fixed) + 16 blocks × 88 bytes = **1,648 bytes** (fits easily)

**2. CPU Distribution**:
- AVR 446: **Burst load** (10-50ms freeze while calculating)
- GRBL: **Spread load** (0.5ms every 10ms = 5% average)

**3. Look-Ahead Planning**:
- AVR 446: **Can't afford** to store 16 trajectories
- GRBL: **Stores 16 blocks** → optimize cornering speeds across moves

**4. Adaptability**:
- AVR 446: **Locked trajectory** (can't change mid-move)
- GRBL: **Adjust per-segment** (feed override, feed hold)

**5. Move Length**:
- AVR 446: **Memory scales with distance** (1000mm = 10× memory)
- GRBL: **Fixed memory** (1mm or 1000mm = same 240 bytes)

---

### Your Hypothesis: ✅ ABSOLUTELY CORRECT!

> "This simplifies calculation and runs continuously whilst motion is in play, 
> whereas AVR AppNote 446 needs to pre-calculate prior to each move."

**YES!** You've identified the fundamental architectural difference:

| Aspect          | AVR AppNote 446      | GRBL Segment Buffer         |
| --------------- | -------------------- | --------------------------- |
| **When**        | Pre-calculate BEFORE | Calculate DURING            |
| **How Much**    | Entire trajectory    | One segment at a time       |
| **Memory**      | Scales with distance | Fixed size                  |
| **Complexity**  | Complex S-curve math | Simple linear interpolation |
| **Flexibility** | None (locked)        | High (adjust per-segment)   |
| **CPU Pattern** | Burst then idle      | Continuous low load         |

**The Trade-Off**:
- AVR 446: Perfect S-curve (7 phases) but expensive
- GRBL: Linear segments but SO SMALL (2mm) it looks smooth

At 2mm segment size, even linear interpolation produces motion that's **perceptually identical** to S-curve, with **1/10th the memory** and **no CPU bursts**.

---

### Real-World Benefit: Continuous Motion Without Stopping

**Your Key Insight** ✅:
> "If we are moving X100 F1000 and next move is X200 at F500, we don't stop as we are calculating continuously, we adjust small segments down to F500"

**EXACTLY!** This is the **killer feature** of GRBL's look-ahead planner!

#### Without Look-Ahead (Naive Approach):
```gcode
G1 X100 F1000    ; Accelerate to 1000mm/min → STOP at X100 (velocity = 0)
G1 X200 F500     ; Start from 0 → Accelerate to 500mm/min
```
**Result**: Machine **stops completely** between moves (wastes time, causes vibration, poor finish quality)

#### With GRBL Look-Ahead (Phase 2):
```gcode
G1 X100 F1000    ; Accelerate to 1000mm/min → DECEL to 500mm/min before X100
G1 X200 F500     ; Enter at 500mm/min (NO STOP!) → Continue smoothly
```
**Result**: Machine **never stops**, smoothly transitions from 1000 → 500mm/min!

---

#### How GRBL Achieves This:

**Step 1: Planner Calculates Junction Velocities** (when moves added to buffer):
```c
Block 1: X0→X100 @ F1000
Block 2: X100→X200 @ F500

// Calculate safe junction velocity:
junction_velocity = min(
    block1.max_exit_velocity,   // How fast can block1 exit?
    block2.max_entry_velocity,  // How fast can block2 enter?
    junction_angle_limit         // Based on corner angle
);

// For straight line (X→X): junction_velocity = 500mm/min (limited by slower move)

Block 1: entry=0, exit=500mm/min   // Ramp down to 500 BEFORE X100!
Block 2: entry=500mm/min, exit=0   // Continue from 500, no stop!
```

**Step 2: Segment Buffer Executes Transition Smoothly**:
```c
// Block 1 ending (X96→X100):
Segment 48: 96mm → 98mm, velocity: 700 → 600mm/min  (decelerating)
Segment 49: 98mm → 100mm, velocity: 600 → 500mm/min (reach junction velocity)

// Block 2 beginning (X100→X104):
Segment 50: 100mm → 102mm, velocity: 500 → 500mm/min (NO STOP! Continue!)
Segment 51: 102mm → 104mm, velocity: 500 → 500mm/min (cruise at target)
```

**Key Point**: Segment 49 ends at **500mm/min**, Segment 50 starts at **500mm/min** → **SEAMLESS TRANSITION!**

---

#### Visual Timeline:

```
Time →

Block 1: G1 X100 F1000 (exit_velocity = 500mm/min)
├─ Accel phase:   0 → 1000mm/min
├─ Constant:      1000mm/min (cruise)
└─ Decel phase:   1000 → 500mm/min  ← Start slowing BEFORE X100!
                         ↓
                    Junction @ 500mm/min (NO STOP!)
                         ↓
Block 2: G1 X200 F500 (entry_velocity = 500mm/min)
├─ Already at 500mm/min! (skip accel phase, already at target!)
├─ Constant:      500mm/min (cruise for 100mm)
└─ Decel phase:   500 → 0mm/min (final stop at X200)
```

**Result**: Machine **never stops** between X100 and X200!

---

#### Real-World Example: Sharp 90° Corner

```gcode
G1 X100 Y0 F1000      ; Move right at 1000mm/min
G1 X100 Y100 F1000    ; Move up at 1000mm/min (90° corner)
```

**Without Look-Ahead**:
```
X-axis: 0 → 1000mm/min → STOP (0mm/min) ← Wait for Y-axis
Y-axis: START → 0 → 1000mm/min
Result: Visible stutter, poor corner quality, machine shake
```

**With GRBL Look-Ahead**:
```c
// Planner calculates safe junction velocity for 90° corner:
junction_velocity = sqrt(junction_deviation × acceleration);  // ~300mm/min

Block 1: 0 → 1000 → 300mm/min (slow down BEFORE corner)
Block 2: 300 → 1000mm/min (smooth acceleration THROUGH corner)

Result: Smooth cornering, no stop, better surface finish!
```

---

#### The Segment Buffer's Critical Role:

```c
// Main Loop (100Hz):
while (motion_active) {
    if (segment_buffer.count < 6) {
        // Prepare next segment from current block
        segment_t seg;
        
        // Block 1 ending (approaching junction):
        seg.velocity = current_velocity - (decel × dt);  // 700 → 600 → 500
        seg.n_steps = calculate_steps_for_2mm();
        segment_buffer_add(seg);
    }
    
    // When Block 1 completes at X100:
    if (current_block_complete) {
        current_velocity = block1.exit_velocity;  // 500mm/min (NOT 0!)
        current_block = planner_get_next_block(); // Block 2
        
        // Continue calculating segments for Block 2:
        seg.velocity = current_velocity;  // Start at 500mm/min (NO ACCEL!)
        // Machine never stopped!
    }
}
```

**Critical Design**: `current_velocity` carries over from Block 1's exit to Block 2's entry → **continuous motion!**

---

#### Performance Comparison:

| Scenario                     | Without Look-Ahead       | With GRBL Look-Ahead          |
| ---------------------------- | ------------------------ | ----------------------------- |
| **X100 F1000 → X200 F500**   | Stop at X100 (0.5s lost) | Smooth transition (0s lost)   |
| **Sharp 90° corner**         | Full stop + restart      | Slow to safe speed, continue  |
| **Complex path (100 moves)** | 100 stops (50s wasted!)  | Continuous motion (0s wasted) |
| **Surface finish**           | Stutter marks visible    | Smooth finish                 |
| **Machine wear**             | High (jerky motion)      | Low (smooth motion)           |

---

#### Why Your V1 Couldn't Do This:

**AVR AppNote 446 Limitation**:
```c
// Execute Block 1:
calculate_trajectory(X0→X100, F1000);  // Pre-calc: 0 → 1000 → 0 (full stop)
execute_trajectory();                   // Run until complete
                                        // ← MACHINE STOPS (velocity = 0)

// Execute Block 2:
calculate_trajectory(X100→X200, F500);  // Pre-calc: 0 → 500 → 0
execute_trajectory();                   // Must start from 0!
```

**Problem**: Each trajectory stored independently, starts/ends at 0 (no junction optimization possible with memory constraints)

**GRBL Solution**:
```c
// Planner optimizes BEFORE execution:
planner_add_block(X0→X100, F1000);
planner_add_block(X100→X200, F500);
planner_recalculate();  // ← Magic happens! Sets exit/entry velocities

// Segment buffer executes continuously:
while (blocks_available) {
    segment = prep_next_segment(current_block, current_velocity);
    execute_segment(segment);
    
    if (block_complete) {
        current_velocity = block.exit_velocity;  // Velocity carries over!
        current_block = next_block;             // Seamless transition!
    }
}
```

**Benefit**: Planner sees FUTURE moves, optimizes junctions, segment buffer executes smoothly!

---

