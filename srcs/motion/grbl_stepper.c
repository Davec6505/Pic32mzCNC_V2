/**
 * @file grbl_stepper.c
 * @brief GRBL stepper driver implementation (Phase 2)
 *
 * Ports GRBL's segment-based stepper algorithm to PIC32MZ with OCR hardware.
 *
 * CRITICAL PATTERN (Dave's Understanding):
 * 1. Planner calculates max velocity/acceleration/junctions ONCE per block
 * 2. THIS MODULE breaks block into 2mm segments continuously
 * 3. Each segment: Interpolate velocity, run Bresenham, calculate OCR period
 * 4. OCR hardware executes segments automatically (zero CPU for pulses)
 *
 * @date October 19, 2025
 * @author Dave (with AI assistance)
 */

// *****************************************************************************
// Section: Included Files
// *****************************************************************************

#include "definitions.h"
#include "motion/grbl_stepper.h"
#include "motion/grbl_planner.h"
#include "motion/motion_math.h"
#include "ugs_interface.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// *****************************************************************************
// Section: Local Variables
// *****************************************************************************

/**
 * @brief Segment buffer (ring buffer for prepared segments)
 */
static st_segment_buffer_t seg_buffer;

/**
 * @brief Segment preparation state (tracks progress through current block)
 */
static st_prep_t prep;

/**
 * @brief Statistics for debugging/tuning
 */
static uint32_t stats_total_segments = 0;
static uint32_t stats_buffer_underruns = 0;

// *****************************************************************************
// Section: Helper Functions (Forward Declarations)
// *****************************************************************************

static void prep_new_block(void);
static bool prep_segment(void);
static uint32_t calculate_segment_period(float velocity_mm_sec, axis_id_t dominant_axis);

// *****************************************************************************
// Section: Public API Implementation
// *****************************************************************************

void GRBLStepper_Initialize(void)
{
    // Clear segment buffer
    memset(&seg_buffer, 0, sizeof(seg_buffer));
    seg_buffer.head = 0;
    seg_buffer.tail = 0;
    seg_buffer.count = 0;

    // Clear prep state
    memset(&prep, 0, sizeof(prep));
    prep.current_block = NULL;
    prep.block_active = false;

    // Reset statistics
    stats_total_segments = 0;
    stats_buffer_underruns = 0;
}

bool GRBLStepper_PrepSegment(void)
{
    // Check if buffer full
    if (seg_buffer.count >= SEGMENT_BUFFER_SIZE)
    {
        return false; // Buffer full, try again later
    }

    // If no active block, get one from planner
    if (!prep.block_active)
    {
        prep_new_block();
        if (!prep.block_active)
        {
            return false; // No blocks available
        }
    }

    // Prepare next segment from current block
    return prep_segment();
}

// NOTE: GetNextSegment/SegmentComplete implementations moved to Phase 2B section below
//       (pointer-based API for efficient OCR ISR usage)

bool GRBLStepper_IsBusy(void)
{
    return (seg_buffer.count > 0) || prep.block_active;
}

void GRBLStepper_Reset(void)
{
    // Emergency stop - clear all state
    seg_buffer.head = 0;
    seg_buffer.tail = 0;
    seg_buffer.count = 0;

    prep.current_block = NULL;
    prep.block_active = false;
    prep.mm_complete = 0.0f;
    prep.mm_remaining = 0.0f;
}

uint8_t GRBLStepper_GetBufferCount(void)
{
    return seg_buffer.count;
}

void GRBLStepper_GetStats(uint32_t *total_segments, uint32_t *buffer_underruns)
{
    if (total_segments != NULL)
    {
        *total_segments = stats_total_segments;
    }
    if (buffer_underruns != NULL)
    {
        *buffer_underruns = stats_buffer_underruns;
    }
}

// *****************************************************************************
// Section: Helper Function Implementations
// *****************************************************************************

/**
 * @brief Get new block from planner and initialize prep state
 *
 * Dave's Understanding:
 * - Gets next optimized block from planner (has entry/exit velocities already!)
 * - Initializes "cursor" to start of block (mm_complete = 0)
 * - Sets current_speed to entry velocity (smooth junction from previous block)
 */
