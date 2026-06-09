/**
 * @file psacp.h
 * @brief Header file for Artie CAN PSACP (Pub/Sub Artie CAN Protocol) implementation.
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "backend.h"
#include "context.h"
#include "err.h"
#include "frame.h"

/** The PSACP (high priority) protocol ID - 100 in binary */
#define ARTIE_CAN_PSACP_HIGH_PROTOCOL_ID 0x04U

/** The PSACP (low priority) protocol ID - 110 in binary */
#define ARTIE_CAN_PSACP_LOW_PROTOCOL_ID 0x06U

/** Maximum number of data bytes in a PSACP frame */
#define ARTIE_CAN_PSACP_MAX_DATA_BYTES 8U

/** The PSACP PUB frame type - always 0001 */
#define ARTIE_CAN_PSACP_FRAME_TYPE_PUB 0x01U

/** Broadcast topic value - send to all subscribers */
#define ARTIE_CAN_PSACP_TOPIC_BROADCAST 0x00U

/** Location of the topic bits in the PSACP frame ID.
 *  Topic is 8 bits wide, occupying bits [13:6] of the 29-bit CAN ID. */
#define PSACP_FRAME_ID_TOPIC_LOCATION 6U

/** Mask for topic bits in the PSACP frame ID */
#define PSACP_FRAME_ID_TOPIC_MASK (0xFFU << PSACP_FRAME_ID_TOPIC_LOCATION)

/**
 * @brief Enumeration for PSACP frame priorities.
 *
 */
typedef enum {
    ARTIE_CAN_FRAME_PRIORITY_PSACP_LOW = 3,       ///< Low priority frame
    ARTIE_CAN_FRAME_PRIORITY_PSACP_MEDIUM_LOW = 2, ///< Medium-low priority frame
    ARTIE_CAN_FRAME_PRIORITY_PSACP_MEDIUM_HIGH = 1,///< Medium-high priority frame
    ARTIE_CAN_FRAME_PRIORITY_PSACP_HIGH = 0,       ///< High priority frame
} artie_can_frame_priority_psacp_t;

/**
 * @brief Structure representing a PSACP frame, as opposed to the more general artie_can_frame_t.
 *
 */
typedef struct {
    bool high_priority;                              ///< true = high priority pub/sub (protocol 0x04), false = low priority (protocol 0x06)
    artie_can_frame_priority_psacp_t priority;       ///< User-assigned priority within the PSACP protocol
    uint8_t source_address;                          ///< Source address of the frame
    uint8_t topic;                                   ///< Topic this message is published to (0x00 = broadcast)
    uint8_t nbytes;                                  ///< Number of data bytes in the message (0-8)
    uint8_t data[ARTIE_CAN_PSACP_MAX_DATA_BYTES];   ///< Data bytes of the message (up to 8 bytes)
} artie_can_frame_psacp_t;

/**
 * @brief Initialize a PSACP context with the specified node address.
 *
 * @param ctx Pointer to the artie_can_context_t struct to initialize.
 * @param node_address Node address to use for the PSACP context.
 * @return artie_can_error_t Error code indicating the result of the initialization.
 */
artie_can_error_t artie_can_init_context_psacp(artie_can_context_t *ctx, uint8_t node_address);

/**
 * @brief Subscribe this node to a given PSACP topic.
 * When a frame is published to this topic (or to the broadcast topic 0x00), this node will
 * receive it via the rx_callback.
 *
 * @param ctx Pointer to the artie_can_context_t struct.
 * @param topic The topic to subscribe to (must be in range 0x0B - 0xF4).
 * @return artie_can_error_t Error code indicating the result of the operation.
 */
artie_can_error_t artie_can_psacp_subscribe(artie_can_context_t *ctx, uint8_t topic);

/**
 * @brief Unsubscribe this node from a given PSACP topic.
 *
 * @param ctx Pointer to the artie_can_context_t struct.
 * @param topic The topic to unsubscribe from.
 * @return artie_can_error_t Error code indicating the result of the operation.
 *         Returns ARTIE_CAN_ERR_NO_DATA if the node was not subscribed to the topic.
 */
artie_can_error_t artie_can_psacp_unsubscribe(artie_can_context_t *ctx, uint8_t topic);

/**
 * @brief Initialize a raw artie_can_frame_t with the PSACP headers and data from an artie_can_frame_psacp_t.
 *
 * @param out Pointer to the artie_can_frame_t to initialize.
 * @param in Pointer to the artie_can_frame_psacp_t describing the message to send.
 * @return artie_can_error_t Error code indicating the result of the operation.
 */
artie_can_error_t artie_can_psacp_init_frame(artie_can_frame_t *out, const artie_can_frame_psacp_t *in);

/**
 * @brief Parse a received raw artie_can_frame_t into an artie_can_frame_psacp_t.
 *
 * @param in Pointer to the raw artie_can_frame_t to parse.
 * @param out Pointer to the artie_can_frame_psacp_t to populate.
 * @return artie_can_error_t Error code indicating the result of the operation.
 */
artie_can_error_t artie_can_psacp_parse_frame(const artie_can_frame_t *in, artie_can_frame_psacp_t *out);

/**
 * @brief Publish a PSACP frame to the bus. This function is fire-and-forget; there is no ACK.
 * If the publishing node is itself subscribed to the topic (or the topic is broadcast), it will
 * also deliver the message locally via the rx_callback before returning.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @param frame Pointer to the raw artie_can_frame_t to publish.
 * @return artie_can_error_t Error code indicating the result of the operation.
 */
artie_can_error_t artie_can_psacp_publish(artie_can_backend_t *handle, const artie_can_frame_t *frame);

/**
 * @brief Handle a received PSACP frame within an ISR context.
 * Checks the frame's topic against the node's subscriptions and calls the rx_callback if matched.
 *
 * @param context Pointer to the artie_can_context_t struct representing the context.
 * @param frame Pointer to the artie_can_frame_t struct representing the received frame.
 */
void psacp_receive_in_isr(artie_can_context_t *context, const artie_can_frame_t *frame);

/**
 * @brief Tick function for the PSACP protocol. Since PSACP is fire-and-forget with no state machine,
 * this function currently does nothing and always returns ARTIE_CAN_ERR_NONE.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @return artie_can_error_t Always returns ARTIE_CAN_ERR_NONE.
 */
artie_can_error_t psacp_tick(artie_can_backend_t *handle);
