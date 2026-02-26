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

/**
 * @brief Initialize SocketCAN backend
 */
int artie_can_backend_socketcan_init(artie_can_backend_t *backend);

/**
 * @brief Initialize MCP2515 backend
 */
int artie_can_backend_mcp2515_init(artie_can_backend_t *backend);

/**
 * @brief Initialize Mock backend (local queue, no networking)
 */
int artie_can_backend_mock_init(artie_can_backend_t *backend);

/**
 * @brief Initialize Mock backend with TCP networking
 */
int artie_can_backend_mock_tcp_init(artie_can_backend_t *backend, const artie_can_mock_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* ARTIE_CAN_BACKENDS_H */