static void prep_new_block(void)
{
    // ═══════════════════════════════════════════════════════════════════════
    // DEBUG: Count blocks fetched from planner (REMOVE AFTER DEBUG!)
    // ═══════════════════════════════════════════════════════════════════════
    static uint32_t block_fetch_count = 0;
    // ═══════════════════════════════════════════════════════════════════════

    // Get next block from planner
    prep.current_block = GRBLPlanner_GetCurrentBlock();
    if (prep.current_block == NULL)
    {
        prep.block_active = false;
        return; // No blocks available
    }

    // ═══════════════════════════════════════════════════════════════════════
    // DEBUG: Count new blocks (LED2 pattern: Clear = new block, Set = discard)
    // ═══════════════════════════════════════════════════════════════════════
    block_fetch_count++;
    LED2_Clear(); // LED2 OFF = new block fetched (expect: 1 time for G0 Z5)
    // ═══════════════════════════════════════════════════════════════════════

    // Initialize prep state for new block
    prep.mm_complete = 0.0f;
    prep.mm_remaining = prep.current_block->millimeters;

    // Start at entry velocity (from planner optimization)
    // CRITICAL: Planner stores velocities in mm/min, convert to mm/sec!
    prep.current_speed = sqrtf(prep.current_block->entry_speed_sqr) / 60.0f;

    // Get acceleration (from planner)
    // CRITICAL: Planner stores acceleration in mm/min², convert to mm/sec²!
    prep.acceleration = prep.current_block->acceleration / (60.0f * 60.0f);

    // Reset step counters
    memset(prep.step_count, 0, sizeof(prep.step_count));

    prep.block_active = true;
}

/**
 * @brief Prepare one segment from current block
 *
 * Dave's Understanding (The Core Algorithm):
 * 1. Calculate segment distance (2mm or whatever's left)
 * 2. Calculate velocity at END of this segment:
 *    - If accelerating: v² = v₀² + 2ad
 *    - If decelerating: Same formula
 *    - If constant: v = v₀
 * 3. Use Bresenham to distribute steps across axes
 * 4. Calculate OCR period from velocity
 * 5. Add segment to buffer
 *
 * @return true if segment prepared, false if buffer full or block complete
 */
static bool prep_segment(void)
{
    // Calculate segment distance (2mm or remainder)
    float segment_mm = MIN_SEGMENT_DISTANCE_MM;
    if (segment_mm > prep.mm_remaining)
    {
        segment_mm = prep.mm_remaining;
    }

    // Calculate target velocity at END of this segment
    // CRITICAL: Convert from mm/min to mm/sec!
    float nominal_speed_sqr = (prep.current_block->programmed_rate / 60.0f) *
                              (prep.current_block->programmed_rate / 60.0f);

    // For segments WITHIN a block, accelerate towards nominal speed
    // (Junction exit speed handling will be added in Phase 2C)

    // Kinematic equation: v² = v₀² + 2ad
    float current_speed_sqr = prep.current_speed * prep.current_speed;
    float segment_exit_speed_sqr = current_speed_sqr +
                                   2.0f * prep.acceleration * segment_mm;

    // Clamp to block's nominal velocity (don't exceed feedrate!)
    if (segment_exit_speed_sqr > nominal_speed_sqr)
    {
        segment_exit_speed_sqr = nominal_speed_sqr;
    }

    float segment_exit_speed = sqrtf(segment_exit_speed_sqr);

    // Average velocity for this segment (for step rate calculation)
    float segment_velocity = (prep.current_speed + segment_exit_speed) * 0.5f;

    // Calculate steps for this segment (Bresenham distribution)
    st_segment_t *segment = &seg_buffer.buffer[seg_buffer.head];

    // Determine dominant axis (axis with most steps)
    uint32_t max_steps = 0;
    axis_id_t dominant_axis = AXIS_X;

    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        // Calculate steps for this axis in this segment
        // block->steps[axis] is total steps for the block
        // Distribute proportionally based on segment distance
        float axis_steps_f = segment_mm * (prep.current_block->steps[axis] /
                                           prep.current_block->millimeters);
        uint32_t axis_steps = (uint32_t)(axis_steps_f + 0.5f); // Round to nearest

        segment->steps[axis] = axis_steps;
        prep.step_count[axis] += axis_steps;

        if (axis_steps > max_steps)
        {
            max_steps = axis_steps;
            dominant_axis = axis;
        }
    }

    segment->n_step = max_steps;
    segment->direction_bits = prep.current_block->direction_bits;

    // Initialize Bresenham counters (for multi-axis synchronization)
    for (axis_id_t axis = AXIS_X; axis < NUM_AXES; axis++)
    {
        segment->bresenham_counter[axis] = -(int32_t)(segment->n_step / 2);
    }

    // Calculate OCR period from segment velocity
    segment->period = calculate_segment_period(segment_velocity, dominant_axis);

    // Update prep state
    prep.mm_complete += segment_mm;
    prep.mm_remaining -= segment_mm;
    prep.current_speed = segment_exit_speed;

    // Add segment to buffer
    seg_buffer.head = (seg_buffer.head + 1) % SEGMENT_BUFFER_SIZE;
    seg_buffer.count++;
    stats_total_segments++;

    // Check if block complete
    if (prep.mm_remaining <= 0.0001f)
    { // Floating point epsilon
        // ═══════════════════════════════════════════════════════════════════
        // DEBUG: Visual block discard (LED2 pattern: Set = block discarded)
        // ═══════════════════════════════════════════════════════════════════
        LED2_Set(); // LED2 ON = block discarded (expect: 1 time for G0 Z5)
        // ═══════════════════════════════════════════════════════════════════

        // Block complete, discard from planner
        GRBLPlanner_DiscardCurrentBlock();
        prep.block_active = false;
    }

    return true;
}

