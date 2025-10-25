/*******************************************************************************
  Multi-Axis S-Curve Motion Control

  Coordinates X, Y, Z axes using individual S-curve profiles per axis.
  Hardware mapping:
    X-axis: OCMP4 + TMR2
    Y-axis: OCMP1 + TMR4
    Z-axis: OCMP5 + TMR3

  MISRA C:2012 Compliance:
    - Static assertions for compile-time validation
    - Runtime parameter validation with bounds checking
    - Const-qualified lookup tables
    - No dynamic memory allocation
    - Fixed-width integer types
*******************************************************************************/

#include "motion/multiaxis_control.h"
#include "motion/motion_math.h"
#include "motion/motion_manager.h" // GRBL-style motion buffer feeding (CoreTimer @ 10ms)
#include "motion/grbl_stepper.h"   // PHASE 2B: Segment execution from GRBL stepper buffer
#include "config/default/peripheral/gpio/plib_gpio.h"
#include "ugs_interface.h"

#include "definitions.h"
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <stdio.h> // For printf debug output

// CRITICAL FIX (Oct 20, 2025): Define NOP for bit-bang delays
// XC32 doesn't have __NOP(), use inline assembly instead
#define NOP() __asm__ __volatile__("nop")

// *****************************************************************************
// MISRA C Compile-Time Assertions
// *****************************************************************************

// Static assertion macro (C11 compatible)
#define STATIC_ASSERT(condition, message) \
    typedef char static_assert_##message[(condition) ? 1 : -1]

// Verify enum values match array indices
STATIC_ASSERT(AXIS_X == 0, axis_x_must_be_zero);
STATIC_ASSERT(AXIS_Y == 1, axis_y_must_be_one);
STATIC_ASSERT(AXIS_Z == 2, axis_z_must_be_two);
STATIC_ASSERT(AXIS_A == 3, axis_a_must_be_three);
// Verify array sizing
STATIC_ASSERT(NUM_AXES == 4, num_axes_must_be_four);

// *****************************************************************************
// Debug Counters (ISR-safe, read by main loop)
// *****************************************************************************

/**
 * @brief Y-axis step counter for debugging (volatile for ISR access)
 *
 * Incremented in ProcessSegmentStep() ISR, read by main loop via
 * MultiAxis_GetDebugYStepCount() for non-blocking debug output.
 */
static volatile uint32_t debug_total_y_pulses = 0;

/**
 * @brief Segment completion counter (volatile for ISR access)
 */
static volatile uint32_t debug_segment_count = 0;

// *****************************************************************************
// Hardware Configuration
// *****************************************************************************/

// Timer clock frequency defined in motion_types.h (1.5625 MHz = 25 MHz ÷ 16 prescaler)

// OCR/Timer assignments per PIC32MZ hardware
typedef struct
{
    void (*OCMP_Enable)(void);
    void (*OCMP_Disable)(void);
    void (*OCMP_CompareValueSet)(uint16_t);
    void (*OCMP_CompareSecondaryValueSet)(uint16_t);
    void (*OCMP_CallbackRegister)(void (*)(uintptr_t), uintptr_t);
    void (*TMR_Start)(void);
    void (*TMR_Stop)(void);
    void (*TMR_PeriodSet)(uint16_t);
} axis_hardware_t;

// Forward declarations for hardware functions
static void OCMP5_StepCounter_X(uintptr_t context);
static void OCMP1_StepCounter_Y(uintptr_t context);
static void OCMP4_StepCounter_Z(uintptr_t context);
#ifdef ENABLE_AXIS_A
static void OCMP3_StepCounter_A(uintptr_t context);
#endif
// Hardware configuration table
static const axis_hardware_t axis_hw[NUM_AXES] = {
    // AXIS_X: OCMP5 + TMR3 (CORRECTED - per hardware wiring)
    {
        .OCMP_Enable = OCMP5_Enable,
        .OCMP_Disable = OCMP5_Disable,
        .OCMP_CompareValueSet = OCMP5_CompareValueSet,
        .OCMP_CompareSecondaryValueSet = OCMP5_CompareSecondaryValueSet,
        .OCMP_CallbackRegister = OCMP5_CallbackRegister,
        .TMR_Start = TMR3_Start,
        .TMR_Stop = TMR3_Stop,
        .TMR_PeriodSet = TMR3_PeriodSet},
    // AXIS_Y: OCMP1 + TMR4 (CORRECT - per hardware wiring)
    {
        .OCMP_Enable = OCMP1_Enable,
        .OCMP_Disable = OCMP1_Disable,
        .OCMP_CompareValueSet = OCMP1_CompareValueSet,
        .OCMP_CompareSecondaryValueSet = OCMP1_CompareSecondaryValueSet,
        .OCMP_CallbackRegister = OCMP1_CallbackRegister,
        .TMR_Start = TMR4_Start,
        .TMR_Stop = TMR4_Stop,
        .TMR_PeriodSet = TMR4_PeriodSet},
    // AXIS_Z: OCMP4 + TMR2 (CORRECTED - per hardware wiring)
    {
        .OCMP_Enable = OCMP4_Enable,
        .OCMP_Disable = OCMP4_Disable,
        .OCMP_CompareValueSet = OCMP4_CompareValueSet,
        .OCMP_CompareSecondaryValueSet = OCMP4_CompareSecondaryValueSet,
        .OCMP_CallbackRegister = OCMP4_CallbackRegister,
        .TMR_Start = TMR2_Start,
        .TMR_Stop = TMR2_Stop,
        .TMR_PeriodSet = TMR2_PeriodSet},
    // AXIS_A: OCMP3 + TMR5 (CORRECT - per hardware wiring)
    {
        .OCMP_Enable = OCMP3_Enable,
        .OCMP_Disable = OCMP3_Disable,
        .OCMP_CompareValueSet = OCMP3_CompareValueSet,
        .OCMP_CompareSecondaryValueSet = OCMP3_CompareSecondaryValueSet,
        .OCMP_CallbackRegister = OCMP3_CallbackRegister,
        .TMR_Start = TMR5_Start,
        .TMR_Stop = TMR5_Stop,
        .TMR_PeriodSet = TMR5_PeriodSet}};

// ******************************************************************************
// Drive Control Pin Macros (Dynamic Function Pointer Lookup)
// *****************************************************************************/
// Wrapper functions for direction pin macros (macros can't be used as function pointers)
static inline void enx_set_wrapper(void) { EnX_Set(); }
static inline void enx_clear_wrapper(void) { EnX_Clear(); }
static inline bool enx_get_wrapper(void) { return EnX_Get(); }
static inline void eny_set_wrapper(void) { EnY_Set(); }
static inline void eny_clear_wrapper(void) { EnY_Clear(); }
static inline bool eny_get_wrapper(void) { return EnY_Get(); }
static inline void enz_set_wrapper(void) { EnZ_Set(); }
static inline void enz_clear_wrapper(void) { EnZ_Clear(); }
static inline bool enz_get_wrapper(void) { return EnZ_Get(); }
#ifdef ENABLE_AXIS_A
static inline void ena_set_wrapper(void) { EnA_Set(); }
static inline void ena_clear_wrapper(void) { EnA_Clear(); }
static inline bool ena_get_wrapper(void) { return EnA_Get(); }
#endif

static void (*const en_set_funcs[NUM_AXES])(void) = {
    enx_set_wrapper, // AXIS_X
    eny_set_wrapper, // AXIS_Y
    enz_set_wrapper, // AXIS_Z
#ifdef ENABLE_AXIS_A
    ena_set_wrapper, // AXIS_A
#endif
};

static void (*const en_clear_funcs[NUM_AXES])(void) = {
    enx_clear_wrapper, // AXIS_X
    eny_clear_wrapper, // AXIS_Y
    enz_clear_wrapper, // AXIS_Z
#ifdef ENABLE_AXIS_A
    ena_clear_wrapper, // AXIS_A
#endif
};

static bool (*const en_get_funcs[NUM_AXES])(void) = {
    enx_get_wrapper, // AXIS_X
    eny_get_wrapper, // AXIS_Y
    enz_get_wrapper, // AXIS_Z
#ifdef ENABLE_AXIS_A
    ena_get_wrapper, // AXIS_A
#endif
};

// *****************************************************************************
// Driver Enable State Tracking
// *****************************************************************************
static volatile bool driver_enabled[NUM_AXES] = {false, false, false, false};

// *****************************************************************************
// Dominant Axis Transition State Tracking (ISR-Safe)
// *****************************************************************************

/**
 * @brief Per-axis transition detection state
 * 
 * Tracks whether each axis was dominant in the previous ISR cycle.
 * Used to detect transitions between dominant/subordinate roles without
 * wasteful operations every ISR.
 * 
 * @note volatile for ISR safety (prevents compiler optimization)
 * @note Initialized to false in MultiAxis_Initialize()
 */
static volatile bool axis_was_dominant_last_isr[NUM_AXES] = {false, false, false, false};

/*! \brief Enable stepper driver for specified axis using dynamic lookup
 *
 *  DRV8825 ENABLE pin is ACTIVE LOW:
 *    - LOW = Driver enabled (motor powered, torque applied)
 *    - HIGH = Driver disabled (motor unpowered, high-Z state)
 *
 *  This function makes the ENABLE pin LOW to activate the driver.
 *
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 *  MISRA Rule 1.3: Validate function pointer before call
 */
void MultiAxis_EnableDriver(axis_id_t axis)
{
    assert(axis < NUM_AXES); // Development-time check

    if (axis >= NUM_AXES || en_clear_funcs[axis] == NULL)
    {
        return; // Production safety check
    }

    if (!driver_enabled[axis]) // Only enable if currently disabled
    {
        en_clear_funcs[axis](); // ACTIVE LOW: Clear = Enable (pin goes LOW)
        driver_enabled[axis] = true;
    }
}

/*! \brief Disable stepper driver for specified axis using dynamic lookup
 *
 *  DRV8825 ENABLE pin is ACTIVE LOW:
 *    - LOW = Driver enabled (motor powered, torque applied)
 *    - HIGH = Driver disabled (motor unpowered, high-Z state)
 *
 *  This function makes the ENABLE pin HIGH to deactivate the driver.
 *
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 *  MISRA Rule 1.3: Validate function pointer before call
 */
void MultiAxis_DisableDriver(axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES || en_set_funcs[axis] == NULL)
    {
        return;
    }

    if (driver_enabled[axis]) // Only disable if currently enabled
    {
        en_set_funcs[axis](); // ACTIVE LOW: Set = Disable (pin goes HIGH)
        driver_enabled[axis] = false;
    }
}

/*! \brief Check if stepper driver is enabled for specified axis
 *
 *  Returns the software-tracked enable state (fast, no GPIO read).
 *
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 *  \return true if driver is enabled, false if disabled (LOGICAL STATE)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 */
bool MultiAxis_IsDriverEnabled(axis_id_t axis)
{
    assert(axis < NUM_AXES);

    if (axis >= NUM_AXES)
        return false;

    return driver_enabled[axis];
}

/*! \brief Read enable pin GPIO state for specified axis
 *
 *  Reads the actual hardware pin state (for debugging/verification).
 *
 *  ⚠️ Returns RAW electrical state, not logical enabled/disabled!
 *  DRV8825 ENABLE is ACTIVE LOW:
 *    - Returns false (LOW) when driver is ENABLED  ✅
 *    - Returns true (HIGH) when driver is DISABLED ❌
 *
 *  For logical state, use MultiAxis_IsDriverEnabled() instead.
 *
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 *  \return Raw GPIO pin state (true = HIGH, false = LOW)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 *  MISRA Rule 1.3: Validate function pointer before call
 */

bool MultiAxis_ReadEnablePin(axis_id_t axis)
{
    // Defensive programming: validate parameters
    assert(axis < NUM_AXES);            // Development-time check
    assert(en_get_funcs[axis] != NULL); // Verify function pointer valid

    if ((axis < NUM_AXES) && (en_get_funcs[axis] != NULL))
    {
        return en_get_funcs[axis]();
    }

    return false; // Default return value
}

// *****************************************************************************
// Direction pin control Pin Macros (Dynamic Function Pointer Lookup)
// *****************************************************************************/
// *****************************************************************************/
// Wrapper functions for direction pin macros (macros can't be used as function pointers)
static inline void dirx_set_wrapper(void) { DirX_Set(); }
static inline void dirx_clear_wrapper(void) { DirX_Clear(); }
static inline void diry_set_wrapper(void) { DirY_Set(); }
static inline void diry_clear_wrapper(void) { DirY_Clear(); }
static inline void dirz_set_wrapper(void) { DirZ_Set(); }
static inline void dirz_clear_wrapper(void) { DirZ_Clear(); }
#ifdef ENABLE_AXIS_A
static inline void dira_set_wrapper(void) { DirA_Set(); }
static inline void dira_clear_wrapper(void) { DirA_Clear(); }
#endif

// Direction pin control function pointer tables
static void (*const dir_set_funcs[NUM_AXES])(void) = {
    dirx_set_wrapper, // AXIS_X
    diry_set_wrapper, // AXIS_Y
    dirz_set_wrapper, // AXIS_Z
#ifdef ENABLE_AXIS_A
    dira_set_wrapper, // AXIS_A
#endif
};

static void (*const dir_clear_funcs[NUM_AXES])(void) = {
    dirx_clear_wrapper, // AXIS_X
    diry_clear_wrapper, // AXIS_Y
    dirz_clear_wrapper, // AXIS_Z
#ifdef ENABLE_AXIS_A
    dira_clear_wrapper, // AXIS_A
#endif
};

/*! \brief Set direction for specified axis using dynamic lookup
 *
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 *  MISRA Rule 1.3: Validate function pointer before call
 */
void MultiAxis_SetDirection(axis_id_t axis)
{
    // Defensive programming: validate parameters
    assert(axis < NUM_AXES);             // Development-time check
    assert(dir_set_funcs[axis] != NULL); // Verify function pointer valid

    if ((axis < NUM_AXES) && (dir_set_funcs[axis] != NULL))
    {
        dir_set_funcs[axis]();
    }
}

/*! \brief Clear direction for specified axis using dynamic lookup
 *
 *  \param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 *  MISRA Rule 1.3: Validate function pointer before call
 */
void MultiAxis_ClearDirection(axis_id_t axis)
{
    // Defensive programming: validate parameters
    assert(axis < NUM_AXES);               // Development-time check
    assert(dir_clear_funcs[axis] != NULL); // Verify function pointer valid

    if ((axis < NUM_AXES) && (dir_clear_funcs[axis] != NULL))
    {
        dir_clear_funcs[axis]();
    }
}

// *****************************************************************************
// S-Curve Motion Parameters
// *****************************************************************************/

// NOTE: Max velocity, acceleration, and jerk are now retrieved per-axis
// from motion_math settings. This allows per-axis tuning (e.g., Z slower than XY).
// Legacy hardcoded values removed in favor of MotionMath_Get*() functions.

