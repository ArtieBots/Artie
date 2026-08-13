/**
 * @file test_bwacp_frame_loss.c
 * @brief Test BWACP recovery when individual DATA frames are lost.
 *
 * These tests use a synchronous in-process bus rather than the UDP multicast backend, so that a
 * specific frame can be dropped at a specific node deterministically. They also drive a virtual
 * clock, so protocol timeouts (which are seconds long) elapse in a few milliseconds of wall time
 * and the whole file runs in well under a second.
 *
 * The scenario these cover is the one that made test_bwacp's larger transfers fail intermittently:
 * a single receiver misses one DATA frame while the other receivers get it, which forces the sender
 * to repeat that frame. Every receiver that did get the original then sees the repeat as a
 * duplicate. Handling that duplicate correctly - and keeping the sender's send cursor intact across
 * the repeat - is what these tests pin down. Over UDP multicast this only reproduces when the
 * network happens to drop a datagram, which is why it took a 46 kB transfer to surface it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "artie_can.h"
#include "util.h"

/** Number of nodes on the simulated bus: node 1 sends, nodes 2 and 3 receive. */
#define NUM_NODES 3

/** Size of each node's BWACP receive buffer. */
#define RECEIVE_BUFFER_SIZE 4096

/** Buffer offset that the payload is written to on the receiving nodes. */
#define BUFFER_OFFSET 0

/** Upper bound on virtual time for a single transfer. Generous: a healthy transfer needs ~1.1 s. */
#define MAX_VIRTUAL_MS 120000

/** Bytes of payload per DATA frame. */
#define BYTES_PER_DATA_FRAME 8

static artie_can_context_t _contexts[NUM_NODES];
static artie_can_backend_t _handles[NUM_NODES];
static uint8_t _receive_buffers[NUM_NODES][RECEIVE_BUFFER_SIZE];

/**
 * Virtual clock. One round of ticks across all nodes advances this by 1 ms, which is roughly the
 * rate at which a real application is expected to call artie_can_tick().
 */
static uint64_t _virtual_time_ms = 0;

static uint64_t _get_virtual_ms(void)
{
    return _virtual_time_ms;
}

/** Index of the node to drop DATA frames at, or -1 to drop nothing. */
static int _drop_node_index = -1;
/** Which DATA frame to drop at that node, 1-based. */
static uint32_t _drop_nth_data_frame = 0;
/** How many DATA frames have been offered to the drop node so far. */
static uint32_t _data_frames_seen_by_drop_node = 0;
/** How many frames were actually dropped, so tests can assert the injection really happened. */
static uint32_t _frames_dropped = 0;

static artie_can_error_t _bus_init(artie_can_context_t *context)
{
    (void)context;
    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _bus_close(artie_can_context_t *context)
{
    (void)context;
    return ARTIE_CAN_ERR_NONE;
}

/**
 * @brief Deliver a frame to every node except the sender, honouring the configured drop rule.
 *
 * bwacp_receive_in_isr() only records the frame and sets a flag, so delivering synchronously here
 * does not re-enter the sender: the receiving node does not act on the frame until its next tick.
 */
static artie_can_error_t _bus_send(artie_can_context_t *context, const artie_can_frame_t *frame)
{
    uint8_t frame_type = (uint8_t)((frame->id & ARTIE_CAN_FRAME_ID_FRAME_TYPE_MASK) >> ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION);

    for (int i = 0; i < NUM_NODES; i++)
    {
        if (&_contexts[i] == context)
        {
            continue; // Nodes ignore their own frames
        }

        if ((i == _drop_node_index) && (frame_type == ARTIE_CAN_FRAME_TYPE_BWACP_DATA))
        {
            _data_frames_seen_by_drop_node++;
            if (_data_frames_seen_by_drop_node == _drop_nth_data_frame)
            {
                _frames_dropped++;
                printf("  [bus] dropping DATA frame #%u destined for node %d\n", _drop_nth_data_frame, i + 1);
                continue;
            }
        }

        bwacp_receive_in_isr(&_contexts[i], frame);
    }

    return ARTIE_CAN_ERR_NONE;
}

static void _empty_rx_callback(const artie_can_frame_t *frame)
{
    // BWACP writes straight into the receive buffer; the tests check the buffers directly.
    (void)frame;
}

/**
 * @brief Run one tick of the event loop for every node and advance the virtual clock by 1 ms.
 */
static void _run_event_loops(void)
{
    for (int i = 0; i < NUM_NODES; i++)
    {
        (void)bwacp_tick(&_handles[i]);
    }
    _virtual_time_ms++;
}

void setUp(void)
{
    artie_can_error_t err;

    _virtual_time_ms = 0;
    _drop_node_index = -1;
    _drop_nth_data_frame = 0;
    _data_frames_seen_by_drop_node = 0;
    _frames_dropped = 0;

    memset(_receive_buffers, 0, sizeof(_receive_buffers));

    for (int i = 0; i < NUM_NODES; i++)
    {
        memset(&_contexts[i], 0, sizeof(_contexts[i]));
        memset(&_handles[i], 0, sizeof(_handles[i]));

        _handles[i].init = _bus_init;
        _handles[i].send = _bus_send;
        _handles[i].close = _bus_close;

        err = artie_can_init_custom(&_contexts[i], &_handles[i], _empty_rx_callback, _get_virtual_ms);
        TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

        // Node 1 is an SBC and does the sending; nodes 2 and 3 are sensors and receive.
        uint8_t node_class = (i == 0) ? ARTIE_CAN_BWACP_CLASS_SBC : ARTIE_CAN_BWACP_CLASS_SENSOR;
        err = artie_can_init_context_bwacp(&_contexts[i], (uint8_t)(i + 1), node_class);
        TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

        err = artie_can_bwacp_set_receive_buffer(&_contexts[i], _receive_buffers[i], RECEIVE_BUFFER_SIZE);
        TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);
    }
}

