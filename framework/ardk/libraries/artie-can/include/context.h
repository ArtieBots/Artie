/**
 * @file context.h
 * @brief Context definitions for Artie CAN library (C implementation).
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "translationlayer.h"

/** The callback function that gets executed whenever a non-filtered CAN frame is received. */
typedef void artie_can_rx_callback_t(artie_can_frame_t *frame);

/**
 * @brief Context for RTACP protocol handling within the Artie CAN library.
 *
 */
typedef struct {
    uint8_t node_address;  ///< The RTACP address of this node on the CAN bus
} rtacp_context_t;

/** Maximum length for the hostname or IP address of the TCP server */
#define ARTIE_CAN_TCP_HOSTNAME_MAX_LENGTH 256

/**
 * @brief Structure representing the context object for the Artie CAN TCP backend.
 *
 */
typedef struct {
    char host[ARTIE_CAN_TCP_HOSTNAME_MAX_LENGTH];   ///< Hostname or IP address of the TCP server
    uint16_t port;                                  ///< Port number of the TCP server
    thread_handle_t server_thread;                  ///< Handle for the server thread
    bool server_ready;                              ///< Flag to indicate when the server thread is ready to accept connections
    bool should_stop;                               ///< Flag to signal the server thread to stop
    socket_t rx_fd;                                 ///< Socket file descriptor for receiving data from the client
    socket_t tx_fd;                                 ///< Socket file descriptor for sending data to the client
    artie_can_rx_callback_t *rx_callback;           ///< Callback function to call when a frame is received
} tcp_context_t;

/**
 * @brief Structure representing the context object for the Artie CAN MCP2515 backend.
 *
 */
typedef struct {
    bool dummy; ///< Placeholder member for now
} mcp2515_context_t;

/**
 * @brief Union of possible backend contexts for the Artie CAN library.
 *
 */
typedef union {
    tcp_context_t tcp;           ///< Context for TCP backend
    mcp2515_context_t mcp2515;   ///< Context for MCP2515 backend
} backend_context_t;

/**
 * @brief Enumeration of supported protocols in the Artie CAN library.
 *
 */
typedef enum {
    ARTIE_CAN_PROTOCOL_FLAG_RTACP = 1 << 0, ///< RTACP protocol
} artie_can_protocol_t;

/**
 * @brief Struct for Artie CAN library's state and configuration.
 *
 */
typedef struct {
    backend_context_t backend_context;   ///< Pointer to backend-specific context data
    rtacp_context_t rtacp_context;       ///< Context for RTACP protocol handling.
    uint16_t protocol_flags;             ///< Mask of protocols that we are interested in. Use OR'd members of the artie_can_protocol_t enum.
} artie_can_context_t;
