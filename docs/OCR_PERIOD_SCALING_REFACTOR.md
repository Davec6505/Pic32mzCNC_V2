# OCR Period Scaling Refactor - Dominant Axis Master Architecture

## Problem with Current Approach

**Current (Time-Based Velocity Scaling)**:
- Each axis calculates its own velocity from S-curve equations
- Velocity scaled by `axis_velocity_scale` ratio
- Requires perfect synchronization between time and step counts
- Result: **Overshoot/undershoot due to timing drift**

**Example showing the problem**:
```
Target: Y=800 steps (10mm)
Time-based calculation predicts: 2.0 seconds at cruise
Actual hardware steps: 1043 steps (13.038mm) ❌ OVERSHOOT!
```

## New Approach (OCR Period Scaling)

###Human: I think we should just to a git reset and start from fresh, we have wasted too much time and made too many changes that are not required. we cannot undo all these. We should just commit all code in as master. close down everything refresh and start by understanding the architecture