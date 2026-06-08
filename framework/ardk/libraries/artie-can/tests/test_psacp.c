/**
 * @file test_psacp.c
 * @brief Test the PSACP (Pub/Sub Artie CAN Protocol) implementation.
 * Uses UDP Multicast backend.
 *
 * Setup:
 *
 * * 4 nodes
 * * node 1: Subscribes to topic 0x10
 * * node 2: Subscribes to topics 0x10, 0x0C
 * * node 3: Subscribes to topics 0x0C, 0x23
 * * node 4: Subscribes to topics 0x23, 0x45
 *
 * Tests:
 *
 * * Test 1: Node 1 sends a single byte to topic 0x0C
 *     - Nodes 2 and 3 receive
 *     - Nodes 1 and 4 do not
 * * Test 2: Node 1 sends a single byte to broadcast (topic 0x00)
 *     - All nodes receive (including the sender - node 1 - note that this means
 *       that a sending node must check an outgoing message to see if it should "receive"
 *       it)
 * * Test 3: Node 1 sends a single byte to topic 0x10
 *     - Nodes 1 and 2 receive
 *     - Nodes 3 and 4 do not
 * * Test 4: Node 1 begins a BWACP transaction to node 4.
 *     While the transaction is going on, Node 3 publishes a byte to topic 0x45 at high priority pub/sub.
 *     BWACP transaction should complete correctly and the byte should have been transferred as well.
 * * Test 5: Node 1 begins a BWACP transaction to node 4.
 *     While the transaction is going on, Node 3 spams the bus with low priority pub/sub messages,
 *     to simulate logging while a BWACP transfer is going on. The BWACP transaction should complete
 *     successfully. We don't bother checking the received bytes of pub/sub in this test - we are just
 *     attempting to ensure that the BWACP transfer is unaffected by the low-priority pub/sub noise
 *     on the bus.
 * * Test 6: Node 1 sends a single byte to topic 0x0C.
 *     - Nodes 2 and 3 receive; nodes 1 and 4 do not.
 *       Node 2 then unsubscribes from topic 0x0C, and Node 1 sends again.
 *     - This time, only node 3 receives; all others do not.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "unity.h"
#include "artie_can.h"
#include "backend_udp_mcast.h"
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

// Size of receive buffers for BWACP (used in tests 4 and 5)
#define RECEIVE_BUFFER_SIZE 65536

// Multicast configuration for all test nodes
static const char *multicast_group = "239.0.0.1";
static const uint16_t multicast_port = 6001; // Different port from other test suites

// Node addresses
#define NODE1_ADDR 0x01U
#define NODE2_ADDR 0x02U
#define NODE3_ADDR 0x03U
#define NODE4_ADDR 0x04U

// Topics used in the tests
#define TOPIC_0x0C 0x0CU
#define TOPIC_0x10 0x10U
#define TOPIC_0x23 0x23U
#define TOPIC_0x45 0x45U

static artie_can_context_t _node1_context;
static artie_can_context_t _node2_context;
static artie_can_context_t _node3_context;
static artie_can_context_t _node4_context;
static artie_can_backend_t _node1;
static artie_can_backend_t _node2;
static artie_can_backend_t _node3;
static artie_can_backend_t _node4;
static artie_can_udp_mcast_context_t _node1_udp_mcast_context;
static artie_can_udp_mcast_context_t _node2_udp_mcast_context;
static artie_can_udp_mcast_context_t _node3_udp_mcast_context;
static artie_can_udp_mcast_context_t _node4_udp_mcast_context;

// BWACP receive buffers (used in tests 4 and 5)
static uint8_t _node4_receive_buffer[RECEIVE_BUFFER_SIZE];

// PSACP received frame tracking (written from ISR/callback context)
static volatile bool _psacp_received_node1 = false;
static volatile bool _psacp_received_node2 = false;
static volatile bool _psacp_received_node3 = false;
static volatile bool _psacp_received_node4 = false;
static volatile artie_can_frame_psacp_t _psacp_frame_node1;
static volatile artie_can_frame_psacp_t _psacp_frame_node2;
static volatile artie_can_frame_psacp_t _psacp_frame_node3;
static volatile artie_can_frame_psacp_t _psacp_frame_node4;

static void _reset_psacp_flags(void)
{
    _psacp_received_node1 = false;
    _psacp_received_node2 = false;
    _psacp_received_node3 = false;
    _psacp_received_node4 = false;
    memset((void *)&_psacp_frame_node1, 0, sizeof(_psacp_frame_node1));
    memset((void *)&_psacp_frame_node2, 0, sizeof(_psacp_frame_node2));
    memset((void *)&_psacp_frame_node3, 0, sizeof(_psacp_frame_node3));
    memset((void *)&_psacp_frame_node4, 0, sizeof(_psacp_frame_node4));
}

static void _receive_callback_node1(const artie_can_frame_t *frame)
{
    artie_can_frame_psacp_t psacp_frame;
    artie_can_error_t err = artie_can_psacp_parse_frame(frame, &psacp_frame);
    if (err == ARTIE_CAN_ERR_NONE)
    {
        _psacp_frame_node1.high_priority = psacp_frame.high_priority;
        _psacp_frame_node1.priority      = psacp_frame.priority;
        _psacp_frame_node1.source_address = psacp_frame.source_address;
        _psacp_frame_node1.topic         = psacp_frame.topic;
        _psacp_frame_node1.nbytes        = psacp_frame.nbytes;
        for (uint8_t i = 0; i < psacp_frame.nbytes; i++)
        {
            _psacp_frame_node1.data[i] = psacp_frame.data[i];
        }
        _psacp_received_node1 = true;
    }
}

static void _receive_callback_node2(const artie_can_frame_t *frame)
{
    artie_can_frame_psacp_t psacp_frame;
    artie_can_error_t err = artie_can_psacp_parse_frame(frame, &psacp_frame);
    if (err == ARTIE_CAN_ERR_NONE)
    {
        _psacp_frame_node2.high_priority  = psacp_frame.high_priority;
        _psacp_frame_node2.priority       = psacp_frame.priority;
        _psacp_frame_node2.source_address = psacp_frame.source_address;
        _psacp_frame_node2.topic          = psacp_frame.topic;
        _psacp_frame_node2.nbytes         = psacp_frame.nbytes;
        for (uint8_t i = 0; i < psacp_frame.nbytes; i++)
        {
            _psacp_frame_node2.data[i] = psacp_frame.data[i];
        }
        _psacp_received_node2 = true;
    }
}

static void _receive_callback_node3(const artie_can_frame_t *frame)
{
    artie_can_frame_psacp_t psacp_frame;
    artie_can_error_t err = artie_can_psacp_parse_frame(frame, &psacp_frame);
    if (err == ARTIE_CAN_ERR_NONE)
    {
        _psacp_frame_node3.high_priority  = psacp_frame.high_priority;
        _psacp_frame_node3.priority       = psacp_frame.priority;
        _psacp_frame_node3.source_address = psacp_frame.source_address;
        _psacp_frame_node3.topic          = psacp_frame.topic;
        _psacp_frame_node3.nbytes         = psacp_frame.nbytes;
        for (uint8_t i = 0; i < psacp_frame.nbytes; i++)
        {
            _psacp_frame_node3.data[i] = psacp_frame.data[i];
        }
        _psacp_received_node3 = true;
    }
}

static void _receive_callback_node4(const artie_can_frame_t *frame)
{
    artie_can_frame_psacp_t psacp_frame;
    artie_can_error_t err = artie_can_psacp_parse_frame(frame, &psacp_frame);
    if (err == ARTIE_CAN_ERR_NONE)
    {
        _psacp_frame_node4.high_priority  = psacp_frame.high_priority;
        _psacp_frame_node4.priority       = psacp_frame.priority;
        _psacp_frame_node4.source_address = psacp_frame.source_address;
        _psacp_frame_node4.topic          = psacp_frame.topic;
        _psacp_frame_node4.nbytes         = psacp_frame.nbytes;
        for (uint8_t i = 0; i < psacp_frame.nbytes; i++)
        {
            _psacp_frame_node4.data[i] = psacp_frame.data[i];
        }
        _psacp_received_node4 = true;
    }
}

static void _run_event_loops(void)
{
    artie_can_error_t err;

    err = artie_can_tick(&_node1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        printf("Error ticking node 1: %d\n", err);
        TEST_FAIL_MESSAGE("Error ticking node 1");
    }

    err = artie_can_tick(&_node2);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        printf("Error ticking node 2: %d\n", err);
        TEST_FAIL_MESSAGE("Error ticking node 2");
    }

    err = artie_can_tick(&_node3);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        printf("Error ticking node 3: %d\n", err);
        TEST_FAIL_MESSAGE("Error ticking node 3");
    }

    err = artie_can_tick(&_node4);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        printf("Error ticking node 4: %d\n", err);
        TEST_FAIL_MESSAGE("Error ticking node 4");
    }
}

/**
 * @brief Setup function called before each test.
 *
 * Initializes 4 nodes, each configured for both PSACP and BWACP (for the mixed tests).
 * Subscriptions per the test description:
 *   Node 1: topic 0x10
 *   Node 2: topics 0x10, 0x0C
 *   Node 3: topics 0x0C, 0x23
 *   Node 4: topics 0x23, 0x45
 */
