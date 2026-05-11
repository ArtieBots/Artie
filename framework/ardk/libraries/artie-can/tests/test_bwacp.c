/**
 * @file test_bwacp.c
 * @brief Test the BWACP (Block Write Artie CAN Protocol) implementation.
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
#define DEFAULT_TIMEOUT_MS ((uint32_t)(1.25 * ARTIE_CAN_BWACP_TIMEOUT_MS))

// Size of receive buffers for BWACP tests
#define RECEIVE_BUFFER_SIZE 65536

// A few nodes that the tests use for communication.
static const artie_can_tcp_addr_t node1_addr = { .host = "127.0.0.1", .port = 6001 };
static const artie_can_tcp_addr_t node2_addr = { .host = "127.0.0.1", .port = 6002 };
static const artie_can_tcp_addr_t node3_addr = { .host = "127.0.0.1", .port = 6003 };
static artie_can_context_t _node1_context;
static artie_can_context_t _node2_context;
static artie_can_context_t _node3_context;
static artie_can_backend_t _node1;
static artie_can_backend_t _node2;
static artie_can_backend_t _node3;
static artie_can_tcp_context_t _node1_tcp_context;
static artie_can_tcp_context_t _node2_tcp_context;
static artie_can_tcp_context_t _node3_tcp_context;

// Receive buffers for BWACP
static uint8_t _node1_receive_buffer[RECEIVE_BUFFER_SIZE];
static uint8_t _node2_receive_buffer[RECEIVE_BUFFER_SIZE];
static uint8_t _node3_receive_buffer[RECEIVE_BUFFER_SIZE];

static void _run_event_loops(void)
{
    // Run one tick of the event loop for each node
    artie_can_tick(&_node1);
    artie_can_tick(&_node2);
    artie_can_tick(&_node3);
}

static void _empty_rx_callback(const artie_can_frame_t *frame)
{
    // Do nothing - we will check the receive buffers directly in the tests
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

    // Clear receive buffers
    memset(_node1_receive_buffer, 0, sizeof(_node1_receive_buffer));
    memset(_node2_receive_buffer, 0, sizeof(_node2_receive_buffer));
    memset(_node3_receive_buffer, 0, sizeof(_node3_receive_buffer));

    // An array of node address information. Okay for it to be on the stack.
    artie_can_tcp_addr_t node_addresses[] = {node1_addr, node2_addr, node3_addr};

    // Set up the nodes with TCP contexts
    err = artie_can_init_context_tcp(&_node1_context, &_node1_tcp_context, &node1_addr, node_addresses, ARRAY_LENGTH(node_addresses));
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_init_context_tcp(&_node2_context, &_node2_tcp_context, &node2_addr, node_addresses, ARRAY_LENGTH(node_addresses));
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_init_context_tcp(&_node3_context, &_node3_tcp_context, &node3_addr, node_addresses, ARRAY_LENGTH(node_addresses));
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Set up the nodes to use BWACP
    err = artie_can_init_context_bwacp(&_node1_context, 0x01, ARTIE_CAN_BWACP_CLASS_SBC); // Node 1: SBC
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_init_context_bwacp(&_node2_context, 0x02, ARTIE_CAN_BWACP_CLASS_SENSOR); // Node 2: Sensor
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_init_context_bwacp(&_node3_context, 0x03, ARTIE_CAN_BWACP_CLASS_SENSOR); // Node 3: Sensor
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Set receive buffers for BWACP
    err = artie_can_bwacp_set_receive_buffer(&_node1_context, _node1_receive_buffer, sizeof(_node1_receive_buffer));
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_bwacp_set_receive_buffer(&_node2_context, _node2_receive_buffer, sizeof(_node2_receive_buffer));
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_bwacp_set_receive_buffer(&_node3_context, _node3_receive_buffer, sizeof(_node3_receive_buffer));
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Set up the backends for the nodes
    err = artie_can_init(&_node1_context, &_node1, ARTIE_CAN_BACKEND_TCP, _empty_rx_callback, get_current_time_ms);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_init(&_node2_context, &_node2, ARTIE_CAN_BACKEND_TCP, _empty_rx_callback, get_current_time_ms);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    err = artie_can_init(&_node3_context, &_node3, ARTIE_CAN_BACKEND_TCP, _empty_rx_callback, get_current_time_ms);
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
 * @brief Test sending a single byte over the BWACP protocol.
 * Send to two other nodes and verify that they both received the byte successfully.
 *
 */
