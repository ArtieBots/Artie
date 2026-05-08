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

/**
 * @brief Setup function called before each test.
 *
 * This function runs before each individual test in this file.
 * Use it to initialize any state needed for your tests.
 */
void setUp(void)
{
}

/**
 * @brief Teardown function called after each test.
 *
 * This function runs after each individual test in this file.
 * Use it to clean up any state created during the test.
 */
void tearDown(void)
{
}

/**
 * @brief Test sending a single byte over the BWACP protocol.
 * Send to two other nodes and verify that they both received the byte successfully.
 *
 */
void test_send_one_byte(void)
{
}

/**
 * @brief Same as test_send_one_byte, but sends 4 bytes.
 *
 */
void test_send_four_bytes(void)
{
}

/**
 * @brief Same as test_send_one_byte, but sends 8 bytes.
 *
 */
void test_send_eight_bytes(void)
{
}

/**
 * @brief Same as test_send_one_byte, but sends 254 bytes.
 *
 */
void test_send_254_bytes(void)
{
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
    // ...

    // Finish and return results
    return UNITY_END();
}