void setUp(void)
{
    artie_can_error_t err;

    _reset_psacp_flags();
    memset(_node4_receive_buffer, 0, sizeof(_node4_receive_buffer));

    // Initialize UDP multicast transport contexts
    err = artie_can_init_context_udp_mcast(&_node1_context, &_node1_udp_mcast_context, multicast_group, multicast_port);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
    err = artie_can_init_context_udp_mcast(&_node2_context, &_node2_udp_mcast_context, multicast_group, multicast_port);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
    err = artie_can_init_context_udp_mcast(&_node3_context, &_node3_udp_mcast_context, multicast_group, multicast_port);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
    err = artie_can_init_context_udp_mcast(&_node4_context, &_node4_udp_mcast_context, multicast_group, multicast_port);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Initialize PSACP protocol contexts
    err = artie_can_init_context_psacp(&_node1_context, NODE1_ADDR);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
    err = artie_can_init_context_psacp(&_node2_context, NODE2_ADDR);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
    err = artie_can_init_context_psacp(&_node3_context, NODE3_ADDR);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
    err = artie_can_init_context_psacp(&_node4_context, NODE4_ADDR);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Initialize BWACP protocol contexts (needed for tests 4 and 5)
    err = artie_can_init_context_bwacp(&_node1_context, NODE1_ADDR, ARTIE_CAN_BWACP_CLASS_SBC);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
    err = artie_can_init_context_bwacp(&_node2_context, NODE2_ADDR, ARTIE_CAN_BWACP_CLASS_SBC);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
    err = artie_can_init_context_bwacp(&_node3_context, NODE3_ADDR, ARTIE_CAN_BWACP_CLASS_SBC);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
    err = artie_can_init_context_bwacp(&_node4_context, NODE4_ADDR, ARTIE_CAN_BWACP_CLASS_SBC);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Set BWACP receive buffer for node 4 (the BWACP receiver in tests 4 and 5)
    err = artie_can_bwacp_set_receive_buffer(&_node4_context, _node4_receive_buffer, sizeof(_node4_receive_buffer));
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Set up PSACP subscriptions
    // Node 1: topic 0x10
    err = artie_can_psacp_subscribe(&_node1_context, TOPIC_0x10);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Node 2: topics 0x10, 0x0C
    err = artie_can_psacp_subscribe(&_node2_context, TOPIC_0x10);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
    err = artie_can_psacp_subscribe(&_node2_context, TOPIC_0x0C);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Node 3: topics 0x0C, 0x23
    err = artie_can_psacp_subscribe(&_node3_context, TOPIC_0x0C);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
    err = artie_can_psacp_subscribe(&_node3_context, TOPIC_0x23);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Node 4: topics 0x23, 0x45
    err = artie_can_psacp_subscribe(&_node4_context, TOPIC_0x23);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
    err = artie_can_psacp_subscribe(&_node4_context, TOPIC_0x45);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Initialize backends
    err = artie_can_init(&_node1_context, &_node1, ARTIE_CAN_BACKEND_UDP_MCAST, _receive_callback_node1, get_current_time_ms);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
    err = artie_can_init(&_node2_context, &_node2, ARTIE_CAN_BACKEND_UDP_MCAST, _receive_callback_node2, get_current_time_ms);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
    err = artie_can_init(&_node3_context, &_node3, ARTIE_CAN_BACKEND_UDP_MCAST, _receive_callback_node3, get_current_time_ms);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
    err = artie_can_init(&_node4_context, &_node4, ARTIE_CAN_BACKEND_UDP_MCAST, _receive_callback_node4, get_current_time_ms);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
}

