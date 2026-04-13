/**
 * @file rtacp.h
 * @brief Header file for Artie CAN RTACP (Real Time Artie CAN Protocol) implementation.
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "backend.h"
#include "err.h"
#include "frame.h"

/**
 * @brief Initialize an RTACP frame with the appropriate headers and metadata for a given backend and frame.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @param frame Pointer to the artie_can_frame_t struct representing the frame to initialize.
 * @return Error code indicating the result of the operation.
 *
 */
artie_can_error_t artie_can_rtacp_init_frame(artie_can_backend_t *handle, artie_can_frame_t *frame, uint8_t todo);
