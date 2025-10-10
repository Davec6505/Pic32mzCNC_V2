/*******************************************************************************
  G-code Parser Test Framework

  File Name:
    gcode_test_framework.h

  Summary:
    Comprehensive testing framework for G-code parser functionality

  Description:
    This framework provides automated testing for the DMA G-code parser
    including command parsing, coordinate handling, feed rates, and error
    conditions. Designed for both hardware-in-the-loop and simulation testing.
*******************************************************************************/

#ifndef GCODE_TEST_FRAMEWORK_H
#define GCODE_TEST_FRAMEWORK_H

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "gcode_parser_dma.h"

// *****************************************************************************
// *****************************************************************************
// Section: Constants
// *****************************************************************************
// *****************************************************************************

#define TEST_MAX_COMMANDS           50
#define TEST_MAX_DESCRIPTION_LEN    128
#define TEST_MAX_GCODE_LEN         256
#define TEST_TOLERANCE              0.001f      // 1 micron tolerance

// *****************************************************************************
// *****************************************************************************
// Section: Data Types
// *****************************************************************************
// *****************************************************************************

/* Test Result Status */
typedef enum {
    TEST_RESULT_PASS = 0,
    TEST_RESULT_FAIL,
    TEST_RESULT_SKIP,
    TEST_RESULT_ERROR
} test_result_t;

/* Test Case Structure */
typedef struct {
    char description[TEST_MAX_DESCRIPTION_LEN];
    char gcode_input[TEST_MAX_GCODE_LEN];
    bool should_parse_successfully;
    
    /* Expected parsed values */
    struct {
        bool has_g_code;
        uint16_t g_code;
        bool has_coordinates;
        float x, y, z;
        bool has_feed_rate;
        float feed_rate;
        bool has_spindle_speed;
        uint16_t spindle_speed;
    } expected;
    
    test_result_t result;
    char error_message[TEST_MAX_DESCRIPTION_LEN];
} test_case_t;

/* Test Suite Statistics */
typedef struct {
    uint16_t total_tests;
    uint16_t passed_tests;
    uint16_t failed_tests;
    uint16_t skipped_tests;
    uint16_t error_tests;
    uint32_t execution_time_ms;
} test_statistics_t;

/* Test Suite Structure */
typedef struct {
    char suite_name[64];
    test_case_t test_cases[TEST_MAX_COMMANDS];
    uint16_t test_count;
    test_statistics_t stats;
    bool verbose_output;
} test_suite_t;

// *****************************************************************************
// *****************************************************************************
// Section: Interface Functions
// *****************************************************************************
// *****************************************************************************

/* Test Suite Management */
bool TEST_InitializeSuite(test_suite_t *suite, const char *name);
bool TEST_AddTestCase(test_suite_t *suite, const test_case_t *test_case);
bool TEST_RunAllTests(test_suite_t *suite);
void TEST_PrintResults(const test_suite_t *suite);
void TEST_ResetSuite(test_suite_t *suite);

/* Individual Test Functions */
test_result_t TEST_ParseGcodeCommand(const char *gcode, const test_case_t *expected);
test_result_t TEST_CoordinateAccuracy(float actual, float expected, float tolerance);
test_result_t TEST_DMABufferIntegrity(void);
test_result_t TEST_RealTimeCommands(void);
test_result_t TEST_ErrorHandling(void);

/* Pre-defined Test Cases */
void TEST_LoadBasicCommands(test_suite_t *suite);
void TEST_LoadCoordinateTests(test_suite_t *suite);
void TEST_LoadFeedRateTests(test_suite_t *suite);
void TEST_LoadErrorTests(test_suite_t *suite);
void TEST_LoadRealTimeTests(test_suite_t *suite);

/* Hardware Testing Functions */
test_result_t TEST_UARTCommunication(void);
test_result_t TEST_DMAPerformance(void);
test_result_t TEST_MotionIntegration(void);

/* Utility Functions */
void TEST_LogMessage(const char *format, ...);
void TEST_LogError(const char *format, ...);
bool TEST_FloatEquals(float a, float b, float tolerance);
uint32_t TEST_GetTimestamp(void);