/**
 * @brief Teardown function called after each test.
 */
void tearDown(void)
{
    artie_can_close(&_node1);
    artie_can_close(&_node2);
    artie_can_close(&_node3);
    artie_can_close(&_node4);

    memset(&_node1, 0, sizeof(_node1));
    memset(&_node2, 0, sizeof(_node2));
    memset(&_node3, 0, sizeof(_node3));
    memset(&_node4, 0, sizeof(_node4));
    memset(&_node1_context, 0, sizeof(_node1_context));
    memset(&_node2_context, 0, sizeof(_node2_context));
    memset(&_node3_context, 0, sizeof(_node3_context));
    memset(&_node4_context, 0, sizeof(_node4_context));
    memset(&_node1_udp_mcast_context, 0, sizeof(_node1_udp_mcast_context));
    memset(&_node2_udp_mcast_context, 0, sizeof(_node2_udp_mcast_context));
    memset(&_node3_udp_mcast_context, 0, sizeof(_node3_udp_mcast_context));
    memset(&_node4_udp_mcast_context, 0, sizeof(_node4_udp_mcast_context));
    memset(&_psacp_frame_node1, 0, sizeof(_psacp_frame_node1));
    memset(&_psacp_frame_node2, 0, sizeof(_psacp_frame_node2));
    memset(&_psacp_frame_node3, 0, sizeof(_psacp_frame_node3));
    memset(&_psacp_frame_node4, 0, sizeof(_psacp_frame_node4));
}

