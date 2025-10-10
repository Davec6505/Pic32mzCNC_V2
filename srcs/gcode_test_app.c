/*******************************************************************************
  G-code Test Application Implementation

  File Name:
    gcode_test_app.c

  Summary:
    Test application integration implementation

  Description:
    Provides easy-to-use testing functions that integrate with your main
    application for G-code parser validation and performance testing.
*******************************************************************************/

#include "gcode_test_app.h"
#include "gcode_parser_dma.h"
#include <stdio.h>

// *****************************************************************************
// Local Variables
// *****************************************************************************

static test_suite_t main_test_suite;
static test_suite_t performance_suite;
static bool test_app_initialized = false;
static bool verbose_mode = false;
static uint16_t max_test_commands = 100;
static float test_tolerance = 0.001f;

// *****************************************************************************
// Test Application Control Functions
// *****************************************************************************

bool GCODE_TEST_Initialize(void)
{
    if (test_app_initialized) {
        return true;
    }
    
    // Initialize the main test suite
    if (!TEST_InitializeSuite(&main_test_suite, "G-code Parser Validation")) {
        return false;
    }
    
    // Initialize performance test suite
    if (!TEST_InitializeSuite(&performance_suite, "Performance Tests")) {
        return false;
    }
    
    // Load basic test cases
    TEST_LoadBasicCommands(&main_test_suite);
    TEST_LoadCoordinateTests(&main_test_suite);
    TEST_LoadFeedRateTests(&main_test_suite);
    TEST_LoadErrorTests(&main_test_suite);
    
    main_test_suite.verbose_output = verbose_mode;
    performance_suite.verbose_output = verbose_mode;
    
    test_app_initialized = true;
    
    printf("G-code Test Application Initialized\n");
    printf("Loaded %d test cases\n", main_test_suite.test_count);
    
    return true;
}

bool GCODE_TEST_RunQuickTests(void)
{
    if (!test_app_initialized) {
        printf("Error: Test application not initialized\n");
        return false;
    }
    
    printf("\n=== Running Quick G-code Parser Tests ===\n");
    
    // Create a subset of critical tests
    test_suite_t quick_suite;
    if (!TEST_InitializeSuite(&quick_suite, "Quick Tests")) {
        return false;
    }
    
    // Add only the most critical test cases
    test_case_t critical_tests[] = {
        CREATE_TEST_CASE("Basic G0", "G0 X10 Y20", true, 0, 10.0f, 20.0f, -999.0f, 0.0f),
        CREATE_TEST_CASE("Basic G1", "G1 X15 Y25 F100", true, 1, 15.0f, 25.0f, -999.0f, 100.0f),
        CREATE_TEST_CASE("Negative coords", "G1 X-10 Y-5", true, 1, -10.0f, -5.0f, -999.0f, 0.0f),
        CREATE_TEST_CASE("Invalid command", "G99 X10", false, 99, 10.0f, -999.0f, -999.0f, 0.0f),
        CREATE_TEST_CASE("Empty line", "", false, -1, -999.0f, -999.0f, -999.0f, 0.0f)
    };
    
    for (int i = 0; i < sizeof(critical_tests) / sizeof(critical_tests[0]); i++) {
        TEST_AddTestCase(&quick_suite, &critical_tests[i]);
    }
    
    quick_suite.verbose_output = verbose_mode;
    
    // Run the tests
    bool result = TEST_RunAllTests(&quick_suite);
    
    // Print compact results
    printf("Quick Test Results: %d/%d passed", 
           quick_suite.stats.passed_tests, 
           quick_suite.stats.total_tests);
    
    if (quick_suite.stats.failed_tests > 0) {
        printf(" (%d FAILED)", quick_suite.stats.failed_tests);
    }
    
    printf("\n");
    
    return result && (quick_suite.stats.failed_tests == 0);
}

bool GCODE_TEST_RunFullSuite(void)
{
    if (!test_app_initialized) {
        printf("Error: Test application not initialized\n");
        return false;
    }
    
    printf("\n=== Running Full G-code Parser Test Suite ===\n");
    
    // Run all loaded tests
    bool result = TEST_RunAllTests(&main_test_suite);
    
    // Print detailed results
    TEST_PrintResults(&main_test_suite);
    
    return result && (main_test_suite.stats.failed_tests == 0);
}

