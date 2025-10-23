/*******************************************************************************
  Arc Converter Header File

  Description:
    Arc-to-segment conversion for G2/G3 circular interpolation commands.
    Converts circular arcs into linear segments that approximate the curve.

  Author:
    October 22, 2025
*******************************************************************************/

#ifndef ARC_CONVERTER_H
#define ARC_CONVERTER_H

// *****************************************************************************
// Section: Included Files
// *****************************************************************************

#include <stdbool.h>
#include "gcode_parser.h"
#include "motion/grbl_planner.h"
#include "motion/motion_types.h"

// *****************************************************************************
// Section: Type Definitions
// *****************************************************************************

// No additional types needed - uses parsed_move_t and grbl_plan_line_data_t

// *****************************************************************************
// Section: Interface Functions
// *****************************************************************************

/**
 * @brief Convert G2/G3 arc into linear segments and buffer to GRBL planner
 * 
 * Simple arc-to-segment conversion that generates circular motion using
 * trigonometric calculations. Calls GRBLPlanner_BufferLine() for each segment.
 * 
 * Implementation details:
 * 1. Calculate arc center from I,J offsets (relative to start point)
 * 2. Calculate start/end angles and angular travel
 * 3. Determine number of segments based on arc tolerance
 * 4. Generate points along arc using trigonometry
 * 5. Buffer each segment to GRBL planner
 * 
 * CRITICAL (Oct 23, 2025): All coordinates must be in MACHINE coordinates (MPos)!
 * The GRBL planner expects absolute machine coordinates after work offset conversion.
 * 
 * @param motion_mode G2 (CW) or G3 (CCW) - from parsed_move_t.motion_mode
 * @param start_pos Starting machine position [X,Y,Z,A] in mm (MACHINE coords)
 * @param target_pos Target machine position [X,Y,Z,A] in mm (MACHINE coords)
 * @param center_offset I,J,K offsets from start point [X,Y,Z] in mm
 * @param pl_data GRBL planner data (feedrate, spindle, etc.)
 * @return true if arc converted successfully, false on error
 * 
 * @note Only supports XY plane (G17) arcs for now
 * @note Uses fixed arc tolerance of 0.002mm (TODO: get from $12 setting)
 */
bool ArcConverter_ConvertToSegments(uint8_t motion_mode,
                                    const float start_pos[NUM_AXES],
                                    const float target_pos[NUM_AXES],
                                    const float center_offset[NUM_AXES],
                                    const grbl_plan_line_data_t *pl_data);

#endif /* ARC_CONVERTER_H */