#define OCMP_PULSE_WIDTH 40
#define UPDATE_FREQ_HZ 1000.0f
#define UPDATE_PERIOD_SEC (1.0f / UPDATE_FREQ_HZ)

// *****************************************************************************
// S-Curve State (per axis)
// *****************************************************************************/

typedef enum
{
    SEGMENT_IDLE = 0,
    SEGMENT_JERK_ACCEL,
    SEGMENT_CONST_ACCEL,
    SEGMENT_JERK_DECEL_ACCEL,
    SEGMENT_CRUISE,
    SEGMENT_JERK_ACCEL_DECEL,
    SEGMENT_CONST_DECEL,
    SEGMENT_JERK_DECEL_DECEL,
    SEGMENT_COMPLETE
} scurve_segment_t;

typedef struct
{
    scurve_segment_t current_segment;
    float elapsed_time;
    float total_elapsed;

    // Segment times
    float t1_jerk_accel;
    float t2_const_accel;
    float t3_jerk_decel_accel;
    float t4_cruise;
    float t5_jerk_accel_decel;
    float t6_const_decel;
    float t7_jerk_decel_decel;

    // Motion state
    float current_velocity;
    float current_accel;
    float cruise_velocity;

    // Velocity milestones
    float v_end_segment1;
    float v_end_segment2;
    float v_end_segment3;
    float v_end_segment5;
    float v_end_segment6;

    // Step tracking
    volatile uint32_t step_count;
    uint32_t total_steps;
    bool direction_forward;
    bool active;

} scurve_state_t;

// Per-axis state (accessed from main code AND TMR1 interrupt @ 1kHz)
static volatile scurve_state_t axis_state[NUM_AXES];

// Absolute machine position tracker (accessed from ISR AND main code!)
// CRITICAL FIX (October 24, 2025): Made volatile for ISR safety!
// - Updated in Execute_Bresenham_Strategy_Internal() ISR context
// - Read by MultiAxis_GetStepCount() in main loop context
// - Must be volatile to prevent compiler optimization/caching
// CRITICAL FIX (October 19, 2025): Track absolute position independently from move progress!
// - axis_state[].step_count tracks progress within current move (0 to total_steps)
// - machine_position[] tracks absolute position from power-on/homing
// - Updated at END of each move in MotionManager_TMR9_ISR()
// - Used by MultiAxis_GetStepCount() for position feedback to GRBL/UGS
static volatile int32_t machine_position[NUM_AXES] = {0, 0, 0, 0};

// *****************************************************************************
// PHASE 2B: Segment Execution State (Per-Axis)
// *****************************************************************************

/**
 * @brief Per-axis segment execution state
 *
 * Tracks hardware execution of GRBL segments using Bresenham algorithm.
 * Each axis independently executes segments from the shared segment buffer.
 */

typedef struct
{
    const st_segment_t *current_segment; ///< Pointer to segment being executed (NULL if idle)
    uint32_t step_count;                 ///< Steps executed in current segment
    int32_t bresenham_counter;           ///< Bresenham error accumulator
    bool active;                         ///< Axis currently executing segment

    // SANITY CHECK COUNTERS (Oct 21, 2025) - Dave's diagnostic pattern
    uint32_t block_steps_commanded; ///< Total steps GRBL planned for current block
    uint32_t block_steps_executed;  ///< Total steps OCR actually pulsed (ISR increment)
} axis_segment_state_t;

// Per-axis segment execution state (accessed from OCR ISR callbacks)
static volatile axis_segment_state_t segment_state[NUM_AXES] = {0};


// *****************************************************************************
// Segment Execution State (Per-Axis)
// *****************************************************************************

// ... existing segment_state and segment_completed_by_axis variables ...

// Motion active flag (checked by all OCR ISRs)
static volatile bool motion_active = false;

// Segment completion control (Dave's scalable bitmask approach)
// Bit N set = Axis N is the dominant axis and should call SegmentComplete()
// This ensures only ONE axis completes each segment, preventing buffer corruption
// Scalable to 8 axes (or 32 with uint32_t), supports future circular interpolation
static volatile uint8_t segment_completed_by_axis = 0;

// *****************************************************************************
// Inline Helper - Dominant Axis Detection (Zero Overhead)
// *****************************************************************************

/**
 * @brief Check if this axis should process segment steps (dominant role)
 * @param axis Axis identifier (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 * @return true if this axis is dominant for current segment
 * @return false if this axis is subordinate (waits for Bresenham)
 * 
 * @note Called from OCR ISR context - must be FAST!
 * @note Marked always_inline for zero-overhead role checking
 * @note Uses segment_completed_by_axis bitmask (set during segment load)
 * @note Compiles to single AND instruction (~1 cycle @ 200MHz)
 */
static inline bool __attribute__((always_inline)) IsDominantAxis(axis_id_t axis)
{
    /* Single bitmask check - compiles to one instruction */
    return (segment_completed_by_axis & (1U << axis)) != 0U;
}

// *****************************************************************************
// Selective Interrupt Masking Helpers (Oct 24, 2025)
// *****************************************************************************

/**
 * @brief Disable ONLY OCR interrupts (surgical interrupt control)
 * 
 * Blocks OCMP1/3/4/5 interrupts to prevent race during dominant axis transition.
 * Leaves TMR9 (MotionManager), UART, and other peripherals ENABLED.
 * 
 * This is superior to global __builtin_disable_interrupts() because:
 *   - TMR9 continues segment prep (no buffer starvation)
 *   - UART continues serial RX (no overflow during G-code streaming)
 *   - Minimal latency (only OCR ISRs blocked, not all interrupts)
 *   - Same race prevention (OCR ISRs can't fire during hardware config)
 * 
 * @return Saved IEC0 state for restoration (bits 0-31 only, IEC1 not used by OCR)
 * 
 * @note Must call DisableOCRInterrupts_Restore() to restore state!
 * @note Critical section duration: ~50µs (negligible vs 1ms ISR period)
 * @note Zero overhead: inline + always_inline = no function call
 */
static inline uint32_t __attribute__((always_inline)) DisableOCRInterrupts_Save(void)
{
    /* Save current interrupt enable state (IEC0 contains OCR interrupt enables) */
    uint32_t iec0 = IEC0;
    
    /* Disable ONLY OCR interrupts (per PIC32MZ2048EFH100 datasheet Table 7-1)
     * IEC0 bit positions for Output Compare interrupts:
     *   - OCMP1: Check device header (typically bit 7-11 range)
     *   - OCMP3: Check device header
     *   - OCMP4: Check device header
     *   - OCMP5: Check device header
     * 
     * Using IEC0CLR (atomic clear) to disable interrupts without affecting other bits.
     * This is safer than read-modify-write which could have its own race condition!
     */
    
    /* CRITICAL: Use PIC32 device-specific interrupt bit positions
     * Check sys/attribs.h or MCC-generated interrupts.h for correct masks
     * For PIC32MZ2048EFH100, OCR interrupts are in IEC0 register
     */
    #ifdef _IEC0_OC1IE_MASK
        IEC0CLR = _IEC0_OC1IE_MASK;  // OCMP1 (Y-axis)
    #endif
    #ifdef _IEC0_OC3IE_MASK
        IEC0CLR = _IEC0_OC3IE_MASK;  // OCMP3 (A-axis)
    #endif
    #ifdef _IEC0_OC4IE_MASK
        IEC0CLR = _IEC0_OC4IE_MASK;  // OCMP4 (Z-axis)
    #endif
    #ifdef _IEC0_OC5IE_MASK
        IEC0CLR = _IEC0_OC5IE_MASK;  // OCMP5 (X-axis)
    #endif
    
    /* NOTE: TMR9 (MotionManager @ 100Hz) left ENABLED - it's safe! */
    /* NOTE: UART2 interrupts left ENABLED - they're safe! */
    /* NOTE: All other peripheral ISRs left ENABLED - unrelated to race */
    
    return iec0;
}

/**
 * @brief Restore OCR interrupt enable state
 * 
 * @param saved_iec0 IEC0 register value from DisableOCRInterrupts_Save()
 * 
 * @note Must be called after every DisableOCRInterrupts_Save()!
 * @note Restores EXACT previous state (not just "enable all")
 * @note Zero overhead: inline + always_inline = no function call
 */
static inline void __attribute__((always_inline)) DisableOCRInterrupts_Restore(uint32_t saved_iec0)
{
    /* Restore original interrupt enable state
     * This is atomic on PIC32 (single register write)
     * Preserves enables for ALL peripherals (not just OCR)
     */
    IEC0 = saved_iec0;
}

// *****************************************************************************
// Step Execution Strategy Function Pointers (Dave's dispatch pattern)
// *****************************************************************************

// Per-axis function pointers for step execution strategies
// Allows dynamic dispatch: Bresenham (G0/G1), Arc (G2/G3), etc.
// CPU overhead: 1 extra cycle vs direct call (~5ns @ 200MHz) - negligible!
static step_execution_func_t axis_step_executor[NUM_AXES];

// Forward declarations of strategy implementations
static void Execute_Bresenham_Strategy_Internal(axis_id_t axis, const st_segment_t *seg);
static void Execute_ArcInterpolation_Strategy_Internal(axis_id_t axis, const st_segment_t *seg);

// Coordinated move state (accessed from main code AND TMR1 interrupt @ 1kHz)
// Stores dominant axis info and velocity scaling for synchronized motion
static motion_coordinated_move_t coord_move;

// *****************************************************************************
// Math Helpers
// *****************************************************************************

static float cbrt_approx(float x)
{
    if (x == 0.0f)
        return 0.0f;

    float sign = (x < 0.0f) ? -1.0f : 1.0f;
    x = fabsf(x);

    float guess = expf(logf(x) / 3.0f);

    for (int i = 0; i < 3; i++)
    {
        guess = (2.0f * guess + x / (guess * guess)) / 3.0f;
    }

    return sign * guess;
}

// *****************************************************************************
// Step Execution Strategies (for function pointer dispatch)
// ************************************************************************

/*! \brief Bresenham execution strategy for linear interpolation
 *
 *  Bit-bangs subordinate axes using Bresenham algorithm.
 *  Called from dominant axis ISR - runs in INTERRUPT CONTEXT!
 *
 *  Algorithm:
 *    For each step on dominant axis:
 *      bresenham_counter[axis] += steps[axis]
 *      if (bresenham_counter[axis] >= n_step)
 *        bresenham_counter[axis] -= n_step
 *        Toggle step pin (GPIO bit-bang)
 *
 *  CPU Cost per subordinate axis per step: ~10-20 cycles
 *    - Integer add: 1 cycle
 *    - Comparison: 1 cycle
 *    - Conditional subtract: 1-2 cycles
 *    - GPIO toggle (if step needed): 2-3 cycles
 *
 *  Example: X dominant @ 25,000 steps, Y subordinate @ 12,500 steps
 *    - X uses OCR hardware (zero CPU)
 *    - Y bit-banged every 2nd X step (12,500 GPIO toggles)
 *    - Total CPU: 12,500 × 15 cycles = 187,500 cycles = 0.94ms @ 200MHz
 */
static void Execute_Bresenham_Strategy_Internal(axis_id_t dominant_axis, const st_segment_t *seg)
{
    /* REFACTORED (Oct 19, 2025): All axis coordination happens here!
     *
     * This function handles:
     * 1. Dominant axis position tracking (OCR generates pulse, we track position)
     * 2. Dominant axis step counting
     * 3. Subordinate axes Bresenham + bit-bang + position tracking
     *
     * Called from dominant axis ISR only - subordinate ISRs are disabled
     */

    // Sanity checks - CRITICAL for ISR safety!
    if (seg == NULL)
        return; // NULL segment pointer

    if (dominant_axis >= NUM_AXES)
        return; // Invalid axis ID

    const st_segment_t *segment = seg;
    uint32_t n_step = segment->n_step; // Dominant axis step count

    // Additional safety: n_step must be non-zero for Bresenham
    if (n_step == 0)
        return; // Avoid division by zero in Bresenham

    // ═════════════════════════════════════════════════════════════════════════
    // STEP 1: Update DOMINANT axis position and step count
    // ═════════════════════════════════════════════════════════════════════════
    volatile axis_segment_state_t *dom_state = &segment_state[dominant_axis];

    // Update dominant axis position (hardware generated the pulse already)
    if (segment->direction_bits & (1 << dominant_axis))
    {
        machine_position[dominant_axis]--; // NEGATIVE direction
    }
    else
    {
        machine_position[dominant_axis]++; // POSITIVE direction
    }

    // Increment dominant axis step counter
    dom_state->step_count++;

    // SANITY CHECK (Oct 21, 2025): Increment block execution counter
    dom_state->block_steps_executed++; // Track actual pulses delivered by OCR

    // ═════════════════════════════════════════════════════════════════════════
    // STEP 2: Handle all SUBORDINATE axes (Bresenham + bit-bang)
    // ═════════════════════════════════════════════════════════════════════════
    for (axis_id_t sub_axis = AXIS_X; sub_axis < NUM_AXES; sub_axis++)
    {
        if (sub_axis == dominant_axis)
            continue; // Skip dominant axis (hardware handles it)

        volatile axis_segment_state_t *sub_state = &segment_state[sub_axis];

        // CRITICAL FIX (Oct 20, 2025): Don't check active flag for subordinates!
        // Subordinates have active=false (they're bit-banged, not independently executing)
        // Instead, check if they have motion in this segment (steps > 0)
        uint32_t steps_sub = segment->steps[sub_axis];
        if (steps_sub == 0)
            continue; // No motion on this subordinate axis

        sub_state->bresenham_counter += (int32_t)steps_sub;

        if (sub_state->bresenham_counter >= (int32_t)n_step)
        {
            sub_state->bresenham_counter -= (int32_t)n_step;

            // ═════════════════════════════════════════════════════════════════════
            // CRITICAL FIX (Oct 25, 2025): Set direction GPIO BEFORE pulse trigger!
            // DRV8825 requires direction stable before step pulse (200ns setup time)
            // ═════════════════════════════════════════════════════════════════════
            bool dir_negative = (segment->direction_bits & (1 << sub_axis)) != 0;
            
#if DEBUG_MOTION_BUFFER >= DEBUG_LEVEL_VERBOSE
            // DEBUG: Track direction changes for subordinate axes
            static uint8_t last_dir_bits[NUM_AXES] = {0};
            uint8_t current_dir_bit = dir_negative ? 1 : 0;
            if (current_dir_bit != last_dir_bits[sub_axis])
            {
                UGS_Printf("[DIR_CHG] Sub axis %u: %s -> %s\r\n",
                           sub_axis,
                           last_dir_bits[sub_axis] ? "NEG" : "POS",
                           current_dir_bit ? "NEG" : "POS");
                last_dir_bits[sub_axis] = current_dir_bit;
            }
#endif
            
            if (dir_negative)
            {
                dir_clear_funcs[sub_axis]();  // Set GPIO LOW for negative
            }
            else
            {
                dir_set_funcs[sub_axis]();    // Set GPIO HIGH for positive
            }

            // ═════════════════════════════════════════════════════════════════════
            // OCR PULSE: Trigger one hardware pulse using MCC PLIB functions!
            // ═════════════════════════════════════════════════════════════════════
            // Pattern (from mikroC verified working code):
            //   OCxR   = 5;             // Rising edge at count 5
            //   OCxRS  = 36;            // Falling edge at count 36 (20µs pulse)
            //   TMRx   = 0xFFFF;        // Force immediate rollover
            //   OCMP_Enable()           // Restart OCR (sets ON bit)
            //
            // Timer clock: 1.5625 MHz (640ns per count)
            // Pulse width: 31 counts = 19.84µs ≈ 20µs (meets DRV8825 1.9µs minimum)
            // ═════════════════════════════════════════════════════════════════════

            switch (sub_axis)
            {
            case AXIS_X:
                OCMP5_CompareValueSet(5);          // OCxR: Rising edge
                OCMP5_CompareSecondaryValueSet(36); // OCxRS: Falling edge (31 count pulse)
                TMR3 = 0xFFFF;                      // Force immediate rollover (timer already running)
                OCMP5_Enable();                     // Restart OCR module
                break;
            case AXIS_Y:
                OCMP1_CompareValueSet(5);
                OCMP1_CompareSecondaryValueSet(36);
                TMR4 = 0xFFFF;                      // Force immediate rollover (timer already running)
                OCMP1_Enable();
                break;
            case AXIS_Z:
                OCMP4_CompareValueSet(5);
                OCMP4_CompareSecondaryValueSet(36);
                TMR2 = 0xFFFF;                      // Force immediate rollover (timer already running)
                OCMP4_Enable();
                break;
            case AXIS_A:
                OCMP3_CompareValueSet(5);
                OCMP3_CompareSecondaryValueSet(36);
                TMR5 = 0xFFFF;                      // Force immediate rollover (timer already running)
                OCMP3_Enable();
                break;
            default:
                break;
            }

            // Update machine position for subordinate axis
            if (segment->direction_bits & (1 << sub_axis))
            {
                machine_position[sub_axis]--; // NEGATIVE direction
            }
            else
            {
                machine_position[sub_axis]++; // POSITIVE direction
            }

            sub_state->step_count++; // Track subordinate progress

            // NOTE (Oct 22, 2025): Do NOT increment block_steps_executed for subordinates!
            // Subordinates are bit-banged (software GPIO), not hardware OCR pulses.
            // Only dominant axis increments block_steps_executed (line 588).
        }
    }
}