bool GCODE_TEST_RunPerformanceTests(void)
{
    if (!test_app_initialized) {
        printf("Error: Test application not initialized\n");
        return false;
    }
    
    printf("\n=== Running Performance Tests ===\n");
    
    performance_metrics_t metrics;
    
    // Test parsing performance
    if (TEST_MeasureParsePerformance(&metrics)) {
        printf("Parse Performance:\n");
        printf("  Commands/sec: %d\n", metrics.commands_per_second);
        printf("  Parse time:   %d us\n", metrics.parse_time_us);
    }
    
    // Test DMA performance
    if (TEST_MeasureDMAPerformance(&metrics)) {
        printf("DMA Performance:\n");
        printf("  Transfer time: %d us\n", metrics.dma_transfer_time_us);
        printf("  CPU usage:     %.1f%%\n", metrics.cpu_utilization_percent);
    }
    
    // Stress test
    printf("Running stress test with %d commands...\n", max_test_commands);
    if (TEST_StressTestParser(max_test_commands, &metrics)) {
        printf("Stress Test Results:\n");
        printf("  Commands processed: %d\n", max_test_commands);
        printf("  Total time:         %d ms\n", metrics.execution_time_ms);
        printf("  Average rate:       %d cmd/sec\n", metrics.commands_per_second);
    }
    
    return true;
}

void GCODE_TEST_Shutdown(void)
{
    if (test_app_initialized) {
        printf("G-code Test Application Shutdown\n");
        test_app_initialized = false;
    }
}

// *****************************************************************************
// Interactive Testing Functions
// *****************************************************************************

bool GCODE_TEST_ParseSingleCommand(const char *gcode_line)
{
    if (!test_app_initialized || !gcode_line) {
        return false;
    }
    
    printf("Testing: '%s'\n", gcode_line);
    
    // Create a single test case
    test_case_t single_test;
    memset(&single_test, 0, sizeof(single_test));
    
    strncpy(single_test.description, "Interactive Test", sizeof(single_test.description) - 1);
    strncpy(single_test.gcode_input, gcode_line, sizeof(single_test.gcode_input) - 1);
    single_test.should_parse_successfully = true; // Assume success for interactive tests
    
    // Run the test
    test_result_t result = TEST_ParseGcodeCommand(gcode_line, &single_test);
    
    switch (result) {
        case TEST_RESULT_PASS:
            printf("Result: PASS - Command parsed successfully\n");
            return true;
            
        case TEST_RESULT_FAIL:
            printf("Result: FAIL - Command parsing failed\n");
            return false;
            
        case TEST_RESULT_ERROR:
            printf("Result: ERROR - Test execution error\n");
            return false;
            
        case TEST_RESULT_SKIP:
            printf("Result: SKIP - Test was skipped\n");
            return false;
    }
    
    return false;
}

bool GCODE_TEST_RunContinuousTest(uint32_t duration_seconds)
{
    if (!test_app_initialized) {
        return false;
    }
    
    printf("Running continuous test for %d seconds...\n", duration_seconds);
    
    uint32_t start_time = TEST_GetTimestamp();
    uint32_t end_time = start_time + (duration_seconds * 1000);
    uint32_t command_count = 0;
    uint32_t success_count = 0;
    
    const char *test_commands[] = {
        "G0 X10 Y20",
        "G1 X30 Y40 F100",
        "G1 X50 Y60 Z10 F200",
        "G0 X0 Y0 Z0",
        "G1 X100 Y100 F500"
    };
    
    int num_commands = sizeof(test_commands) / sizeof(test_commands[0]);
    
    while (TEST_GetTimestamp() < end_time) {
        const char *cmd = test_commands[command_count % num_commands];
        
        if (GCODE_TEST_ParseSingleCommand(cmd)) {
            success_count++;
        }
        
        command_count++;
        
        // Small delay to prevent overwhelming the system
        // In real implementation, this might use a proper delay function
    }
    
    printf("Continuous test completed:\n");
    printf("  Commands tested: %d\n", command_count);
    printf("  Successful:      %d\n", success_count);
    printf("  Success rate:    %.1f%%\n", (float)success_count / command_count * 100.0f);
    
    return (success_count == command_count);
}

