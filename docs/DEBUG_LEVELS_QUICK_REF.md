# Debug Levels Quick Reference (October 25, 2025)

## Overview

The tiered debug system allows selective output to prevent serial flooding while debugging specific subsystems.

## Debug Levels

| Level | Name      | Use Case                          | Typical Output Rate |
|-------|-----------|-----------------------------------|---------------------|
| 0     | NONE      | Production (no debug)             | 0 msg/sec          |
| 1     | CRITICAL  | Critical errors only              | <1 msg/sec         |
| 2     | PARSE     | G-code parsing events             | ~10 msg/sec        |
| 3     | PLANNER   | Planner operations                | ~10 msg/sec        |
| 4     | STEPPER   | Stepper state machine             | ~10 msg/sec        |
| 5     | SEGMENT   | Segment execution                 | ~50 msg/sec        |
| 6     | VERBOSE   | High-frequency events             | ~100 msg/sec       |
| 7     | ALL       | Everything (extreme verbosity)    | >1000 msg/sec      |

## Build Commands

```bash
# Production build (no debug output)
make all

# Debug at specific level
make all DEBUG_MOTION_BUFFER=1   # Critical errors only
make all DEBUG_MOTION_BUFFER=2   # Parse + Critical
make all DEBUG_MOTION_BUFFER=3   # Planner + Parse + Critical (recommended!)
make all DEBUG_MOTION_BUFFER=4   # Stepper + Planner + Parse + Critical
make all DEBUG_MOTION_BUFFER=5   # Segment + all above
make all DEBUG_MOTION_BUFFER=6   # Verbose (high output rate!)
make all DEBUG_MOTION_BUFFER=7   # ALL (warning: very high output!)

# Legacy backward compatibility
make all DEBUG_MOTION_BUFFER=1   # Maps to level 3 (PLANNER)
```

## Message Categories by Level

### Level 1 (CRITICAL)
- `[GRBL] BUFFER FULL!` - Motion buffer overflow (retry mechanism active)
- Fatal errors that prevent operation

### Level 2 (PARSE)
- `[PARSE] 'G1X10Y20'` - Shows each command being parsed
- G-code syntax errors

### Level 3 (PLANNER - RECOMMENDED)
- `[PLAN] pl.pos=(X,Y) tgt=(X,Y)` - Block planning details
- `[JUNC] cos=... vj^2=...` - Junction velocity calculations
- `[GRBL] REJECTED zero-length` - Zero-step moves filtered

### Level 4 (STEPPER)
- `[STEPPER] prep_new_block: Got block` - New block fetched from planner
- `[STEPPER] prep_new_block: NULL from planner` - Planner empty
- `[STEPPER] Block complete` - Block execution finished

### Level 5 (SEGMENT)
- Segment preparation events
- OCR configuration changes

### Level 6 (VERBOSE)
- `[MOTION] mode=... is_motion=...` - Every command motion detection
- `[PLANNER] GetCurrentBlock: Buffer empty` - Frequent planner queries
- High-frequency state changes

### Level 7 (ALL)
- Every possible debug message
- WARNING: Can flood serial buffer (1000+ messages/sec)

## Recommended Levels for Common Debugging

### Circle Test Debugging (Level 3)
```bash
make all DEBUG_MOTION_BUFFER=3
```
**Output:**
- Shows each segment being planned (`[PLAN]`)
- Junction velocities between segments (`[JUNC]`)
- No flooding from high-frequency events

**Example:**
```
[PARSE] 'G1X9.511Y3.090F1000'
[PLAN] pl.pos=(10.000,0.000) tgt=(9.511,3.090) delta=(-31,198) steps=(31,198)
[JUNC] cos=-0.950084 vj^2=297620.3
ok
```

### Post-Completion State Machine Issue (Level 4)
```bash
make all DEBUG_MOTION_BUFFER=4
```
**Output:**
- All level 3 output
- Plus stepper state transitions
- Shows when blocks fetched/completed

**Example:**
```
[STEPPER] prep_new_block: Got block, mm=3.124 steps=(31,198,0,0)
... motion executes ...
[STEPPER] Block complete - block_active set to FALSE
[STEPPER] prep_new_block: NULL from planner (planner empty)
```

### Serial Buffer Issues (Level 2)
```bash
make all DEBUG_MOTION_BUFFER=2
```
**Output:**
- Shows each command received
- No planner/stepper spam

**Example:**
```
[PARSE] 'G1X10Y20'
ok
[PARSE] 'G1X20Y30'
ok
```

### Production Testing (Level 1)
```bash
make all DEBUG_MOTION_BUFFER=1
```
**Output:**
- Only critical errors
- Clean output for normal operation

## Code Usage Pattern

```c
// In your code:
#if DEBUG_MOTION_BUFFER >= DEBUG_LEVEL_PLANNER
    UGS_Printf("[PLAN] Block added to buffer\r\n");
#endif

#if DEBUG_MOTION_BUFFER >= DEBUG_LEVEL_CRITICAL
    UGS_Printf("[CRITICAL] Buffer overflow!\r\n");
#endif
```

## Tips

1. **Start with level 3** for most debugging - good balance of info vs spam
2. **Use level 1** for production to catch critical errors
3. **Avoid level 7** unless absolutely necessary - can overwhelm UART
4. **Level 4** is perfect for state machine debugging
5. **Level 2** is best for testing serial communication

## Backward Compatibility

Old makefiles using `DEBUG_MOTION_BUFFER=1` automatically map to **Level 3 (PLANNER)** to maintain expected output behavior from earlier testing.
