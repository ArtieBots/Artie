/**
 * @file backend_tcp.h
 * @brief Header file for Artie CAN TCP backend. This backend allows sending and receiving Artie CAN
 * protocol frames over a TCP connection, which can be useful for testing and simulation purposes.
 * It can be used to send locally or remotely.
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "backend.h"
#include "context.h"
#include "frame.h"

/**
 * @brief Initialize an artie_can_tcp_context_t struct with the provided parameters.
 *
 * Please note that a server should be set up BEFORE any client nodes.
 *
 * @param context Pointer to the artie_can_tcp_context_t struct to initialize.
 * @param host Hostname or IP address of the TCP server.
 * @param port Port number of the TCP server.
 * @return artie_can_error_t Error code indicating the result of the initialization.
 *
 */
artie_can_error_t artie_can_init_context_tcp(artie_can_context_t *context, const char *host, uint16_t port);

/**
 * @brief Initialize the Artie CAN backend struct with the TCP backend, using the provided context for configuration.
 *
 * Note that this function is not expected to call the node handle's init() function - that will be done
 * after this function returns.
 *
 * @param context Pointer to the artie_can_tcp_context_t struct.
 * @param handle Pointer to the artie_can_backend_t struct that will be populated with the function pointers and context for the TCP backend.
 * @param rx_callback User-supplied callback function that the backend should call whenever a CAN frame is received that matches the filters configured in the backend's context.
 * @param get_ms_fn User-supplied function that the backend can call to get the current time in milliseconds for timeout purposes.
 * @return artie_can_error_t Error code indicating the result of the initialization.
 */
artie_can_error_t tcp_init(artie_can_context_t *context, artie_can_backend_t *handle, artie_can_rx_callback_t rx_callback, artie_can_get_ms_t get_ms_fn);