bool GCODE_TEST_ValidateHardwareIntegration(void)
{
    if (!test_app_initialized) {
        return false;
    }
    
    printf("Validating hardware integration...\n");
    
    // Test UART communication
    test_result_t uart_result = TEST_UARTCommunication();
    printf("UART Test: %s\n", (uart_result == TEST_RESULT_PASS) ? "PASS" : "FAIL");
    
    // Test DMA buffer integrity
    test_result_t dma_result = TEST_DMABufferIntegrity();
    printf("DMA Test:  %s\n", (dma_result == TEST_RESULT_PASS) ? "PASS" : "FAIL");
    
    // Test motion integration
    test_result_t motion_result = TEST_MotionIntegration();
    printf("Motion Test: %s\n", (motion_result == TEST_RESULT_PASS) ? "PASS" : "FAIL");
    
    bool all_passed = (uart_result == TEST_RESULT_PASS) && 
                      (dma_result == TEST_RESULT_PASS) && 
                      (motion_result == TEST_RESULT_PASS);
    
    printf("Hardware Integration: %s\n", all_passed ? "PASS" : "FAIL");
    
    return all_passed;
}

// *****************************************************************************
// Test Reporting Functions
// *****************************************************************************

void GCODE_TEST_PrintQuickStatus(void)
{
    if (!test_app_initialized) {
        printf("Test system not initialized\n");
        return;
    }
    
    printf("G-code Parser Status:\n");
    printf("  DMA Initialized: %s\n", GCODE_DMA_IsInitialized() ? "YES" : "NO");
    printf("  Command Queue:   %d/%d\n", GCODE_DMA_GetCommandCount(), GCODE_COMMAND_QUEUE_SIZE);
    printf("  Emergency Stop:  %s\n", GCODE_DMA_IsEmergencyStopActive() ? "ACTIVE" : "INACTIVE");
    printf("  Feed Hold:       %s\n", GCODE_DMA_IsFeedHoldActive() ? "ACTIVE" : "INACTIVE");
    
    gcode_statistics_t stats = GCODE_DMA_GetStatistics();
    printf("  Lines Processed: %d\n", stats.lines_processed);
    printf("  Parse Errors:    %d\n", stats.parse_errors);
    printf("  Buffer Overflows: %d\n", stats.buffer_overflows);
}

void GCODE_TEST_PrintDetailedReport(void)
{
    if (!test_app_initialized) {
        return;
    }
    
    GCODE_TEST_PrintQuickStatus();
    printf("\nDetailed Test Results:\n");
    TEST_PrintResults(&main_test_suite);
}

void GCODE_TEST_ExportTestResults(void)
{
    // This could export results to a file or external system
    printf("Test result export not implemented\n");
}

// *****************************************************************************
// Development Helper Functions
// *****************************************************************************

bool GCODE_TEST_DebugParser(const char *gcode_line)
{
    if (!gcode_line) {
        return false;
    }
    
    printf("=== Debug Parsing: '%s' ===\n", gcode_line);
    
    // This would provide detailed debugging information
    // about how the parser processes the command
    
    printf("1. Tokenization...\n");
    printf("2. Parameter extraction...\n");
    printf("3. Validation...\n");
    printf("4. Command queuing...\n");
    
    return GCODE_TEST_ParseSingleCommand(gcode_line);
}

void GCODE_TEST_EnableVerboseMode(bool enable)
{
    verbose_mode = enable;
    
    if (test_app_initialized) {
        main_test_suite.verbose_output = enable;
        performance_suite.verbose_output = enable;
    }
    
    printf("Verbose mode: %s\n", enable ? "ENABLED" : "DISABLED");
}

void GCODE_TEST_SetTestParameters(uint16_t max_commands, float tolerance)
{
    max_test_commands = max_commands;
    test_tolerance = tolerance;
    
    printf("Test parameters updated:\n");
    printf("  Max commands: %d\n", max_commands);
    printf("  Tolerance:    %.6f\n", tolerance);
}

/*******************************************************************************
 End of File
 */