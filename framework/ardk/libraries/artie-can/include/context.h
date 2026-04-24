/**
 * @file context.h
 * @brief Context definitions for Artie CAN library (C implementation).
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "rtacp_context.h"
#include "translationlayer.h"

/**
 * @brief Enumeration of supported protocols in the Artie CAN library.
 */
typedef enum {
    ARTIE_CAN_PROTOCOL_FLAG_RTACP = 1 << 0,     ///< RTACP protocol
    ARTIE_CAN_PROTOCOL_FLAG_RPCACP = 1 << 1,    ///< RPCACP protocol
    ARTIE_CAN_PROTOCOL_FLAG_PSACP = 1 << 2,     ///< PSAP protocol
    ARTIE_CAN_PROTOCOL_FLAG_BWACP = 1 << 3,     ///< BWACP protocol
} artie_can_protocol_t;

/** Number of items in the artie_can_protocol_t enum */
#define ARTIE_CAN_PROTOCOL_COUNT 4U

/**
 * @brief Struct for Artie CAN library's state and configuration.
 *
 */
typedef struct {
    void *backend_context;               ///< Pointer to backend-specific context data (which could be custom)
    rtacp_context_t rtacp_context;       ///< Context for RTACP protocol handling.
    uint16_t protocol_flags;             ///< Mask of protocols that we are interested in. Use OR'd members of the artie_can_protocol_t enum.
} artie_can_context_t;
