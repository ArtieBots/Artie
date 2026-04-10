/**
 * @file artie_can_core.c
 * @brief Core initialization and backend management for Artie CAN library
 */

#include "artie_can.h"
#include "artie_can_backends.h"
#include <string.h>

/**
 * @brief Initialize the Artie CAN context with a specific backend type
 */
int artie_can_init(artie_can_context_t *ctx, uint8_t node_address, artie_can_backend_type_t backend_type, const void *backend_config)
{
    if (!ctx) {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    /* Validate node address (6 bits) */
    if (node_address > 0x3F) {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    /* Initialize context */
    memset(ctx, 0, sizeof(artie_can_context_t));
    ctx->node_address = node_address;

    /* Initialize the appropriate backend */
    int result = 0;
    switch (backend_type) {
        case ARTIE_CAN_BACKEND_DEADEND:
            result = artie_can_backend_mock_init(ctx, backend_config);
            break;

        case ARTIE_CAN_BACKEND_MCP2515:
            result = artie_can_backend_mcp2515_init(ctx, backend_config);
            break;

        case ARTIE_CAN_BACKEND_TCP:
            result = artie_can_backend_mock_tcp_init(ctx, (const artie_can_mock_config_t *)backend_config);
            break;

        default:
            return ARTIE_CAN_ERR_INVALID_ARG;
    }

    if (result != 0) {
        return result;
    }

    /* Initialize the backend */
    if (ctx->backend.init) {
        return ctx->backend.init(ctx->backend.context);
    }

    return 0;
}

/**
 * @brief Close the CAN context
 */
int artie_can_close(artie_can_context_t *ctx)
{
    if (!ctx) {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    /* Close backend */
    if (ctx->backend.close) {
        return ctx->backend.close(ctx->backend.context);
    }

    return 0;
}