/*! \brief Arc interpolation strategy (placeholder for G2/G3)
 *
 *  Future implementation for circular interpolation.
 *  Will calculate arc trajectory in real-time.
 */
static void Execute_ArcInterpolation_Strategy_Internal(axis_id_t axis, const st_segment_t *seg)
{
    /* TODO: Implement circular interpolation
     *
     * Arc interpolation calculates X/Y positions from:
     *   - Center point (I, J offsets)
     *   - Radius
     *   - Start/end angles
     *   - Step count along arc
     *
     * This requires trigonometry or DDA (Digital Differential Analyzer).
     * For now, fall back to Bresenham (linear approximation).
     */

    Execute_Bresenham_Strategy_Internal(axis, seg);
}

// Public API wrappers (called from other modules)
void Execute_Bresenham_Strategy(axis_id_t axis, const st_segment_t *seg)
{
    Execute_Bresenham_Strategy_Internal(axis, seg);
}

void Execute_ArcInterpolation_Strategy(axis_id_t axis, const st_segment_t *seg)
{
    Execute_ArcInterpolation_Strategy_Internal(axis, seg);
}

/*! \brief Set step execution strategy for an axis
 *
 *  \param axis Axis to configure (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 *  \param strategy Function pointer to execution strategy
 *
 *  Example:
 *    MultiAxis_SetStepStrategy(AXIS_X, Execute_Bresenham_Strategy);
 *    MultiAxis_SetStepStrategy(AXIS_X, Execute_ArcInterpolation_Strategy);
 */
void MultiAxis_SetStepStrategy(axis_id_t axis, step_execution_func_t strategy)
{
    // Sanity checks
    if (axis >= NUM_AXES)
        return; // Bounds check

    if (strategy == NULL)
        return; // Don't allow NULL strategy (would crash ISR!)

    axis_step_executor[axis] = strategy;
}

// *****************************************************************************
// S-Curve Profile Calculation
// *****************************************************************************/

static bool calculate_scurve_profile(axis_id_t axis, uint32_t distance)
{
    volatile scurve_state_t *s = &axis_state[axis];
    float d_total = (float)distance;

    // Get per-axis motion limits from motion_math settings
    // This supports per-axis tuning (e.g., Z slower than XY)
    float max_velocity = MotionMath_GetMaxVelocityStepsPerSec(axis);
    float max_accel = MotionMath_GetAccelStepsPerSec2(axis);
    float max_jerk = MotionMath_GetJerkStepsPerSec3(axis);

#ifdef DEBUG_MOTION_BUFFER
    // Debug: Print S-curve inputs
    UGS_Debug("[SCURVE] axis=%d distance=%lu maxV=%.1f maxA=%.1f maxJ=%.1f\r\n",
              axis, distance, max_velocity, max_accel, max_jerk);
#endif

    float t_jerk = max_accel / max_jerk;
    float v_jerk = 0.5f * max_accel * t_jerk;
    float d_jerk = (1.0f / 6.0f) * max_jerk * t_jerk * t_jerk * t_jerk;

    float v_between_jerks = max_velocity - 2.0f * v_jerk;
    float d_const_accel = 0.0f;

    if (v_between_jerks > 0.0f)
    {
        d_const_accel = v_between_jerks * v_between_jerks / (2.0f * max_accel);
    }

    float d_accel_total = 2.0f * d_jerk + d_const_accel;
    float d_decel_total = d_accel_total;

    if (d_total >= d_accel_total + d_decel_total)
    {
        // LONG MOVE - reach max velocity
        s->cruise_velocity = max_velocity;

        s->t1_jerk_accel = t_jerk;
        s->t3_jerk_decel_accel = t_jerk;
        s->t5_jerk_accel_decel = t_jerk;
        s->t7_jerk_decel_decel = t_jerk;

        if (v_between_jerks > 0.0f)
        {
            s->t2_const_accel = v_between_jerks / max_accel;
            s->t6_const_decel = s->t2_const_accel;
        }
        else
        {
            s->t2_const_accel = 0.0f;
            s->t6_const_decel = 0.0f;
        }

        float d_cruise = d_total - d_accel_total - d_decel_total;
        s->t4_cruise = d_cruise / s->cruise_velocity;

        s->v_end_segment1 = v_jerk;
        s->v_end_segment2 = s->v_end_segment1 + max_accel * s->t2_const_accel;
        s->v_end_segment3 = s->cruise_velocity;

        // Deceleration milestones
        s->v_end_segment5 = s->cruise_velocity - v_jerk;
        s->v_end_segment6 = s->v_end_segment5 - max_accel * s->t6_const_decel;
    }
    else
    {
        // SHORT MOVE
        if (d_total <= 4.0f * d_jerk)
        {
            // VERY SHORT - only jerk segments
            float t_jerk_reduced = cbrt_approx(d_total / (4.0f * (1.0f / 6.0f) * max_jerk));
            s->cruise_velocity = 0.5f * max_jerk * t_jerk_reduced * t_jerk_reduced;

            s->t1_jerk_accel = t_jerk_reduced;
            s->t2_const_accel = 0.0f;
            s->t3_jerk_decel_accel = t_jerk_reduced;
            s->t4_cruise = 0.0f;
            s->t5_jerk_accel_decel = t_jerk_reduced;
            s->t6_const_decel = 0.0f;
            s->t7_jerk_decel_decel = t_jerk_reduced;

            s->v_end_segment1 = 0.5f * max_jerk * t_jerk_reduced * t_jerk_reduced;
            s->v_end_segment2 = s->v_end_segment1;
            s->v_end_segment3 = s->cruise_velocity;

            s->v_end_segment5 = s->v_end_segment1;
            s->v_end_segment6 = 0.0f;
        }
        else
        {
            // MEDIUM - jerk + reduced constant accel
            float d_remaining = d_total - 4.0f * d_jerk;

            float a = max_accel;
            float b = 4.0f * v_jerk;
            float c = -d_remaining;

            float discriminant = b * b - 4.0f * a * c;
            if (discriminant < 0.0f)
                discriminant = 0.0f;

            float t_const = (-b + sqrtf(discriminant)) / (2.0f * a);

            s->cruise_velocity = 2.0f * v_jerk + max_accel * t_const;

            s->t1_jerk_accel = t_jerk;
            s->t2_const_accel = t_const;
            s->t3_jerk_decel_accel = t_jerk;
            s->t4_cruise = 0.0f;
            s->t5_jerk_accel_decel = t_jerk;
            s->t6_const_decel = t_const;
            s->t7_jerk_decel_decel = t_jerk;

            s->v_end_segment1 = v_jerk;
            s->v_end_segment2 = s->v_end_segment1 + max_accel * t_const;
            s->v_end_segment3 = s->cruise_velocity;

            s->v_end_segment5 = s->cruise_velocity - v_jerk;
            s->v_end_segment6 = s->v_end_segment5 - max_accel * t_const;
        }
    }

    s->total_steps = distance;

#ifdef DEBUG_MOTION_BUFFER
    // Debug: Print S-curve segment times
    {
        float total_time = s->t1_jerk_accel + s->t2_const_accel + s->t3_jerk_decel_accel +
                           s->t4_cruise + s->t5_jerk_accel_decel + s->t6_const_decel + s->t7_jerk_decel_decel;
        (void)total_time; // Suppress unused warning if UGS_Debug is disabled
        UGS_Debug("[SCURVE] Times: t1=%.3f t2=%.3f t3=%.3f t4=%.3f t5=%.3f t6=%.3f t7=%.3f total=%.3f\r\n",
                  s->t1_jerk_accel, s->t2_const_accel, s->t3_jerk_decel_accel,
                  s->t4_cruise, s->t5_jerk_accel_decel, s->t6_const_decel, s->t7_jerk_decel_decel, total_time);
        UGS_Debug("[SCURVE] Velocities: v1=%.1f v2=%.1f v3=%.1f cruise=%.1f v5=%.1f v6=%.1f\r\n",
                  s->v_end_segment1, s->v_end_segment2, s->v_end_segment3,
                  s->cruise_velocity, s->v_end_segment5, s->v_end_segment6);
    }
#endif

    return true;
}

// *****************************************************************************
// OCR Interrupt Handlers (PHASE 2B: Segment Execution with Bresenham)
// *****************************************************************************/

/**
 * @brief CENTRAL segment step processing - called by all OCR ISRs
 *
 * REFACTORED (Oct 19, 2025): Single function handles ALL segment logic!
 *
 * This eliminates duplicate code across 4 ISRs. Each OCR ISR is now just
 * a thin trampoline that calls this function with its axis ID.
 *
 * @param dominant_axis Which axis ISR called this (AXIS_X, AXIS_Y, AXIS_Z, AXIS_A)
 */
