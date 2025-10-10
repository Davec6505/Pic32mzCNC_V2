/*******************************************************************************/*******************************************************************************/*******************************************************************************

  G-code Parser Implementation

  Enhanced G-code Parser Implementation - Polling Version  Enhanced G-code Parser DMA Implementation - Clean Version

  File Name:

    gcode_parser_dma.c



  Summary:  File Name:  File Name:

    Simple polling implementation for GRBL functionality

    gcode_parser_dma.c    gcode_parser_dma.c

  Description:

    Uses UART2 polling for reliable GRBL command processing

*******************************************************************************/

  Summary:  Summary:

#include "gcode_parser_dma.h"

#include "grbl_settings.h"    Simple polling implementation for immediate GRBL functionality    Clean implementation without debug messages for production use

#include "peripheral/uart/plib_uart2.h"

#include <string.h>

#include <stdio.h>

#include <stdlib.h>  Description:  Description:



// *****************************************************************************    Uses UART2 polling for reliable GRBL command processing    This leverages your DMA channels 0/1 (IRQ 146/147) for professional

// Global Data

// ************************************************************************************************************************************************************/    UART2 handling with zero CPU overhead during motion operations.



gcode_dma_parser_t gcode_dma_parser;*******************************************************************************/



// *****************************************************************************#include "gcode_parser_dma.h"

// Local Function Prototypes

// *****************************************************************************#include "grbl_settings.h"#include "gcode_parser_dma.h"



static bool parse_and_queue_line(const char *line);#include "device.h"#include "grbl_settings.h"

static bool extract_tokens(const char *line, gcode_parsed_line_t *parsed);

static void handle_real_time_character(char c);#include "peripheral/uart/plib_uart2.h"#include "device.h"  // For direct register access



// *****************************************************************************

// Interface Implementation

// *****************************************************************************// *****************************************************************************// *****************************************************************************



bool GCODE_DMA_Initialize(void)// Global Data// Global Data

{

    memset(&gcode_dma_parser, 0, sizeof(gcode_dma_parser_t));// *****************************************************************************// *****************************************************************************

    gcode_dma_parser.initialized = true;

    gcode_dma_parser.enabled = false;

    return true;

}gcode_dma_parser_t gcode_dma_parser;gcode_dma_parser_t gcode_dma_parser;



void GCODE_DMA_Enable(void)

{

    gcode_dma_parser.enabled = true;// *****************************************************************************// *****************************************************************************

    gcode_dma_parser.emergency_stop = false;

}// Local Function Prototypes// Local Function Prototypes



void GCODE_DMA_Disable(void)// *****************************************************************************// *****************************************************************************

{

    gcode_dma_parser.enabled = false;

}

static bool parse_and_queue_line(const char *line);static bool setup_dma_uart_integration(void);

void GCODE_DMA_Reset(void)

{static bool extract_tokens(const char *line, gcode_parsed_line_t *parsed);static void process_received_data(void);

    GCODE_DMA_ClearCommandQueue();

    gcode_dma_parser.emergency_stop = false;static void handle_real_time_character(char c);static bool parse_and_queue_line(const char *line);

    gcode_dma_parser.feed_hold_active = false;

    gcode_dma_parser.status_report_requested = false;static bool extract_tokens(const char *line, gcode_parsed_line_t *parsed);

}

// *****************************************************************************static void handle_real_time_character(char c);

void GCODE_DMA_Tasks(void)

