/**
 * @file circular_buffer.h
 * @brief A circular buffer implementation for frames.
 *
 */

#pragma once

#include <stddef.h>
#include "err.h"
#include "frame.h"

/** The size of the buffer in frames. */
#define ARTIE_CAN_BUFFER_N_FRAMES 100U

/**
 * @brief Get the current number of frames in the buffer.
 *
 * @return The number of frames currently in the buffer.
 */
size_t cb_get_count(void);

/**
 * @brief Read a frame from the circular buffer if there is one.
 *
 * Byte-by-byte copies the next frame from the buffer into the output pointer.
 *
 * @param out Pointer to the artie_can_frame_t struct to copy the read frame into.
 * @return ARTIE_CAN_ERR_NONE on success, or ARTIE_CAN_ERR_NO_DATA if the buffer has no frames to read.
 */
artie_can_error_t cb_read(artie_can_frame_t *out);

/**
 * @brief Write a frame to the circular buffer.
 *
 * If an insertion is successful, it is done by means of a byte-by-byte copy of the frame,
 * allowing the caller to free the input frame.
 *
 * @param in Pointer to the artie_can_frame_t struct to copy into the buffer.
 * @return ARTIE_CAN_ERR_NONE on success, or ARTIE_CAN_ERR_NO_SPACE if inserting would overwrite
 * a frame that has not yet been read.
 */
artie_can_error_t cb_write(const artie_can_frame_t *in);