static void ProcessSegmentStep(axis_id_t dominant_axis)
{
    // DEBUG_MOTION_BUFFER: ISR debug output disabled (October 25, 2025)
    // Reason: Floods UART during motion, blocks stream, causes stalls
    // Keep JUNC debug in planner - that's the valuable data!
    
    // Guard: Only execute if this is actually the dominant axis
    if (!(segment_completed_by_axis & (1 << dominant_axis)))
    {
        return; // This axis is subordinate - ignore callback
    }

    volatile axis_segment_state_t *state = &segment_state[dominant_axis];

    if (!state->active || state->current_segment == NULL)
    {
        return; // Not active or no segment - nothing to do
    }

    // ═════════════════════════════════════════════════════════════════════════
    // CRITICAL FIX (October 24, 2025): Cache segment pointer to prevent race!
    // ═════════════════════════════════════════════════════════════════════════
    // RACE CONDITION: state->current_segment is volatile (modified by MotionManager)
    // Between null check above and usage below, segment could become NULL!
    // SOLUTION: Cache pointer locally - if it was non-NULL at null check, use cached value
    // Even if MotionManager clears it mid-ISR, we execute with safe cached pointer.
    // ═════════════════════════════════════════════════════════════════════════
    const st_segment_t *segment = state->current_segment;

    // ═════════════════════════════════════════════════════════════════════════
    // STEP 1: Call Bresenham to handle ALL axis coordination
    // ═════════════════════════════════════════════════════════════════════════
    // Bresenham handles:
    //   - Dominant axis position tracking and step counting
    //   - Subordinate axes Bresenham + bit-bang + position tracking
    // ═════════════════════════════════════════════════════════════════════════
    if (axis_step_executor[dominant_axis] != NULL)
    {
        axis_step_executor[dominant_axis](dominant_axis, segment);
    }
    else
    {
        // DEBUG_MOTION_BUFFER: ISR error debug disabled (October 25, 2025)
        // Reason: Floods UART, blocks stream during motion
        // If executor is NULL, segment will stall (visible in motion)
        return; // Can't execute without function pointer
    }

    // ═════════════════════════════════════════════════════════════════════════
    // STEP 2: Check if segment complete (DOMINANT AXIS ONLY!)
    // ═════════════════════════════════════════════════════════════════════════
    // CRITICAL FIX (October 20, 2025): Check dominant axis's ACTUAL step count!
    // GRBL rounding can cause n_step=8 but steps[dominant]=9
    // Must run dominant axis for ALL its steps, not just n_step!
    //
    // Example: Segment 8 has n_step=8, X=9, Y=9
    //   - Old: X stops at 8 steps → Y gets 8 Bresenham iterations = 8 Y steps ❌
    //   - New: X runs 9 steps → Y gets 9 Bresenham iterations = 9 Y steps ✅
    // ═════════════════════════════════════════════════════════════════════════
    uint32_t dominant_steps = segment->steps[dominant_axis];
    if (state->step_count < dominant_steps)
    {
        return; // Dominant axis hasn't completed all its steps yet
    }

    // ═════════════════════════════════════════════════════════════════════════
    // STEP 3: Stop dominant axis OCR (leave timer running)
    // ═════════════════════════════════════════════════════════════════════════
    // Dominant reached n_step - safe to disable OCR now!
    // Subordinates are within +/- 1 step (acceptable Bresenham rounding error)
    // OPTIMIZATION (Oct 25): Keep timer running - only disable OCR to stop pulses
    // PIC32 datasheet: Starting/stopping timers has synchronization penalty
    // ═════════════════════════════════════════════════════════════════════════
    axis_hw[dominant_axis].OCMP_Disable();
    // NOTE: Timer keeps running - no TMR_Stop() call (optimization)

    // CRITICAL FIX (Oct 20, 2025): Clear dominant's active flag AND bitmask NOW!
    // Don't wait for next_seg==NULL - clear it as soon as hardware stops
    // Must clear bitmask too, otherwise OCR ISR keeps firing and passes guard check!
    state->active = false;
    segment_completed_by_axis &= ~(1 << dominant_axis); // Clear this axis's bit

    // ═════════════════════════════════════════════════════════════════════════
    // STEP 4: Advance to next segment
    // ═════════════════════════════════════════════════════════════════════════
    // LED1_Toggle(); // Visual: segment boundary

    // Complete segment (advances tail)
    GRBLStepper_SegmentComplete();

    // DEBUG: Accumulate Y-axis step count (non-blocking ISR-safe counter)
    debug_total_y_pulses += segment_state[AXIS_Y].step_count;
    debug_segment_count++;

    // Get next segment
    const st_segment_t *next_seg = GRBLStepper_GetNextSegment();

    // ═════════════════════════════════════════════════════════════════════════
    // CRITICAL FIX (October 20, 2025): Clear all axes BEFORE updating bitmask!
    // ═════════════════════════════════════════════════════════════════════════
    // Race condition: When last segment completes:
    //   1. Dominant axis fires ISR, sees next_seg == NULL
    //   2. Sets segment_completed_by_axis = 0
    //   3. Subordinate axis ISR fires, checks bitmask → sees 0 → returns early!
    //   4. Subordinate axes never clear their active flags!
    //
    // Solution: Clear ALL axis states BEFORE clearing bitmask, so subordinate
    // ISRs won't see them as active anymore even if they fire late.
    // ═════════════════════════════════════════════════════════════════════════
    if (next_seg == NULL)
    {
        /* ✅ CRITICAL FIX (Oct 24): Clear motion guard when segments drain */
        motion_active = false;
        
        // No more segments - DISABLE ALL OCR modules (leave timers running)
        // OPTIMIZATION (Oct 25): Keep timers running to avoid start/stop penalty
        // SANITY CHECK (Oct 21, 2025): Verify commanded vs executed steps
        for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
        {
            volatile axis_segment_state_t *ax_state = &segment_state[axis];
            
            // Check if this axis had motion in the completed block
            if (ax_state->block_steps_commanded > 0)
            {
                // Verify OCR delivered all commanded pulses
                if (ax_state->block_steps_executed != ax_state->block_steps_commanded)
                {
                    const char *axis_names[] = {"X", "Y", "Z", "A"};
                    UGS_Printf("ERROR: %s axis step mismatch! Commanded=%lu, Executed=%lu, Diff=%ld\r\n",
                               axis_names[axis],
                               ax_state->block_steps_commanded,
                               ax_state->block_steps_executed,
                               (int32_t)ax_state->block_steps_executed - (int32_t)ax_state->block_steps_commanded);
                }
            }
            
            ax_state->current_segment = NULL;
            ax_state->active = false;
            ax_state->step_count = 0;

            // CRITICAL FIX (Oct 21, 2025): Reset Bresenham accumulator between blocks!
            // Without this, error accumulates across blocks causing drift:
            //   - First X100: undershoots
            //   - Second X-100: overshoots (accumulated error)
            //   - Third X100: overshoots more
            ax_state->bresenham_counter = 0;

            // Disable OCR hardware (both dominant and subordinate)
            // OPTIMIZATION (Oct 25): Keep timers running - only disable OCR
            axis_hw[axis].OCMP_Disable();
            // NOTE: No TMR_Stop() - timers keep running for efficiency
        }

        // NOW safe to clear bitmask (axes already inactive)
        segment_completed_by_axis = 0;
    }
    else
    {
        // ═════════════════════════════════════════════════════════════════════════
        // ATOMIC TRANSITION PATTERN (Oct 24, 2025): SELECTIVE INTERRUPT MASKING
        // ═════════════════════════════════════════════════════════════════════════
        // Problem: Updating segment_completed_by_axis BEFORE hardware configured
        //          creates race window where OCR ISRs see inconsistent state:
        //
        //          Time: ────────────────────────────────────>
        //          Bitmask:  [OLD] [NEW].....................  ← Updated early!
        //          Hardware: .........[configure]...........  ← Race window!
        //                              ↑
        //                              ISR could fire with wrong bitmask!
        //
        // Solution: Configure hardware FIRST, update bitmask LAST (atomic commit)
        //
        // OPTIMIZATION: Mask ONLY OCR interrupts (not TMR9/UART/etc.)
        //   - OCR ISRs check segment_completed_by_axis → must block during transition
        //   - TMR9 ISR safe (doesn't touch transition logic) → keep running
        //   - UART ISRs safe (unrelated to motion) → keep running
        //
        // Benefits vs global __builtin_disable_interrupts():
        //   - TMR9 continues segment prep (no buffer starvation)
        //   - UART continues serial RX (no overflow during streaming)
        //   - Minimal latency (only OCR ISRs blocked)
        //   - Same race prevention (OCR ISRs can't fire during config)
        //
        // Critical section duration: ~50-100µs (negligible vs 1ms ISR period)
        // ═════════════════════════════════════════════════════════════════════════
        
        // ─────────────────────────────────────────────────────────────────────────
        // CRITICAL SECTION START: Disable ONLY OCR interrupts (surgical masking)
        // ─────────────────────────────────────────────────────────────────────────
        // XC32 interrupt control: Mask ONLY what we need (not all interrupts!)
        uint32_t saved_iec0 = DisableOCRInterrupts_Save();
        
        // ─────────────────────────────────────────────────────────────────────────
        // STEP 4A: Determine new dominant axis (max steps logic)
        // ─────────────────────────────────────────────────────────────────────────
        // CRITICAL FIX (Oct 20, 2025): GRBL segment prep can have rounding errors
        // where no axis has steps[axis] == n_step (e.g., n_step=8, X=9, Y=9)
        // Solution: Pick axis with MOST steps as dominant
        // ─────────────────────────────────────────────────────────────────────────
        axis_id_t new_dominant_axis = AXIS_X;
        uint32_t max_steps_new = 0;
        for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
        {
            if (next_seg->steps[axis] > max_steps_new)
            {
                max_steps_new = next_seg->steps[axis];
                new_dominant_axis = axis;
            }
        }
        
        if (max_steps_new == 0)
        {
            // No motion in segment - error condition
            DisableOCRInterrupts_Restore(saved_iec0);  // ✅ Restore before return!
            segment_completed_by_axis = 0;
            return;
        }
        
        // ─────────────────────────────────────────────────────────────────────────
        // STEP 4B: Clear old dominant's active flag if dominant changed
        // ─────────────────────────────────────────────────────────────────────────
        if (dominant_axis != new_dominant_axis)
        {
            // Dominant axis changed! Clear old dominant's active flag
            state->active = false;
        }
        
        // ─────────────────────────────────────────────────────────────────────────
        // STEP 4C: Configure NEW dominant axis STATE (before hardware!)
        // ─────────────────────────────────────────────────────────────────────────
        volatile axis_segment_state_t *new_state = &segment_state[new_dominant_axis];
        new_state->current_segment = next_seg;
        new_state->step_count = 0;
        new_state->bresenham_counter = 0;
        new_state->active = true; // NEW dominant becomes active
        
        // ─────────────────────────────────────────────────────────────────────────
        // STEP 4D: Update subordinate axes (before hardware!)
        // ─────────────────────────────────────────────────────────────────────────
        // CRITICAL FIX (Oct 20, 2025): Don't check active flag!
        // Subordinates have active=false but still need segment updates.
        // CRITICAL FIX (Oct 24, 2025): Stop hardware for subordinates with zero motion!
        // ─────────────────────────────────────────────────────────────────────────
        for (axis_id_t sub_axis = AXIS_X; sub_axis < NUM_AXES; sub_axis++)
        {
            if (sub_axis == new_dominant_axis)
                continue; // Skip new dominant (already updated above)

            volatile axis_segment_state_t *sub_state = &segment_state[sub_axis];
            if (next_seg->steps[sub_axis] > 0)
            {
                // Subordinate has motion in new segment - update state
                sub_state->current_segment = next_seg;
                sub_state->step_count = 0;
                sub_state->bresenham_counter = next_seg->bresenham_counter[sub_axis];
            }
            else
            {
                // CRITICAL: Subordinate has ZERO motion in new segment
                // Must stop its hardware to prevent runaway from previous segment!
                axis_hw[sub_axis].OCMP_Disable();
                axis_hw[sub_axis].TMR_Stop();
                sub_state->current_segment = NULL;
                sub_state->step_count = 0;
            }
        }
        
        // ─────────────────────────────────────────────────────────────────────────
        // STEP 4E: Configure NEW dominant axis HARDWARE (GPIO + OCR + TIMER)
        // ─────────────────────────────────────────────────────────────────────────
        // CRITICAL FIX (Oct 21, 2025): Enable driver for NEW dominant axis!
        // Without this, driver stays disabled when dominant axis changes
        // (e.g., Z→Y transition: Y driver never enabled, motor doesn't move)
        // ─────────────────────────────────────────────────────────────────────────
        MultiAxis_EnableDriver(new_dominant_axis);
        
        // Set direction GPIO for NEW dominant axis
        bool dir_negative = (next_seg->direction_bits & (1 << new_dominant_axis)) != 0;
        
#if DEBUG_MOTION_BUFFER >= DEBUG_LEVEL_VERBOSE
        // DEBUG: Track direction changes for dominant axis
        static uint8_t last_dom_dir_bits[NUM_AXES] = {0};
        uint8_t current_dir_bit = dir_negative ? 1 : 0;
        if (current_dir_bit != last_dom_dir_bits[new_dominant_axis])
        {
            UGS_Printf("[DIR_CHG] Dom axis %u: %s -> %s\r\n",
                       new_dominant_axis,
                       last_dom_dir_bits[new_dominant_axis] ? "NEG" : "POS",
                       current_dir_bit ? "NEG" : "POS");
            last_dom_dir_bits[new_dominant_axis] = current_dir_bit;
        }
#endif
        
        switch (new_dominant_axis)
        {
        case AXIS_X:
            if (dir_negative)
                DirX_Clear();
            else
                DirX_Set();
            break;
        case AXIS_Y:
            if (dir_negative)
                DirY_Clear();
            else
                DirY_Set();
            break;
        case AXIS_Z:
            if (dir_negative)
                DirZ_Clear();
            else
                DirZ_Set();
            break;
        case AXIS_A:
            if (dir_negative)
                DirA_Clear();
            else
                DirA_Set();
            break;
        default:
            break;
        }

        // Configure OCR period for new segment (NEW dominant axis)
        uint32_t period = next_seg->period;
        if (period > 65485)
            period = 65485;
        if (period <= OCMP_PULSE_WIDTH)
            period = OCMP_PULSE_WIDTH + 10;

        // CRITICAL FIX (Oct 23, 2025): Complete hardware reset sequence!
        // PIC32MZ OCR modules require specific shutdown/reconfigure sequence
        // to prevent spurious pulses during register updates.
        
        // Step 1: Disable OCR output (stops pulse generation)
        axis_hw[new_dominant_axis].OCMP_Disable();
        
        // Step 2: Stop timer (halts counter incrementing)
        axis_hw[new_dominant_axis].TMR_Stop();
        
        // Step 3: Clear timer counter to prevent rollover glitch
        // CRITICAL: Direct register write - no PLIB function exists for this!
        switch (new_dominant_axis)
        {
        case AXIS_X:
            TMR2 = 0;  // Reset counter
            break;
        case AXIS_Y:
            TMR4 = 0;
            break;
        case AXIS_Z:
            TMR3 = 0;
            break;
        case AXIS_A:
            TMR5 = 0;
            break;
        default:
            break;
        }
        
        // Step 4: Reconfigure registers while hardware is fully stopped
        axis_hw[new_dominant_axis].TMR_PeriodSet((uint16_t)period);
        axis_hw[new_dominant_axis].OCMP_CompareValueSet((uint16_t)(period - OCMP_PULSE_WIDTH));
        axis_hw[new_dominant_axis].OCMP_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);
        
        // Step 5: Re-enable in correct order (OCR first, then timer)
        axis_hw[new_dominant_axis].OCMP_Enable();
        axis_hw[new_dominant_axis].TMR_Start();
        
        // ─────────────────────────────────────────────────────────────────────────
        // STEP 4F: ATOMIC COMMIT - Update bitmask LAST (after hardware ready!)
        // ─────────────────────────────────────────────────────────────────────────
        // ✅ NOW safe to update bitmask - all hardware configured and ready!
        // Any ISR that fires after this point will see consistent state:
        //   - Hardware configured ✅
        //   - Bitmask updated ✅
        //   - No race condition ✅
        // ─────────────────────────────────────────────────────────────────────────
        segment_completed_by_axis = (1 << new_dominant_axis);
        
        // ─────────────────────────────────────────────────────────────────────────
        // CRITICAL SECTION END: Restore ONLY OCR interrupts (surgical restore)
        // ─────────────────────────────────────────────────────────────────────────
        DisableOCRInterrupts_Restore(saved_iec0);
        
        // ✅ Atomic transition complete!
        // OLD dominant ISR: Will see new bitmask, knows it's subordinate now
        // NEW dominant ISR: Will see new bitmask, hardware already configured
        // No race window exists!
        // TMR9: Never stopped, continued segment prep during transition ✅
        // UART: Never stopped, continued serial RX during transition ✅
    }
}

/**
 * @brief X-axis step execution (OCMP5 callback)
 *
 * OCM=0b101 (Dual Compare Continuous Mode) fires ISR on FALLING EDGE.
 *
 * Uses inline IsDominantAxis() helper with direct struct member access for:
 *   - Zero stack usage (no local variables)
 *   - Minimal instruction count (~12 cycles vs ~25 with locals)
 *   - Clean dominant/subordinate role transitions
 *
 * DOMINANT axis behavior:
 *   - Process segment step (runs Bresenham for subordinates)
 *   - Update OCR period every ISR (velocity may change)
 *   - NEVER stop timer, NEVER disable OCR, NEVER load 0xFFFF
 *
 * SUBORDINATE axis behavior:
 *   - Auto-disable OCR after pulse completes (stop continuous pulsing)
 *   - Stop timer - wait for Bresenham 0xFFFF trigger from dominant
 *   - Do NOT process segment (dominant handles it)
 *
 * TRANSITION detection (subordinate → dominant):
 *   - Enable driver (Oct 21 fix - ONLY on transition)
 *   - Set direction GPIO (ONLY on transition)
 *   - Configure OCR for continuous operation (ONLY on transition)
 *   - Enable OCR and start timer (ONLY on transition)
 *
 * TRANSITION detection (dominant → subordinate):
 *   - Disable OCR - stop continuous pulsing
 *   - Stop timer - wait for Bresenham trigger
 *   - Driver stays enabled (motor still energized)
 */
