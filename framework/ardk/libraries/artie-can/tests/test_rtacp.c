/**
 * @file test_rtacp.c
 * @brief Test the RTACP (Real Time Artie CAN Protocol) implementation.
 * Uses TCP backend.
 */

#include <string.h>
#include <time.h>
#include "unity.h"
#include "artie_can.h"
#include "util.h"

// Platform-specific includes for sleep
#ifdef _WIN32
    #include <windows.h>
    #define SLEEP_MS(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

// Default timeout for receive calls in tests (in milliseconds)
#define DEFAULT_TIMEOUT_MS 3000

// A few nodes that the tests use for communication.
static const artie_can_tcp_addr_t node1_addr = { .host = "127.0.0.1", .port = 5000 };
static const artie_can_tcp_addr_t node2_addr = { .host = "127.0.0.1", .port = 5001 };
static artie_can_context_t _node1_context;
static artie_can_context_t _node2_context;
static artie_can_backend_t _node1;
static artie_can_backend_t _node2;

// A flag to indicate whether the callback has been called for tests that use the callback.
static volatile bool _callback_called = false;

/** The callback that node1 uses to receive messages. */
static void _receive_callback_node1(artie_can_frame_t *frame)
{
    // TODO
}

/** The callback that node2 uses to receive messages. */
static void _receive_callback_node2(artie_can_frame_t *frame)
{
    // TODO
}

/** The function to get the current time in milliseconds. */
static uint64_t _get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec) * 1000 + (ts.tv_nsec / 1000000);
}

/**
 * @brief Setup function called before each test.
 *
 * This function runs before each individual test in this file.
 * Use it to initialize any state needed for your tests.
 */
void setUp(void)
{
    artie_can_error_t err;

    // Reset the callback called flag before each test
    _callback_called = false;

    // An array of node address information. Okay for it to be on the stack.
    artie_can_tcp_addr_t node_addresses[] = {node1_addr, node2_addr};

    // Set up the nodes with TCP contexts
    err = artie_can_init_context_tcp(&_node1_context, &node1_addr, node_addresses, ARRAY_LENGTH(node_addresses));
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_init_context_tcp(&_node2_context, &node2_addr, node_addresses, ARRAY_LENGTH(node_addresses));
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Set up the nodes to use RTACP
    err = artie_can_init_context_rtacp(&_node1_context, 0x01); // Source address 0x01
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_init_context_rtacp(&_node2_context, 0x02); // Source address 0x02
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Set up the backends for the nodes
    err = artie_can_init(&_node1_context, &_node1, ARTIE_CAN_BACKEND_TCP, _receive_callback_node1, _get_time_ms);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_init(&_node2_context, &_node2, ARTIE_CAN_BACKEND_TCP, _receive_callback_node2, _get_time_ms);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Set up a thread to run the eventloop for the nodes
    err = artie_can_start_event_loop(&_node1);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_start_event_loop(&_node2);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
}

/**
 * @brief Teardown function called after each test.
 *
 * This function runs after each individual test in this file.
 * Use it to clean up any state created during the test.
 */
void tearDown(void)
{
    artie_can_error_t err;

    // Close the backends for the nodes
    err = artie_can_close(&_node1);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_close(&_node2);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Clean up contexts by zeroing them out (not strictly necessary, but good practice since we are reusing them in setUp)
    memset(&_node1, 0, sizeof(_node1));
    memset(&_node2, 0, sizeof(_node2));
}

/**
 * @brief Test that we can broadcast a message from one node and receive it on another.
 *
 */