{// Interface Implementationstatic bool start_dma_rx_transfer(void);

    static bool auto_enabled = false;

    if (!auto_enabled) {// *****************************************************************************static bool start_dma_tx_transfer(const char *data, size_t length);

        auto_enabled = true;

        gcode_dma_parser.enabled = true;static void swap_rx_buffers(void);

    }

    bool GCODE_DMA_Initialize(void)

    if (!gcode_dma_parser.enabled) {

        return;{// *****************************************************************************

    }

        /* Clear parser structure */// Interface Implementation

    static uint32_t polling_counter = 0;

    polling_counter++;    memset(&gcode_dma_parser, 0, sizeof(gcode_dma_parser_t));// *****************************************************************************

    

    if (polling_counter >= 10000) {    

        polling_counter = 0;

            /* Initialize basic structure */bool GCODE_DMA_Initialize(void)

        if (UART2_ReceiverIsReady()) {

            uint8_t received_char = UART2_ReadByte();    gcode_dma_parser.initialized = true;{

            

            if (received_char == '?' || received_char == '!' || received_char == '~' || received_char == 0x18) {    gcode_dma_parser.enabled = false;    /* Clear parser structure */

                handle_real_time_character(received_char);

            }        memset(&gcode_dma_parser, 0, sizeof(gcode_dma_parser_t));

            else if (received_char == '\n' || received_char == '\r') {

                if (gcode_dma_parser.line_assembly.line_position > 0) {    return true;    

                    gcode_dma_parser.line_assembly.current_line[gcode_dma_parser.line_assembly.line_position] = '\0';

                    parse_and_queue_line(gcode_dma_parser.line_assembly.current_line);}    /* Initialize DMA buffers */

                    gcode_dma_parser.line_assembly.line_position = 0;

                }    gcode_dma_parser.dma.rx_active_buffer = gcode_dma_parser.dma.rx_buffer_a;

            }

            else if (gcode_dma_parser.line_assembly.line_position < GCODE_LINE_BUFFER_SIZE - 1) {void GCODE_DMA_Enable(void)    gcode_dma_parser.dma.rx_processing_buffer = gcode_dma_parser.dma.rx_buffer_b;

                gcode_dma_parser.line_assembly.current_line[gcode_dma_parser.line_assembly.line_position++] = received_char;

            }{    gcode_dma_parser.dma.rx_state = GCODE_DMA_STATE_IDLE;

        }

    }    gcode_dma_parser.enabled = true;    gcode_dma_parser.dma.tx_state = GCODE_DMA_STATE_IDLE;

    

    if (gcode_dma_parser.motion_system_callback && gcode_dma_parser.command_queue.count > 0) {    gcode_dma_parser.emergency_stop = false;    

        gcode_parsed_line_t command;

        if (GCODE_DMA_GetCommand(&command)) {}    /* Setup DMA-UART integration using your existing channels */

            gcode_dma_parser.motion_system_callback(&command);

        }    if (!setup_dma_uart_integration()) {

    }

    void GCODE_DMA_Tasks(void)        return false;

    if (gcode_dma_parser.status_report_requested) {

        GCODE_DMA_SendStatusReport();{    }

        gcode_dma_parser.status_report_requested = false;

    }    /* Auto-enable if not already enabled */    

}

    static bool auto_enabled = false;    /* Register DMA callbacks with your existing Harmony setup */

bool GCODE_DMA_GetCommand(gcode_parsed_line_t *command)

{    if (!auto_enabled) {    DMAC_ChannelCallbackRegister(GCODE_DMA_RX_CHANNEL, GCODE_DMA_RxEventHandler, 

    if (!command || gcode_dma_parser.command_queue.count == 0) {

        return false;        auto_enabled = true;                                (uintptr_t)&gcode_dma_parser);

    }

            gcode_dma_parser.enabled = true;    DMAC_ChannelCallbackRegister(GCODE_DMA_TX_CHANNEL, GCODE_DMA_TxEventHandler, 

    *command = gcode_dma_parser.command_queue.commands[gcode_dma_parser.command_queue.tail];

    gcode_dma_parser.command_queue.tail = GCODE_BUFFER_NEXT(gcode_dma_parser.command_queue.tail, GCODE_COMMAND_QUEUE_SIZE);    }                                (uintptr_t)&gcode_dma_parser);

    gcode_dma_parser.command_queue.count--;

    gcode_dma_parser.command_queue.commands_processed++;        

    return true;

}    if (!gcode_dma_parser.enabled) {    /* Start initial DMA receive */



uint8_t GCODE_DMA_GetCommandCount(void)        return;    if (!start_dma_rx_transfer()) {

{

    return gcode_dma_parser.command_queue.count;    }        return false;

}

        }

bool GCODE_DMA_IsCommandQueueFull(void)

{    /* Polling UART for character reception */    

    return GCODE_BUFFER_IS_FULL(gcode_dma_parser.command_queue.head, gcode_dma_parser.command_queue.tail, GCODE_COMMAND_QUEUE_SIZE);

}    static uint32_t polling_counter = 0;    gcode_dma_parser.initialized = true;



void GCODE_DMA_ClearCommandQueue(void)    polling_counter++;    return true;

{

    gcode_dma_parser.command_queue.head = 0;    }

    gcode_dma_parser.command_queue.tail = 0;

    gcode_dma_parser.command_queue.count = 0;    if (polling_counter >= 10000) {  // Check every 10000 cycles for reasonable polling rate

    gcode_dma_parser.command_queue.overflow_flag = false;

}        polling_counter = 0;void GCODE_DMA_Enable(void)



bool GCODE_DMA_SendResponse(const char *response)        {

{

    if (!response) return false;        /* Check for received data */    /* Force enable for testing - bypass initialization check */

    

    size_t length = strlen(response);        if (UART2_ReceiverIsReady()) {    gcode_dma_parser.enabled = true;

    if (length == 0) return false;

                uint8_t received_char = UART2_ReadByte();    gcode_dma_parser.emergency_stop = false;

    for (size_t i = 0; i < length; i++) {

        while (!UART2_TransmitterIsReady());                

        UART2_WriteByte(response[i]);

    }            /* Handle real-time commands immediately */    /* If not initialized, still enable but skip DMA */

    

    while (!UART2_TransmitterIsReady());            if (received_char == '?' || received_char == '!' || received_char == '~' || received_char == 0x18) {    if (!gcode_dma_parser.initialized) {

    UART2_WriteByte('\r');

    while (!UART2_TransmitterIsReady());                handle_real_time_character(received_char);        return;

    UART2_WriteByte('\n');

                }    }

    return true;

}            /* Handle line assembly for regular commands */    



bool GCODE_DMA_SendFormattedResponse(const char *format, ...)            else if (received_char == '\n' || received_char == '\r') {    /* Ensure DMA RX is active */

{

    char buffer[128];                if (gcode_dma_parser.line_assembly.line_position > 0) {    if (gcode_dma_parser.dma.rx_state == GCODE_DMA_STATE_IDLE) {

    va_list args;

    va_start(args, format);                    gcode_dma_parser.line_assembly.current_line[        start_dma_rx_transfer();

    vsnprintf(buffer, sizeof(buffer), format, args);

    va_end(args);                        gcode_dma_parser.line_assembly.line_position] = '\0';    }

    return GCODE_DMA_SendResponse(buffer);

}                    }



void GCODE_DMA_SendStatusReport(void)                    /* Parse and queue the line */

{

    GRBL_SendStatusReport();                    parse_and_queue_line(gcode_dma_parser.line_assembly.current_line);void GCODE_DMA_Tasks(void)

}

                    {

void GCODE_DMA_SendOK(void)

{                    /* Reset for next line */    /* FORCE ENABLE FOR TESTING - ignore initialization status */

    GCODE_DMA_SendResponse("ok");

}                    gcode_dma_parser.line_assembly.line_position = 0;    static bool force_enabled = false;



void GCODE_DMA_SendError(const char *error_message)                }    if (!force_enabled) {

{

    char error_buffer[64];            }        force_enabled = true;

    snprintf(error_buffer, sizeof(error_buffer), "error:%s", error_message);

    GCODE_DMA_SendResponse(error_buffer);            /* Add to line buffer */        gcode_dma_parser.enabled = true;

}

            else if (gcode_dma_parser.line_assembly.line_position < GCODE_LINE_BUFFER_SIZE - 1) {    }

void GCODE_DMA_ProcessRealTimeCommands(void)

{                gcode_dma_parser.line_assembly.current_line[    

    // Real-time commands are processed immediately in GCODE_DMA_Tasks

}                    gcode_dma_parser.line_assembly.line_position++] = received_char;    if (!gcode_dma_parser.enabled) {



bool GCODE_DMA_IsEmergencyStopActive(void)            }        return;

{

    return gcode_dma_parser.emergency_stop;        }    }

}

    }    

bool GCODE_DMA_IsFeedHoldActive(void)

{        /* Polling fallback - check for data less frequently to allow typing */

    return gcode_dma_parser.feed_hold_active;

}    /* Process command callbacks */    static uint32_t polling_counter = 0;



void GCODE_DMA_ClearFeedHold(void)    if (gcode_dma_parser.motion_system_callback &&     polling_counter++;

{

    gcode_dma_parser.feed_hold_active = false;        gcode_dma_parser.command_queue.count > 0) {    if (polling_counter >= 10000) {  // Check every 10000 cycles for slower polling

}

                polling_counter = 0;

gcode_statistics_t GCODE_DMA_GetStatistics(void)

{        gcode_parsed_line_t command;        

    return gcode_dma_parser.stats;

}        if (GCODE_DMA_GetCommand(&command)) {        /* Check for received data */



void GCODE_DMA_ResetStatistics(void)            gcode_dma_parser.motion_system_callback(&command);        if ((U2STA & _U2STA_URXDA_MASK) != 0) {

{

    memset(&gcode_dma_parser.stats, 0, sizeof(gcode_statistics_t));        }            uint8_t received_char = (uint8_t)U2RXREG;

}

    }            

float GCODE_DMA_GetBufferUtilization(void)

{                /* Handle real-time commands immediately */

    return (float)gcode_dma_parser.command_queue.count / GCODE_COMMAND_QUEUE_SIZE;

}    /* Handle status report requests */            if (received_char == '?' || received_char == '!' || received_char == '~' || received_char == 0x18) {



void GCODE_DMA_RegisterMotionCallback(void (*callback)(gcode_parsed_line_t *command))    if (gcode_dma_parser.status_report_requested) {                handle_real_time_character(received_char);

{

    gcode_dma_parser.motion_system_callback = callback;        GCODE_DMA_SendStatusReport();            }

}

        gcode_dma_parser.status_report_requested = false;            /* Handle line assembly for regular commands */

void GCODE_DMA_RegisterStatusCallback(void (*callback)(void))

{    }            else if (received_char == '\n' || received_char == '\r') {

    gcode_dma_parser.status_report_callback = callback;

}}                if (gcode_dma_parser.line_assembly.line_position > 0) {



void GCODE_DMA_RegisterEmergencyCallback(void (*callback)(void))                    gcode_dma_parser.line_assembly.current_line[

{

    gcode_dma_parser.emergency_stop_callback = callback;bool GCODE_DMA_GetCommand(gcode_parsed_line_t *command)                        gcode_dma_parser.line_assembly.line_position] = '\0';

}

{                    

void GCODE_DMA_RxEventHandler(DMAC_TRANSFER_EVENT event, uintptr_t context)

{    if (!command || gcode_dma_parser.command_queue.count == 0) {                    /* Parse and queue the line */

    // DMA event handler placeholder - currently using polling

}        return false;                    parse_and_queue_line(gcode_dma_parser.line_assembly.current_line);



void GCODE_DMA_TxEventHandler(DMAC_TRANSFER_EVENT event, uintptr_t context)    }                    

{

    // DMA event handler placeholder - currently using polling                        /* Reset for next line */

}

    /* Copy command from queue */                    gcode_dma_parser.line_assembly.line_position = 0;

// *****************************************************************************

// Local Function Implementations    *command = gcode_dma_parser.command_queue.commands[gcode_dma_parser.command_queue.tail];                }

// *****************************************************************************

                }

static bool parse_and_queue_line(const char *line)

{    /* Advance tail pointer */            /* Add to line buffer */

    if (line[0] == '$') {

        GRBL_ProcessSystemCommand(line);    gcode_dma_parser.command_queue.tail =             else if (gcode_dma_parser.line_assembly.line_position < GCODE_LINE_BUFFER_SIZE - 1) {

        return true;

    }        GCODE_BUFFER_NEXT(gcode_dma_parser.command_queue.tail, GCODE_COMMAND_QUEUE_SIZE);                gcode_dma_parser.line_assembly.current_line[

    

    if (strcmp(line, "?") == 0) {    gcode_dma_parser.command_queue.count--;                    gcode_dma_parser.line_assembly.line_position++] = received_char;

        gcode_dma_parser.status_report_requested = true;

        return true;                }

    }

        gcode_dma_parser.command_queue.commands_processed++;        }

    if (strcmp(line, "!") == 0) {

        gcode_dma_parser.feed_hold_active = true;    return true;        

        return true;

    }}        /* Original UART2 function check as backup */

    

    if (strcmp(line, "~") == 0) {        if (UART2_ReceiverIsReady()) {

        gcode_dma_parser.cycle_start_requested = true;

        return true;bool GCODE_DMA_SendResponse(const char *response)            uint8_t received_char = UART2_ReadByte();

    }

    {            

    if (strcmp(line, "\x18") == 0) {

        gcode_dma_parser.emergency_stop = true;    if (!response) {            /* Handle real-time commands immediately */

        return true;

    }        return false;            if (received_char == '?' || received_char == '!' || received_char == '~' || received_char == 0x18) {

    

    if (strlen(line) == 0) {    }                handle_real_time_character(received_char);

        GRBL_SendOK();

        return true;                }

    }

        /* Use polling for reliable immediate transmission */            /* Handle line assembly for regular commands */

    if (GCODE_DMA_IsCommandQueueFull()) {

        GRBL_SendError(1);    size_t length = strlen(response);            else if (received_char == '\n' || received_char == '\r') {

        return false;

    }    if (length == 0) {                if (gcode_dma_parser.line_assembly.line_position > 0) {

    

    gcode_parsed_line_t *command = &gcode_dma_parser.command_queue.commands[gcode_dma_parser.command_queue.head];        return false;                    gcode_dma_parser.line_assembly.current_line[

    

    if (!extract_tokens(line, command)) {    }                        gcode_dma_parser.line_assembly.line_position] = '\0';

        GRBL_SendError(2);

        return false;                        

    }

        /* Send each character via polling UART */                    /* Parse and queue the line */

    strncpy(command->original_line, line, GCODE_LINE_BUFFER_SIZE - 1);

    command->original_line[GCODE_LINE_BUFFER_SIZE - 1] = '\0';    for (size_t i = 0; i < length; i++) {                    parse_and_queue_line(gcode_dma_parser.line_assembly.current_line);

    

    gcode_dma_parser.command_queue.head = GCODE_BUFFER_NEXT(gcode_dma_parser.command_queue.head, GCODE_COMMAND_QUEUE_SIZE);        while (!UART2_TransmitterIsReady());                    

    gcode_dma_parser.command_queue.count++;

    gcode_dma_parser.stats.lines_processed++;        UART2_WriteByte(response[i]);                    /* Reset for next line */

    

    GRBL_SendOK();    }                    gcode_dma_parser.line_assembly.line_position = 0;

    return true;

}                    }



static bool extract_tokens(const char *line, gcode_parsed_line_t *parsed)    /* Add line endings */            }

{

    memset(parsed, 0, sizeof(gcode_parsed_line_t));    while (!UART2_TransmitterIsReady());            /* Add to line buffer */

    

    const char *ptr = line;    UART2_WriteByte('\r');            else if (gcode_dma_parser.line_assembly.line_position < GCODE_LINE_BUFFER_SIZE - 1) {

    while (*ptr && parsed->token_count < GCODE_MAX_TOKENS_PER_LINE) {

        while (*ptr && (*ptr == ' ' || *ptr == '\t')) ptr++;    while (!UART2_TransmitterIsReady());                gcode_dma_parser.line_assembly.current_line[

        if (!*ptr) break;

            UART2_WriteByte('\n');                    gcode_dma_parser.line_assembly.line_position++] = received_char;

        gcode_token_t *token = &parsed->tokens[parsed->token_count];

        token->letter = *ptr++;                }

        

        char *endptr;    return true;        }

        token->value = strtof(ptr, &endptr);

        ptr = endptr;}    }

        

        parsed->token_count++;    

    }

    void GCODE_DMA_SendStatusReport(void)    /* Process any received data from DMA */

    parsed->is_valid = (parsed->token_count > 0);

    return parsed->is_valid;{    if (gcode_dma_parser.dma.rx_buffer_ready) {

}

    GRBL_SendStatusReport();        process_received_data();

static void handle_real_time_character(char c)

{}        gcode_dma_parser.dma.rx_buffer_ready = false;

    switch (c) {

        case '?':    }

            gcode_dma_parser.status_report_requested = true;

            break;// *****************************************************************************    

        case '!':

            gcode_dma_parser.feed_hold_active = true;// Callback Registration Functions    /* Process command callbacks */

            break;

        case '~':// *****************************************************************************    if (gcode_dma_parser.motion_system_callback && 

            gcode_dma_parser.cycle_start_requested = true;

            gcode_dma_parser.feed_hold_active = false;        gcode_dma_parser.command_queue.count > 0) {

            break;

        case 0x18:void GCODE_DMA_RegisterMotionCallback(void (*callback)(gcode_parsed_line_t *command))        

            gcode_dma_parser.emergency_stop = true;

            break;{        gcode_parsed_line_t command;

    }

        gcode_dma_parser.motion_system_callback = callback;        if (GCODE_DMA_GetCommand(&command)) {

    gcode_dma_parser.stats.real_time_commands++;

}}            gcode_dma_parser.motion_system_callback(&command);



/*******************************************************************************        }

 End of File

 */void GCODE_DMA_RegisterStatusCallback(void (*callback)(void))    }

{    

    gcode_dma_parser.status_report_callback = callback;    /* Handle status report requests */

}    if (gcode_dma_parser.status_report_requested) {

        GCODE_DMA_SendStatusReport();

void GCODE_DMA_RegisterEmergencyCallback(void (*callback)(void))        gcode_dma_parser.status_report_requested = false;

{    }

    gcode_dma_parser.emergency_stop_callback = callback;}

}

bool GCODE_DMA_GetCommand(gcode_parsed_line_t *command)

void GCODE_DMA_ClearCommandQueue(void){

{    if (!command || gcode_dma_parser.command_queue.count == 0) {

    gcode_dma_parser.command_queue.head = 0;        return false;

    gcode_dma_parser.command_queue.tail = 0;    }

    gcode_dma_parser.command_queue.count = 0;    

    gcode_dma_parser.command_queue.overflow_flag = false;    /* Copy command from queue */

}    *command = gcode_dma_parser.command_queue.commands[gcode_dma_parser.command_queue.tail];

    

uint8_t GCODE_DMA_GetCommandCount(void)    /* Advance tail pointer */

{    gcode_dma_parser.command_queue.tail = 

    return gcode_dma_parser.command_queue.count;        GCODE_BUFFER_NEXT(gcode_dma_parser.command_queue.tail, GCODE_COMMAND_QUEUE_SIZE);

}    gcode_dma_parser.command_queue.count--;

    

void GCODE_DMA_SendOK(void)    gcode_dma_parser.command_queue.commands_processed++;

{    return true;

    GCODE_DMA_SendResponse("ok");}

}

bool GCODE_DMA_SendResponse(const char *response)

void GCODE_DMA_SendError(uint8_t error_code){

{    if (!response) {

    char error_buffer[32];        return false;

    snprintf(error_buffer, sizeof(error_buffer), "error:%d", error_code);    }

    GCODE_DMA_SendResponse(error_buffer);    

}    /* Use polling for reliable immediate transmission */

    size_t length = strlen(response);

// *****************************************************************************    if (length == 0) {

// Local Function Implementations        return false;

// *****************************************************************************    }

    

static bool parse_and_queue_line(const char *line)    /* Send each character via polling UART */

{    for (size_t i = 0; i < length; i++) {

    /* Handle UGS/GRBL system commands first */        while (!UART2_TransmitterIsReady());

    if (line[0] == '$') {        UART2_WriteByte(response[i]);

        GRBL_ProcessSystemCommand(line);    }

        return true;    

    }    /* Add line endings */

        while (!UART2_TransmitterIsReady());

    /* Handle UGS real-time query commands */    UART2_WriteByte('\r');

    if (strcmp(line, "?") == 0) {    while (!UART2_TransmitterIsReady());

        gcode_dma_parser.status_report_requested = true;    UART2_WriteByte('\n');

        return true;    

    }    return true;

    }

    if (strcmp(line, "!") == 0) {

        gcode_dma_parser.feed_hold_active = true;void GCODE_DMA_SendStatusReport(void)

        return true;{

    }    GRBL_SendStatusReport();

    }

    if (strcmp(line, "~") == 0) {

        gcode_dma_parser.cycle_start_requested = true;bool GCODE_DMA_IsInitialized(void)

        return true;{

    }    return gcode_dma_parser.initialized;

    }

    if (strcmp(line, "\x18") == 0) { // Ctrl+X

        gcode_dma_parser.emergency_stop = true;// *****************************************************************************

        return true;// DMA Event Handlers - Called by your existing Harmony DMA interrupts

    }// *****************************************************************************

    

    /* Handle empty lines */void GCODE_DMA_RxEventHandler(DMAC_TRANSFER_EVENT event, uintptr_t context)

    if (strlen(line) == 0) {{

        GRBL_SendOK();    gcode_dma_parser_t *parser = (gcode_dma_parser_t*)context;

        return true;    

    }    switch (event) {

            case DMAC_TRANSFER_EVENT_COMPLETE:

    /* Regular G-code processing */            /* Buffer transfer complete - swap buffers and mark ready */

    if (GCODE_BUFFER_IS_FULL(gcode_dma_parser.command_queue.head,            parser->dma.rx_bytes_received = GCODE_DMA_RX_BUFFER_SIZE;

                            gcode_dma_parser.command_queue.tail,             parser->dma.rx_buffer_ready = true;

                            GCODE_COMMAND_QUEUE_SIZE)) {            swap_rx_buffers();

        GRBL_SendError(1); // Buffer full error            

        return false;            /* Start new transfer on swapped buffer */

    }            start_dma_rx_transfer();

                break;

    gcode_parsed_line_t *command =             

        &gcode_dma_parser.command_queue.commands[gcode_dma_parser.command_queue.head];        case DMAC_TRANSFER_EVENT_ERROR:

                /* Handle DMA error - restart transfer */

    /* Parse the line */            parser->dma.rx_state = GCODE_DMA_STATE_ERROR;

    if (!extract_tokens(line, command)) {            start_dma_rx_transfer();

        GRBL_SendError(2); // Parse error            break;

        return false;            

    }        case DMAC_TRANSFER_EVENT_HALF_COMPLETE:

                /* Half buffer complete - could implement ping-pong if needed */

    /* Store original line for debugging */            break;

    strncpy(command->original_line, line, GCODE_LINE_BUFFER_SIZE - 1);            

    command->original_line[GCODE_LINE_BUFFER_SIZE - 1] = '\0';        default:

                break;

    /* Advance queue head */    }

    gcode_dma_parser.command_queue.head = }

        GCODE_BUFFER_NEXT(gcode_dma_parser.command_queue.head, GCODE_COMMAND_QUEUE_SIZE);

    gcode_dma_parser.command_queue.count++;void GCODE_DMA_TxEventHandler(DMAC_TRANSFER_EVENT event, uintptr_t context)

    {

    gcode_dma_parser.stats.lines_processed++;    gcode_dma_parser_t *parser = (gcode_dma_parser_t*)context;

        

    /* Send OK for valid G-code lines */    switch (event) {

    GRBL_SendOK();        case DMAC_TRANSFER_EVENT_COMPLETE:

    return true;            /* Transmission complete */

}            parser->dma.tx_busy = false;

            parser->dma.tx_state = GCODE_DMA_STATE_IDLE;

static bool extract_tokens(const char *line, gcode_parsed_line_t *parsed)            break;

{            

    memset(parsed, 0, sizeof(gcode_parsed_line_t));        default:

                break;

    const char *ptr = line;    }

    while (*ptr && parsed->token_count < GCODE_MAX_TOKENS_PER_LINE) {}

        /* Skip whitespace */

        while (*ptr && (*ptr == ' ' || *ptr == '\t')) ptr++;// *****************************************************************************

        if (!*ptr) break;// Local Function Implementations

        // *****************************************************************************

        /* Extract token */

        gcode_token_t *token = &parsed->tokens[parsed->token_count];static bool setup_dma_uart_integration(void)

        token->letter = *ptr++;{

            /* MCC has already configured DMA channels in DMAC_Initialize() */

        /* Extract number */    /* Verify DMA channels are available and not busy */

        char *endptr;    if (DMAC_ChannelIsBusy(GCODE_DMA_RX_CHANNEL) || 

        token->value = strtof(ptr, &endptr);        DMAC_ChannelIsBusy(GCODE_DMA_TX_CHANNEL)) {

        ptr = endptr;        return false;

            }

        parsed->token_count++;    

    }    /* Verify UART2 is enabled (should be done by MCC) */

        if (!(U2MODE & _U2MODE_ON_MASK)) {

    parsed->is_valid = (parsed->token_count > 0);        return false;

    return parsed->is_valid;    }

}    

    return true;

static void handle_real_time_character(char c)}

{

    switch (c) {static void process_received_data(void)

        case '?':{

            gcode_dma_parser.status_report_requested = true;    uint8_t *buffer = gcode_dma_parser.dma.rx_processing_buffer;

            break;    uint16_t bytes_received = gcode_dma_parser.dma.rx_bytes_received;

        case '!':    

            gcode_dma_parser.feed_hold_active = true;    for (uint16_t i = 0; i < bytes_received; i++) {

            break;        char received_char = (char)buffer[i];

        case '~':        

            gcode_dma_parser.cycle_start_requested = true;        /* Handle real-time commands immediately */

            gcode_dma_parser.feed_hold_active = false;        if (received_char == '?' || received_char == '!' || received_char == '~' || received_char == 0x18) {

            break;            handle_real_time_character(received_char);

        case 0x18: // Ctrl+X            continue;

            gcode_dma_parser.emergency_stop = true;        }

            break;        

    }        /* Handle line termination */

            if (received_char == '\n' || received_char == '\r') {

    gcode_dma_parser.stats.real_time_commands++;            if (gcode_dma_parser.line_assembly.line_position > 0) {

}                gcode_dma_parser.line_assembly.current_line[

                    gcode_dma_parser.line_assembly.line_position] = '\0';

/*******************************************************************************                

 End of File                /* Parse and queue the line */

 */                parse_and_queue_line(gcode_dma_parser.line_assembly.current_line);
                
                /* Reset for next line */
                gcode_dma_parser.line_assembly.line_position = 0;
            }
            continue;
        }
        
        /* Add to line buffer */
        if (gcode_dma_parser.line_assembly.line_position < GCODE_LINE_BUFFER_SIZE - 1) {
            gcode_dma_parser.line_assembly.current_line[
                gcode_dma_parser.line_assembly.line_position++] = received_char;
        }
    }
    
    gcode_dma_parser.stats.bytes_received += bytes_received;
}

static bool parse_and_queue_line(const char *line)
{
    /* Handle UGS/GRBL system commands first */
    if (line[0] == '$') {
        GRBL_ProcessSystemCommand(line);
        return true;
    }
    
    /* Handle UGS real-time query commands */
    if (strcmp(line, "?") == 0) {
        gcode_dma_parser.status_report_requested = true;
        return true;
    }
    
    if (strcmp(line, "!") == 0) {
        gcode_dma_parser.feed_hold_active = true;
        return true;
    }
    
    if (strcmp(line, "~") == 0) {
        gcode_dma_parser.cycle_start_requested = true;
        return true;
    }
    
    if (strcmp(line, "\x18") == 0) { // Ctrl+X
        gcode_dma_parser.emergency_stop = true;
        return true;
    }
    
    /* Handle empty lines */
    if (strlen(line) == 0) {
        GRBL_SendOK();
        return true;
    }
    
    /* Regular G-code processing */
    if (GCODE_BUFFER_IS_FULL(gcode_dma_parser.command_queue.head,
                            gcode_dma_parser.command_queue.tail, 
                            GCODE_COMMAND_QUEUE_SIZE)) {
        GRBL_SendError("Command buffer full");
        return false;
    }
    
    gcode_parsed_line_t *command = 
        &gcode_dma_parser.command_queue.commands[gcode_dma_parser.command_queue.head];
    
    /* Parse the line */
    if (!extract_tokens(line, command)) {
        GRBL_SendError("Parse error");
        return false;
    }
    
    /* Store original line for debugging */
    strncpy(command->original_line, line, GCODE_LINE_BUFFER_SIZE - 1);
    command->original_line[GCODE_LINE_BUFFER_SIZE - 1] = '\0';
    
    /* Advance queue head */
    gcode_dma_parser.command_queue.head = 
        GCODE_BUFFER_NEXT(gcode_dma_parser.command_queue.head, GCODE_COMMAND_QUEUE_SIZE);
    gcode_dma_parser.command_queue.count++;
    
    gcode_dma_parser.stats.lines_processed++;
    
    /* Send OK for valid G-code lines */
    GRBL_SendOK();
    return true;
}

static bool extract_tokens(const char *line, gcode_parsed_line_t *parsed)
{
    memset(parsed, 0, sizeof(gcode_parsed_line_t));
    
    const char *ptr = line;
    while (*ptr && parsed->token_count < GCODE_MAX_TOKENS_PER_LINE) {
        /* Skip whitespace */
        while (*ptr && (*ptr == ' ' || *ptr == '\t')) ptr++;
        if (!*ptr) break;
        
        /* Extract token */
        gcode_token_t *token = &parsed->tokens[parsed->token_count];
        token->letter = *ptr++;
        
        /* Extract number */
        char *endptr;
        token->value = strtof(ptr, &endptr);
        ptr = endptr;
        
        parsed->token_count++;
    }
    
    parsed->is_valid = (parsed->token_count > 0);
    return parsed->is_valid;
}

static void handle_real_time_character(char c)
{
    switch (c) {
        case '?':
            gcode_dma_parser.status_report_requested = true;
            break;
        case '!':
            gcode_dma_parser.feed_hold_active = true;
            break;
        case '~':
            gcode_dma_parser.cycle_start_requested = true;
            gcode_dma_parser.feed_hold_active = false;
            break;
        case 0x18: // Ctrl+X
            gcode_dma_parser.emergency_stop = true;
            break;
    }
    
    gcode_dma_parser.stats.real_time_commands++;
}

static bool start_dma_rx_transfer(void)
{
    /* Setup DMA transfer from UART2 RX to our buffer */
    bool result = DMAC_ChannelTransfer(
        GCODE_DMA_RX_CHANNEL,
        (const void*)&U2RXREG,
        gcode_dma_parser.dma.rx_active_buffer,
        GCODE_DMA_RX_BUFFER_SIZE
    );
    
    if (result) {
        gcode_dma_parser.dma.rx_state = GCODE_DMA_STATE_BUSY;
    }
    
    return result;
}

static bool start_dma_tx_transfer(const char *data, size_t length)
{
    if (gcode_dma_parser.dma.tx_busy) {
        return false;
    }
    
    /* Setup DMA transfer from our buffer to UART2 TX */
    bool result = DMAC_ChannelTransfer(
        GCODE_DMA_TX_CHANNEL,
        data,
        (void*)&U2TXREG,
        length
    );
    
    if (result) {
        gcode_dma_parser.dma.tx_busy = true;
        gcode_dma_parser.dma.tx_state = GCODE_DMA_STATE_BUSY;
    }
    
    return result;
}

static void swap_rx_buffers(void)
{
    uint8_t *temp = gcode_dma_parser.dma.rx_active_buffer;
    gcode_dma_parser.dma.rx_active_buffer = gcode_dma_parser.dma.rx_processing_buffer;
    gcode_dma_parser.dma.rx_processing_buffer = temp;
}

/*******************************************************************************
 End of File
 */