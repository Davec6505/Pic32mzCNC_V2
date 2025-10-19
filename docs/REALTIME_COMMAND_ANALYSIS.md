# Real-Time Command Handling Analysis - October 19, 2025

## User's Critical Insight

**Question**: "If we do this it will mean we will need to go to coretimer ISR for buffer surely otherwise we won't pick up estop command"

**Answer**: You're absolutely right! ✅

## The Problem with Blocking Loops

### Proposed Solution (WRONG - Would Block Real-Time Commands)
```c
void ExecuteMotion(void) {
    /* BLOCKING - BAD! */
    while (!MultiAxis_IsBusy() && MotionBuffer_HasData()) {
        motion_block_t block;
        MotionBuffer_GetNext(&block);
        MultiAxis_ExecuteCoordinatedMove(block.steps);
        
        /* This waits for motion to complete */
        while (MultiAxis_IsBusy()) {
            /* STUCK HERE! Can't check for E-stop, feed hold, status query */
        }
    }
}
```

**Why this is bad**:
- Main loop stuck in ExecuteMotion()
- Never returns to check `Serial_GetRealtimeCommand()`
- E-stop (Ctrl-X), feed hold (!), status (?) all ignored
- Safety-critical failure!

## Current Architecture (CORRECT - Non-Blocking)

### Main Loop Pattern
```c
while (true) {
    ProcessSerialRx();                              // Check serial data
    
    /* CRITICAL: Real-time command check EVERY iteration (~1kHz) */
    uint8_t realtime_cmd = Serial_GetRealtimeCommand();
    if (realtime_cmd != 0) {
        GCode_HandleControlChar(realtime_cmd);      // E-stop, feed hold, etc.
    }
    
    /* Process commands (non-blocking, fixed iterations) */
    for (uint8_t i = 0; i < 16; i++) {
        ProcessCommandBuffer();                      // Max 16 per loop
    }
    
    /* Execute motion (non-blocking, one block per call) */
    ExecuteMotion();                                 // Returns immediately
    
    APP_Tasks();                                     // Background tasks
    SYS_Tasks();                                     // Harmony drivers
}
```

