/**
 * @file rtacp_context.h
 * @brief Definitions for RTACP context and related functions in the Artie CAN library.
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "frame.h"
#include "translationlayer.h"

/**
 * @brief States that the RTACP state machine can be in for a given node.
 *
 */
typedef enum {
    RTACP_STATE_IDLE,          ///< The node is idle and not currently processing any frames.
    RTACP_STATE_WAITING_ACK,   ///< The node has sent a frame and is waiting for an ACK.
} rtacp_state_t;

typedef enum {
    RTACP_ISR_FLAG_NONE = 0,                 ///< No special conditions for this ISR call
    RTACP_ISR_FLAG_PENDING_ACK_RX = 1 << 0,  ///< We received an ACK in the ISR that we need to process from the main thread context
    RTACP_ISR_FLAG_PENDING_ACK_TX0 = 1 << 1, ///< We have an ACK that we need to send from the main thread context (buffer 0)
    RTACP_ISR_FLAG_PENDING_ACK_TX1 = 1 << 2, ///< We have an ACK that we need to send from the main thread context (buffer 1)
} rtacp_isr_flags_t;

/**
 * @brief Context for RTACP protocol handling within the Artie CAN library.
 *
 */
typedef struct {
    // Written to by main thread
    uint8_t node_address;               ///< The RTACP address of this node on the CAN bus
    uint64_t ack_start_time_ms;         ///< The time in milliseconds when we started waiting for an ACK for a sent frame. Used to check for ACK timeouts.
    artie_can_frame_t in_flight_frame;  ///< The frame that is currently in flight and waiting for an ACK
    rtacp_state_t state;                ///< The current state of the RTACP protocol for this node

    // Written to by ISR
    artie_can_frame_t ack_frame0;       ///< The ACK frame (0) that we need to send when we receive a frame that we need to ACK. Stored here so that we can send it from the main thread context instead of the ISR context.
    artie_can_frame_t ack_frame1;       ///< The ACK frame (1) that we need to send when we receive a frame that we need to ACK. Stored here so that we can send it from the main thread context instead of the ISR context.
    artie_can_frame_t received_ack;     ///< The ACK frame that we received in the ISR that we need to process from the main thread context. Stored here so that we can process it from the main thread context instead of the ISR context.
    atomic_uint32_t isr_flags;          ///< Flags to indicate special conditions found during ISR; cleared by main thread (atomic operations required)
} rtacp_context_t;
