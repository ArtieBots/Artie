/**
 * @file debug_main.c
 * @brief Standalone debug test program for Artie CAN library
 *
 * This file provides a simple main() function that you can customize for debugging
 * specific issues with the Artie CAN library. You can build this as a standalone
 * executable and run it with a debugger (like Visual Studio, GDB, or LLDB).
 *
 * To build on Windows with CMake:
 *   mkdir build
 *   cd build
 *   cmake .. -DBUILD_DEBUG_EXECUTABLE=ON
 *   cmake --build . --config Debug
 *
 * The executable will be in build/Debug/artie_can_debug.exe (or build/ on Linux)
 *
 * You can then load it into Visual Studio, VS Code with C++ debugger, or any
 * other debugger and step through the code.
 */

#include "artie_can.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Simple helper to print hex bytes
 */
static void print_hex(const char *label, const uint8_t *data, size_t len)
{
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

/**
 * @brief Example: Test RTACP message creation and parsing
 */
static void test_rtacp_example(artie_can_context_t *ctx)
{
    printf("\n=== Testing RTACP ===\n");

    // Create and send a message
    const char *message_data = "Broadcast";
    artie_can_rtacp_msg_t msg;
    msg.frame_type = ARTIE_CAN_RTACP_MSG;
    msg.priority = ARTIE_CAN_PRIORITY_HIGH;
    msg.sender_addr = ctx->node_address;
    msg.target_addr = 0x00;
    msg.data_len = (uint8_t)strlen(message_data);
    memcpy(msg.data, message_data, msg.data_len);

    printf("Sending RTACP message to 0x%02X\n", msg.target_addr);
    print_hex("Data", msg.data, msg.data_len);

    int result = artie_can_rtacp_send(ctx, &msg, false);
    if (result == 0) {
        printf("Message sent successfully\n");
    } else {
        printf("Failed to send message: %d\n", result);
    }

    // Try to receive a message (with short timeout)
    artie_can_rtacp_msg_t recv_msg;
    result = artie_can_rtacp_receive(ctx, &recv_msg, 100);  // 100ms timeout
    if (result == 0) {
        printf("Received RTACP message from 0x%02X to 0x%02X\n",
               recv_msg.sender_addr, recv_msg.target_addr);
        print_hex("Received data", recv_msg.data, recv_msg.data_len);
    } else {
        printf("No message received (timeout or error: %d)\n", result);
    }
}

/**
 * @brief Example: Test PSACP publish/subscribe
 */
static void test_psacp_example(artie_can_context_t *ctx)
{
    printf("\n=== Testing PSACP ===\n");

    uint8_t topic = 0x10;
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};

    printf("Publishing to topic 0x%02X\n", topic);
    print_hex("Data", data, sizeof(data));

    int result = artie_can_psacp_publish(ctx, topic, ARTIE_CAN_PRIORITY_MED_LOW,
                                         false, data, sizeof(data));
    if (result == 0) {
        printf("Published successfully\n");
    } else {
        printf("Failed to publish: %d\n", result);
    }
}

/**
 * @brief Example: Test BWACP block write
 */
static void test_bwacp_example(artie_can_context_t *ctx)
{
    printf("\n=== Testing BWACP ===\n");

    uint8_t target = 0x02;
    uint32_t block_id = 42;
    uint8_t data[64];

    // Fill with test pattern
    for (size_t i = 0; i < sizeof(data); i++) {
        data[i] = (uint8_t)(i & 0xFF);
    }

    printf("Writing block %u to address 0x%02X (%zu bytes)\n", block_id, target, sizeof(data));
    print_hex("First 16 bytes", data, 16);

    int result = artie_can_bwacp_send_ready(ctx, target, 0, ARTIE_CAN_PRIORITY_MED_LOW, block_id, data, sizeof(data), false);
    if (result == 0) {
        printf("Block write completed successfully\n");
    } else {
        printf("Failed to write block: %d\n", result);
    }
}

/**
 * @brief Example: Test utility functions
 */
static void test_utilities(void)
{
    printf("\n=== Testing Utilities ===\n");

    uint8_t data[] = "Test data for CRC calculation";

    uint16_t crc16 = artie_can_crc16(data, sizeof(data) - 1);
    printf("CRC16 of \"%s\": 0x%04X\n", data, crc16);

    uint32_t crc24 = artie_can_crc24(data, sizeof(data) - 1);
    printf("CRC24 of \"%s\": 0x%06X\n", data, crc24);
}

/**
 * @brief Main entry point for debugging
 *
 * CUSTOMIZE THIS FUNCTION FOR YOUR DEBUGGING NEEDS!
 *
 * This is where you should add your test code. Set breakpoints, step through,
 * and inspect variables to debug issues with the library.
 */
int main(int argc, char *argv[])
{
    printf("Artie CAN Library Debug Test\n");
    printf("=============================\n");

    // Initialize CAN context with mock backend for testing
    artie_can_context_t ctx;
    uint8_t node_address = 0x01;

    printf("\nInitializing CAN with node address 0x%02X (Mock backend)\n", node_address);

    int result = artie_can_init(&ctx, node_address, ARTIE_CAN_BACKEND_MOCK);
    if (result != 0) {
        printf("ERROR: Failed to initialize CAN context: %d\n", result);
        return 1;
    }

    printf("CAN context initialized successfully\n");

    // ========================================================================
    // YOUR CUSTOM TEST CODE GOES HERE
    // ========================================================================
    // Uncomment any of the example tests below, or write your own!
    // Set breakpoints in Visual Studio or your debugger and step through.

    // Test RTACP (Real-Time messages)
    test_rtacp_example(&ctx);

    // Test PSACP (Pub/Sub)
    // test_psacp_example(&ctx);

    // Test BWACP (Block Write)
    // test_bwacp_example(&ctx);

    // Test utility functions (CRC, etc.)
    // test_utilities();

    // ========================================================================
    // Add your custom debugging code here:
    // ========================================================================

    // Example: Set a breakpoint on the next line and inspect the ctx variable
    printf("\n--- Ready for debugging ---\n");
    printf("Set breakpoints, step through code, and inspect variables.\n");

    // Your code here...


    // ========================================================================
    // END OF CUSTOM CODE
    // ========================================================================

    // Clean up
    artie_can_close(&ctx);
    printf("\nCAN context closed\n");

    printf("\nTest completed successfully!\n");
    return 0;
}
