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
int artie_can_init(artie_can_context_t *ctx, uint8_t node_address, artie_can_backend_type_t backend_type)
{
    if (!ctx) {
        return -1;
    }

    /* Validate node address (6 bits) */
    if (node_address > 0x3F) {
        return -1;
    }

    /* Initialize context */
    memset(ctx, 0, sizeof(artie_can_context_t));
    ctx->node_address = node_address;

    /* Initialize the appropriate backend */
    int result = 0;
    switch (backend_type) {
        case ARTIE_CAN_BACKEND_SOCKETCAN:
            result = artie_can_backend_socketcan_init(&ctx->backend);
            break;

        case ARTIE_CAN_BACKEND_MCP2515:
            result = artie_can_backend_mcp2515_init(&ctx->backend);
            break;

        case ARTIE_CAN_BACKEND_MOCK:
            result = artie_can_backend_mock_init(&ctx->backend);
            break;

        default:
            return -1;
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
 * @brief Initialize the Artie CAN context with mock backend configuration
 */
int artie_can_init_mock(artie_can_context_t *ctx, uint8_t node_address, const artie_can_mock_config_t *mock_config)
{
    if (!ctx || !mock_config) {
        return -1;
    }

    /* Validate node address (6 bits) */
    if (node_address > 0x3F) {
        return -1;
    }

    /* Initialize context */
    memset(ctx, 0, sizeof(artie_can_context_t));
    ctx->node_address = node_address;

    /* Initialize mock backend with TCP */
    int result = artie_can_backend_mock_tcp_init(&ctx->backend, mock_config);
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
 * @brief Initialize with a custom backend
 */
int artie_can_init_custom(artie_can_context_t *ctx, uint8_t node_address, const artie_can_backend_t *backend)
{
    if (!ctx || !backend) {
        return -1;
    }

    /* Validate node address (6 bits) */
    if (node_address > 0x3F) {
        return -1;
    }

    /* Initialize context */
    memset(ctx, 0, sizeof(artie_can_context_t));
    ctx->node_address = node_address;

    /* Copy backend */
    memcpy(&ctx->backend, backend, sizeof(artie_can_backend_t));

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
        return -1;
    }

    /* Close backend */
    if (ctx->backend.close) {
        return ctx->backend.close(ctx->backend.context);
    }

    return 0;
}
