/**
 * @file circular_buffer.h
 * @brief A circular buffer implementation for frames.
 *
 */

#pragma once

#include "err.h"
#include "frame.h"

/** The size of the buffer in frames. */
#define ARTIE_CAN_TCP_BUFFER_N_FRAMES 100U

/**
 * @brief Get the current number of frames in the buffer.
 *
 * @return The number of frames currently in the buffer.
 */
size_t cb_get_count(void);

/**
 * @brief Read a frame from the circular buffer if there is one.
 *
 * If the circular buffer has no more frames to read, returns ARTIE_CAN_ERR_NO_DATA.
 * Otherwise, byte-by-byte copies the next frame from the buffer into the output pointer.
 *
 */
artie_can_error_t cb_read(artie_can_frame_t *out);

/**
 * @brief Write a frame to the circular buffer.
 *
 * If inserting will overwrite a frame that has not been read, ARTIE_CAN_NO_SPACE is returned
 * and the frame is not written to the buffer.
 *
 * If an insertion is successful, it is done by means of a byte-by-byte copy of the frame,
 * allowing the caller to free the input frame.
 *
 */
artie_can_error_t cb_write(const artie_can_frame_t *in);