void tearDown(void)
{
    for (int i = 0; i < NUM_NODES; i++)
    {
        (void)artie_can_close(&_handles[i]);
    }
}

/**
 * @brief Send a multicast payload from node 1 and assert that nodes 2 and 3 both receive it intact.
 *
 * @param payload_size Number of bytes to send.
 * @param drop_node_index Index into _contexts of the node to drop a DATA frame at, or -1 for none.
 * @param drop_nth_data_frame Which DATA frame (1-based) to drop at that node.
 */
static void _send_and_verify(size_t payload_size, int drop_node_index, uint32_t drop_nth_data_frame)
{
    uint8_t *payload = (uint8_t *)malloc(payload_size);
    TEST_ASSERT_NOT_NULL_MESSAGE(payload, "Failed to allocate payload");

    for (size_t i = 0; i < payload_size; i++)
    {
        payload[i] = (uint8_t)(i & 0xFF);
    }

    _drop_node_index = drop_node_index;
    _drop_nth_data_frame = drop_nth_data_frame;

    artie_can_error_t err = artie_can_bwacp_send(&_handles[0], payload, (uint32_t)payload_size, BUFFER_OFFSET,
                                                 ARTIE_CAN_BWACP_MULTICAST_ADDRESS, ARTIE_CAN_BWACP_CLASS_SENSOR,
                                                 ARTIE_CAN_FRAME_PRIORITY_BWACP_MEDIUM);
    TEST_ASSERT_EQUAL_INT(ARTIE_CAN_ERR_NONE, err);

    bool sender_done = false;
    while (!sender_done && (_virtual_time_ms < MAX_VIRTUAL_MS))
    {
        _run_event_loops();
        if (artie_can_bwacp_is_busy(&_handles[0]) == false)
        {
            sender_done = true;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(sender_done, "Node 1 did not complete sending");

    if (drop_node_index >= 0)
    {
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, _frames_dropped, "Expected to have dropped exactly one DATA frame");
    }

    // Both receivers must end up with the full payload, at the right offset, and back in IDLE.
    for (int i = 1; i < NUM_NODES; i++)
    {
        for (size_t j = 0; j < payload_size; j++)
        {
            TEST_ASSERT_EQUAL_UINT8_MESSAGE((uint8_t)(j & 0xFF), _receive_buffers[i][BUFFER_OFFSET + j], "Received data mismatch");
        }
        TEST_ASSERT_EQUAL_UINT32(BUFFER_OFFSET, _contexts[i].bwacp_context.receive_address);
        TEST_ASSERT_EQUAL_INT_MESSAGE(BWACP_STATE_IDLE, _contexts[i].bwacp_context.state, "Receiver did not return to IDLE");
    }

    free(payload);
}

/**
 * @brief Baseline: with no frames dropped the transfer completes normally.
 *
 * Guards against the drop-injection harness itself being what makes the other tests pass.
 */
void test_no_loss_baseline(void)
{
    printf("!!!!!!!! Starting test_no_loss_baseline !!!!!!!!!!!\n");
    _send_and_verify(256, -1, 0);
    printf("!!!!!!!! test_no_loss_baseline PASSED !!!!!!!!!!!!!\n");
}

/**
 * @brief One receiver misses a DATA frame mid-transfer.
 *
 * Node 2 never sees frame 10, so node 1 times out waiting for its ACK and repeats the frame. Node 3
 * already has that frame, so the repeat is a duplicate for node 3: it has to be ACKed and discarded
 * rather than NACKed, and node 1's send cursor has to survive the repeat unchanged.
 */
void test_drop_data_frame_mid_transfer(void)
{
    printf("!!! Starting test_drop_data_frame_mid_transfer !!!!\n");
    _send_and_verify(256, 1, 10);
    printf("!!! test_drop_data_frame_mid_transfer PASSED !!!!!!\n");
}

/**
 * @brief The very first DATA frame is lost at one receiver.
 *
 * The receiver has accepted nothing yet when the repeat arrives, so the duplicate check must not
 * fire here - this is a genuine first delivery for node 2.
 */
void test_drop_first_data_frame(void)
{
    printf("!!!!! Starting test_drop_first_data_frame !!!!!!!!!\n");
    _send_and_verify(256, 1, 1);
    printf("!!!!! test_drop_first_data_frame PASSED !!!!!!!!!!!\n");
}

/**
 * @brief The final, full-width DATA frame is lost at one receiver.
 *
 * 256 bytes is exactly 32 frames of 8 bytes, so frame 32 is the last one and the repeat happens
 * immediately before the COMPLETE frame.
 */
void test_drop_last_data_frame(void)
{
    printf("!!!!!! Starting test_drop_last_data_frame !!!!!!!!!\n");
    _send_and_verify(256, 1, 256 / BYTES_PER_DATA_FRAME);
    printf("!!!!!! test_drop_last_data_frame PASSED !!!!!!!!!!!\n");
}

/**
 * @brief A short final DATA frame is lost at one receiver.
 *
 * 257 bytes is 32 full frames plus a 1-byte frame, so this covers repeating a frame whose DLC is
 * not 8. Rebuilding the repeat has to reproduce the original 1-byte length rather than assuming a
 * full-width frame.
 */
void test_drop_short_final_data_frame(void)
{
    printf("!! Starting test_drop_short_final_data_frame !!!!!!\n");
    _send_and_verify(257, 1, (257 / BYTES_PER_DATA_FRAME) + 1);
    printf("!! test_drop_short_final_data_frame PASSED !!!!!!!!\n");
}

/**
 * @brief The other receiver is the one that misses a frame.
 *
 * Same recovery, mirrored, to make sure nothing depends on which node address drops the frame.
 */
void test_drop_data_frame_at_other_receiver(void)
{
    printf("! Starting test_drop_data_frame_at_other_receiver !\n");
    _send_and_verify(256, 2, 17);
    printf("! test_drop_data_frame_at_other_receiver PASSED !!!\n");
}

/**
 * @brief Losing a frame during a larger transfer still recovers.
 *
 * Exercises the same recovery well past the point where the repeat and the frames around it are the
 * only traffic in flight.
 */
void test_drop_data_frame_in_large_transfer(void)
{
    printf("! Starting test_drop_data_frame_in_large_transfer !\n");
    _send_and_verify(2048, 1, 200);
    printf("! test_drop_data_frame_in_large_transfer PASSED !!!\n");
}

/**
 * @brief Main function - runs all tests.
 */
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_no_loss_baseline);
    RUN_TEST(test_drop_data_frame_mid_transfer);
    RUN_TEST(test_drop_first_data_frame);
    RUN_TEST(test_drop_last_data_frame);
    RUN_TEST(test_drop_short_final_data_frame);
    RUN_TEST(test_drop_data_frame_at_other_receiver);
    RUN_TEST(test_drop_data_frame_in_large_transfer);

    return UNITY_END();
}