/**
 * @brief Test 1: Node 1 publishes to topic 0x0C.
 * Expected: nodes 2 and 3 receive; nodes 1 and 4 do not.
 */
void test_publish_to_non_subscribed_topic(void)
{
    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    printf("!!! Starting test_publish_to_non_subscribed_topic !!\n");
    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

    artie_can_error_t err;

    // Build the PSACP frame: node 1 publishes one byte to topic 0x0C (low priority)
    uint8_t payload = 0xA5;
    artie_can_frame_psacp_t psacp_frame = {
        .high_priority  = false,
        .priority       = ARTIE_CAN_FRAME_PRIORITY_PSACP_MEDIUM_LOW,
        .source_address = NODE1_ADDR,
        .topic          = TOPIC_0x0C,
        .nbytes         = 1,
        .data           = {7}
    };
    psacp_frame.data[0] = payload;

    artie_can_frame_t frame_to_send;
    err = artie_can_psacp_init_frame(&frame_to_send, &psacp_frame);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Publish from node 1
    err = artie_can_psacp_publish(&_node1, &frame_to_send);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Node 1 is not subscribed to 0x0C - it should NOT have received via local delivery
    TEST_ASSERT_FALSE(_psacp_received_node1);

    // Node 2 is subscribed to 0x0C - wait for it to receive
    err = wait_with_timeout(&_psacp_received_node2, DEFAULT_TIMEOUT_MS, _run_event_loops);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ARTIE_CAN_ERR_NONE, err, "Node 2 timed out waiting for PSACP frame");

    TEST_ASSERT_EQUAL_UINT8(TOPIC_0x0C, _psacp_frame_node2.topic);
    TEST_ASSERT_EQUAL_UINT8(NODE1_ADDR, _psacp_frame_node2.source_address);
    TEST_ASSERT_EQUAL_UINT8(1, _psacp_frame_node2.nbytes);
    TEST_ASSERT_EQUAL_UINT8(payload, _psacp_frame_node2.data[0]);

    // Node 3 is subscribed to 0x0C - wait for it to receive
    err = wait_with_timeout(&_psacp_received_node3, DEFAULT_TIMEOUT_MS, _run_event_loops);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ARTIE_CAN_ERR_NONE, err, "Node 3 timed out waiting for PSACP frame");

    TEST_ASSERT_EQUAL_UINT8(TOPIC_0x0C, _psacp_frame_node3.topic);
    TEST_ASSERT_EQUAL_UINT8(NODE1_ADDR, _psacp_frame_node3.source_address);
    TEST_ASSERT_EQUAL_UINT8(1, _psacp_frame_node3.nbytes);
    TEST_ASSERT_EQUAL_UINT8(payload, _psacp_frame_node3.data[0]);

    // Node 4 should NOT have received (not subscribed to 0x0C per test spec)
    // Wait a short time to be sure no late delivery arrives
    SLEEP_MS(100);
    _run_event_loops();
    TEST_ASSERT_FALSE_MESSAGE(_psacp_received_node4, "Node 4 should not have received the frame");

    printf("!!! test_publish_to_non_subscribed_topic PASSED !!!!\n");
}

