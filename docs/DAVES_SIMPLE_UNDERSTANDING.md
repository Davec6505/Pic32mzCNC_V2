# Dave's Simple Understanding of GRBL Motion Planning

**Date**: October 19, 2025  
**Purpose**: Document the breakthrough moment when complex theory became simple practice

---

## The Problem I Was Trying to Solve

**My V1 CNC controller worked BUT**:
- ❌ Machine **STOPPED between every move**
- ❌ G1 X100 F1000 → G1 X200 F500 = **full stop at X100, then restart**
- ❌ **I KNEW it should flow smoothly** but couldn't figure out HOW
- ❌ Academic papers made it sound impossibly complex
- ❌ Spent months "pulling my hair out" trying different approaches

**What I wanted**: Machine should smoothly slow down from 1000mm/min to 500mm/min WITHOUT stopping!

---

## The Breakthrough Question

> *"If we are moving X100 F1000 and next move is X200 at F500, we don't stop as we are calculating continuously, we adjust small segments down to F500, is that it?"*

**The Answer**: **YES!** That's literally the entire concept!

---

## The Simple Truth (In Plain English)

**What GRBL Does** (just four steps):

1. **Look ahead** - Buffer multiple G-code commands (16 moves)
2. **Calculate junctions** - Figure out safe speed through each junction
3. **Break into chunks** - Split current move into tiny segments (2mm each)
4. **Adjust smoothly** - Ramp velocity up/down between entry/exit speeds

**That's it.** Everything else is implementation details.

---

## The Car Driving Analogy (My Mental Model)

### Bad Way (My V1):
```
Stop Sign Driving:
├─ Accelerate to 60mph
├─ See stop sign
├─ STOP completely (0mph) ← Annoying!
├─ Wait
├─ Accelerate to 30mph  
├─ See next stop sign
└─ STOP completely (0mph) ← Wasteful!

Result: Jerky, slow, frustrating
```

### Good Way (GRBL):
```
Smooth Traffic Flow:
├─ Accelerate to 60mph
├─ See slow zone ahead (30mph)
├─ Start slowing BEFORE zone (60→50→40→30)
├─ Enter zone at 30mph (NO STOP!) ← Smooth!
├─ Continue at 30mph
└─ Eventually stop when journey ends

Result: Smooth, fast, comfortable
```

**GRBL is just "smooth traffic flow" for CNC motion!**

---

## The Three-Part System (Simplified)

### Part 1: The Planner (The GPS)
- **Job**: Look at upcoming moves, calculate safe junction speeds
- **When**: Runs in background when G-code arrives
- **Output**: "Move 1 should end at 500mm/min"
- **Analogy**: GPS saying "slow down, curve ahead"

### Part 2: The Segment Buffer (The Cruise Control)
- **Job**: Break current move into 2mm chunks, adjust velocity smoothly
- **When**: Runs continuously (100 times/second)
- **Output**: "Next 2mm: run at 625mm/min, then 600mm/min..."
- **Analogy**: Cruise control adjusting throttle as you approach curve

### Part 3: The Hardware (The Engine)
- **Job**: Generate physical step pulses at requested rate
- **When**: Runs in hardware (zero CPU)
- **Output**: Step pulses to stepper motors
- **Analogy**: Engine responding to throttle

---

## Visual Example (My Understanding)

```gcode
G1 X100 F1000    ; Move 1: Go to X100 @ 1000mm/min
G1 X200 F500     ; Move 2: Go to X200 @ 500mm/min
```

### What Happens:

**Step 1: Planner Sees Both Moves**
```
Planner: "Oh! Move 1 is at 1000, Move 2 is at 500"
Planner: "Move 1 should END at 500 so Move 2 can START at 500"
Planner: "No stop needed! Just slow down smoothly!"

Block 1: entry=0mm/min, exit=500mm/min ✓
Block 2: entry=500mm/min, exit=0mm/min ✓
```