// *****************************************************************************
// *****************************************************************************
// Section: Test Case Definitions
// *****************************************************************************
// *****************************************************************************

/* Macro for creating test cases */
#define CREATE_TEST_CASE(desc, gcode, should_pass, g_val, x_val, y_val, z_val, f_val) \
    { \
        .description = desc, \
        .gcode_input = gcode, \
        .should_parse_successfully = should_pass, \
        .expected = { \
            .has_g_code = (g_val >= 0), \
            .g_code = (uint16_t)g_val, \
            .has_coordinates = (x_val != -999.0f || y_val != -999.0f || z_val != -999.0f), \
            .x = x_val, .y = y_val, .z = z_val, \
            .has_feed_rate = (f_val > 0), \
            .feed_rate = f_val, \
            .has_spindle_speed = false, \
            .spindle_speed = 0 \
        }, \
        .result = TEST_RESULT_SKIP, \
        .error_message = "" \
    }

// Sample test cases for immediate use
static const test_case_t SAMPLE_TEST_CASES[] = {
    CREATE_TEST_CASE("Basic G0 rapid positioning", "G0 X10 Y20", true, 0, 10.0f, 20.0f, -999.0f, 0.0f),
    CREATE_TEST_CASE("G1 linear interpolation with feed", "G1 X15.5 Y25.3 F100", true, 1, 15.5f, 25.3f, -999.0f, 100.0f),
    CREATE_TEST_CASE("Three-axis movement", "G1 X10 Y20 Z5 F200", true, 1, 10.0f, 20.0f, 5.0f, 200.0f),
    CREATE_TEST_CASE("Negative coordinates", "G0 X-10 Y-5.5", true, 0, -10.0f, -5.5f, -999.0f, 0.0f),
    CREATE_TEST_CASE("Large coordinates", "G1 X999.999 Y-999.999", true, 1, 999.999f, -999.999f, -999.0f, 0.0f),
    CREATE_TEST_CASE("Empty command", "", false, -1, -999.0f, -999.0f, -999.0f, 0.0f),
    CREATE_TEST_CASE("Invalid G-code", "G99 X10", false, 99, 10.0f, -999.0f, -999.0f, 0.0f),
    CREATE_TEST_CASE("Missing coordinate value", "G1 X Y10", false, 1, -999.0f, 10.0f, -999.0f, 0.0f),
    CREATE_TEST_CASE("Very high feed rate", "G1 X10 F9999", true, 1, 10.0f, -999.0f, -999.0f, 9999.0f),
    CREATE_TEST_CASE("Decimal precision test", "G1 X1.2345 Y6.7890", true, 1, 1.2345f, 6.7890f, -999.0f, 0.0f)
};

#define SAMPLE_TEST_COUNT (sizeof(SAMPLE_TEST_CASES) / sizeof(SAMPLE_TEST_CASES[0]))

// *****************************************************************************
// *****************************************************************************
// Section: Performance Testing
// *****************************************************************************
// *****************************************************************************

/* Performance metrics structure */
typedef struct {
    uint32_t parse_time_us;
    uint32_t dma_transfer_time_us;
    uint32_t command_queue_time_us;
    uint32_t execution_time_ms;
    float cpu_utilization_percent;
    uint16_t commands_per_second;
} performance_metrics_t;

/* Performance testing functions */
bool TEST_MeasureParsePerformance(performance_metrics_t *metrics);
bool TEST_MeasureDMAPerformance(performance_metrics_t *metrics);
bool TEST_StressTestParser(uint16_t command_count, performance_metrics_t *metrics);

#endif /* GCODE_TEST_FRAMEWORK_H */

/*******************************************************************************
 Example Usage:
 
 // Initialize test suite
 test_suite_t main_suite;
 TEST_InitializeSuite(&main_suite, "G-code Parser Tests");
 
 // Load predefined test cases
 TEST_LoadBasicCommands(&main_suite);
 TEST_LoadCoordinateTests(&main_suite);
 TEST_LoadErrorTests(&main_suite);
 
 // Run all tests
 TEST_RunAllTests(&main_suite);
 
 // Print results
 TEST_PrintResults(&main_suite);
 
 *******************************************************************************/