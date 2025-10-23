/*******************************************************************************
  Arc Converter Implementation

  Description:
    Arc-to-segment conversion for G2/G3 circular interpolation commands.
    Converts circular arcs into linear segments that approximate the curve.

  Author:
    October 22, 2025
*******************************************************************************/

// *****************************************************************************
// Section: Included Files
// *****************************************************************************

#include "arc_converter.h"
#include "ugs_interface.h"
#include <math.h>

// *****************************************************************************
// Section: Constants
// *****************************************************************************

#define ARC_TOLERANCE_DEFAULT   0.002f  // Default arc tolerance in mm (TODO: get from $12)
#define ARC_MIN_SEGMENTS        1       // Minimum segments per arc
#define ARC_MAX_SEGMENTS        100     // Maximum segments per arc (safety limit)
#define ARC_MIN_RADIUS          0.001f  // Minimum arc radius in mm

// *****************************************************************************
// Section: Implementation
// *****************************************************************************

/**
 * @brief Convert G2/G3 arc into linear segments and buffer to GRBL planner
 * 
 * CRITICAL (Oct 23, 2025): All positions must be in MACHINE coordinates!
 */
bool ArcConverter_ConvertToSegments(uint8_t motion_mode,
                                    const float start_pos[NUM_AXES],
                                    const float target_pos[NUM_AXES],
                                    const float center_offset[NUM_AXES],
                                    const grbl_plan_line_data_t *pl_data)
{
    // Validate inputs
    if (start_pos == NULL || target_pos == NULL || center_offset == NULL || pl_data == NULL)
    {
        return false;
    }
    
    // Validate motion mode
    if (motion_mode != 2 && motion_mode != 3)
    {
        UGS_Print("error:33 - Arc must be G2 or G3\r\n");
        return false;
    }
    
    // Debug: Show input parameters
    UGS_Printf("[ARC] Input: start=(%.3f,%.3f,%.3f) target=(%.3f,%.3f,%.3f) I=%.3f J=%.3f mode=%u\r\n",
               start_pos[AXIS_X], start_pos[AXIS_Y], start_pos[AXIS_Z],
               target_pos[AXIS_X], target_pos[AXIS_Y], target_pos[AXIS_Z],
               center_offset[AXIS_X], center_offset[AXIS_Y],
               motion_mode);
    
    // Calculate arc center (absolute machine coordinates)
    float center_x = start_pos[AXIS_X] + center_offset[AXIS_X];
    float center_y = start_pos[AXIS_Y] + center_offset[AXIS_Y];
    
    // Calculate start radius
    float dx_start = start_pos[AXIS_X] - center_x;
    float dy_start = start_pos[AXIS_Y] - center_y;
    float radius = sqrtf(dx_start * dx_start + dy_start * dy_start);
    
    // Validate radius
    if (radius < ARC_MIN_RADIUS)
    {
        UGS_Print("error:35 - Arc radius too small\r\n");
        return false;
    }
    
    // Calculate start and end angles
    float start_angle = atan2f(dy_start, dx_start);
    float dx_end = target_pos[AXIS_X] - center_x;
    float dy_end = target_pos[AXIS_Y] - center_y;
    float end_angle = atan2f(dy_end, dx_end);
    
    // Calculate angular travel (handle CW vs CCW)
    // CRITICAL (Oct 23, 2025): GRBL arc behavior
    // - G2 (CW): Negative angular travel, takes clockwise path
    // - G3 (CCW): Positive angular travel, takes counter-clockwise path
    // - Always take the SHORT way unless full circle (start == end)
    float angular_travel = end_angle - start_angle;
    
    if (motion_mode == 2)  // G2 = CW (clockwise)
    {
        // For CW arcs, angular travel must be negative
        if (angular_travel >= 0.0f)
        {
            angular_travel -= 2.0f * M_PI;  // Make negative
        }
        // CRITICAL FIX: If magnitude > 180°, we went the long way - adjust!
        // Example: start=180°, end=0° gives -180° (semicircle CCW)
        // But G2 should go CW: -90° (quarter circle through negative Y)
        else if (angular_travel < -M_PI)
        {
            angular_travel += 2.0f * M_PI;  // Reduce magnitude to < 180°
        }
    }
    else  // G3 = CCW (counter-clockwise)
    {
        // For CCW arcs, angular travel must be positive
        if (angular_travel <= 0.0f)
        {
            angular_travel += 2.0f * M_PI;  // Make positive
        }
        // CRITICAL FIX: If magnitude > 180°, we went the long way - adjust!
        else if (angular_travel > M_PI)
        {
            angular_travel -= 2.0f * M_PI;  // Reduce magnitude to < 180°
        }
    }
    
    // Calculate number of segments based on arc tolerance
    // Formula from GRBL: segments = abs(0.5 * angular_travel * radius / sqrt(tolerance * (2*radius - tolerance)))
    float arc_tolerance = ARC_TOLERANCE_DEFAULT;
    float segments_float = fabsf(0.5f * angular_travel * radius / 
                                  sqrtf(arc_tolerance * (2.0f * radius - arc_tolerance)));
    uint16_t segments = (uint16_t)ceilf(segments_float);
    
    // Apply segment limits
    if (segments < ARC_MIN_SEGMENTS)
    {
        segments = ARC_MIN_SEGMENTS;
    }
    if (segments > ARC_MAX_SEGMENTS)
    {
        segments = ARC_MAX_SEGMENTS;
    }
    
#ifdef DEBUG_MOTION_BUFFER
    UGS_Printf("[ARC] Center(%.3f,%.3f) R=%.3f Segments=%u Angle=%.2f deg\r\n",
               center_x, center_y, radius, segments, angular_travel * 180.0f / M_PI);
#endif

    // ALWAYS show first few segments for debugging
    UGS_Printf("[ARC] Center(%.3f,%.3f) R=%.3f Segments=%u Angle=%.2f deg\r\n",
               center_x, center_y, radius, segments, angular_travel * 180.0f / M_PI);
    
    // Generate segments
    float angle_per_segment = angular_travel / (float)segments;
    float target_segment[NUM_AXES];
    grbl_plan_line_data_t seg_data = *pl_data;
    
    // Debug: Show first few segment points
    UGS_Printf("[ARC] Generating %u segments from angle %.2f to %.2f deg\r\n",
               segments, start_angle * 180.0f / M_PI, 
               (start_angle + angular_travel) * 180.0f / M_PI);
    
    for (uint16_t i = 1; i <= segments; i++)
    {
        float angle = start_angle + angle_per_segment * (float)i;
        
        // Calculate point on arc (XY plane)
        target_segment[AXIS_X] = center_x + radius * cosf(angle);
        target_segment[AXIS_Y] = center_y + radius * sinf(angle);
        
        // Linear interpolation for Z and A axes
        // CRITICAL: Use target_pos (machine coords), not arc_move->target (work coords)!
        target_segment[AXIS_Z] = target_pos[AXIS_Z];
        target_segment[AXIS_A] = target_pos[AXIS_A];
        
        // Debug: Show first 5 segments
        if (i <= 5)
        {
            UGS_Printf("  [SEG %u] angle=%.1f deg, target=(%.3f, %.3f, %.3f)\r\n",
                       i, angle * 180.0f / M_PI,
                       target_segment[AXIS_X], target_segment[AXIS_Y], target_segment[AXIS_Z]);
        }
        
        // Buffer segment to GRBL planner
        if (!GRBLPlanner_BufferLine(target_segment, &seg_data))
        {
            UGS_Printf("error:36 - Arc segment buffer full at %u/%u\r\n", i, segments);
            return false;
        }
    }
    
    return true;
}