static void OCMP5_StepCounter_X(uintptr_t context)
{
    /* ✅ CRITICAL FIX (Oct 24): Guard against ISRs firing after motion completes */
    if (!motion_active) {
        return;
    }
    
    axis_id_t axis = AXIS_X;

    /* TRANSITION DETECTION: Check if role changed since last ISR */
    if (IsDominantAxis(axis) && !axis_was_dominant_last_isr[axis])
    {
        /* ✅ TRANSITION: Subordinate → Dominant (ONE-TIME SETUP) */
        
        /* A. Enable driver (Oct 21 fix - ONLY on transition) */
        MultiAxis_EnableDriver(axis);
        
        /* B. Get segment pointer and validate */
        volatile axis_segment_state_t *state = &segment_state[axis];
        
        /* ✅ CRITICAL FIX (Oct 24): Check if segment EXISTS before accessing */
        if (state->current_segment != NULL) {
            /* Set direction GPIO (ONLY on transition) */
            bool dir_negative = (state->current_segment->direction_bits & (1U << axis)) != 0U;
            if (dir_negative) {
                DirX_Clear();  /* Negative direction */
            } else {
                DirX_Set();    /* Positive direction */
            }
            
            /* C. Configure OCR with ACTUAL segment period (not hardcoded 100!) */
            uint32_t period = state->current_segment->period;
            if (period > 65485) period = 65485;
            if (period <= OCMP_PULSE_WIDTH) period = OCMP_PULSE_WIDTH + 10;
            
            TMR3_PeriodSet((uint16_t)period);
            OCMP5_CompareValueSet((uint16_t)(period - OCMP_PULSE_WIDTH));
            OCMP5_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);
            
            /* D. Enable OCR and start timer (ONLY on transition) */
            OCMP5_Enable();
            TMR3_Start();
        }
        
        /* E. Update state for next ISR */
        axis_was_dominant_last_isr[axis] = true;
    }
    else if (IsDominantAxis(axis))
    {
        /* ✅ CONTINUOUS: Still dominant, process step and update period */
        ProcessSegmentStep(axis);
        
        /* ✅ CRITICAL FIX (Oct 24): Update period from actual segment data */
        volatile axis_segment_state_t *state = &segment_state[axis];
        if (state->current_segment != NULL) {
            uint32_t period = state->current_segment->period;
            if (period > 65485) period = 65485;
            if (period <= OCMP_PULSE_WIDTH) period = OCMP_PULSE_WIDTH + 10;
            
            TMR3_PeriodSet((uint16_t)period);
            OCMP5_CompareValueSet((uint16_t)(period - OCMP_PULSE_WIDTH));
        }
    }
    else if (axis_was_dominant_last_isr[axis])
    {
        /* ✅ TRANSITION: Dominant → Subordinate (ONE-TIME TEARDOWN) */
        
        /* A. Disable OCR - stop continuous pulsing */
        OCMP5_Disable();
        
        /* B. OPTIMIZATION (Oct 25): Keep timer running - no stop/start penalty */
        // NOTE: Timer keeps running, OCR disable stops pulses
        
        /* C. Update state for next ISR */
        axis_was_dominant_last_isr[axis] = false;
    }
    else
    {
        /* ✅ SUBORDINATE: Pulse completed (triggered by Bresenham 0xFFFF), auto-disable */
        OCMP5_Disable();
        // OPTIMIZATION (Oct 25): Keep timer running - Bresenham will trigger next pulse
    }
}

/**
 * @brief Y-axis step execution (OCMP1 callback)
 *
 * OCM=0b101 (Dual Compare Continuous Mode) fires ISR on FALLING EDGE.
 *
 * Uses inline IsDominantAxis() helper with direct struct member access for:
 *   - Zero stack usage (no local variables)
 *   - Minimal instruction count (~12 cycles vs ~25 with locals)
 *   - Clean dominant/subordinate role transitions
 *
 * DOMINANT axis behavior:
 *   - Process segment step (runs Bresenham for subordinates)
 *   - Update OCR period every ISR (velocity may change)
 *   - NEVER stop timer, NEVER disable OCR, NEVER load 0xFFFF
 *
 * SUBORDINATE axis behavior:
 *   - Auto-disable OCR after pulse completes (stop continuous pulsing)
 *   - Stop timer - wait for Bresenham 0xFFFF trigger from dominant
 *   - Do NOT process segment (dominant handles it)
 *
 * TRANSITION detection (subordinate → dominant):
 *   - Enable driver (Oct 21 fix - ONLY on transition)
 *   - Set direction GPIO (ONLY on transition)
 *   - Configure OCR for continuous operation (ONLY on transition)
 *   - Enable OCR and start timer (ONLY on transition)
 *
 * TRANSITION detection (dominant → subordinate):
 *   - Disable OCR - stop continuous pulsing
 *   - Stop timer - wait for Bresenham trigger
 *   - Driver stays enabled (motor still energized)
 */
static void OCMP1_StepCounter_Y(uintptr_t context)
{
    /* ✅ CRITICAL FIX (Oct 24): Guard against ISRs firing after motion completes */
    if (!motion_active) {
        return;
    }
    
    axis_id_t axis = AXIS_Y;

    /* TRANSITION DETECTION: Check if role changed since last ISR */
    if (IsDominantAxis(axis) && !axis_was_dominant_last_isr[axis])
    {
        /* ✅ TRANSITION: Subordinate → Dominant (ONE-TIME SETUP) */
        
        /* A. Enable driver (Oct 21 fix - ONLY on transition) */
        MultiAxis_EnableDriver(axis);
        
        /* B. Get segment pointer and validate */
        volatile axis_segment_state_t *state = &segment_state[axis];
        
        /* ✅ CRITICAL FIX (Oct 24): Check if segment EXISTS before accessing */
        if (state->current_segment != NULL) {
            /* Set direction GPIO (ONLY on transition) */
            bool dir_negative = (state->current_segment->direction_bits & (1U << axis)) != 0U;
            if (dir_negative) {
                DirY_Clear();  /* Negative direction */
            } else {
                DirY_Set();    /* Positive direction */
            }
            
            /* C. Configure OCR with ACTUAL segment period (not hardcoded 100!) */
            uint32_t period = state->current_segment->period;
            if (period > 65485) period = 65485;
            if (period <= OCMP_PULSE_WIDTH) period = OCMP_PULSE_WIDTH + 10;
            
            TMR4_PeriodSet((uint16_t)period);
            OCMP1_CompareValueSet((uint16_t)(period - OCMP_PULSE_WIDTH));
            OCMP1_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);
            
            /* D. Enable OCR and start timer (ONLY on transition) */
            OCMP1_Enable();
            TMR4_Start();
        }
        
        /* E. Update state for next ISR */
        axis_was_dominant_last_isr[axis] = true;
    }
    else if (IsDominantAxis(axis))
    {
        /* ✅ CONTINUOUS: Still dominant, process step and update period */
        ProcessSegmentStep(axis);
        
        /* ✅ CRITICAL FIX (Oct 24): Update period from actual segment data */
        volatile axis_segment_state_t *state = &segment_state[axis];
        if (state->current_segment != NULL) {
            uint32_t period = state->current_segment->period;
            if (period > 65485) period = 65485;
            if (period <= OCMP_PULSE_WIDTH) period = OCMP_PULSE_WIDTH + 10;
            
            TMR4_PeriodSet((uint16_t)period);
            OCMP1_CompareValueSet((uint16_t)(period - OCMP_PULSE_WIDTH));
        }
    }
    else if (axis_was_dominant_last_isr[axis])
    {
        /* ✅ TRANSITION: Dominant → Subordinate (ONE-TIME TEARDOWN) */
        
        /* A. Disable OCR - stop continuous pulsing */
        OCMP1_Disable();
        
        /* B. OPTIMIZATION (Oct 25): Keep timer running - no stop/start penalty */
        // NOTE: Timer keeps running, OCR disable stops pulses
        
        /* C. Update state for next ISR */
        axis_was_dominant_last_isr[axis] = false;
    }
    else
    {
        /* ✅ SUBORDINATE: Pulse completed (triggered by Bresenham 0xFFFF), auto-disable */
        OCMP1_Disable();
        // OPTIMIZATION (Oct 25): Keep timer running - Bresenham will trigger next pulse
    }
}

/**
 * @brief Z-axis step execution (OCMP4 callback)
 *
 * OCM=0b101 (Dual Compare Continuous Mode) fires ISR on FALLING EDGE.
 *
 * Uses inline IsDominantAxis() helper with direct struct member access for:
 *   - Zero stack usage (no local variables)
 *   - Minimal instruction count (~12 cycles vs ~25 with locals)
 *   - Clean dominant/subordinate role transitions
 *
 * DOMINANT axis behavior:
 *   - Process segment step (runs Bresenham for subordinates)
 *   - Update OCR period every ISR (velocity may change)
 *   - NEVER stop timer, NEVER disable OCR, NEVER load 0xFFFF
 *
 * SUBORDINATE axis behavior:
 *   - Auto-disable OCR after pulse completes (stop continuous pulsing)
 *   - Stop timer - wait for Bresenham 0xFFFF trigger from dominant
 *   - Do NOT process segment (dominant handles it)
 *
 * TRANSITION detection (subordinate → dominant):
 *   - Enable driver (Oct 21 fix - ONLY on transition)
 *   - Set direction GPIO (ONLY on transition)
 *   - Configure OCR for continuous operation (ONLY on transition)
 *   - Enable OCR and start timer (ONLY on transition)
 *
 * TRANSITION detection (dominant → subordinate):
 *   - Disable OCR - stop continuous pulsing
 *   - Stop timer - wait for Bresenham trigger
 *   - Driver stays enabled (motor still energized)
 */
static void OCMP4_StepCounter_Z(uintptr_t context)
{
    /* ✅ CRITICAL FIX (Oct 24): Guard against ISRs firing after motion completes */
    if (!motion_active) {
        return;
    }
    
    axis_id_t axis = AXIS_Z;

    /* TRANSITION DETECTION: Check if role changed since last ISR */
    if (IsDominantAxis(axis) && !axis_was_dominant_last_isr[axis])
    {
        /* ✅ TRANSITION: Subordinate → Dominant (ONE-TIME SETUP) */
        
        /* A. Enable driver (Oct 21 fix - ONLY on transition) */
        MultiAxis_EnableDriver(axis);
        
        /* B. Get segment pointer and validate */
        volatile axis_segment_state_t *state = &segment_state[axis];
        
        /* ✅ CRITICAL FIX (Oct 24): Check if segment EXISTS before accessing */
        if (state->current_segment != NULL) {
            /* Set direction GPIO (ONLY on transition) */
            bool dir_negative = (state->current_segment->direction_bits & (1U << axis)) != 0U;
            if (dir_negative) {
                DirZ_Clear();  /* Negative direction */
            } else {
                DirZ_Set();    /* Positive direction */
            }
            
            /* C. Configure OCR with ACTUAL segment period (not hardcoded 100!) */
            uint32_t period = state->current_segment->period;
            if (period > 65485) period = 65485;
            if (period <= OCMP_PULSE_WIDTH) period = OCMP_PULSE_WIDTH + 10;
            
            TMR2_PeriodSet((uint16_t)period);
            OCMP4_CompareValueSet((uint16_t)(period - OCMP_PULSE_WIDTH));
            OCMP4_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);
            
            /* D. Enable OCR and start timer (ONLY on transition) */
            OCMP4_Enable();
            TMR2_Start();
        }
        
        /* E. Update state for next ISR */
        axis_was_dominant_last_isr[axis] = true;
    }
    else if (IsDominantAxis(axis))
    {
        /* ✅ CONTINUOUS: Still dominant, process step and update period */
        ProcessSegmentStep(axis);
        
        /* ✅ CRITICAL FIX (Oct 24): Update period from actual segment data */
        volatile axis_segment_state_t *state = &segment_state[axis];
        if (state->current_segment != NULL) {
            uint32_t period = state->current_segment->period;
            if (period > 65485) period = 65485;
            if (period <= OCMP_PULSE_WIDTH) period = OCMP_PULSE_WIDTH + 10;
            
            TMR2_PeriodSet((uint16_t)period);
            OCMP4_CompareValueSet((uint16_t)(period - OCMP_PULSE_WIDTH));
        }
    }
    else if (axis_was_dominant_last_isr[axis])
    {
        /* ✅ TRANSITION: Dominant → Subordinate (ONE-TIME TEARDOWN) */
        
        /* A. Disable OCR - stop continuous pulsing */
        OCMP4_Disable();
        
        /* B. OPTIMIZATION (Oct 25): Keep timer running - no stop/start penalty */
        // NOTE: Timer keeps running, OCR disable stops pulses
        
        /* C. Update state for next ISR */
        axis_was_dominant_last_isr[axis] = false;
    }
    else
    {
        /* ✅ SUBORDINATE: Pulse completed (triggered by Bresenham 0xFFFF), auto-disable */
        OCMP4_Disable();
        // OPTIMIZATION (Oct 25): Keep timer running - Bresenham will trigger next pulse
    }
}

#ifdef ENABLE_AXIS_A
/**
 * @brief A-axis step execution (OCMP3 callback)
 *
 * OCM=0b101 (Dual Compare Continuous Mode) fires ISR on FALLING EDGE.
 *
 * Uses inline IsDominantAxis() helper with direct struct member access for:
 *   - Zero stack usage (no local variables)
 *   - Minimal instruction count (~12 cycles vs ~25 with locals)
 *   - Clean dominant/subordinate role transitions
 *
 * DOMINANT axis behavior:
 *   - Process segment step (runs Bresenham for subordinates)
 *   - Update OCR period every ISR (velocity may change)
 *   - NEVER stop timer, NEVER disable OCR, NEVER load 0xFFFF
 *
 * SUBORDINATE axis behavior:
 *   - Auto-disable OCR after pulse completes (stop continuous pulsing)
 *   - Stop timer - wait for Bresenham 0xFFFF trigger from dominant
 *   - Do NOT process segment (dominant handles it)
 *
 * TRANSITION detection (subordinate → dominant):
 *   - Enable driver (Oct 21 fix - ONLY on transition)
 *   - Set direction GPIO (ONLY on transition)
 *   - Configure OCR for continuous operation (ONLY on transition)
 *   - Enable OCR and start timer (ONLY on transition)
 *
 * TRANSITION detection (dominant → subordinate):
 *   - Disable OCR - stop continuous pulsing
 *   - Stop timer - wait for Bresenham trigger
 *   - Driver stays enabled (motor still energized)
 */
