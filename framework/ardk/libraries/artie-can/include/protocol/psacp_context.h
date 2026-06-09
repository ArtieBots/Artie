/**
 * @file psacp_context.h
 * @brief Definitions for PSACP context and related functions in the Artie CAN library.
 *
 */

#pragma once

#include <stdint.h>
#include "translationlayer.h"

/** Maximum number of topics a single node can subscribe to simultaneously */
#define ARTIE_CAN_PSACP_MAX_SUBSCRIPTIONS 32U

/**
 * @brief Context for PSACP protocol handling within the Artie CAN library.
 *
 * PSACP is a fire-and-forget pub/sub protocol with no ACK or retransmission.
 * The context simply tracks the node address and its subscribed topics.
 */
typedef struct {
    uint8_t node_address;                                           ///< The address of this node on the CAN bus
    uint8_t subscribed_topics[ARTIE_CAN_PSACP_MAX_SUBSCRIPTIONS];  ///< Array of topics this node has subscribed to
    uint8_t subscribed_topic_count;                                 ///< Number of active subscriptions
} psacp_context_t;
