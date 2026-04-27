/**
 * @file backend_tcp.h
 * @brief Header file for Artie CAN TCP backend. This backend allows sending and receiving Artie CAN
 * protocol frames over a TCP connection, which can be useful for testing and simulation purposes.
 * It can be used to send locally or remotely.
 *
 * Note that this backend is not intended for embedded or production use.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "backend.h"
#include "context.h"
#include "frame.h"

/** Maximum length for the hostname or IP address of the TCP server */
#define ARTIE_CAN_TCP_HOSTNAME_MAX_LENGTH 256

/** Maximum nodes in the TCP address space */
#define ARTIE_CAN_MAX_TCP_NODES ARTIE_CAN_MAX_NODES

/**
 * @brief Structure representing the address of a TCP node.
 *
 */
typedef struct {
    char host[ARTIE_CAN_TCP_HOSTNAME_MAX_LENGTH];           ///< Hostname or IP address of the TCP server
    uint16_t port;                                          ///< Port number of the TCP server
} artie_can_tcp_addr_t;

/**
 * @brief Structure representing the context object for the Artie CAN TCP backend.
 *
 */
typedef struct {
    size_t address_index;                                   ///< The index of this node's own address in the all_node_addresses array
    thread_handle_t server_thread;                          ///< Handle for the server thread
    bool server_ready;                                      ///< Flag to indicate when the server thread is ready to accept connections
    bool should_stop;                                       ///< Flag to signal the server thread to stop
    socket_t rx_fd;                                         ///< Socket file descriptor for receiving data from the client
    socket_t tx_fd;                                         ///< Socket file descriptor for sending data to the client
    artie_can_rx_callback_t *rx_callback;                   ///< Callback function to call when a frame is received
    size_t num_nodes;                                       ///< The number of nodes in the TCP network (including this node)
    artie_can_tcp_addr_t address_mapping[ARTIE_CAN_MAX_TCP_NODES];    ///< Mapping of node IDs to TCP addresses
} artie_can_tcp_context_t;

/**
 * @brief Initialize an artie_can_context_t struct with the provided parameters.
 *
 * Please note that the context struct must have already been allocated by the caller
 * and it should have a lifetime that matches the lifetime of the backend handle that will be using it.
 *
 * @param context Pointer to the artie_can_context_t struct to initialize.
 * @param tcp_context Pointer to the artie_can_tcp_context_t struct that will be initialized with the provided parameters.
 * @param own_address Pointer to the artie_can_tcp_addr_t struct representing this node's own TCP address.
 * @param all_node_addresses Pointer to an array of artie_can_tcp_addr_t structs representing the TCP addresses of all nodes in the network (including this node).
 * @param num_nodes The number of nodes in the all_node_addresses array.
 * @return artie_can_error_t Error code indicating the result of the initialization.
 *
 */
artie_can_error_t artie_can_init_context_tcp(artie_can_context_t *context, artie_can_tcp_context_t *tcp_context, const artie_can_tcp_addr_t *own_address, const artie_can_tcp_addr_t *all_node_addresses, size_t num_nodes);

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
