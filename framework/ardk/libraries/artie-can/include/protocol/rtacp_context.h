/**
 * @file rtacp_context.h
 * @brief Definitions for RTACP context and related functions in the Artie CAN library.
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "frame.h"

/**
 * @brief States that the RTACP state machine can be in for a given node.
 *
 */
typedef enum {
    RTACP_STATE_IDLE,          ///< The node is idle and not currently processing any frames.
    RTACP_STATE_WAITING_ACK,   ///< The node has sent a frame and is waiting for an ACK.
} rtacp_state_t;

/**
 * @brief Context for RTACP protocol handling within the Artie CAN library.
 *
 */
typedef struct {
    uint8_t node_address;               ///< The RTACP address of this node on the CAN bus
    uint32_t ack_start_time_ms;         ///< The time in milliseconds when we started waiting for an ACK for a sent frame. Used to check for ACK timeouts.
    artie_can_frame_t in_flight_frame;  ///< The frame that is currently in flight and waiting for an ACK
    rtacp_state_t state;                ///< The current state of the RTACP protocol for this node
} rtacp_context_t;
