/*******************************************************************************
  UART2 Debug Implementation for Clicker2 Hardware

  File Name:
    uart_debug.c

  Summary:
    Debug functions to test UART2 communication

  Description:
    Provides basic UART2 testing to identify communication issues.
*******************************************************************************/

#include "uart_debug.h"
#include "peripheral/uart/plib_uart2.h"
#include "peripheral/coretimer/plib_coretimer.h"
#include "peripheral/gpio/plib_gpio.h"
#include <string.h>
#include <stdio.h>

// *****************************************************************************
// Debug Functions
// *****************************************************************************

void UART_DEBUG_Initialize(void)
{
    // UART2 should already be initialized by Harmony
    // This is just for verification

    // Force re-initialize UART2 with known parameters
    U2MODE = 0; // Reset UART mode

    // Configure for 8-bit, no parity, 1 stop bit
    U2MODE = 0x8; // Standard mode

    // Configure status and control
    U2STA = 0;                                          // Reset status
    U2STASET = (_U2STA_UTXEN_MASK | _U2STA_URXEN_MASK); // Enable TX and RX

    // Start with 115200 baud - most reliable and common
    // For 50MHz UART clock: BRG = (50000000 / (4 * 115200)) - 1 = 108
    U2BRG = 108; // 115200 baud (reliable)

    // Turn on UART
    U2MODESET = _U2MODE_ON_MASK;

    // Small delay for stabilization
    for (volatile int i = 0; i < 10000; i++)
        ;
}

void UART_DEBUG_SendCharacter(char c)
{
    // Wait for transmitter to be ready
    while (!(U2STA & _U2STA_TRMT_MASK))
        ;

    // Send character
    U2TXREG = c;

    // Wait for transmission to complete
    while (!(U2STA & _U2STA_TRMT_MASK))
        ;
}

void UART_DEBUG_SendString(const char *str)
{
    if (!str)
        return;

    while (*str)
    {
        UART_DEBUG_SendCharacter(*str);
        str++;
    }
}

bool UART_DEBUG_IsTransmitReady(void)
{
    return (U2STA & _U2STA_TRMT_MASK) != 0;
}

bool UART_DEBUG_IsReceiveReady(void)
{
    return (U2STA & _U2STA_URXDA_MASK) != 0;
}

char UART_DEBUG_ReadCharacter(void)
{
    while (!UART_DEBUG_IsReceiveReady())
        ;
    return (char)U2RXREG;
}

void UART_DEBUG_BasicTest(void)
{
    UART_DEBUG_SendString("\r\n=== UART2 Basic Test ===\r\n");
    UART_DEBUG_SendString("If you can see this, UART2 TX is working!\r\n");
    UART_DEBUG_SendString("Clock: 50MHz\r\n");
    UART_DEBUG_SendString("Baud: 115200\r\n");
    UART_DEBUG_SendString("Data: 8N1\r\n");
    UART_DEBUG_SendString("=========================\r\n");
}

void UART_DEBUG_EchoTest(void)
{
    UART_DEBUG_SendString("\r\n=== UART2 Echo Test ===\r\n");
    UART_DEBUG_SendString("Type characters - they should echo back\r\n");
    UART_DEBUG_SendString("Send 'q' to quit echo test\r\n");

    char c = 0; // Initialize to avoid compiler warning
    char last_char = 0;
    do
    {
        if (UART_DEBUG_IsReceiveReady())
        {
            c = UART_DEBUG_ReadCharacter();
            UART_DEBUG_SendCharacter(c); // Echo back

            // Handle line endings more robustly
            if (c == '\r')
            {
                UART_DEBUG_SendCharacter('\n'); // Add line feed after CR
            }
            else if (c == '\n' && last_char != '\r')
            {
                UART_DEBUG_SendCharacter('\r'); // Add CR before standalone LF
            }

            last_char = c;
        }
    } while (c != 'q');

    UART_DEBUG_SendString("\r\nEcho test complete\r\n");
}

void UART_DEBUG_BaudRateTest(void)
{
    UART_DEBUG_SendString("\r\n=== Baud Rate Test ===\r\n");

    // Test different baud rates
    uint32_t baud_rates[] = {9600, 19200, 38400, 115200, 250000};
    uint32_t brg_values[] = {1301, 651, 325, 108, 49}; // BRG values for 50MHz UART clock

    for (int i = 0; i < 5; i++)
    {
        // Set new baud rate
        U2BRG = brg_values[i];

        // Small delay
        for (volatile int j = 0; j < 10000; j++)
            ;

        char msg[100];
        sprintf(msg, "Testing %u baud (BRG=%u)\r\n",
                baud_rates[i], brg_values[i]);
        UART_DEBUG_SendString(msg);

        // Delay before next test
        for (volatile int j = 0; j < 100000; j++)
            ;
    }

    // Return to 115200
    U2BRG = 108;
    UART_DEBUG_SendString("Returned to 115200 baud\r\n");
}

