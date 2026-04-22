#include <stdbool.h>
#include <stdint.h>
#include "unity.h"
#include "artie_can.h"
#include "util.h"

void assert_frames_equal(volatile const artie_can_frame_t *frame1, volatile const artie_can_frame_t *frame2)
{
    // Member item comparison
    TEST_ASSERT_EQUAL_UINT32(frame1->id, frame2->id);
    TEST_ASSERT_EQUAL_UINT(frame1->dlc, frame2->dlc);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(frame1->data, frame2->data, frame1->dlc);
}

void assert_rtacp_frames_equal(volatile const artie_can_frame_rtacp_t *frame1, volatile const artie_can_frame_rtacp_t *frame2)
{
    // Member item comparison
    TEST_ASSERT_EQUAL_UINT8(frame1->ack, frame2->ack);
    TEST_ASSERT_EQUAL_UINT8(frame1->priority, frame2->priority);
    TEST_ASSERT_EQUAL_UINT8(frame1->source_address, frame2->source_address);
    TEST_ASSERT_EQUAL_UINT8(frame1->target_address, frame2->target_address);
    TEST_ASSERT_EQUAL_UINT8(frame1->nbytes, frame2->nbytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(frame1->data, frame2->data, frame1->nbytes);
}

uint64_t get_current_time_ms(void)
{
#if defined(_WIN32) || defined(_WIN64)
    // Windows implementation using GetSystemTimeAsFileTime
    FILETIME ft;
    uint64_t ts;
    GetSystemTimeAsFileTime(&ft);

    ts = 0;
    ts |= ft.dwHighDateTime;
    ts <<= 32;
    ts |= ft.dwLowDateTime;
    return ts;
#else
    // Here is a simple implementation using gettimeofday for POSIX systems:
    // (AI generated, untested code)
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)(tv.tv_sec) * 1000 + (int64_t)(tv.tv_usec) / 1000;
#endif
}

artie_can_error_t wait_with_timeout(volatile bool *condition, uint32_t timeout_ms)
{
    uint64_t start_time_ms = get_current_time_ms();
    while (!(*condition))
    {
        if ((get_current_time_ms() - start_time_ms) >= (uint64_t)timeout_ms)
        {
            return ARTIE_CAN_ERR_TIMEOUT;
        }
        SLEEP_MS(10);
    }
    return ARTIE_CAN_ERR_NONE;
}
