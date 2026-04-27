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
static const artie_can_tcp_addr_t node3_addr = { .host = "127.0.0.1", .port = 5002 };
static artie_can_context_t _node1_context;
static artie_can_context_t _node2_context;
static artie_can_context_t _node3_context;
static artie_can_backend_t _node1;
static artie_can_backend_t _node2;
static artie_can_backend_t _node3;
static artie_can_tcp_context_t _node1_tcp_context;
static artie_can_tcp_context_t _node2_tcp_context;
static artie_can_tcp_context_t _node3_tcp_context;

// A flag to indicate whether the callback has been called for tests that use the callback.
static volatile bool _callback_called1 = false;
static volatile bool _callback_called2 = false;
static volatile bool _callback_called3 = false;
static volatile artie_can_frame_rtacp_t _frame_received_in_callback1;
static volatile artie_can_frame_rtacp_t _frame_received_in_callback2;
static volatile artie_can_frame_rtacp_t _frame_received_in_callback3;

/** The callback that node1 uses to receive messages. */
static void _receive_callback_node1(const artie_can_frame_t *frame)
{
    // Parse the received frame into RTACP format and store it in the callback storage variable for node 1.
    artie_can_frame_rtacp_t rtacp_frame_received;
    artie_can_error_t err = artie_can_rtacp_parse_frame(frame, &rtacp_frame_received);
    if (err == ARTIE_CAN_ERR_NONE)
    {
        // Due to volatile qualifier, we can't use memcpy here, so we have to copy member by member.
        _frame_received_in_callback1.ack = rtacp_frame_received.ack;
        _frame_received_in_callback1.priority = rtacp_frame_received.priority;
        _frame_received_in_callback1.source_address = rtacp_frame_received.source_address;
        _frame_received_in_callback1.target_address = rtacp_frame_received.target_address;
        _frame_received_in_callback1.nbytes = rtacp_frame_received.nbytes;
        for (uint8_t i = 0; i < rtacp_frame_received.nbytes; i++)
        {
            _frame_received_in_callback1.data[i] = rtacp_frame_received.data[i];
        }
        _callback_called1 = true;
    }
}

/** The callback that node2 uses to receive messages. */
static void _receive_callback_node2(const artie_can_frame_t *frame)
{
    // Parse the received frame into RTACP format and store it in the callback storage variable for node 2.
    artie_can_frame_rtacp_t rtacp_frame_received;
    artie_can_error_t err = artie_can_rtacp_parse_frame(frame, &rtacp_frame_received);
    if (err == ARTIE_CAN_ERR_NONE)
    {
        // Due to volatile qualifier, we can't use memcpy here, so we have to copy member by member.
        _frame_received_in_callback2.ack = rtacp_frame_received.ack;
        _frame_received_in_callback2.priority = rtacp_frame_received.priority;
        _frame_received_in_callback2.source_address = rtacp_frame_received.source_address;
        _frame_received_in_callback2.target_address = rtacp_frame_received.target_address;
        _frame_received_in_callback2.nbytes = rtacp_frame_received.nbytes;
        for (uint8_t i = 0; i < rtacp_frame_received.nbytes; i++)
        {
            _frame_received_in_callback2.data[i] = rtacp_frame_received.data[i];
        }
        _callback_called2 = true;
    }
}

/** The callback that node3 uses to receive messages. */
static void _receive_callback_node3(const artie_can_frame_t *frame)
{
    // Parse the received frame into RTACP format and store it in the callback storage variable for node 3.
    artie_can_frame_rtacp_t rtacp_frame_received;
    artie_can_error_t err = artie_can_rtacp_parse_frame(frame, &rtacp_frame_received);
    if (err == ARTIE_CAN_ERR_NONE)
    {
        // Due to volatile qualifier, we can't use memcpy here, so we have to copy member by member.
        _frame_received_in_callback3.ack = rtacp_frame_received.ack;
        _frame_received_in_callback3.priority = rtacp_frame_received.priority;
        _frame_received_in_callback3.source_address = rtacp_frame_received.source_address;
        _frame_received_in_callback3.target_address = rtacp_frame_received.target_address;
        _frame_received_in_callback3.nbytes = rtacp_frame_received.nbytes;
        for (uint8_t i = 0; i < rtacp_frame_received.nbytes; i++)
        {
            _frame_received_in_callback3.data[i] = rtacp_frame_received.data[i];
        }
        _callback_called3 = true;
    }
}