void test_broadcast(void)
{
    artie_can_error_t err;

    // Create a frame to send
    uint8_t data_bytes[] = {0xDE, 0xAD, 0xBE, 0xEF};
    artie_can_frame_rtacp_t rtacp_frame = {
        .ack = false,
        .priority = ARTIE_CAN_FRAME_PRIORITY_RTACP_MEDIUM,
        .source_address = 0x01,
        .target_address = ARTIE_CAN_RTACP_TARGET_ADDRESS_BROADCAST,
        .nbytes = sizeof(data_bytes),
        .data = {0}
    };
    memcpy(rtacp_frame.data, data_bytes, sizeof(data_bytes));
    artie_can_frame_t frame_to_send;
    err = artie_can_rtacp_init_frame(&frame_to_send, &rtacp_frame);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Send the frame from node 1
    err = artie_can_send(&_node1, &frame_to_send);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Blocking receive on node 2 to get the frame (with a 3 second timeout)
    artie_can_frame_t frame_received;
    err = artie_can_receive(&_node2, &frame_received, DEFAULT_TIMEOUT_MS);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Check that the received frame matches the sent frame
    assert_frames_equal(&frame_to_send, &frame_received);

    // Parse the received frame back into RTACP format and check that it matches the original RTACP frame
    artie_can_frame_rtacp_t rtacp_frame_received;
    err = artie_can_rtacp_parse_frame(&frame_received, &rtacp_frame_received);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
    assert_rtacp_frames_equal(&rtacp_frame, &rtacp_frame_received);
}

/**
 * @brief Test non-blocking receive with callback.
 *
 */
void test_nonblocking_receive_with_callback(void)
{
    artie_can_error_t err;

    // Create a frame to send
    uint8_t data_bytes[] = {0xDE, 0xAD, 0xBE, 0xEF};
    artie_can_frame_rtacp_t rtacp_frame = {
        .ack = false,
        .priority = ARTIE_CAN_FRAME_PRIORITY_RTACP_MEDIUM,
        .source_address = 0x01,
        .target_address = ARTIE_CAN_RTACP_TARGET_ADDRESS_BROADCAST,
        .nbytes = sizeof(data_bytes),
        .data = {0}
    };
    memcpy(rtacp_frame.data, data_bytes, sizeof(data_bytes));
    artie_can_frame_t frame_to_send;
    err = artie_can_rtacp_init_frame(&_node1, &frame_to_send, &rtacp_frame);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Set up a spot to store the frame that we will receive
    artie_can_frame_t frame_received_in_callback;

    // Start non-blocking receive on node 2 with the callback
    err = artie_can_receive_nonblocking(&_node2, &frame_received_in_callback, _receive_callback);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Send the frame from node 1
    err = artie_can_send(&_node1, &frame_to_send);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Wait for the callback to be called with timeout protection
    time_t start_time = time(NULL);
    time_t timeout_seconds = (DEFAULT_TIMEOUT_MS / 1000) > 0 ? (DEFAULT_TIMEOUT_MS / 1000) : 1;

    while (!_callback_called)
    {
        // Check if we've exceeded the timeout
        if (difftime(time(NULL), start_time) > timeout_seconds)
        {
            TEST_FAIL_MESSAGE("Timeout waiting for callback to be called");
        }

        // Sleep briefly to avoid busy waiting (10ms)
        SLEEP_MS(10);
    }

    // Check that the frame received in the callback matches the sent frame
    assert_frames_equal(&frame_to_send, &frame_received_in_callback);
}

/**
 * @brief Test that we can send a message from one node to another and get an ACK back.
 *
 */
void test_send_and_ack(void)
{
    artie_can_error_t err;

    // Create a frame to send
    uint8_t data_bytes[] = {0xDE, 0xAD, 0xBE, 0xEF};
    artie_can_frame_rtacp_t rtacp_frame = {
        .ack = false,
        .priority = ARTIE_CAN_FRAME_PRIORITY_RTACP_MEDIUM,
        .source_address = 0x01,
        .target_address = 0x02, // Target specific node to ensure we get an ACK back
        .nbytes = sizeof(data_bytes),
        .data = {0}
    };
    memcpy(rtacp_frame.data, data_bytes, sizeof(data_bytes));
    artie_can_frame_t frame_to_send;
    err = artie_can_rtacp_init_frame(&_node1, &frame_to_send, &rtacp_frame);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Send the frame from node 1 (no error means we got an ACK back successfully)
    err = artie_can_send(&_node1, &frame_to_send);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
}

/**
 * @brief Main function - runs all tests.
 *
 * This function sets up the Unity test runner and executes all tests.
 */
int main(void)
{
    // Initialize Unity test framework
    UNITY_BEGIN();

    // Run tests
    RUN_TEST(test_broadcast);
    RUN_TEST(test_nonblocking_receive_with_callback);

    // Finish and return results
    return UNITY_END();
}
