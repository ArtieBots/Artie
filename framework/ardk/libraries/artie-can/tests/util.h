/**
 * @file util.h
 * @brief Utility functions and definitions for Artie CAN tests.
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "artie_can.h"

/**
 * @brief Test that two CAN frames are equal.
 *
 * @param frame1 The first CAN frame to compare.
 * @param frame2 The second CAN frame to compare.
 */
void assert_frames_equal(const artie_can_frame_t *frame1, const artie_can_frame_t *frame2);