static void _reset_frame(volatile artie_can_frame_rtacp_t *frame)
{
    frame->ack = false;
    frame->priority = 0;
    frame->source_address = 0;
    frame->target_address = 0;
    frame->nbytes = 0;
    for (size_t i = 0; i < ARTIE_CAN_RTACP_MAX_DATA_BYTES; i++)
    {
        frame->data[i] = 0;
    }
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

    // Reset the callback items
    _callback_called1 = false;
    _callback_called2 = false;
    _callback_called3 = false;

    // Due to volatile qualifier, we can't use memset here, so we have to reset member by member.
    _reset_frame(&_frame_received_in_callback1);
    _reset_frame(&_frame_received_in_callback2);
    _reset_frame(&_frame_received_in_callback3);

    // An array of node address information. Okay for it to be on the stack.
    artie_can_tcp_addr_t node_addresses[] = {node1_addr, node2_addr, node3_addr};

    // Set up the nodes with TCP contexts
    err = artie_can_init_context_tcp(&_node1_context, &_node1_tcp_context, &node1_addr, node_addresses, ARRAY_LENGTH(node_addresses));
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_init_context_tcp(&_node2_context, &_node2_tcp_context, &node2_addr, node_addresses, ARRAY_LENGTH(node_addresses));
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_init_context_tcp(&_node3_context, &_node3_tcp_context, &node3_addr, node_addresses, ARRAY_LENGTH(node_addresses));
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Set up the nodes to use RTACP
    err = artie_can_init_context_rtacp(&_node1_context, 0x01); // Source address 0x01
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_init_context_rtacp(&_node2_context, 0x02); // Source address 0x02
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_init_context_rtacp(&_node3_context, 0x03); // Source address 0x03
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Set up the backends for the nodes
    err = artie_can_init(&_node1_context, &_node1, ARTIE_CAN_BACKEND_TCP, _receive_callback_node1, get_current_time_ms);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_init(&_node2_context, &_node2, ARTIE_CAN_BACKEND_TCP, _receive_callback_node2, get_current_time_ms);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_init(&_node3_context, &_node3, ARTIE_CAN_BACKEND_TCP, _receive_callback_node3, get_current_time_ms);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Set up a thread to run the eventloop for the nodes (tick every 150us)
    err = artie_can_start_event_loop(&_node1, 150);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_start_event_loop(&_node2, 150);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_start_event_loop(&_node3, 150);
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

    err = artie_can_close(&_node3);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Clean up contexts by zeroing them out (not strictly necessary, but good practice since we are reusing them in setUp)
    memset(&_node1, 0, sizeof(_node1));
    memset(&_node2, 0, sizeof(_node2));
    memset(&_node3, 0, sizeof(_node3));
    memset(&_node1_context, 0, sizeof(_node1_context));
    memset(&_node2_context, 0, sizeof(_node2_context));
    memset(&_node3_context, 0, sizeof(_node3_context));
    memset(&_node1_tcp_context, 0, sizeof(_node1_tcp_context));
    memset(&_node2_tcp_context, 0, sizeof(_node2_tcp_context));
    memset(&_node3_tcp_context, 0, sizeof(_node3_tcp_context));
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

    // Blocking receive on node 2 to get the frame
    err = wait_with_timeout(&_callback_called2, DEFAULT_TIMEOUT_MS);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Check that the received frame matches the sent frame
    assert_rtacp_frames_equal(&rtacp_frame, &_frame_received_in_callback2);

    // Blocking receive on node 3 to get the frame
    err = wait_with_timeout(&_callback_called3, DEFAULT_TIMEOUT_MS);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Check that the received frame matches the sent frame
    assert_rtacp_frames_equal(&rtacp_frame, &_frame_received_in_callback3);
}

// Tests that need to be implemented:
// - Test that we can send a message from one node to another (not broadcast) and receive it.
// - Test echo
// - Test that if we send a message to a specific target address, only the node with that address receives it and not other nodes.
// - Update broadcast test to include multiple recipient nodes and check that they all receive the message.
// - Test sending multiple messages in a row to a single address

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

    // Finish and return results
    return UNITY_END();
}