static void OCMP3_StepCounter_A(uintptr_t context)
{
    axis_id_t axis = AXIS_A;

    /* TRANSITION DETECTION: Check if role changed since last ISR */
    if (IsDominantAxis(axis) && !axis_was_dominant_last_isr[axis])
    {
        /* ✅ TRANSITION: Subordinate → Dominant (ONE-TIME SETUP) */
        
        /* A. Enable driver (Oct 21 fix - ONLY on transition) */
        MultiAxis_EnableDriver(axis);
        
        /* B. Get segment pointer and validate */
        volatile axis_segment_state_t *state = &segment_state[axis];
        
        /* ✅ CRITICAL FIX (Oct 24): Check if segment EXISTS before accessing */
        if (state->current_segment != NULL) {
            /* Set direction GPIO (ONLY on transition) */
            bool dir_negative = (state->current_segment->direction_bits & (1U << axis)) != 0U;
            if (dir_negative) {
                DirA_Clear();  /* Negative direction */
            } else {
                DirA_Set();    /* Positive direction */
            }
            
            /* C. Configure OCR with ACTUAL segment period (not hardcoded 100!) */
            uint32_t period = state->current_segment->period;
            if (period > 65485) period = 65485;
            if (period <= OCMP_PULSE_WIDTH) period = OCMP_PULSE_WIDTH + 10;
            
            TMR5_PeriodSet((uint16_t)period);
            OCMP3_CompareValueSet((uint16_t)(period - OCMP_PULSE_WIDTH));
            OCMP3_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);
            
            /* D. Enable OCR and start timer (ONLY on transition) */
            OCMP3_Enable();
            TMR5_Start();
        }
        
        /* E. Update state for next ISR */
        axis_was_dominant_last_isr[axis] = true;
    }
    else if (IsDominantAxis(axis))
    {
        /* ✅ CONTINUOUS: Still dominant, process step and update period */
        ProcessSegmentStep(axis);
        
        /* ✅ CRITICAL FIX (Oct 24): Update period from actual segment data */
        volatile axis_segment_state_t *state = &segment_state[axis];
        if (state->current_segment != NULL) {
            uint32_t period = state->current_segment->period;
            if (period > 65485) period = 65485;
            if (period <= OCMP_PULSE_WIDTH) period = OCMP_PULSE_WIDTH + 10;
            
            TMR5_PeriodSet((uint16_t)period);
            OCMP3_CompareValueSet((uint16_t)(period - OCMP_PULSE_WIDTH));
        }
    }
    else if (axis_was_dominant_last_isr[axis])
    {
        /* ✅ TRANSITION: Dominant → Subordinate (ONE-TIME TEARDOWN) */
        
        /* A. Disable OCR - stop continuous pulsing */
        OCMP3_Disable();
        
        /* B. OPTIMIZATION (Oct 25): Keep timer running - no stop/start penalty */
        // NOTE: Timer keeps running, OCR disable stops pulses
        
        /* C. Update state for next ISR */
        axis_was_dominant_last_isr[axis] = false;
    }
    else
    {
        /* ✅ SUBORDINATE: Pulse completed (triggered by Bresenham 0xFFFF), auto-disable */
        OCMP3_Disable();
        // OPTIMIZATION (Oct 25): Keep timer running - Bresenham will trigger next pulse
    }
}
#endif

// *****************************************************************************
// TMR1 @ 1kHz - Multi-Axis S-CURVE STATE MACHINE (Phase 1 - DISABLED)
// *****************************************************************************/

#if 0 // PHASE 2B: TMR1 S-curve disabled, using GRBL segment-based execution
      // Preserved for reference during Phase 2 development
      // TODO: Remove entirely once Phase 2C validated

static void TMR1_MultiAxisControl(uint32_t status, uintptr_t context)
{
    // DEBUG: Heartbeat to show TMR1 is running
    static uint16_t heartbeat = 0;
    if (++heartbeat > 1000)
    {
        LED1_Toggle(); // Blink every second to show TMR1 alive
        heartbeat = 0;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // STEP 1: Update DOMINANT AXIS (Master) - Full S-Curve Profile
    // ═══════════════════════════════════════════════════════════════════════
    // The dominant axis (longest distance) controls timing for ALL axes.
    // It calculates velocity from S-curve equations and generates OCR period.
    // Subordinate axes will scale this OCR period to maintain synchronization.

    axis_id_t dominant_axis = coord_move.dominant_axis;
    volatile scurve_state_t *s = &axis_state[dominant_axis];
    uint32_t dominant_ocr_period = 65000; // Default slow period

    if (s->active && s->current_segment != SEGMENT_IDLE)
    {
        // Get per-axis motion limits (cached for efficiency in interrupt context)
        float max_velocity = MotionMath_GetMaxVelocityStepsPerSec(dominant_axis);
        float max_accel = MotionMath_GetAccelStepsPerSec2(dominant_axis);
        float max_jerk = MotionMath_GetJerkStepsPerSec3(dominant_axis);

        s->elapsed_time += UPDATE_PERIOD_SEC;
        s->total_elapsed += UPDATE_PERIOD_SEC;

        float new_velocity = 0.0f;
        float new_accel = 0.0f;

        switch (s->current_segment)
        {
        case SEGMENT_JERK_ACCEL:
            new_accel = max_jerk * s->elapsed_time;
            new_velocity = 0.5f * max_jerk * s->elapsed_time * s->elapsed_time;

            if (s->elapsed_time >= s->t1_jerk_accel)
            {
                s->current_segment = SEGMENT_CONST_ACCEL;
                s->elapsed_time = 0.0f;
                s->current_velocity = s->v_end_segment1;
            }
            break;

        case SEGMENT_CONST_ACCEL:
            new_accel = max_accel;
            new_velocity = s->v_end_segment1 + (max_accel * s->elapsed_time);

            if (s->elapsed_time >= s->t2_const_accel)
            {
                s->current_segment = SEGMENT_JERK_DECEL_ACCEL;
                s->elapsed_time = 0.0f;
                s->current_velocity = s->v_end_segment2;
            }
            break;

        case SEGMENT_JERK_DECEL_ACCEL:
            new_accel = max_accel - (max_jerk * s->elapsed_time);
            new_velocity = s->v_end_segment2 +
                           (max_accel * s->elapsed_time) -
                           (0.5f * max_jerk * s->elapsed_time * s->elapsed_time);

            if (s->elapsed_time >= s->t3_jerk_decel_accel)
            {
                s->current_segment = SEGMENT_CRUISE;
                s->elapsed_time = 0.0f;
                s->current_velocity = s->cruise_velocity;
            }
            break;

        case SEGMENT_CRUISE:
            new_accel = 0.0f;
            new_velocity = s->cruise_velocity;

            // Transition based on TIME only (S-curve is time-based, not step-based)
            if (s->elapsed_time >= s->t4_cruise)
            {
                s->current_segment = SEGMENT_JERK_ACCEL_DECEL;
                s->elapsed_time = 0.0f;
            }
            break;

        case SEGMENT_JERK_ACCEL_DECEL:
            new_accel = -(max_jerk * s->elapsed_time);
            new_velocity = s->cruise_velocity -
                           (0.5f * max_jerk * s->elapsed_time * s->elapsed_time);

            if (s->elapsed_time >= s->t5_jerk_accel_decel)
            {
                s->current_segment = SEGMENT_CONST_DECEL;
                s->elapsed_time = 0.0f;
                s->current_velocity = s->v_end_segment5;
            }
            break;

        case SEGMENT_CONST_DECEL:
            new_accel = -max_accel;
            new_velocity = s->v_end_segment5 - (max_accel * s->elapsed_time);

            if (s->elapsed_time >= s->t6_const_decel)
            {
                s->current_segment = SEGMENT_JERK_DECEL_DECEL;
                s->elapsed_time = 0.0f;
                s->current_velocity = s->v_end_segment6;
            }
            break;

        case SEGMENT_JERK_DECEL_DECEL:
            new_accel = -(max_accel - max_jerk * s->elapsed_time);
            new_velocity = s->v_end_segment6 -
                           (max_accel * s->elapsed_time) +
                           (0.5f * max_jerk * s->elapsed_time * s->elapsed_time);

            // Transition based on TIME or velocity reaching zero
            // Step count check removed - S-curve is time-based!
            if (s->elapsed_time >= s->t7_jerk_decel_decel || new_velocity <= 0.1f)
            {
                s->current_segment = SEGMENT_COMPLETE;
                new_velocity = 0.0f;
            }
            break;

        case SEGMENT_COMPLETE:
            // CRITICAL: Stop timer then disable OCR
            axis_hw[dominant_axis].TMR_Stop();
            axis_hw[dominant_axis].OCMP_Disable();

#ifdef DEBUG_MOTION_BUFFER
            // Debug: Report final motion state
            UGS_Debug("[COMPLETE] axis=%d steps=%lu/%lu (%.1f%%) time=%.3fs\r\n",
                      dominant_axis, s->step_count, s->total_steps,
                      (float)s->step_count * 100.0f / (float)s->total_steps,
                      s->total_elapsed);
#endif

            // Clear state
            s->active = false;
            s->current_velocity = 0.0f;
            s->current_accel = 0.0f;
            s->current_segment = SEGMENT_IDLE;
            break; // Exit switch - per-axis control, no global flag needed

        default:
            break;
        }

        s->current_velocity = new_velocity;
        s->current_accel = new_accel;

        if (s->current_velocity < 0.0f)
            s->current_velocity = 0.0f;
        if (s->current_velocity > max_velocity)
            s->current_velocity = max_velocity;

        // CRITICAL SAFETY: Check if dominant axis reached target steps
        if (s->step_count >= s->total_steps)
        {
            // Stop immediately - target reached
            axis_hw[dominant_axis].TMR_Stop();
            axis_hw[dominant_axis].OCMP_Disable();
            s->active = false;
            s->current_velocity = 0.0f;
            s->current_accel = 0.0f;
            s->current_segment = SEGMENT_IDLE;
        }
        else if (s->active && s->current_velocity > 1.0f)
        {
            // Calculate OCR period from dominant axis velocity
            // This period will be scaled for subordinate axes
            dominant_ocr_period = (uint32_t)((float)TMR_CLOCK_HZ / s->current_velocity);

            if (dominant_ocr_period > 65485)
                dominant_ocr_period = 65485;
            if (dominant_ocr_period <= OCMP_PULSE_WIDTH)
                dominant_ocr_period = OCMP_PULSE_WIDTH + 10;

            // Update dominant axis OCR hardware
            axis_hw[dominant_axis].TMR_PeriodSet(dominant_ocr_period);
            axis_hw[dominant_axis].OCMP_CompareValueSet(dominant_ocr_period - OCMP_PULSE_WIDTH);
            axis_hw[dominant_axis].OCMP_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // STEP 2: Update SUBORDINATE AXES (Slaves) - OCR Period Scaling
    // ═══════════════════════════════════════════════════════════════════════
    // Subordinate axes DO NOT calculate velocity from S-curve equations.
    // Instead, they scale the dominant axis OCR period by their step ratio.
    // This guarantees synchronized completion with exact step counts!
    //
    // Formula: subordinate_OCR_period = dominant_OCR_period × step_ratio
    // Where:   step_ratio = dominant_steps / subordinate_steps
    //
    // Example: Dominant Y=800 steps, Subordinate X=400 steps
    //          step_ratio = 800/400 = 2.0
    //          If dominant_period = 3906, then subordinate_period = 7812
    //          Result: X runs at half frequency, generates exactly 400 steps ✅

    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        // Skip dominant axis (already handled above)
        if (axis == dominant_axis)
            continue;

        volatile scurve_state_t *sub = &axis_state[axis];

        // Skip inactive axes
        if (!sub->active || sub->current_segment == SEGMENT_IDLE)
            continue;

        // Check if axis velocity scale is zero (axis not moving in this coordinated move)
        if (coord_move.axis_velocity_scale[axis] == 0.0f)
            continue;

        // Check if dominant axis completed motion (subordinate must also stop!)
        // NOTE: Check this BEFORE copying segment state, because dominant may have
        // already transitioned from SEGMENT_COMPLETE to SEGMENT_IDLE!
        if (!axis_state[dominant_axis].active)
        {
            // Stop subordinate axis - dominant has completed
            axis_hw[axis].TMR_Stop();
            axis_hw[axis].OCMP_Disable();
            sub->active = false;
            sub->current_velocity = 0.0f;
            sub->current_accel = 0.0f;
            sub->current_segment = SEGMENT_IDLE;
            continue;
        }

        // Update segment timing from dominant axis (time synchronization)
        sub->current_segment = axis_state[dominant_axis].current_segment;
        sub->elapsed_time = axis_state[dominant_axis].elapsed_time;
        sub->total_elapsed = axis_state[dominant_axis].total_elapsed;

        // CRITICAL SAFETY: Check if subordinate axis reached target steps
        if (sub->step_count >= sub->total_steps)
        {
            // Stop immediately - target reached
            axis_hw[axis].TMR_Stop();
            axis_hw[axis].OCMP_Disable();
            sub->active = false;
            sub->current_velocity = 0.0f;
            sub->current_accel = 0.0f;
            sub->current_segment = SEGMENT_IDLE;
            continue;
        }

        // Calculate subordinate OCR period by scaling dominant period
        // step_ratio = dominant_steps / subordinate_steps
        // subordinate_period = dominant_period × step_ratio
        float step_ratio = (float)axis_state[dominant_axis].total_steps / (float)sub->total_steps;
        uint32_t subordinate_ocr_period = (uint32_t)((float)dominant_ocr_period * step_ratio);

        // Apply same safety limits as dominant axis
        if (subordinate_ocr_period > 65485)
            subordinate_ocr_period = 65485;
        if (subordinate_ocr_period <= OCMP_PULSE_WIDTH)
            subordinate_ocr_period = OCMP_PULSE_WIDTH + 10;

        // Update subordinate axis OCR hardware
        axis_hw[axis].TMR_PeriodSet(subordinate_ocr_period);
        axis_hw[axis].OCMP_CompareValueSet(subordinate_ocr_period - OCMP_PULSE_WIDTH);
        axis_hw[axis].OCMP_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);

        // Update velocity for informational purposes (not used for control!)
        // velocity = TMR_CLOCK_HZ / period
        sub->current_velocity = (float)TMR_CLOCK_HZ / (float)subordinate_ocr_period;
    }

    // No global motion_running flag needed - each axis manages its own state
}
#endif // PHASE 2B: End of TMR1 S-curve code

// *****************************************************************************
// Public API
// *****************************************************************************/

void MultiAxis_Initialize(void)
{
    // Initialize motion math settings (GRBL-compatible defaults)
    MotionMath_InitializeSettings();

    // Initialize step execution strategies (default: Bresenham for all axes)
    // Can be changed at runtime via MultiAxis_SetStepStrategy()
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        axis_step_executor[axis] = Execute_Bresenham_Strategy;
    }

    // Initialize all axis states
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        axis_state[axis].current_segment = SEGMENT_IDLE;
        axis_state[axis].step_count = 0;
        axis_state[axis].active = false;

        // Initialize absolute machine position (all axes at origin on startup)
        // CRITICAL FIX (October 19, 2025): Must track absolute position for GRBL feedback!
        machine_position[axis] = 0;

        // ✅ Initialize transition detection state (Oct 24, 2025)
        axis_was_dominant_last_isr[axis] = false;

        // Disable all drivers on startup (safe default)
        MultiAxis_DisableDriver(axis);
    }

    // Register OCR callbacks
    axis_hw[AXIS_X].OCMP_CallbackRegister(OCMP5_StepCounter_X, 0);
    axis_hw[AXIS_Y].OCMP_CallbackRegister(OCMP1_StepCounter_Y, 0);
    axis_hw[AXIS_Z].OCMP_CallbackRegister(OCMP4_StepCounter_Z, 0);

    /* PHASE 2B: TMR1 S-curve control disabled (will be replaced with segment execution)
     * TMR1 freed up for future 6-axis expansion or other uses.
     * Segment execution will be driven by OCR step-complete callbacks instead.
     *
     * TODO: Remove TMR1_MultiAxisControl() ISR entirely once Phase 2B proven working.
     */
    // TMR1_CallbackRegister(TMR1_MultiAxisControl, 0);  // ← DISABLED for Phase 2B
    // TMR1_Start();                                      // ← DISABLED for Phase 2B

    // Initialize motion manager (GRBL-style auto-start with TMR9 @ 100Hz)
    // Must be called LAST after all motion subsystems initialized
    MotionManager_Initialize();
}