/**
 * @brief Test 2: Node 1 publishes to the broadcast topic (0x00).
 * Expected: all four nodes receive, including the sender (node 1).
 */
void test_publish_broadcast(void)
{
    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    printf("!!!!!!!!!!!!! Starting test_publish_broadcast !!!!!!!\n");
    printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

    artie_can_error_t err;

    // Build the PSACP frame: node 1 publishes one byte to the broadcast topic (0x00)
    uint8_t payload = 0xBC;
    artie_can_frame_psacp_t psacp_frame = {
        .high_priority  = false,
        .priority       = ARTIE_CAN_FRAME_PRIORITY_PSACP_MEDIUM_LOW,
        .source_address = NODE1_ADDR,
        .topic          = ARTIE_CAN_PSACP_TOPIC_BROADCAST,
        .nbytes         = 1,
        .data           = {9}
    };
    psacp_frame.data[0] = payload;

    artie_can_frame_t frame_to_send;
    err = artie_can_psacp_init_frame(&frame_to_send, &psacp_frame);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Publish from node 1. Local delivery should fire immediately for node 1
    // (broadcast is always delivered, regardless of topic subscriptions).
    err = artie_can_psacp_publish(&_node1, &frame_to_send);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    // Node 1 should have received via local delivery already
    TEST_ASSERT_TRUE_MESSAGE(_psacp_received_node1, "Node 1 should have received its own broadcast via local delivery");
    TEST_ASSERT_EQUAL_UINT8(ARTIE_CAN_PSACP_TOPIC_BROADCAST, _psacp_frame_node1.topic);
    TEST_ASSERT_EQUAL_UINT8(NODE1_ADDR, _psacp_frame_node1.source_address);
    TEST_ASSERT_EQUAL_UINT8(1, _psacp_frame_node1.nbytes);
    TEST_ASSERT_EQUAL_UINT8(payload, _psacp_frame_node1.data[0]);

    // Nodes 2, 3, and 4 should all receive via the bus
    err = wait_with_timeout(&_psacp_received_node2, DEFAULT_TIMEOUT_MS, _run_event_loops);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ARTIE_CAN_ERR_NONE, err, "Node 2 timed out waiting for broadcast");
    TEST_ASSERT_EQUAL_UINT8(ARTIE_CAN_PSACP_TOPIC_BROADCAST, _psacp_frame_node2.topic);
    TEST_ASSERT_EQUAL_UINT8(NODE1_ADDR, _psacp_frame_node2.source_address);
    TEST_ASSERT_EQUAL_UINT8(1, _psacp_frame_node2.nbytes);
    TEST_ASSERT_EQUAL_UINT8(payload, _psacp_frame_node2.data[0]);

    err = wait_with_timeout(&_psacp_received_node3, DEFAULT_TIMEOUT_MS, _run_event_loops);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ARTIE_CAN_ERR_NONE, err, "Node 3 timed out waiting for broadcast");
    TEST_ASSERT_EQUAL_UINT8(ARTIE_CAN_PSACP_TOPIC_BROADCAST, _psacp_frame_node3.topic);
    TEST_ASSERT_EQUAL_UINT8(NODE1_ADDR, _psacp_frame_node3.source_address);
    TEST_ASSERT_EQUAL_UINT8(1, _psacp_frame_node3.nbytes);
    TEST_ASSERT_EQUAL_UINT8(payload, _psacp_frame_node3.data[0]);

    err = wait_with_timeout(&_psacp_received_node4, DEFAULT_TIMEOUT_MS, _run_event_loops);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ARTIE_CAN_ERR_NONE, err, "Node 4 timed out waiting for broadcast");
    TEST_ASSERT_EQUAL_UINT8(ARTIE_CAN_PSACP_TOPIC_BROADCAST, _psacp_frame_node4.topic);
    TEST_ASSERT_EQUAL_UINT8(NODE1_ADDR, _psacp_frame_node4.source_address);
    TEST_ASSERT_EQUAL_UINT8(1, _psacp_frame_node4.nbytes);
    TEST_ASSERT_EQUAL_UINT8(payload, _psacp_frame_node4.data[0]);

    printf("!!!!!!!!!!!!! test_publish_broadcast PASSED !!!!!!!!!\n");
}

