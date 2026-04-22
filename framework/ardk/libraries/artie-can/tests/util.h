/**
 * @file util.h
 * @brief Utility functions and definitions for Artie CAN tests.
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "artie_can.h"

/** Good ol' array length macro. */
#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof((arr)[0]))

/**
 * @brief Test that two CAN frames are equal.
 *
 * @param frame1 The first CAN frame to compare.
 * @param frame2 The second CAN frame to compare.
 */
void assert_frames_equal(volatile const artie_can_frame_t *frame1, volatile const artie_can_frame_t *frame2);

/**
 * @brief Test that two RTACP frames are equal.
 *
 * @param frame1 The first RTACP frame to compare.
 * @param frame2 The second RTACP frame to compare.
 */
void assert_rtacp_frames_equal(volatile const artie_can_frame_rtacp_t *frame1, volatile const artie_can_frame_rtacp_t *frame2);

/**
 * @brief Get the current time in ms.
 *
 * @return uint64_t Time in ms.
 */
uint64_t get_current_time_ms(void);

/**
 * @brief Wait for a condition to become true with a timeout.
 *
 * @param condition Pointer to a volatile boolean that represents the condition we are waiting for.
 * @param timeout_ms The timeout in milliseconds to wait for the condition to become true.
 * @return artie_can_error_t Error code indicating whether the condition became true or if we timed out.
 */
artie_can_error_t wait_with_timeout(volatile bool *condition, uint32_t timeout_ms);
