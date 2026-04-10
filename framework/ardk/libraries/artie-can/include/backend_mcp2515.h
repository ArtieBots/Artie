/**
 * @file backend_mcp2515.h
 * @brief Header file for Artie CAN MCP2515 backend. This backend allows sending and receiving Artie CAN
 * protocol frames using the MCP2515 CAN controller, which is a popular standalone CAN controller IC
 * that interfaces with a microcontroller over SPI.
 *
 * This backend attempts to be as hardware-agnostic (in terms of the microcontroller) as possible.
 * In order to work, it requires a SPI interface and two GPIO pins for the MCP2515's interrupt and reset lines.
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "backend.h"
#include "err.h"

typedef struct {
    // TODO: placeholder for now
    uint8_t dummy;
} artie_can_mcp2515_context_t;

/**
 * @brief Initialize an artie_can_mcp2515_context_t struct with the provided parameters.
 *
 * @param context
 * @param dummy
 * @return artie_can_error_t
 */
artie_can_error_t artie_can_init_context_mcp2515(artie_can_mcp2515_context_t *context, uint8_t dummy);

/**
 * @brief Initialize the Artie CAN backend struct with the MCP2515 backend, using the provided context for configuration.
 *
 * Note that this function is not expected to call the node handle's init() function - that will be done
 * after this function returns.
 *
 * @param context Pointer to the artie_can_mcp2515_context_t struct.
 * @param handle Pointer to the artie_can_backend_t struct that will be
 * populated with the backend's function pointers and context.
 * @return artie_can_error_t Error code indicating the result of the initialization.
 */
artie_can_error_t artie_can_init_mcp2515(artie_can_mcp2515_context_t *context, artie_can_backend_t *handle);
