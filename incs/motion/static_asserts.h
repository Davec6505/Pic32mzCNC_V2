
#ifndef MOTION_STATIC_ASSERTS_H
#define MOTION_STATIC_ASSERTS_H



#include <stdbool.h>
#include <stdint.h>
#include <assert.h>




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


#endif // MOTION_STATIC_ASSERTS_H
