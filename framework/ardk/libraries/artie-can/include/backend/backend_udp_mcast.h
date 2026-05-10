/**
 * @file backend_udp_mcast.h
 * @brief Header file for Artie CAN UDP Multicast backend. This backend allows sending and
 * receiving Artie CAN protocol frames over UDP multicast, which simulates a broadcast bus
 * like a real CAN bus where all nodes can send/receive simultaneously.
 *
 * This is ideal for testing and simulation purposes on local networks or localhost.
 * Note that this backend is not intended for embedded or production use.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "backend.h"
#include "context.h"
#include "frame.h"

/** Maximum length for the multicast group IP address */
#define ARTIE_CAN_UDP_MCAST_ADDR_MAX_LENGTH 256

/**
 * @brief Structure representing the configuration for UDP multicast.
 */
typedef struct {
    char group_addr[ARTIE_CAN_UDP_MCAST_ADDR_MAX_LENGTH];   ///< Multicast group IP address (e.g., "239.0.0.1")
    uint16_t port;                                          ///< Multicast port number
} artie_can_udp_mcast_config_t;

/**
 * @brief Structure representing the context object for the Artie CAN UDP Multicast backend.
 */
typedef struct {
    artie_can_udp_mcast_config_t config;                    ///< Multicast configuration
    thread_handle_t receiver_thread;                        ///< Handle for the receiver thread
    bool receiver_ready;                                    ///< Flag to indicate when the receiver thread is ready
    bool should_stop;                                       ///< Flag to signal the receiver thread to stop
    socket_t socket_fd;                                     ///< UDP socket for both sending and receiving
    artie_can_rx_callback_t rx_callback;                    ///< Callback function to call when a frame is received
} artie_can_udp_mcast_context_t;

/**
 * @brief Initialize an artie_can_context_t struct with UDP multicast configuration.
 *
 * Please note that the context struct must have already been allocated by the caller
 * and it should have a lifetime that matches the lifetime of the backend handle that will be using it.
 *
 * @param context Pointer to the artie_can_context_t struct to initialize.
 * @param udp_mcast_context Pointer to the artie_can_udp_mcast_context_t struct that will be initialized.
 * @param multicast_group IP address of the multicast group (e.g., "239.0.0.1").
 * @param port Port number for the multicast group.
 * @return artie_can_error_t Error code indicating the result of the initialization.
 */
artie_can_error_t artie_can_init_context_udp_mcast(artie_can_context_t *context, artie_can_udp_mcast_context_t *udp_mcast_context, const char *multicast_group, uint16_t port);

/**
 * @brief Initialize the Artie CAN backend struct with the UDP multicast backend.
 *
 * Note that this function is not expected to call the node handle's init() function - that will be done
 * after this function returns.
 *
 * @param context Pointer to the artie_can_context_t struct.
 * @param handle Pointer to the artie_can_backend_t struct that will be populated with the function pointers and context.
 * @param rx_callback User-supplied callback function that the backend should call whenever a CAN frame is received.
 * @param get_ms_fn User-supplied function that the backend can call to get the current time in milliseconds.
 * @return artie_can_error_t Error code indicating the result of the initialization.
 */
artie_can_error_t udp_mcast_init(artie_can_context_t *context, artie_can_backend_t *handle, artie_can_rx_callback_t rx_callback, artie_can_get_ms_t get_ms_fn);
