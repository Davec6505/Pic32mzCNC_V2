# PIC32MZ CNC V2 - Coding Standards

## File Organization

### Variable Declaration Standard

**CRITICAL RULE**: All file-level variables (static or non-static) MUST be declared at the **top of each file** under a clear comment section.

**Required Comment Header:**
```c
// *****************************************************************************
// Local Variables
// *****************************************************************************
```

**Rationale:**
1. **ISR Access**: Interrupt Service Routines (ISRs) need access to file-scope variables
2. **Readability**: Easy to find all state variables in one place
3. **Maintenance**: Clear overview of module's data structures
4. **Debugging**: Quick identification of all shared state

### Example (Correct Pattern):

```c
// File: multiaxis_control.c

/* Include headers */
#include "definitions.h"
#include "motion_types.h"

/* Type definitions */
typedef struct {
    float velocity;
    uint32_t step_count;
} axis_state_t;

// *****************************************************************************
// Local Variables
// *****************************************************************************

// Per-axis motion state (accessed by TMR1 ISR @ 1kHz)
static volatile axis_state_t axis_state[NUM_AXES];

// Coordinated move parameters (accessed by TMR1 ISR @ 1kHz)
static motion_coordinated_move_t coord_move;

// Hardware configuration table
static const axis_hardware_t axis_hw[NUM_AXES] = {
    /* ... initialization ... */
};

// Enable flags for optional features
static bool driver_enable_state[NUM_AXES] = {false};

// *****************************************************************************
// Private Function Prototypes
// *****************************************************************************

static void TMR1_InterruptHandler(void);
static void calculate_scurve_profile(axis_id_t axis);

// *****************************************************************************
// Function Implementations
// *****************************************************************************

void SomePublicFunction(void) {
    /* ... */
}

static void TMR1_InterruptHandler(void) {
    // Can safely access axis_state and coord_move
    axis_state[AXIS_X].velocity = 100.0f;
}
```

### Example (INCORRECT - DO NOT DO THIS):

```c
// ❌ WRONG: Variable declared in middle of file

static void HelperFunction(void) {
    /* ... */
}

// ❌ This is too late! ISRs above can't access it
static motion_coordinated_move_t coord_move;

static void TMR1_InterruptHandler(void) {
    // ❌ COMPILE ERROR: coord_move not yet declared!
    coord_move.dominant_axis = AXIS_X;
}
```

## Variable Naming Conventions

### File-Scope Variables (Static)
- **Pattern**: `lowercase_with_underscores`
- **Purpose**: Module-internal state, not exposed to other files
- **Examples**:
  - `axis_state` - Per-axis motion state
  - `coord_move` - Coordinated move parameters
  - `heartbeat_counter` - ISR heartbeat timing

### Global Variables (Non-Static)
- **Pattern**: `ModuleName_CamelCase` (avoid if possible!)
- **Purpose**: Shared between multiple files
- **Examples**:
  - `MotionMath_Settings` - GRBL settings structure
  - Prefer accessor functions over global variables

### Volatile Qualifier
- **Use when**: Variable accessed by ISR AND main loop
- **Examples**:
  - `static volatile scurve_state_t axis_state[NUM_AXES];`
  - `static volatile uint32_t step_count;`

## ISR Safety Rules

### Critical ISR Patterns
1. **Always declare ISR-accessed variables at file scope**
2. **Use `volatile` for ISR-shared state**
3. **Keep ISR code minimal** (no printf, no complex math)
4. **Document which ISR accesses each variable**

### Example:
```c
// *****************************************************************************
// Local Variables
// *****************************************************************************

// Step counter (incremented by OCR ISR, read by main loop)
static volatile uint32_t step_count[NUM_AXES];

// Motion state (updated by TMR1 ISR @ 1kHz, initialized by main)
static volatile scurve_state_t axis_state[NUM_AXES];

// Coordinated move (written by main, read by TMR1 ISR)
static motion_coordinated_move_t coord_move;
```

## File Structure Template

```c
/******************************************************************************
  File: module_name.c
  Description: Brief description of module purpose
  Date: YYYY-MM-DD
******************************************************************************/

// *****************************************************************************
// Section: Included Files
// *****************************************************************************
#include "definitions.h"
#include "module_name.h"

// *****************************************************************************
// Section: Type Definitions
// *****************************************************************************
typedef struct {
    /* ... */
} internal_type_t;

// *****************************************************************************
// Local Variables
// *****************************************************************************

// Variable comments explain purpose and access pattern
static volatile state_t module_state;
static config_t module_config;

// *****************************************************************************
// Private Function Prototypes
// *****************************************************************************
static void helper_function(void);
static void interrupt_handler(void);

// *****************************************************************************
// Public API Implementation
// *****************************************************************************

void ModuleName_Initialize(void) {
    /* ... */
}

// *****************************************************************************
// Private Function Implementation
// *****************************************************************************

static void helper_function(void) {
    /* ... */
}
```

## Why This Matters: Real-World Bug Example

**Before (Broken):**
```c
// multiaxis_control.c (excerpt)

static void TMR1_MultiAxisControl(uint32_t status, uintptr_t context)
{
    // ❌ Tries to access coord_move here (line 607)
    axis_id_t dominant = coord_move.dominant_axis;
}

// ... 400 lines later ...

// ❌ Variable declared here (line 1040) - TOO LATE!
static motion_coordinated_move_t coord_move;
```

**Compiler Error:**
```
multiaxis_control.c:607:31: error: 'coord_move' undeclared (first use in this function)
```

**After (Fixed):**
```c
// multiaxis_control.c (excerpt)

// *****************************************************************************
// Local Variables
// *****************************************************************************

// Coordinated move state (accessed by TMR1 ISR @ 1kHz)
static motion_coordinated_move_t coord_move;  // ✅ Declared at top!

// ... rest of file ...

static void TMR1_MultiAxisControl(uint32_t status, uintptr_t context)
{
    // ✅ Works! Variable declared before ISR definition
    axis_id_t dominant = coord_move.dominant_axis;
}
```

## Checklist for New Files

- [ ] All `#include` statements at top
- [ ] Type definitions (`typedef`, `struct`, `enum`)
- [ ] **"Local Variables" comment section**
- [ ] All static variables declared with comments
- [ ] Volatile qualifier for ISR-accessed variables
- [ ] Private function prototypes
- [ ] Public API implementations
- [ ] Private function implementations

## Enforcement

When reviewing code:
1. Check first 50 lines for "Local Variables" section
2. Verify no variables declared in middle of file
3. Confirm ISR-accessed variables are `volatile`
4. Ensure proper comments document ISR access patterns

---

**Last Updated:** October 18, 2025
**Status:** MANDATORY for all new code
