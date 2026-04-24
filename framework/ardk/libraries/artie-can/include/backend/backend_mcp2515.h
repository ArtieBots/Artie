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
#include "context.h"
#include "err.h"

/**
 * @brief Structure representing the context object for the Artie CAN MCP2515 backend.
 *
 */
typedef struct {
    bool dummy; ///< Placeholder member for now
    artie_can_rx_callback_t rx_callback; ///< The callback function that the backend should call whenever a CAN frame is received.
} mcp2515_context_t;

/**
 * @brief Initialize an artie_can_mcp2515_context_t struct with the provided parameters.
 *
 * @param context
 * @param dummy
 * @return artie_can_error_t
 */
artie_can_error_t artie_can_init_context_mcp2515(artie_can_context_t *context, uint8_t dummy);

/**
 * @brief Initialize the Artie CAN backend struct with the MCP2515 backend, using the provided context for configuration.
 *
 * Note that this function is not expected to call the node handle's init() function - that will be done
 * after this function returns.
 *
 * @param context Pointer to the artie_can_context_t struct.
 * @param handle Pointer to the artie_can_backend_t struct that will be
 * populated with the backend's function pointers and context.
 * @param rx_callback The callback function that the backend should call whenever a CAN frame is received.
 * @param get_ms_fn A function that the backend can call to get the current time in milliseconds for timeout purposes.
 * @return artie_can_error_t Error code indicating the result of the initialization.
 */
artie_can_error_t mcp2515_init(artie_can_context_t *context, artie_can_backend_t *handle, artie_can_rx_callback_t rx_callback, artie_can_get_ms_t get_ms_fn);