/*! \brief Execute single-axis motion with S-curve profile
 *
 *  \param axis Axis identifier (must be < NUM_AXES)
 *  \param steps Number of steps to move (absolute value used)
 *  \param forward Direction: true = forward, false = reverse
 *
 *  MISRA Rule 8.7: Parameter validation with early return
 *  MISRA Rule 17.4: Array bounds checking
 */
void MultiAxis_MoveSingleAxis(axis_id_t axis, int32_t steps, bool forward)
{
    //   LED1_Toggle(); // DEBUG: Show function called

    // MISRA-compliant parameter validation
    assert(axis < NUM_AXES); // Development-time check

    if (axis >= NUM_AXES)
    {
        return; // Defensive: reject invalid axis
    }

    volatile scurve_state_t *s = &axis_state[axis];

    assert(s != NULL); // Verify pointer validity

    uint32_t abs_steps = (uint32_t)((steps < 0) ? -steps : steps);

    if (abs_steps == 0U)
    {
        return; // No motion required
    }

    // Ensure hardware is stopped before restarting
    axis_hw[axis].OCMP_Disable();
    axis_hw[axis].TMR_Stop();

    // Enable driver before motion (DRV8825 active-low enable)
    MultiAxis_EnableDriver(axis);

    // Calculate S-curve profile
    calculate_scurve_profile(axis, abs_steps);

    // Initialize state
    s->current_segment = SEGMENT_JERK_ACCEL;
    s->elapsed_time = 0.0f;
    s->total_elapsed = 0.0f;
    s->current_velocity = 0.0f;
    s->current_accel = 0.0f;
    s->step_count = 0U;
    s->direction_forward = forward;
    s->active = true;

    // Set direction pin using dynamic lookup
    if (forward)
    {
        MultiAxis_SetDirection(axis);
    }
    else
    {
        MultiAxis_ClearDirection(axis);
    }

    // Start OCR - set initial period for first segment
    uint32_t initial_period = 65000U; // Slow start for S-curve
    axis_hw[axis].TMR_PeriodSet(initial_period);
    axis_hw[axis].OCMP_CompareValueSet(initial_period - OCMP_PULSE_WIDTH);
    axis_hw[axis].OCMP_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);

    axis_hw[axis].OCMP_Enable();
    axis_hw[axis].TMR_Start();

    // Per-axis control - no global motion_running flag needed
}

/*! \brief Check if any axis is currently moving
 *
 *  PHASE 2B: Now checks segment execution state instead of S-curve state.
 *
 *  \return true if motion in progress, false if all axes idle
 */
bool MultiAxis_IsBusy(void)
{
    // Check if any axis is actively executing segments
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        if (segment_state[axis].active)
        {
            return true;
        }
    }
    return false;
}

/*! \brief Check if specific axis is moving
 *
 *  \param axis Axis identifier to check
 *  \return true if axis active, false if idle or invalid axis
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 */
bool MultiAxis_IsAxisBusy(axis_id_t axis)
{
    assert(axis < NUM_AXES); // Development-time check

    if (axis >= NUM_AXES)
    {
        return false; // Defensive: invalid axis always idle
    }

    return axis_state[axis].active;
}

/*! \brief Emergency stop all axes
 *
 *  Immediately halts all motion and disables OCR hardware.
 *  Also disables all stepper drivers for safety.
 *  Safe to call at any time, including from interrupt context.
 */
void MultiAxis_StopAll(void)
{
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        assert(axis < NUM_AXES); // Loop bounds check

        // Phase 2B: Stop hardware and clear segment execution state
        axis_hw[axis].OCMP_Disable();
        axis_hw[axis].TMR_Stop();

        // CRITICAL FIX (Oct 19, 2025): Clear Phase 2B segment state, not Phase 1!
        segment_state[axis].current_segment = NULL;
        segment_state[axis].active = false;
        segment_state[axis].step_count = 0;

        // Disable driver for safety (DRV8825 high-Z state)
        MultiAxis_DisableDriver(axis);
    }

    // No global motion_running flag - each axis manages its own state
}

/*! \brief Start segment execution from GRBL stepper buffer (Phase 2B)
 *
 *  Kicks off segment execution for all axes that have pending segments.
 *  Called by motion_manager when new planner block is available.
 *
 *  Dave's Understanding:
 *    - Segment prep (TMR9 @ 100Hz) has filled segment buffer with segments
 *    - This function starts hardware execution of those segments
 *    - Each axis independently pulls segments from shared buffer
 *    - OCR callbacks auto-advance to next segment when current done
 *
 *  \return true if segment execution started, false if no segments available
 */
bool MultiAxis_StartSegmentExecution(void)
{
 
    // Try to get first segment for each axis
    const st_segment_t *first_seg = GRBLStepper_GetNextSegment();

    if (first_seg == NULL)
    {
        return false; // No segments available
    }

    /* ✅ CRITICAL FIX (Oct 25): Only enable motion_active if segments exist!
     * Previously set motion_active=true BEFORE null check, causing it to stay
     * true even when no segments available. This let OCR ISRs continue firing
     * after program completion, causing "walk to Japan" bug.
     */
    motion_active = true;  // ✅ CORRECT - only set when segments actually exist!


    // Determine dominant axis (axis with MOST steps)
    // CRITICAL FIX (Oct 21, 2025): Use SAME logic as ProcessSegmentStep() line 1006
    // GRBL can have rounding where no axis has steps[axis] == n_step
    segment_completed_by_axis = 0; // Clear previous segment's mask
    uint32_t max_steps_startup = 0;
    axis_id_t dominant_candidate_startup = AXIS_X;

    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        if (first_seg->steps[axis] > max_steps_startup)
        {
            max_steps_startup = first_seg->steps[axis];
            dominant_candidate_startup = axis;
        }
    }

    // Mark the axis with most steps as dominant
    if (max_steps_startup > 0)
    {
        segment_completed_by_axis = (1 << dominant_candidate_startup);
#ifdef DEBUG_MOTION_BUFFER
        // Trace dominant axis selection and per-axis step counts
        const char *axis_names[] = {"X", "Y", "Z", "A"};
        UGS_Printf("[SEG_START] Dominant=%s bitmask=0x%02X n_step=%lu X=%lu Y=%lu Z=%lu A=%lu period=%lu\r\n",
                   axis_names[dominant_candidate_startup],
                   segment_completed_by_axis,
                   (unsigned long)first_seg->n_step,
                   (unsigned long)first_seg->steps[AXIS_X],
                   (unsigned long)first_seg->steps[AXIS_Y],
                   (unsigned long)first_seg->steps[AXIS_Z],
                   (unsigned long)first_seg->steps[AXIS_A],
                   (unsigned long)first_seg->period);
#endif
    }
    else
    {
        // DEBUG: Print segment details if no motion at all
        UGS_Printf("ERROR: No motion in segment! n_step=%lu, X=%lu, Y=%lu, Z=%lu, A=%lu\r\n",
                   (unsigned long)first_seg->n_step,
                   (unsigned long)first_seg->steps[AXIS_X],
                   (unsigned long)first_seg->steps[AXIS_Y],
                   (unsigned long)first_seg->steps[AXIS_Z],
                   (unsigned long)first_seg->steps[AXIS_A]);
        return false; // No motion in segment!
    }

    // Start axes that are IDLE and have motion in this segment
    bool any_axis_started = false;

    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        volatile axis_segment_state_t *state = &segment_state[axis];

        // CRITICAL FIX (Oct 19, 2025): If axis is active but NOT dominant in new segment,
        // it must transition from OCR (dominant) to bit-bang (subordinate)!
        // Disable OCR hardware before continuing (leave timer running).
        bool is_dominant = (segment_completed_by_axis & (1 << axis)) != 0;

        if (state->active && !is_dominant && first_seg->steps[axis] > 0)
        {
            // Axis was dominant, now subordinate → DISABLE OCR (timer keeps running)
            axis_hw[axis].OCMP_Disable();
            // OPTIMIZATION (Oct 25): No TMR_Stop() - timer stays running

            // CRITICAL FIX (Oct 20, 2025): Set active=false for subordinates!
            // Subordinates are bit-banged, not independently executing
            // Keeping active=true causes MultiAxis_IsBusy() to stay true forever!
            state->active = false;
        }

        // Skip if axis is already active (busy with previous segment)
        if (state->active)
        {
#ifdef DEBUG_MOTION_BUFFER
            const char *axis_names[] = {"X", "Y", "Z", "A"};
            UGS_Printf("  [SEG_START] %s already active, skipping\r\n", axis_names[axis]);
#endif
            continue;
        }

        // Check if this axis has motion in the next segment (non-zero step count)
        if (first_seg->steps[axis] > 0)
        {
            // Initialize segment execution state
            state->current_segment = first_seg;
            state->step_count = 0;
            
            // CRITICAL FIX (October 24, 2025): Use GRBL's pre-calculated Bresenham counter!
            // GRBL prepares segments with optimal Bresenham starting values for subordinate axes.
            // Setting to 0 caused uneven step distribution and position errors.
            // Original bug: state->bresenham_counter = 0;  // ❌ WRONG!
            state->bresenham_counter = first_seg->bresenham_counter[axis];

            // SANITY CHECK (Oct 21, 2025): Initialize block counters from segment data
            // Segment carries block's total steps - use this for commanded count
            state->block_steps_commanded = first_seg->block_steps[axis];
            state->block_steps_executed = 0;  // Incremented by OCR ISR on each pulse

            // CRITICAL FIX (Oct 20, 2025): Only set active=true for DOMINANT axis!
            // Subordinate axes are bit-banged by dominant ISR - they have NO independent state.
            // Setting subordinates to active=true causes MultiAxis_IsBusy() to return true forever!
            state->active = is_dominant;

            // Set direction GPIO based on direction bits
            // CRITICAL: GRBL sets direction_bits=1 for NEGATIVE motion!
            if (first_seg->direction_bits & (1 << axis))
            {
                // Negative direction (direction_bits=1)
                switch (axis)
                {
                case AXIS_X:
                    DirX_Clear(); // Inverted: bit=1 → Clear for negative
                    break;
                case AXIS_Y:
                    DirY_Clear(); // Inverted: bit=1 → Clear for negative
                    break;
                case AXIS_Z:
                    DirZ_Clear(); // Inverted: bit=1 → Clear for negative
                    break;
                case AXIS_A:
                    DirA_Clear(); // Inverted: bit=1 → Clear for negative
                    break;
                default:
                    break;
                }
            }
            else
            {
                // Positive direction (direction_bits=0)
                switch (axis)
                {
                case AXIS_X:
                    DirX_Set(); // Inverted: bit=0 → Set for positive
                    break;
                case AXIS_Y:
                    DirY_Set(); // Inverted: bit=0 → Set for positive
                    break;
                case AXIS_Z:
                    DirZ_Set(); // Inverted: bit=0 → Set for positive
                    break;
                case AXIS_A:
                    DirA_Set(); // Inverted: bit=0 → Set for positive
                    break;
                default:
                    break;
                }
            }

            // Enable driver
            MultiAxis_EnableDriver(axis);

#ifdef DEBUG_MOTION_BUFFER
            {
                const char *axis_names[] = {"X", "Y", "Z", "A"};
                const char *dom_str = is_dominant ? "DOMINANT" : "subordinate";
                UGS_Printf("  [SEG_START] %s: enabled driver, role=%s\r\n", axis_names[axis], dom_str);
            }
#endif

            // Configure OCR period (step rate)
            uint32_t period = first_seg->period;
            if (period > 65485)
                period = 65485; // 16-bit limit
            if (period <= OCMP_PULSE_WIDTH)
                period = OCMP_PULSE_WIDTH + 10;

            // ═════════════════════════════════════════════════════════════════════
            // HYBRID OCR/BIT-BANG APPROACH (Dave's Architecture)
            // ═════════════════════════════════════════════════════════════════════
            // ONLY start OCR hardware for DOMINANT axis (longest distance)
            // Subordinate axes will be bit-banged in dominant's ISR via function pointer
            //
            // Benefits:
            //   - Single ISR (no synchronization issues)
            //   - Matches original GRBL architecture
            //   - Eliminates rocket ship bug
            // ═════════════════════════════════════════════════════════════════════

            // NOTE: is_dominant already calculated at line 1699 - reuse that value!

            if (is_dominant)
            {
                // DOMINANT AXIS: Use OCR hardware in continuous pulse mode
                // OCM=0b101 already configured by MCC (Dual Compare Continuous)

                // Configure OCR period (step rate)
                axis_hw[axis].TMR_PeriodSet((uint16_t)period);
                axis_hw[axis].OCMP_CompareValueSet((uint16_t)(period - OCMP_PULSE_WIDTH));
                axis_hw[axis].OCMP_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);

                // Start hardware (OCR pulses + ISR callbacks)
                axis_hw[axis].OCMP_Enable();
                axis_hw[axis].TMR_Start();

#ifdef DEBUG_MOTION_BUFFER
                {
                    const char *axis_names[] = {"X", "Y", "Z", "A"};
                    UGS_Printf("  [SEG_START] %s: OCR STARTED continuous mode, period=%lu\r\n", 
                               axis_names[axis], (unsigned long)period);
                }
#endif

                any_axis_started = true;
            }
            else
            {
                // SUBORDINATE AXIS: Single-pulse-on-demand using OCR
                // Pattern:
                //   - Keep timer running continuously (TMRx_Start)
                //   - Keep OCMP DISABLED until a pulse is needed
                //   - When Bresenham requests a step:
                //       OCxR=5; OCxRS=36; TMRx=0xFFFF; OCMP_Enable();
                //   - ISR (falling edge) auto-disables OCMP to stop further pulses

                // Ensure subordinate OCMP is disabled at segment start (no stray pulse)
                axis_hw[axis].OCMP_Disable();

                // Set timer period (must exceed pulse width); run it continuously
                // so TMRx=0xFFFF forces an immediate rollover when enabling OCMP.
                axis_hw[axis].TMR_PeriodSet(200); // ~128µs period @ 1.5625MHz
                axis_hw[axis].TMR_Start();

#ifdef DEBUG_MOTION_BUFFER
                {
                    const char *axis_names[] = {"X", "Y", "Z", "A"};
                    UGS_Printf("  [SEG_START] %s: TMR STARTED for subordinate (OCMP disabled)\r\n", 
                               axis_names[axis]);
                }
#endif
            }
        }
    }

    return any_axis_started;
}