**Step 2: Segment Buffer Executes**
```
Move 1 ending:
Segment 46: 92→94mm, velocity 800→700mm/min (slowing down)
Segment 47: 94→96mm, velocity 700→600mm/min
Segment 48: 96→98mm, velocity 600→550mm/min
Segment 49: 98→100mm, velocity 550→500mm/min (reach junction!)

Move 2 beginning:
Segment 50: 100→102mm, velocity 500→500mm/min (NO STOP! Continue!)
Segment 51: 102→104mm, velocity 500→500mm/min (cruise)
...
```

**Result**: Machine velocity goes `...→800→700→600→550→500→500→500...` (NEVER zero!)

---

## Why Was This So Hard to Find?

**The Frustration** (every hobbyist feels this):

1. **Academic Papers** - Full of equations, Bézier curves, matrix algebra
   - Reality: You just need smooth velocity transitions!

2. **Industrial Docs** - Assume $10,000 motion controllers
   - Reality: DIY CNC with $50 microcontroller!

3. **GRBL Source Code** - 10,000+ lines, minimal comments
   - Reality: Core concept is simple!

4. **Forums** - "Just implement trapezoidal profiling with junction deviation!"
   - Reality: What does that MEAN?!

**The Missing Piece**: Someone to translate academic math to practical implementation!

---

## The Answer to My Key Question

> *"Where does max acceleration and velocity calculation take place?"*

**Answer**: **In the PLANNER, not the segment buffer!**

### Division of Labor:

**GRBL Planner** (Strategic Layer):
- Calculates: Max velocity, max acceleration, junction speeds, entry/exit velocities
- When: Once when G-code added to buffer
- Priority: Low (background task, can take 10-50ms)

**Segment Buffer** (Tactical Layer):
- Calculates: Current velocity (interpolate), steps for 2mm, step rate
- When: Continuously (every 10ms)
- Priority: Medium (predictable timing)

**OCR Hardware** (Execution Layer):
- Calculates: Nothing! Just executes!
- When: Hardware timing (zero CPU)
- Priority: Highest (but zero CPU load)

### Summary Table:

| Component      | Calculates                                                          | Uses Planner's Data         |
| -------------- | ------------------------------------------------------------------- | --------------------------- |
| Planner        | ✅ Max velocity<br>✅ Max accel<br>✅ Junctions<br>✅ Entry/exit speeds | G-code + $120-$123 settings |
| Segment Buffer | ✅ Current velocity<br>✅ Steps per segment<br>✅ Step rate            | Planner's entry/exit speeds |
| OCR Hardware   | ❌ Nothing                                                           | Segment's steps + rate      |

**Key Insight**: 
- Planner does **HARD math ONCE** in background (low priority)
- Segment buffer does **SIMPLE math CONTINUOUSLY** (medium priority)  
- Hardware does **NO math**, just executes (zero CPU)

---

## My V1 vs GRBL (The Comparison)

### My V1 (AVR AppNote 446):

**Approach**: Pre-calculate entire trajectory, store in memory, execute

```c
// Execute Move 1:
calculate_trajectory(X0→X100, 1000mm/min);  // Calculate: 0→1000→0
store_trajectory(500 points);                // Store: 428 bytes
execute_trajectory();                        // Execute until complete
// ← Machine at 0mm/min (STOPPED!)

// Execute Move 2:
calculate_trajectory(X100→X200, 500mm/min); // Calculate: 0→500→0
store_trajectory(500 points);                // Store: 428 bytes
execute_trajectory();                        // Execute until complete
```

**Problems**:
- ❌ Each move starts/ends at 0 (no junction optimization)
- ❌ Memory scales with distance (1000mm = 10× memory!)
- ❌ Can't look ahead (16 moves × 428 bytes = 6,848 bytes = doesn't fit!)
- ❌ CPU burst during calculation (10-50ms freeze)

---

### GRBL (Phase 2):

**Approach**: Store strategic data, calculate tactical segments continuously

