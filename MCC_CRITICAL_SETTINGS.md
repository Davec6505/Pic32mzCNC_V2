# MCC CRITICAL CONFIGURATION SETTINGS
**DO NOT REGENERATE MCC CODE WITHOUT APPLYING THESE SETTINGS!**

This file documents critical MCC Harmony configuration settings that MUST be preserved when regenerating peripheral code.

---

## ⚠️ TMR1 - Arc Generator Timer (October 25, 2025)

**Purpose**: Generates arc segments @ 10ms intervals for G2/G3 circular interpolation

**MCC GUI Configuration:**
```
Peripheral: TMR1
├─ Enable Timer: ✓ YES
├─ Prescaler: 1:64 (TCKPS = 2)
├─ Period Register (PR1): 7812 decimal (0x1E84 hex)
├─ Timer Clock Source: PBCLK3 (50 MHz)
├─ Interrupt: 
│  ├─ Enable Interrupt: ✓ YES
│  ├─ Priority: 1 (CRITICAL - different SRS from TMR9!)
│  └─ Subpriority: 1
└─ Callback: ArcGenerator_TMR1Callback (registered in motion_buffer.c)
```

**Calculated Frequency:**
```
Timer Clock = PBCLK3 / Prescaler = 50 MHz / 64 = 781,250 Hz
Interrupt Rate = Timer Clock / Period = 781,250 / 7,812 = 100 Hz (10ms)
```

**Why These Values:**
- **10ms rate**: Prevents planner buffer overflow during arc generation
- **Priority 1 Sub 1**: Different Shadow Register Set from TMR9 (Priority 1 Sub 0)
- **Period 7812**: Gives exactly 100 Hz with 1:64 prescaler

**Code Dependencies:**
- `srcs/motion/motion_buffer.c` - ArcGenerator_TMR1Callback() expects 10ms rate
- `srcs/motion/motion_buffer.c` - LED2 toggle counter set to 10 (100ms blink)

**If MCC Overwrites:**
1. Open MCC GUI
2. Navigate to Peripherals → TMR1
3. Set Prescaler = 1:64
4. Set Period = 7812
5. Set Priority = 1, Subpriority = 1
6. Regenerate code
7. Verify `plib_tmr1.c` has: `PR1 = 0x1E84;` and `T1CONbits.TCKPS = 2;`

---

## ⚠️ TMR9 - Motion Manager Timer (Existing)

**Purpose**: Prepares motion segments from planner buffer @ 1kHz

**MCC GUI Configuration:**
```
Peripheral: TMR9
├─ Enable Timer: ✓ YES
├─ Prescaler: 1:64
├─ Period Register (PR9): 780 decimal (0x030C hex)
├─ Interrupt:
│  ├─ Priority: 1
│  └─ Subpriority: 0 (CRITICAL - different SRS from TMR1!)
```

**Calculated Frequency:**
```
Timer Clock = 50 MHz / 64 = 781,250 Hz
Interrupt Rate = 781,250 / 780 = 1001.6 Hz (~1kHz)
```

**Why Subpriority 0:**
- TMR1 uses Subpriority 1 (different Shadow Register Set)
- Prevents register corruption when both ISRs run

---

## ⚠️ EVIC Interrupt Priorities (October 24, 2025)

**File**: `srcs/config/default/peripheral/evic/plib_evic.c`

**CRITICAL MANUAL EDITS (Lines 59, 64):**
```c
// Line 59 - TMR1 interrupt (Arc Generator)
IPC1SET = 0x4U | 0x1U;  // Priority 1, Subpriority 1

// Line 64 - TMR9 interrupt (Motion Manager)
IPC10SET = 0x4U | 0x0U;  // Priority 1, Subpriority 0
```

**Why Manual Edit Required:**
- MCC generates subpriority in same register write
- Must ensure TMR1 and TMR9 have DIFFERENT subpriorities
- Different subpriorities = different Shadow Register Sets = no corruption

**After MCC Regeneration:**
1. Open `srcs/config/default/peripheral/evic/plib_evic.c`
2. Find `IPC1SET` (TMR1) - verify ends with `| 0x1U`
3. Find `IPC10SET` (TMR9) - verify ends with `| 0x0U`

---

## 📋 Pre-Regeneration Checklist

Before clicking "Generate" in MCC:

- [ ] TMR1 Period = 7812 (0x1E84)
- [ ] TMR1 Prescaler = 1:64 (TCKPS=2)
- [ ] TMR1 Priority = 1, Subpriority = 1
- [ ] TMR9 Period = 780 (0x030C)
- [ ] TMR9 Priority = 1, Subpriority = 0

After MCC generation:

- [ ] Verify `plib_tmr1.c` has `PR1 = 0x1E84;`
- [ ] Verify `plib_evic.c` line 59: `IPC1SET = 0x4U | 0x1U;`
- [ ] Verify `plib_evic.c` line 64: `IPC10SET = 0x4U | 0x0U;`
- [ ] Rebuild and test arc generation (G2/G3 commands)

---

## 🔧 Related Documentation

- `docs/DOMINANT_AXIS_HANDOFF_OCT24_2025.md` - Shadow Register Set collision fix
- `docs/ATOMIC_TRANSITION_OCT24_2025.md` - ISR state tracking
- `srcs/motion/motion_buffer.c` - Arc generator implementation

---

## $28 - Limit Switch Inversion Mask

**Purpose**: Configure whether limit switches are Normally Open (NO) or Normally Closed (NC)

**Format**: Bitmask where each bit represents one axis
- Bit 0 (0x01): X-axis
- Bit 1 (0x02): Y-axis  
- Bit 2 (0x04): Z-axis
- Bit 3 (0x08): A-axis

**Logic**:
- Bit SET (1): Switch is Normally Closed (NC) - invert pin reading
- Bit CLEAR (0): Switch is Normally Open (NO) - use pin reading as-is

**Examples**:
```gcode
$28=0    ; All axes use NO switches (default)
$28=7    ; X/Y/Z use NC switches (0b0111)
$28=4    ; Only Z uses NC switch (0b0100)
$28=15   ; All axes use NC switches (0b1111)
```

**Hardware**:
- NO switch: Connect between GPIO and VCC, enable internal pull-down
- NC switch: Connect between GPIO and GND, enable internal pull-up

---

**Last Updated**: October 25, 2025 (Evening)
**Critical**: DO NOT delete this file! Pin to project root for visibility.
