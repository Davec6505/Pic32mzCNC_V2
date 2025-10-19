/**
 * @file grbl_stepper.h
 * @brief GRBL stepper driver with segment buffer for PIC32MZ (Phase 2)
 *
 * This module ports GRBL's proven stepper algorithm with segment-based
 * execution, replacing the Phase 1 time-based S-curve interpolator.
 *
 * Key Differences from GRBL:
 * - Uses OCR hardware for step pulse generation (NOT bit-bang ISR)
 * - TMR9 @ 100Hz for segment prep (NOT 30kHz step ISR)
 * - Segment complete callback from OCR ISRs (NOT step counting in ISR)
 *
 * Architecture (Dave's Understanding):
 * ┌─────────────────────────────────────────────────────────────┐
 * │ GRBL Planner (grbl_planner.c)                               │
 * │ - Calculates max velocity, acceleration, junction speeds    │
 * │ - Optimizes entry/exit velocities for all 16 blocks         │
 * └──────────────────────┬──────────────────────────────────────┘
 *                        ↓
 * ┌─────────────────────────────────────────────────────────────┐
 * │ GRBL Stepper (THIS MODULE)                                  │
 * │ - Breaks planner blocks into 2mm segments                   │
 * │ - Interpolates velocity between entry/exit speeds           │
 * │ - Uses Bresenham for step distribution                      │
 * │ - Feeds segments to OCR hardware                            │
 * └──────────────────────┬──────────────────────────────────────┘
 *                        ↓
 * ┌─────────────────────────────────────────────────────────────┐
 * │ OCR Hardware (OCMP1/3/4/5)                                  │
 * │ - Generates step pulses at calculated rates                 │
 * │ - Calls segment complete callback when done                 │
 * └─────────────────────────────────────────────────────────────┘
 *
 * @date October 19, 2025
 * @author Dave (with AI assistance)
 */

#ifndef GRBL_STEPPER_H
#define GRBL_STEPPER_H

#include <stdint.h>
#include <stdbool.h>
#include "motion_types.h"

//=============================================================================
// CONFIGURATION CONSTANTS
//=============================================================================

/**
 * @brief Segment buffer size (must be power of 2 for efficient modulo)
 *
 * GRBL uses 6 segments as optimal balance:
 * - Small enough for fast prep (0.5ms per segment)
 * - Large enough to prevent starvation (60ms @ 100Hz prep rate)
 * - Fits in limited RAM (6 × ~40 bytes = 240 bytes)
 */
#define SEGMENT_BUFFER_SIZE 6

/**
 * @brief Minimum segment distance (mm)
 *
 * GRBL uses 2mm segments as optimal trade-off:
 * - Small enough for smooth velocity interpolation
 * - Large enough to prevent excessive CPU load
 * - Typical 100mm move = 50 segments (only 6 in memory at once)
 */
#define MIN_SEGMENT_DISTANCE_MM 2.0f

/**
 * @brief Maximum segment prep rate (Hz)
 *
 * GRBL preps segments at ~100Hz in background:
 * - 10ms period = plenty of time for calculations
 * - Low priority (won't interfere with real-time tasks)
 * - Segment buffer refilled before hardware exhausts it
 */
#define SEGMENT_PREP_RATE_HZ 100

//=============================================================================
// SEGMENT DATA STRUCTURES
//=============================================================================

/**
 * @brief Segment execution state
 *
 * Each segment represents a small portion of the current planner block
 * with pre-calculated step counts and timing for hardware execution.
 */
typedef struct
{
    uint32_t n_step;                     ///< Number of steps for dominant axis
    uint32_t steps[NUM_AXES];            ///< Step count per axis (for Bresenham)
    uint32_t period;                     ///< OCR timer period (controls step rate)
    uint8_t direction_bits;              ///< Direction bits (bit N = axis N direction)
    int32_t bresenham_counter[NUM_AXES]; ///< Bresenham error accumulator per axis
} st_segment_t;

