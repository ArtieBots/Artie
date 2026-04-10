#include <stdbool.h>
#include <stdint.h>
#include "unity.h"
#include "artie_can.h"
#include "util.h"

void assert_frames_equal(const artie_can_frame_t *frame1, const artie_can_frame_t *frame2)
{
    // Member item comparison
    TEST_ASSERT_EQUAL_UINT32(frame1->id, frame2->id);
    TEST_ASSERT_EQUAL_UINT(frame1->dlc, frame2->dlc);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(frame1->data, frame2->data, frame1->dlc);
}

void assert_rtacp_frames_equal(const artie_can_frame_rtacp_t *frame1, const artie_can_frame_rtacp_t *frame2)
{
    // Member item comparison
    TEST_ASSERT_EQUAL_UINT8(frame1->ack, frame2->ack);
    TEST_ASSERT_EQUAL_UINT8(frame1->priority, frame2->priority);
    TEST_ASSERT_EQUAL_UINT8(frame1->source_address, frame2->source_address);
    TEST_ASSERT_EQUAL_UINT8(frame1->target_address, frame2->target_address);
    TEST_ASSERT_EQUAL_UINT8(frame1->nbytes, frame2->nbytes);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(frame1->data, frame2->data, frame1->nbytes);
}