/**
 * @brief Calculate OCR timer period from velocity
 *
 * Dave's Understanding:
 * - Convert mm/sec to steps/sec using steps_per_mm
 * - Calculate timer period: period = TIMER_CLOCK / step_rate
 * - Clamp to 16-bit timer limits (50 to 65485)
 *
 * @param velocity_mm_sec Segment velocity in mm/sec
 * @param dominant_axis Axis with most steps (for steps_per_mm conversion)
 * @return Timer period for OCR module
 */
static uint32_t calculate_segment_period(float velocity_mm_sec, axis_id_t dominant_axis)
{
    // Convert mm/sec to steps/sec
    float steps_per_mm = MotionMath_GetStepsPerMM(dominant_axis);
    float step_rate = velocity_mm_sec * steps_per_mm;

    // Calculate timer period
    uint32_t period = (uint32_t)(TMR_CLOCK_HZ / step_rate);

    // Clamp to safe 16-bit range (with margin for pulse width)
    if (period < 50)
    {
        period = 50; // Max speed limit
    }
    if (period > 65485)
    {
        period = 65485; // Min speed limit (16-bit max - safety margin)
    }

    return period;
}

// *****************************************************************************
// Section: Phase 2B Segment Execution API
// *****************************************************************************

/**
 * @brief Get next segment for OCR execution
 *
 * Called by OCR step-complete callbacks when ready for next segment.
 * Returns pointer to next segment in buffer, or NULL if buffer empty.
 *
 * Dave's Understanding:
 *   - OCR callback finishes segment (all steps executed)
 *   - Calls this to peek at next segment
 *   - If not NULL: configure OCR with new period/steps, start executing
 *   - If NULL: axis goes idle (wait for segment prep to catch up)
 *
 * CRITICAL: This does NOT remove segment from buffer!
 * Call GRBLStepper_SegmentComplete() AFTER executing the segment.
 *
 * @return const pointer to next segment, or NULL if buffer empty
 */
const st_segment_t *GRBLStepper_GetNextSegment(void)
{
    // Check if buffer empty
    if (seg_buffer.count == 0)
    {
        stats_buffer_underruns++; // Track underruns for tuning
        return NULL;
    }

    // Return pointer to segment at tail (oldest segment = next to execute)
    return &seg_buffer.buffer[seg_buffer.tail];
}

/**
 * @brief Notify completion of segment execution
 *
 * Called by OCR callbacks after fully executing a segment (all n_step steps done).
 * Advances the tail pointer, freeing this buffer slot for new segments.
 *
 * Dave's Understanding:
 *   - OCR callback counted all steps: step_count >= segment->n_step
 *   - Segment fully executed, can discard it now
 *   - This frees space in ring buffer for segment prep to add more
 *
 * CRITICAL: Only call this AFTER segment fully executed!
 * Calling prematurely will corrupt buffer (prep will overwrite active segment).
 *
 * @return None
 */
void GRBLStepper_SegmentComplete(void)
{
    // ═══════════════════════════════════════════════════════════════════════
    // DEBUG: Visual segment completion (REMOVE AFTER DEBUG!)
    // LED1 toggles on EVERY segment complete (count pulses on scope!)
    // ═══════════════════════════════════════════════════════════════════════
    LED1_Toggle();
    // ═══════════════════════════════════════════════════════════════════════

    // Advance tail pointer (mark segment as consumed)
    seg_buffer.tail = (seg_buffer.tail + 1) % SEGMENT_BUFFER_SIZE;

    // Decrement count (atomic - safe for ISR)
    if (seg_buffer.count > 0)
    {
        seg_buffer.count--;
    }
}