```c
// Add moves to planner:
GRBLPlanner_BufferLine(X0→X100, 1000mm/min);   // Store: 88 bytes
GRBLPlanner_BufferLine(X100→X200, 500mm/min);  // Store: 88 bytes

// Planner optimizes:
GRBLPlanner_Recalculate();  // Sets: Block1.exit=500, Block2.entry=500

// Segment buffer executes continuously:
while (motion_active) {
    segment = prep_next_segment();  // Calculate next 2mm
    execute_segment(segment);       // Hardware executes
    
    if (block_complete) {
        velocity = block.exit_velocity;  // Carry velocity forward!
        block = get_next_block();        // Seamless transition!
    }
}
```

**Benefits**:
- ✅ Look ahead (16 moves × 88 bytes = 1,408 bytes = fits!)
- ✅ Fixed memory (240 bytes segment buffer, any move length)
- ✅ Smooth junctions (velocity carries forward)
- ✅ Spread CPU load (0.5ms every 10ms = 5% average)

---

## Memory Comparison (The Numbers)

### My V1 (Store Full Trajectories):
```
1 move:  428 bytes (velocity + position arrays)
16 moves: 428 × 16 = 6,848 bytes ← Doesn't fit in ATmega328 (2KB RAM)!
```

### GRBL (Store Strategic + Tactical):
```
Planner: 16 blocks × 88 bytes = 1,408 bytes (strategic)
Segments: 6 segments × 40 bytes = 240 bytes (tactical)
Total: 1,648 bytes ← Fits easily! Plus look-ahead planning!
```

**GRBL uses 1/4 the memory AND enables look-ahead planning!**

---

## The One-Sentence Summary

**GRBL Motion Planning**:
> *"Look at multiple moves ahead, figure out how fast you can safely go through each junction, then break the current move into tiny chunks that smoothly transition from one speed to the next, all while hardware generates actual step pulses so CPU stays free."*

**That's the entire concept.** Everything else is implementation details.

---

## My Learning Journey (Documented)

**Stage 1: The Problem**
- V1 worked but stopped between moves
- I knew it should be smooth but couldn't figure out how
- Academic papers were incomprehensible

**Stage 2: The Questions**
- "What are we doing with steppers in Phase 2?"
- "How do continuous pulses fit in?"
- "Do segments fit into blocks?"

**Stage 3: The Breakthrough**
- "Ah Ha! Same as GRBL TMR1 ISR but with hardware pulse generation!"
- "We don't stop - we adjust small segments down to F500!"

**Stage 4: The Understanding**
- Don't pre-calculate everything → Calculate continuously
- Don't store full trajectories → Store strategic data + small window
- Don't stop between moves → Carry velocity forward

**Stage 5: Relief**
- "This is exactly the issue I had with V1 - I just didn't know how to achieve this!"
- Simple concept, just needed someone to explain it clearly

---

## Why This Matters (Personal Reflection)

**What I Learned**:
- Complex problems often have simple solutions
- Academic explanations obscure simple concepts
- Sometimes you just need someone to translate jargon to English
- The right mental model (car driving) makes everything click

**What I'll Remember**:
- GRBL = "Smooth traffic flow" for CNC
- Planner = GPS (strategic)
- Segment buffer = Cruise control (tactical)
- Hardware = Engine (execution)

**Moving Forward**:
- Phase 2 implementation won't be scary anymore
- I understand WHY each part exists
- I can explain it to others in simple terms
- I have a reference document (this!) for when I forget details

---

## Final Thoughts

> *"Why is this information so hard to find? Why do people explain such a simple theory in such a convoluted way?"*

**The Answer**: 
- Experts forget what it's like to be confused
- Academic focus is on proving optimality, not explaining intuition
- Forums assume knowledge you don't have yet

**The Solution**: 
- This document (written in my words, with my analogies)
- Captures the "Ah Ha!" moment while it's fresh
- Reference for future me (and anyone else struggling with this!)

---

**Date Written**: October 19, 2025  
**Status**: ✅ Understanding achieved! Ready for Phase 2 implementation!  
**Next**: Port GRBL stepper code with confidence (I know WHY I'm doing it now!)