/*! \brief Get absolute machine position for axis
 *
 *  CRITICAL FIX (October 19, 2025): Returns ABSOLUTE position, not move progress!
 *
 *  \param axis Axis identifier to query
 *  \return Absolute machine position in steps (signed), or 0 if invalid axis
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 */
int32_t MultiAxis_GetStepCount(axis_id_t axis)
{
    assert(axis < NUM_AXES); // Development-time check

    if (axis >= NUM_AXES)
    {
        return 0; // Defensive: return safe value for invalid axis
    }

    // CRITICAL FIX (Oct 20, 2025): Return signed int32_t!
    // machine_position[] is signed (can be negative for bidirectional motion)
    // Casting to uint32_t was causing huge positive numbers for negative positions!
    // MotionMath_StepsToMM() expects signed int32_t parameter
    return machine_position[axis];
}

/*! \brief Update absolute machine position after move completion
 *
 *  CRITICAL FIX (October 19, 2025): Call this when motion completes!
 *
 *  This function adds the move delta to the absolute machine position.
 *  Must be called from MotionManager_TMR9_ISR() after discarding completed block.
 *
 *  \param steps Array of step deltas [X, Y, Z, A] (signed: negative = backward)
 *
 *  MISRA Rule 17.4: Bounds checking before array access
 */
void MultiAxis_UpdatePosition(const int32_t steps[NUM_AXES])
{
    assert(steps != NULL); // Development-time check

    if (steps == NULL)
    {
        return; // Defensive: null pointer check
    }

    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        machine_position[axis] += steps[axis];
    }

/* DEBUG: Show updated position */
#ifdef DEBUG_MOTION_BUFFER
    UGS_Printf("[POSITION] Updated: X=%ld Y=%ld Z=%ld A=%ld\r\n",
               (long)machine_position[AXIS_X],
               (long)machine_position[AXIS_Y],
               (long)machine_position[AXIS_Z],
               (long)machine_position[AXIS_A]);
#endif
}

/*******************************************************************************
  Time-Based Vector Interpolation for Multi-Axis S-Curve Motion

  Algorithm:
  1. Find dominant axis (longest distance)
  2. Calculate S-curve profile for dominant axis
  3. Scale other axes' velocities to match dominant axis timing
  4. All axes use same segment times, different velocities
*******************************************************************************/

// coord_move is now declared at file scope (line ~415) for ISR access

/*! \brief Calculate coordinated multi-axis move with time synchronization
 *
 *  \param steps Array of target steps for each axis [X, Y, Z, A]
 *
 *  Algorithm:
 *    1. Find dominant axis (max absolute steps)
 *    2. Calculate S-curve profile for dominant axis → determines total time
 *    3. Scale velocities of other axes: v_axis = (distance_axis / total_time)
 *    4. All axes share same segment timing (t1-t7 from dominant axis)
 */
bool MultiAxis_CalculateCoordinatedMove(int32_t steps[NUM_AXES])
{
    assert(steps != NULL);

    if (steps == NULL)
    {
        return false;
    }

    // Step 1: Find dominant axis (longest distance)
    uint32_t max_steps = 0;
    axis_id_t dominant = AXIS_X;

    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        uint32_t abs_steps = (uint32_t)abs(steps[axis]);

        if (abs_steps > max_steps)
        {
            max_steps = abs_steps;
            dominant = axis;
        }
    }

    if (max_steps == 0U)
    {
        return false; // No motion required
    }

    coord_move.dominant_axis = dominant;

    // Step 2: Calculate S-curve profile for dominant axis
    // This determines the total move time and segment times (t1-t7)
    volatile scurve_state_t *dominant_state = &axis_state[dominant];

    if (!calculate_scurve_profile(dominant, max_steps))
    {
        return false;
    }

    // Calculate total move time from dominant axis S-curve
    coord_move.total_move_time = dominant_state->t1_jerk_accel +
                                 dominant_state->t2_const_accel +
                                 dominant_state->t3_jerk_decel_accel +
                                 dominant_state->t4_cruise +
                                 dominant_state->t5_jerk_accel_decel +
                                 dominant_state->t6_const_decel +
                                 dominant_state->t7_jerk_decel_decel;

    // Step 3: Calculate velocity scaling for subordinate axes
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        if (axis == dominant)
        {
            coord_move.axis_velocity_scale[axis] = 1.0f; // Dominant runs at full profile
        }
        else
        {
            uint32_t axis_steps = (uint32_t)abs(steps[axis]);

            if (axis_steps == 0U)
            {
                coord_move.axis_velocity_scale[axis] = 0.0f; // Axis not moving
            }
            else
            {
                // Scale velocity so this axis completes in same time as dominant
                // scale_factor = axis_distance / dominant_distance
                coord_move.axis_velocity_scale[axis] =
                    (float)axis_steps / (float)max_steps;
            }
        }
    }

    return true;
}

/*! \brief Execute time-synchronized coordinated multi-axis motion
 *
 *  MISRA C compliant implementation of coordinated motion where the dominant
 *  axis (longest distance) determines total move time, and all subordinate
 *  axes scale their velocities to finish simultaneously.
 *
 *  \param steps Array of signed step counts [X, Y, Z, A]
 *               Positive = forward direction
 *               Negative = reverse direction
 *               Zero = axis remains stationary
 *
 *  Algorithm Flow:
 *  ──────────────
 *  1. Validate parameters and calculate dominant axis (via helper)
 *  2. Dominant axis calculates full S-curve profile → determines total_time
 *  3. For each active axis:
 *       a. Copy segment times (t1-t7) from dominant axis
 *       b. Scale velocities by ratio: steps[axis] / steps[dominant]
 *       c. Initialize hardware (direction GPIO, OCR modules)
 *       d. Start synchronized motion
 *
 *  MISRA Compliance Notes:
 *  ─────────────────────
 *  - Rule 17.4: Array indexing checked via NUM_AXES bounds in loop
 *  - Rule 8.13: Volatile pointer required for ISR-shared state
 *  - Rule 10.1: Explicit type conversions with range validation
 *  - Rule 14.4: Single exit via early return for invalid parameters
 *
 *  Thread Safety:
 *  ─────────────
 *  - Must call from main context only (not from ISR)
 *  - Uses volatile axis_state[] modified by TMR1 ISR @ 1kHz
 *  - Hardware modules (OCR/TMR) started in synchronized sequence
 */
void MultiAxis_ExecuteCoordinatedMove(int32_t steps[NUM_AXES])
{
    /* MISRA Rule 17.4: Parameter validation */
    assert(steps != NULL); /* Development-time check */

    /* Calculate coordinated move parameters (dominant axis, velocity scales) */
    if (!MultiAxis_CalculateCoordinatedMove(steps))
    {
        return; /* Defensive: Invalid move parameters (all axes zero or error) */
    }

    /* Get dominant axis S-curve profile - this determines timing for all axes
     * MISRA Rule 8.13: Volatile required as axis_state modified by TMR1 ISR */
    volatile scurve_state_t *dominant = &axis_state[coord_move.dominant_axis];

    /* ═══════════════════════════════════════════════════════════════════════
     * TIME SYNCHRONIZATION: Critical Section
     * ═══════════════════════════════════════════════════════════════════════
     * All axes MUST share identical segment times (t1-t7) from dominant axis.
     * This ensures simultaneous completion regardless of distance ratios.
     *
     * Why this works:
     * ──────────────
     * - Dominant axis: Calculates S-curve for max(|steps|) → sets total_time
     * - Subordinate axes: Use same total_time but scaled velocities
     * - Formula: v_subordinate = v_dominant × (steps_subordinate / steps_dominant)
     * - Result: distance = velocity × time is satisfied for ALL axes
     * ═══════════════════════════════════════════════════════════════════════ */

    /* MISRA Rule 17.4: Loop bounds checked against NUM_AXES constant */
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        /* Skip axes with zero motion (velocity_scale == 0.0f indicates no movement)
         * CRITICAL: Must explicitly deactivate these axes to prevent them from
         * continuing with state from previous moves! */
        if (coord_move.axis_velocity_scale[axis] == 0.0f)
        {
#ifdef DEBUG_MOTION_BUFFER
            /* DEBUG: Print which axes are being skipped */
            const char *axis_names[] = {"X", "Y", "Z", "A"};
            UGS_Printf("[COORD] Skipping %s axis (velocity_scale=0.0)\r\n", axis_names[axis]);
#endif

            /* Explicitly deactivate this axis - it's not part of this move */
            volatile scurve_state_t *s = &axis_state[axis];
            s->active = false;
            s->current_segment = SEGMENT_IDLE;
            s->current_velocity = 0.0f;
            s->current_accel = 0.0f;

            /* Stop hardware if it was running */
            axis_hw[axis].TMR_Stop();
            axis_hw[axis].OCMP_Disable();

            continue; /* MISRA Rule 14.5: Continue acceptable for skip logic */
        }

        /* Get axis state structure (volatile for ISR safety)
         * MISRA Rule 8.13: Cannot be const as ISR will modify */
        volatile scurve_state_t *s = &axis_state[axis];

        /* ─────────────────────────────────────────────────────────────────
         * STEP 1: Copy Segment Times (Time Synchronization)
         * ─────────────────────────────────────────────────────────────────
         * CRITICAL: All axes use IDENTICAL segment times from dominant axis.
         * This is the core of coordinated motion - shared timing ensures
         * all axes complete their moves at exactly the same instant.
         * ───────────────────────────────────────────────────────────────── */
        s->t1_jerk_accel = dominant->t1_jerk_accel;             /* Jerk ramp up */
        s->t2_const_accel = dominant->t2_const_accel;           /* Constant accel */
        s->t3_jerk_decel_accel = dominant->t3_jerk_decel_accel; /* Jerk ramp down */
        s->t4_cruise = dominant->t4_cruise;                     /* Constant velocity */
        s->t5_jerk_accel_decel = dominant->t5_jerk_accel_decel; /* Decel jerk ramp */
        s->t6_const_decel = dominant->t6_const_decel;           /* Constant decel */
        s->t7_jerk_decel_decel = dominant->t7_jerk_decel_decel; /* Final jerk ramp */

        /* ─────────────────────────────────────────────────────────────────
         * STEP 2: Scale Velocities (Distance Compensation)
         * ─────────────────────────────────────────────────────────────────
         * Each axis moves at a proportional velocity to maintain the correct
         * distance ratio while using the same time intervals.
         *
         * Formula: v_axis = v_dominant × (distance_axis / distance_dominant)
         * ───────────────────────────────────────────────────────────────── */
        float velocity_scale = coord_move.axis_velocity_scale[axis];

        s->cruise_velocity = dominant->cruise_velocity * velocity_scale;
        s->v_end_segment1 = dominant->v_end_segment1 * velocity_scale;
        s->v_end_segment2 = dominant->v_end_segment2 * velocity_scale;
        s->v_end_segment3 = dominant->v_end_segment3 * velocity_scale;
        s->v_end_segment5 = dominant->v_end_segment5 * velocity_scale;
        s->v_end_segment6 = dominant->v_end_segment6 * velocity_scale;

        /* ─────────────────────────────────────────────────────────────────
         * STEP 3: Initialize Axis State
         * ─────────────────────────────────────────────────────────────────
         * Set initial conditions for motion execution.
         * MISRA Rule 10.1: Explicit type conversion with bounds check.
         * ───────────────────────────────────────────────────────────────── */

        /* Convert signed steps to unsigned absolute value for distance tracking
         * MISRA Rule 10.1: Explicit cast required for abs() return */
        uint32_t abs_steps = (uint32_t)abs(steps[axis]);
        s->total_steps = abs_steps;               /* Target distance (steps) */
        s->step_count = 0U;                       /* Current position (incremented by OCR ISR) */
        s->direction_forward = (steps[axis] > 0); /* Direction from sign of input */

        /* Initialize S-curve state machine */
        s->current_segment = SEGMENT_JERK_ACCEL; /* Start with first segment */
        s->elapsed_time = 0.0f;                  /* Time within current segment */
        s->total_elapsed = 0.0f;                 /* Total time since motion start */
        s->current_velocity = 0.0f;              /* Initial velocity (always zero) */
        s->current_accel = 0.0f;                 /* Initial acceleration (zero) */
        s->active = true;                        /* Mark axis as active for TMR1 ISR */

        /* ─────────────────────────────────────────────────────────────────
         * STEP 4: Configure Hardware (Direction GPIO)
         * ─────────────────────────────────────────────────────────────────
         * CRITICAL: Direction must be set BEFORE enabling step pulses.
         * DRV8825 requires 200ns setup time (GPIO write provides this).
         * ───────────────────────────────────────────────────────────────── */
        if (s->direction_forward)
        {
            MultiAxis_SetDirection(axis); /* GPIO HIGH for forward */
        }
        else
        {
            MultiAxis_ClearDirection(axis); /* GPIO LOW for reverse */
        }

        /* ─────────────────────────────────────────────────────────────────
         * STEP 5: Start Hardware Motion (OCR + Timer)
         * ─────────────────────────────────────────────────────────────────
         * Configure OCR dual-compare mode for step pulse generation.
         *
         * Timing:
         * - Initial period = 65000 counts (slow start for S-curve)
         * - Pulse width = 40 counts (25.6µs @ 1.5625MHz - exceeds DRV8825 1.9µs minimum)
         * - TMR must be restarted for each move (stops when motion completes)
         *
         * MISRA Rule 10.3: Explicit cast for unsigned arithmetic
         * ───────────────────────────────────────────────────────────────── */
        uint32_t initial_period = 65000U; /* Slow initial velocity */

        axis_hw[axis].TMR_PeriodSet(initial_period);
        axis_hw[axis].OCMP_CompareValueSet(initial_period - OCMP_PULSE_WIDTH);
        axis_hw[axis].OCMP_CompareSecondaryValueSet(OCMP_PULSE_WIDTH);

        axis_hw[axis].OCMP_Enable(); /* Enable OCR output */
        axis_hw[axis].TMR_Start();   /* Start timer (CRITICAL - must restart each move) */
    }

    /* Visual feedback: LED1 solid during coordinated motion */
    LED1_Set();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DEBUG FUNCTIONS (Non-blocking ISR counter access)
 * ═══════════════════════════════════════════════════════════════════════════
 * These functions provide read access to volatile counters that are incremented
 * in ISR context. Main loop can call these after motion completes to print
 * debug info without blocking the ISR.
 * ═══════════════════════════════════════════════════════════════════════════ */

uint32_t MultiAxis_GetDebugYStepCount(void)
{
    return debug_total_y_pulses;
}

uint32_t MultiAxis_GetDebugSegmentCount(void)
{
    return debug_segment_count;
}

void MultiAxis_ResetDebugCounters(void)
{
    debug_total_y_pulses = 0;
    debug_segment_count = 0;
}

bool MultiAxis_GetAxisState(axis_id_t axis, uint32_t *step_count, bool *active)
{
    if (axis >= NUM_AXES || step_count == NULL || active == NULL)
    {
        return false;
    }

    *step_count = segment_state[axis].step_count;
    *active = segment_state[axis].active;
    return true;
}