void UART_DEBUG_PrintSystemInfo(void)
{
    UART_DEBUG_SendString("\r\n=== System Information ===\r\n");

    char info[200];

    // UART2 register values
    sprintf(info, "U2MODE: 0x%08X\r\n", U2MODE);
    UART_DEBUG_SendString(info);

    sprintf(info, "U2STA:  0x%08X\r\n", U2STA);
    UART_DEBUG_SendString(info);

    sprintf(info, "U2BRG:  %u\r\n", U2BRG);
    UART_DEBUG_SendString(info);

    // Clock information
    sprintf(info, "UART Clock: 50MHz, System Clock: ~100MHz\r\n");
    UART_DEBUG_SendString(info);

    // Pin assignments
    UART_DEBUG_SendString("Pin Assignments:\r\n");
    sprintf(info, "RPB3R: %u (should be 11 for U2TX)\r\n", RPB3R);
    UART_DEBUG_SendString(info);

    sprintf(info, "RPD4R: %u\r\n", RPD4R);
    UART_DEBUG_SendString(info);

    sprintf(info, "RPD5R: %u\r\n", RPD5R);
    UART_DEBUG_SendString(info);

    sprintf(info, "RPF0R: %u\r\n", RPF0R);
    UART_DEBUG_SendString(info);

    // GPIO states
    UART_DEBUG_SendString("GPIO States:\r\n");
    sprintf(info, "SW1 (RC3): %u, SW2 (RB0): %u\r\n", SW1_Get(), SW2_Get());
    UART_DEBUG_SendString(info);

    sprintf(info, "LED1 (RE7): %u, LED2 (RA9): %u\r\n", LED1_Get(), LED2_Get());
    UART_DEBUG_SendString(info);

    //    sprintf(info, "LIMIT_X (RB1): %u, LIMIT_Y (RB15): %u, LIMIT_Z (RF4): %u\r\n",
    //            LIMIT_X_Get(), LIMIT_Y_Get(), LIMIT_Z_Get());
    //    UART_DEBUG_SendString(info);

    // OCR-controlled pulse pins (read-only)
    UART_DEBUG_SendString("OCR-Controlled Pulse Pins:\r\n");
    sprintf(info, "PulseX (RD4/OCR1): %u, PulseY (RD5/OCR4): %u, PulseZ (RF0/OCR5): %u\r\n",
            PulseX_Get(), PulseY_Get(), PulseZ_Get());
    UART_DEBUG_SendString(info);

    UART_DEBUG_SendString("========================\r\n");
}

void UART_DEBUG_TestLoop(void)
{
    // First, send identification at 115200 baud
    UART_DEBUG_SendString("\r\n\r\n=== CLICKER2 UART2 DEBUG @ 115200 ===\r\n");
    UART_DEBUG_SendString("If you can read this clearly, 115200 baud is working!\r\n");
    UART_DEBUG_SendString("Press 'b' to try different baud rates\r\n");
    UART_DEBUG_SendString("Press 'i' for system info\r\n");
    UART_DEBUG_SendString("Current: 115200 baud, 50MHz UART clock\r\n\r\n");

    // Print system info
    UART_DEBUG_PrintSystemInfo();

    // Continuous heartbeat with user commands
    uint32_t counter = 0;
    while (1)
    {
        char heartbeat[50];
        sprintf(heartbeat, "Heartbeat @ 115200: %u\r\n", counter++);
        UART_DEBUG_SendString(heartbeat);

        // Delay ~1 second
        for (volatile int i = 0; i < 1000000; i++)
            ;

        // Check for received data
        if (UART_DEBUG_IsReceiveReady())
        {
            char c = UART_DEBUG_ReadCharacter();

            if (c == 'b' || c == 'B')
            {
                UART_DEBUG_SendString("Testing different baud rates...\r\n");
                UART_DEBUG_TestMultipleBaudRates();
            }
            else if (c == 't' || c == 'T')
            {
                UART_DEBUG_SendString("Running tests...\r\n");
                UART_DEBUG_BasicTest();
                UART_DEBUG_BaudRateTest();
            }
            else if (c == 'e' || c == 'E')
            {
                UART_DEBUG_EchoTest();
            }
            else if (c == 'i' || c == 'I')
            {
                UART_DEBUG_PrintSystemInfo();
            }
            else
            {
                sprintf(heartbeat, "Received: 0x%02X ('%c')\r\n", c,
                        (c >= 32 && c <= 126) ? c : '?');
                UART_DEBUG_SendString(heartbeat);
            }
        }
    }
}

void UART_DEBUG_TestMultipleBaudRates(void)
{
    UART_DEBUG_SendString("\r\n=== Multi-Baud Rate Test ===\r\n");
    UART_DEBUG_SendString("Testing common baud rates...\r\n\r\n");

    // Array of common baud rates and their BRG values for 50MHz
    struct
    {
        uint32_t baud;
        uint32_t brg;
        const char *name;
    } rates[] = {
        {9600, 1301, "9600"},
        {19200, 651, "19200"},
        {38400, 325, "38400"},
        {57600, 216, "57600"},
        {115200, 108, "115200"},
        {230400, 54, "230400"},
        {250000, 49, "250000"},
        {460800, 27, "460800"},
        {500000, 24, "500000"},
        {921600, 13, "921600"}};

    for (int i = 0; i < 10; i++)
    {
        // Change baud rate
        U2BRG = rates[i].brg;

        // Wait for change
        for (volatile int j = 0; j < 50000; j++)
            ;

        // Send test message at this baud rate
        char msg[100];
        sprintf(msg, "*** TESTING %s BAUD (BRG=%u) ***\r\n", rates[i].name, rates[i].brg);
        UART_DEBUG_SendString(msg);
        sprintf(msg, "If you can read this clearly, use %s baud in PuTTY!\r\n", rates[i].name);
        UART_DEBUG_SendString(msg);
        UART_DEBUG_SendString("===========================================\r\n\r\n");

        // Wait 3 seconds at this baud rate
        for (volatile int j = 0; j < 3000000; j++)
            ;
    }

    // Return to 115200
    U2BRG = 108;
    for (volatile int j = 0; j < 50000; j++)
        ;
    UART_DEBUG_SendString("Back to 115200 baud. Which one was clearest?\r\n");
}

/*******************************************************************************
 End of File
 */