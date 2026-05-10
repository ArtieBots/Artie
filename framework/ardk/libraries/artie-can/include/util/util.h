/**
 * @file util.h
 * @brief General utility functions for Artie CAN library.
 *
 */

#pragma once

#include "backend.h"
#include "err.h"
#include "frame.h"

/**
 * @brief Send a CAN frame with automatic retry logic.
 *
 * This function attempts to send a CAN frame up to 10 times for retriable errors
 * (SEND_BUSY, TIMEOUT, SEND_FAIL). For each retry, it waits 1ms before attempting
 * again. If all 10 attempts fail with retriable errors, it makes one final attempt.
 *
 * @param handle Pointer to the backend handle.
 * @param frame Pointer to the frame to send.
 * @return ARTIE_CAN_ERR_NONE on success, or an error code on failure.
 */
artie_can_error_t artie_can_send_with_retry(artie_can_backend_t *handle, const artie_can_frame_t *frame);