void test_send_one_byte(void)
{
    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    printf("!!!!!!!!!!!! Starting test_send_one_byte !!!!!!!!!!!\n");
    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

    artie_can_error_t err;

    // Create a 1-byte payload
    uint8_t send_data[] = {0x42};
    // Address specifies the offset within the receive buffer where data should be written
    uint32_t buffer_offset = 0x1000;

    // Send from node 1 to multicast address targeting SENSOR class (nodes 2 and 3 are sensors)
    err = artie_can_bwacp_send(&_node1, send_data, sizeof(send_data), buffer_offset, ARTIE_CAN_BWACP_MULTICAST_ADDRESS, ARTIE_CAN_BWACP_CLASS_SENSOR, ARTIE_CAN_FRAME_PRIORITY_BWACP_MEDIUM);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Run event loops and wait for node 1 to finish sending
    // Node 1 should transition to WAITING_COMPLETE, then back to IDLE after timeout
    bool node1_complete = false;
    uint64_t start_time = get_current_time_ms();
    while (!node1_complete && (get_current_time_ms() - start_time) < DEFAULT_TIMEOUT_MS)
    {
        _run_event_loops();
        SLEEP_MS(1);
        if (_node1_context.bwacp_context.state == BWACP_STATE_IDLE)
        {
            node1_complete = true;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(node1_complete, "Node 1 did not complete sending");

    // Check that node 2 received the byte at the correct buffer offset
    TEST_ASSERT_EQUAL_UINT8(0x42, _node2_receive_buffer[buffer_offset]);
    TEST_ASSERT_EQUAL_UINT32(buffer_offset, _node2_context.bwacp_context.receive_address);

    // Check that node 3 received the byte at the correct buffer offset
    TEST_ASSERT_EQUAL_UINT8(0x42, _node3_receive_buffer[buffer_offset]);
    TEST_ASSERT_EQUAL_UINT32(buffer_offset, _node3_context.bwacp_context.receive_address);

    printf("!!!!!!!!!!!! test_send_one_byte PASSED !!!!!!!!!!!!!!\n");
}

/**
 * @brief Same as test_send_one_byte, but sends 4 bytes.
 *
 */
void test_send_four_bytes(void)
{
    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    printf("!!!!!!!!!!!! Starting test_send_four_bytes !!!!!!!!!!\n");
    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

    artie_can_error_t err;

    // Create a 4-byte payload
    uint8_t send_data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    // Address specifies the offset within the receive buffer where data should be written
    uint32_t buffer_offset = 0x2000;

    // Send from node 1 to multicast address targeting SENSOR class (nodes 2 and 3 are sensors)
    err = artie_can_bwacp_send(&_node1, send_data, sizeof(send_data), buffer_offset, ARTIE_CAN_BWACP_MULTICAST_ADDRESS, ARTIE_CAN_BWACP_CLASS_SENSOR, ARTIE_CAN_FRAME_PRIORITY_BWACP_MEDIUM);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Run event loops and wait for node 1 to finish sending
    // Node 1 should transition to WAITING_COMPLETE, then back to IDLE after timeout
    bool node1_complete = false;
    uint64_t start_time = get_current_time_ms();
    while (!node1_complete && (get_current_time_ms() - start_time) < DEFAULT_TIMEOUT_MS)
    {
        _run_event_loops();
        SLEEP_MS(1);
        if (_node1_context.bwacp_context.state == BWACP_STATE_IDLE)
        {
            node1_complete = true;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(node1_complete, "Node 1 did not complete sending");

    // Check that node 2 received the bytes at the correct buffer offset
    TEST_ASSERT_EQUAL_UINT8(0xDE, _node2_receive_buffer[buffer_offset]);
    TEST_ASSERT_EQUAL_UINT8(0xAD, _node2_receive_buffer[buffer_offset + 1]);
    TEST_ASSERT_EQUAL_UINT8(0xBE, _node2_receive_buffer[buffer_offset + 2]);
    TEST_ASSERT_EQUAL_UINT8(0xEF, _node2_receive_buffer[buffer_offset + 3]);
    TEST_ASSERT_EQUAL_UINT32(buffer_offset, _node2_context.bwacp_context.receive_address);

    // Check that node 3 received the bytes at the correct buffer offset
    TEST_ASSERT_EQUAL_UINT8(0xDE, _node3_receive_buffer[buffer_offset]);
    TEST_ASSERT_EQUAL_UINT8(0xAD, _node3_receive_buffer[buffer_offset + 1]);
    TEST_ASSERT_EQUAL_UINT8(0xBE, _node3_receive_buffer[buffer_offset + 2]);
    TEST_ASSERT_EQUAL_UINT8(0xEF, _node3_receive_buffer[buffer_offset + 3]);
    TEST_ASSERT_EQUAL_UINT32(buffer_offset, _node3_context.bwacp_context.receive_address);

    printf("!!!!!!!!!!!! test_send_four_bytes PASSED !!!!!!!!!!!\n");
}

/**
 * @brief Same as test_send_one_byte, but sends 8 bytes.
 *
 */
void test_send_eight_bytes(void)
{
    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    printf("!!!!!!!!!!!! Starting test_send_eight_bytes !!!!!!!!!\n");
    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

    artie_can_error_t err;

    // Create an 8-byte payload
    uint8_t send_data[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    // Address specifies the offset within the receive buffer where data should be written
    uint32_t buffer_offset = 0x3000;

    // Send from node 1 to multicast address targeting SENSOR class (nodes 2 and 3 are sensors)
    err = artie_can_bwacp_send(&_node1, send_data, sizeof(send_data), buffer_offset, ARTIE_CAN_BWACP_MULTICAST_ADDRESS, ARTIE_CAN_BWACP_CLASS_SENSOR, ARTIE_CAN_FRAME_PRIORITY_BWACP_MEDIUM);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Run event loops and wait for node 1 to finish sending
    // Node 1 should transition to WAITING_COMPLETE, then back to IDLE after timeout
    bool node1_complete = false;
    uint64_t start_time = get_current_time_ms();
    while (!node1_complete && (get_current_time_ms() - start_time) < DEFAULT_TIMEOUT_MS)
    {
        _run_event_loops();
        SLEEP_MS(1);
        if (_node1_context.bwacp_context.state == BWACP_STATE_IDLE)
        {
            node1_complete = true;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(node1_complete, "Node 1 did not complete sending");

    // Check that node 2 received the bytes at the correct buffer offset
    TEST_ASSERT_EQUAL_UINT8(0x01, _node2_receive_buffer[buffer_offset]);
    TEST_ASSERT_EQUAL_UINT8(0x23, _node2_receive_buffer[buffer_offset + 1]);
    TEST_ASSERT_EQUAL_UINT8(0x45, _node2_receive_buffer[buffer_offset + 2]);
    TEST_ASSERT_EQUAL_UINT8(0x67, _node2_receive_buffer[buffer_offset + 3]);
    TEST_ASSERT_EQUAL_UINT8(0x89, _node2_receive_buffer[buffer_offset + 4]);
    TEST_ASSERT_EQUAL_UINT8(0xAB, _node2_receive_buffer[buffer_offset + 5]);
    TEST_ASSERT_EQUAL_UINT8(0xCD, _node2_receive_buffer[buffer_offset + 6]);
    TEST_ASSERT_EQUAL_UINT8(0xEF, _node2_receive_buffer[buffer_offset + 7]);
    TEST_ASSERT_EQUAL_UINT32(buffer_offset, _node2_context.bwacp_context.receive_address);

    // Check that node 3 received the bytes at the correct buffer offset
    TEST_ASSERT_EQUAL_UINT8(0x01, _node3_receive_buffer[buffer_offset]);
    TEST_ASSERT_EQUAL_UINT8(0x23, _node3_receive_buffer[buffer_offset + 1]);
    TEST_ASSERT_EQUAL_UINT8(0x45, _node3_receive_buffer[buffer_offset + 2]);
    TEST_ASSERT_EQUAL_UINT8(0x67, _node3_receive_buffer[buffer_offset + 3]);
    TEST_ASSERT_EQUAL_UINT8(0x89, _node3_receive_buffer[buffer_offset + 4]);
    TEST_ASSERT_EQUAL_UINT8(0xAB, _node3_receive_buffer[buffer_offset + 5]);
    TEST_ASSERT_EQUAL_UINT8(0xCD, _node3_receive_buffer[buffer_offset + 6]);
    TEST_ASSERT_EQUAL_UINT8(0xEF, _node3_receive_buffer[buffer_offset + 7]);
    TEST_ASSERT_EQUAL_UINT32(buffer_offset, _node3_context.bwacp_context.receive_address);

    printf("!!!!!!!!!!!! test_send_eight_bytes PASSED !!!!!!!!!!!\n");
}

/**
 * @brief Same as test_send_one_byte, but sends 254 bytes.
 *
 */
void test_send_254_bytes(void)
{
    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    printf("!!!!!!!!!!!! Starting test_send_254_bytes !!!!!!!!!!\n");
    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

    artie_can_error_t err;

    // Create a 254-byte payload with incrementing pattern
    uint8_t send_data[254];
    for (size_t i = 0; i < sizeof(send_data); i++)
    {
        send_data[i] = (uint8_t)i;
    }
    // Address specifies the offset within the receive buffer where data should be written
    uint32_t buffer_offset = 0x4000;

    // Send from node 1 to multicast address targeting SENSOR class (nodes 2 and 3 are sensors)
    err = artie_can_bwacp_send(&_node1, send_data, sizeof(send_data), buffer_offset, ARTIE_CAN_BWACP_MULTICAST_ADDRESS, ARTIE_CAN_BWACP_CLASS_SENSOR, ARTIE_CAN_FRAME_PRIORITY_BWACP_MEDIUM);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Run event loops and wait for node 1 to finish sending
    // Node 1 should transition to WAITING_COMPLETE, then back to IDLE after timeout
    bool node1_complete = false;
    uint64_t start_time = get_current_time_ms();
    while (!node1_complete && (get_current_time_ms() - start_time) < DEFAULT_TIMEOUT_MS)
    {
        _run_event_loops();
        SLEEP_MS(1);
        if (_node1_context.bwacp_context.state == BWACP_STATE_IDLE)
        {
            node1_complete = true;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(node1_complete, "Node 1 did not complete sending");

    // Check that node 2 received the bytes at the correct buffer offset
    for (size_t i = 0; i < sizeof(send_data); i++)
    {
        TEST_ASSERT_EQUAL_UINT8((uint8_t)i, _node2_receive_buffer[buffer_offset + i]);
    }
    TEST_ASSERT_EQUAL_UINT32(buffer_offset, _node2_context.bwacp_context.receive_address);

    // Check that node 3 received the bytes at the correct buffer offset
    for (size_t i = 0; i < sizeof(send_data); i++)
    {
        TEST_ASSERT_EQUAL_UINT8((uint8_t)i, _node3_receive_buffer[buffer_offset + i]);
    }
    TEST_ASSERT_EQUAL_UINT32(buffer_offset, _node3_context.bwacp_context.receive_address);

    printf("!!!!!!!!!!!! test_send_254_bytes PASSED !!!!!!!!!!!\n");
}

/**
 * @brief Same as test_send_one_byte, but sends 255 bytes.
 *
 */
void test_send_255_bytes(void)
{
}

/**
 * @brief Same as test_send_one_byte, but sends 256 bytes.
 *
 */
void test_send_256_bytes(void)
{
}

/**
 * @brief Same as test_send_one_byte, but sends 257 bytes.
 *
 */
void test_send_257_bytes(void)
{
}

/**
 * @brief Same as test_send_one_byte, but sends 46kB.
 *
 */
void test_send_46k_bytes(void)
{
}

/**
 * @brief Test to ensure we ask for a repeat when the data is invalid.
 *
 */
void test_crc_mismatch(void)
{
    // This one's tricky. Need to set up a really long bulk write, and
    // while that's going on, flip a bit in the place where the rx is happening (and has already been written to),
    // so we invalidate the data and the CRC check should fail, causing the
    // whole thing to happen again.
    // After it finishes, we check if the data is actually correct now (since
    // the write should have started over and the buffer that we meddled with
    // should now have the correct contents this time)
}

/**
 * @brief Test addressing a single target node.
 *
 */
void test_one_target_node(void)
{
}

/**
 * @brief Test addressing a class of nodes.
 *
 */
void test_class_of_target_nodes(void)
{
    // One node should consider itself a motor node and another should consider itself a
    // sensor node. Test that we write to only the sensor node when we attempt to address
    // to the sensor class.
}

/**
 * @brief Test RTACP can happen at the same time as a bulk write.
 *
 */
void test_rtacp_while_bwacp(void)
{
    // Set up a longish bulk write, and in the middle of it,
    // test that an RTACP transfer to a BWACP-receiving node still works.
    // The node should successfully receive the RTACP data and the BWACP data.
}

/**
 * @brief Test that two concurrent BWACP transfers do not mess
 * each other up.
 *
 */
void test_concurrent_bwacp(void)
{
    // Four nodes: A, B, C, and D
    // Nodes C and D will be receiving bulk transfers and are
    // both sensor nodes.
    // Node A starts a bwacp transfer to node C by address.
    // Node B starts a bwacp transfer to sensor class.
    // Node D should receive all the bytes from node B,
    // while node C should receive all the bytes from node A.
    // Node C is busy receiving from A so even though it is a sensor
    // node, it can't listen to the bwacp transfer from B.
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
    RUN_TEST(test_send_one_byte);
    #if 0
    RUN_TEST(test_send_four_bytes);
    RUN_TEST(test_send_eight_bytes);
    RUN_TEST(test_send_254_bytes);
    RUN_TEST(test_send_255_bytes);
    RUN_TEST(test_send_256_bytes);
    RUN_TEST(test_send_257_bytes);
    RUN_TEST(test_send_46k_bytes);
    RUN_TEST(test_crc_mismatch);
    RUN_TEST(test_one_target_node);
    RUN_TEST(test_class_of_target_nodes);
    RUN_TEST(test_rtacp_while_bwacp);
    RUN_TEST(test_concurrent_bwacp);
    #endif

    // Finish and return results
    return UNITY_END();
}
