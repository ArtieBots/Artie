/**
 * @file artie_can_backends.h
 * @brief Backend initialization functions for Artie CAN library
 */

#ifndef ARTIE_CAN_BACKENDS_H
#define ARTIE_CAN_BACKENDS_H

#include "artie_can.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Map a node address (0-63) to a context index. */
#define ARTIE_CAN_MAP_NODE_ADDRESS_TO_INDEX(node_address, num_contexts) \
    (((node_address) > 0x3F || (num_contexts) == 0) ? 0xFF : ((node_address) % (num_contexts)))

/**
 * @brief Initialize MCP2515 backend
 */
int artie_can_backend_mcp2515_init(artie_can_context_t *ctx, const void *config);

/**
 * @brief Initialize Mock backend (dead-end: discards sends, never receives)
 */
int artie_can_backend_mock_init(artie_can_context_t *ctx, const void *config);

/**
 * @brief Initialize Mock backend with TCP networking
 */
int artie_can_backend_mock_tcp_init(artie_can_context_t *ctx, const artie_can_mock_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* ARTIE_CAN_BACKENDS_H */