/**
 * @brief Test 3: Node 1 publishes to topic 0x10.
 * Expected: nodes 1 and 2 receive; nodes 3 and 4 do not.
 */
void test_publish_to_subscribed_topic(void)
{
    TEST_IGNORE_MESSAGE("Not yet implemented");
}

/**
 * @brief Test 4: Node 1 begins a BWACP transaction to node 4.
 * Concurrently, node 3 publishes one byte to topic 0x45 at high-priority pub/sub.
 * Expected: BWACP transaction completes correctly AND node 4 receives the pub/sub byte.
 */
void test_psacp_high_priority_during_bwacp(void)
{
    TEST_IGNORE_MESSAGE("Not yet implemented");
}

/**
 * @brief Test 5: Node 1 begins a BWACP transaction to node 4.
 * Concurrently, node 3 spams the bus with low-priority pub/sub messages.
 * Expected: BWACP transaction completes successfully despite the pub/sub noise.
 */
void test_bwacp_unaffected_by_low_priority_psacp_noise(void)
{
    TEST_IGNORE_MESSAGE("Not yet implemented");
}

/**
 * @brief Test 6: Node 1 publishes to topic 0x0C; nodes 2 and 3 receive, nodes 1 and 4 do not.
 * Node 2 then unsubscribes from topic 0x0C and Node 1 publishes again.
 * Expected second publish: only node 3 receives; all others do not.
 */
void test_unsubscribe(void)
{
    TEST_IGNORE_MESSAGE("Not yet implemented");
}

/**
 * @brief Main function - runs all tests.
 */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_publish_to_non_subscribed_topic);
    RUN_TEST(test_publish_broadcast);
    RUN_TEST(test_publish_to_subscribed_topic);
    RUN_TEST(test_psacp_high_priority_during_bwacp);
    RUN_TEST(test_bwacp_unaffected_by_low_priority_psacp_noise);
    RUN_TEST(test_unsubscribe);

    return UNITY_END();
}