/**
 * @brief Segment buffer (circular queue)
 *
 * Ring buffer pattern (Dave's understanding):
 * - head = next write index (where new segments added)
 * - tail = next read index (where hardware reads from)
 * - Empty: head == tail
 * - Full: (head + 1) % SIZE == tail
 */
typedef struct
{
    st_segment_t buffer[SEGMENT_BUFFER_SIZE]; ///< Circular segment array
    volatile uint8_t head;                    ///< Write index (modified by prep task)
    volatile uint8_t tail;                    ///< Read index (modified by ISR callback)
    volatile uint8_t count;                   ///< Number of segments in buffer
} st_segment_buffer_t;

/**
 * @brief Segment preparation state
 *
 * Tracks progress through current planner block as it's broken into segments.
 * Think of it like a "cursor" moving through the block.
 */
typedef struct
{
    grbl_plan_block_t *current_block; ///< Current planner block being segmented
    float mm_complete;                 ///< Distance traveled in current block (mm)
    float mm_remaining;                ///< Distance left in current block (mm)
    float current_speed;               ///< Current velocity (mm/sec)
    float acceleration;                ///< Acceleration for this block (mm/sec²)
    uint32_t step_count[NUM_AXES];    ///< Accumulated step count for position tracking
    bool block_active;                 ///< True if currently executing a block
} st_prep_t;

//=============================================================================
// PUBLIC API
//=============================================================================

/**
 * @brief Initialize GRBL stepper module
 *
 * Call once at system startup to initialize segment buffer and state.
 * Must be called BEFORE GRBLStepper_PrepSegment() or any motion.
 *
 * @return None
 */
void GRBLStepper_Initialize(void);

/**
 * @brief Prepare next segment from planner buffer
 *
 * Called by TMR9 @ 100Hz (replaces motion_manager.c auto-feed pattern).
 * This is the main "segment prep" function (GRBL's st_prep_buffer()).
 *
 * Dave's Understanding:
 * - Gets current block from planner (if no block active)
 * - Calculates next 2mm segment:
 *   - Interpolates velocity between entry/exit speeds
 *   - Uses Bresenham to distribute steps across axes
 *   - Calculates OCR period from velocity
 * - Adds segment to buffer for hardware execution
 *
 * @return true if segment prepared, false if buffer full or no blocks
 */
bool GRBLStepper_PrepSegment(void);

/**
 * @brief Get next segment for hardware execution
 *
 * Called by OCR ISR when previous segment completes.
 * Dequeues oldest segment from ring buffer.
 *
 * @param[out] segment Pointer to segment structure to fill
 * @return true if segment available, false if buffer empty
 */
bool GRBLStepper_GetNextSegment(st_segment_t *segment);

/**
 * @brief Notify stepper that segment execution completed
 *
 * Called by OCR ISR callbacks (OCMP1/3/4/5) when all steps executed.
 * Advances tail pointer in segment buffer.
 *
 * Pattern (in OCR callback):
 *   if (step_count >= segment.n_step) {
 *       GRBLStepper_SegmentComplete();  // Advance to next segment
 *   }
 *
 * @return None
 */
void GRBLStepper_SegmentComplete(void);

/**
 * @brief Check if stepper is busy executing segments
 *
 * @return true if segments in buffer or block active, false if idle
 */
bool GRBLStepper_IsBusy(void);

/**
 * @brief Emergency stop - clear all segments
 *
 * Called on feed hold (!), soft reset (^X), or alarm state.
 * Discards all pending segments and resets state.
 *
 * @return None
 */
void GRBLStepper_Reset(void);

/**
 * @brief Get current segment buffer count (for debugging)
 *
 * @return Number of segments currently in buffer (0-6)
 */
uint8_t GRBLStepper_GetBufferCount(void);

/**
 * @brief Get segment prep statistics (for debugging/tuning)
 *
 * @param[out] total_segments Total segments prepared since init
 * @param[out] buffer_underruns Number of times buffer went empty
 * @return None
 */
void GRBLStepper_GetStats(uint32_t *total_segments, uint32_t *buffer_underruns);

#endif // GRBL_STEPPER_H
