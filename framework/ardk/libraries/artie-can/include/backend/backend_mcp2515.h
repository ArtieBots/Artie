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
#include "backend_mcp2515_context.h"
#include "context.h"
#include "err.h"

/**
 * @brief Initialize an artie_can_mcp2515_context_t struct with the provided parameters.
 *
 * @param context Pointer to the artie_can_context_t struct that the MCP2515 context will be stored in.
 * @param mcp2515_ctx Pointer to the artie_can_mcp2515_context_t struct that will be initialized with the provided parameters.
 * @param driver_config Pointer to a driver_mcp2515_config_t struct containing configuration parameters for the MCP2515. This will be copied into the context struct, so it does not need to remain valid after this function returns.
 * @param write_byte_fn Function pointer for writing a byte to the MCP2515 over SPI. This function should also fill the read_byte field in the context with the byte that was read from SPI during the write operation.
 * @param write_cs_pin_fn Function pointer for controlling the MCP2515's CS pin. This function should set the pin low when selecting the device and high when deselecting. If hardware CS is used, this function can simply be empty.
 * @return artie_can_error_t Error code indicating the result of the initialization.
 */
artie_can_error_t artie_can_init_context_mcp2515(artie_can_context_t *context, artie_can_mcp2515_context_t *mcp2515_ctx, driver_mcp2515_config_t *driver_config, artie_can_write_byte_t write_byte_fn, artie_can_write_cs_pin_t write_cs_pin_fn);

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