**Why this works**:
- Loop runs at ~1kHz (1ms per iteration)
- Real-time commands checked EVERY iteration
- ExecuteMotion() returns immediately (doesn't wait for move to complete)
- If motion is busy, we skip and check again next iteration
- E-stop response time: <1ms ✅

## The Real Issue (Not What We Thought)

Looking at debug output again:
```
[PARSE] 'G1 Y0' -> motion=1
[DEBUG] Motion block: X=0 Y=-800 Z=0 (active: 010)
[DEBUG] Planned pos: X=10.000 Y=0.000 Z=0.000

[TOKEN] Line: 'G1X0' -> 2 tokens
ok
← NO [PROCBUF] MESSAGE ←

<Run|MPos:10.000,4.613,5.002|...>
<Idle|MPos:10.000,10.000,5.002|...>  ← Stopped at (10,10) not (0,0)!
```

**Analysis**:
1. G1 Y0 parsed and added to motion buffer (motion_count=5)
2. G1 X0 tokenized (enters command buffer)
3. Machine starts executing moves
4. ProcessCommandBuffer() runs but motion_count >= 5, threshold check may fail?
5. OR: Machine executes 3 moves, goes IDLE, but last 2 blocks never dequeued from motion buffer

**Hypothesis**: The issue isn't ExecuteMotion() logic, it's that G1 Y0 and G1 X0 were added to motion buffer but **motion buffer state became inconsistent**.

## Alternative Solutions (All Non-Blocking)

### Option 1: Timer-Based Command Processing (User's Suggestion)
```c
/* Core Timer ISR @ 10ms (100Hz) */
void CORETIMER_Handler(void) {
    /* Process one command from command buffer */
    if (!MotionBuffer_IsFull()) {
        ProcessCommandBuffer();  // One command only
    }
}

/* Main loop simplified */
while (true) {
    ProcessSerialRx();
    
    uint8_t realtime_cmd = Serial_GetRealtimeCommand();
    if (realtime_cmd != 0) {
        GCode_HandleControlChar(realtime_cmd);
    }
    
    ExecuteMotion();  // Still in main loop
    APP_Tasks();
    SYS_Tasks();
}
```

**Advantages**:
- ✅ Command processing happens in background (timer-driven)
- ✅ Main loop free to handle real-time commands
- ✅ More predictable timing

**Disadvantages**:
- ⚠️ Need to ensure ProcessCommandBuffer() is ISR-safe
- ⚠️ Need to protect motion buffer with critical sections
- ⚠️ More complex interrupt coordination

### Option 2: Increase Main Loop Iterations (Current Approach)
```c
/* Already implemented - just needs verification */
for (uint8_t i = 0; i < 16; i++) {
    ProcessCommandBuffer();  // Drain aggressively
}
```

**Advantages**:
- ✅ Simple, no ISR changes
- ✅ Already implemented
- ✅ Works for most cases

**Disadvantages**:
- ⚠️ Fixed iteration count may not be enough under some conditions
- ⚠️ If motion buffer fills, commands wait until next iteration

### Option 3: Adaptive Loop (Best of Both)
```c
/* Process commands until buffer full OR limit reached */
uint8_t count = 0;
while (count < 16 && !MotionBuffer_IsFull() && CommandBuffer_HasData()) {
    ProcessCommandBuffer();
    count++;
}
```

**Advantages**:
- ✅ Drains command buffer completely
- ✅ Respects motion buffer full condition
- ✅ Still has safety limit (16 iterations max)
- ✅ Non-blocking (returns after 16 iterations max)

**Disadvantages**:
- None! This is the sweet spot.

## Recommended Solution

**Modify main loop command processing**:

```c
/* Stage 2: Process Command Buffer → Motion Buffer (16 blocks)
 * ADAPTIVE PROCESSING: Drain command buffer until:
 * - Motion buffer is full (15/16 blocks), OR
 * - Command buffer is empty, OR
 * - Safety limit reached (16 iterations)
 */
uint8_t cmd_count = 0;
while (cmd_count < 16 && MotionBuffer_GetCount() < 15 && CommandBuffer_HasData())
{
    ProcessCommandBuffer();
    cmd_count++;
}
```

**Why this works**:
- ✅ Drains command buffer as fast as possible
- ✅ Respects motion buffer capacity (15/16 threshold)
- ✅ Still non-blocking (max 16 iterations ≈ 100µs)
- ✅ Real-time commands checked every loop (1kHz)
- ✅ No ISR changes needed
- ✅ Simple and predictable

## Debug Next Steps

1. **Add motion buffer count logging**:
   ```c
   UGS_Printf("[EXEC] Motion buffer count: %d\r\n", MotionBuffer_GetCount());
   ```

2. **Track which blocks are dequeued**:
   ```c
   UGS_Printf("[EXEC] Dequeued block: X=%ld Y=%ld Z=%ld\r\n", ...);
   ```

3. **Verify motion buffer state after all commands parsed**:
   - After G1 X0 tokenized, what is motion_count?
   - Are all 5 blocks (G0 Z0, G1 Y10, G1 X10, G1 Y0, G1 X0) in buffer?
   - Or are some blocks being executed before others are added?

## Timer-Based Solution (If Needed)

If adaptive loop doesn't solve it, implement timer-based processing:

```c
/* In configuration code */
void Setup_CommandProcessing_Timer(void) {
    /* Use Core Timer or TMR6 @ 100Hz (10ms) */
    CORETIMER_CallbackRegister(CommandProcessing_Handler, 0);
}

/* Timer ISR */
void CommandProcessing_Handler(uintptr_t context) {
    /* Process up to 4 commands per timer tick
     * 100Hz × 4 commands = 400 commands/sec max throughput */
    for (uint8_t i = 0; i < 4; i++) {
        if (MotionBuffer_GetCount() < 15 && CommandBuffer_HasData()) {
            ProcessCommandBuffer();
        }
    }
}
```

**Trade-off**: More complex but guarantees command processing even if main loop gets slow.

---

**Conclusion**: You're absolutely right about the E-stop concern! We need to keep the non-blocking pattern. The adaptive loop solution should fix the incomplete execution without compromising real-time response.
