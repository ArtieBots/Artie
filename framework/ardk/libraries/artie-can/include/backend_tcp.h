/**
 * @file backend_tcp.h
 * @brief Header file for Artie CAN TCP backend. This backend allows sending and receiving Artie CAN
 * protocol frames over a TCP connection, which can be useful for testing and simulation purposes.
 * It can be used to send locally or remotely.
 *
 */

#pragma once

#include <stdint.h>
#include "backend.h"
#include "frame.h"

// OS-dependent includes for socket programming
#ifdef _WIN32
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    typedef HANDLE thread_handle_t;
    #define CLOSE_SOCKET closesocket
    #define SOCK_ERRNO WSAGetLastError()
    #define INVALID_THREAD_HANDLE NULL
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <errno.h>
    #include <pthread.h>
    typedef int socket_t;
    typedef pthread_t thread_handle_t;
    #define CLOSE_SOCKET close
    #define SOCK_ERRNO errno
    #define INVALID_SOCKET -1
    #define INVALID_THREAD_HANDLE 0
#endif


/** Maximum length for the hostname or IP address of the TCP server */
#define ARTIE_CAN_TCP_HOSTNAME_MAX_LENGTH 256

/**
 * @brief Structure representing the context object for the Artie CAN TCP backend.
 *
 */
typedef struct {
    char host[ARTIE_CAN_TCP_HOSTNAME_MAX_LENGTH];   /**< Hostname or IP address of the TCP server */
    uint16_t port;                                  /**< Port number of the TCP server */
    socket_t socket_fd;                             /**< File descriptor for the TCP socket */
    socket_t listen_fd;                             /**< File descriptor for the listening socket (server mode only) */
    bool is_server;                                 /**< Flag indicating whether this context is for a server (true) or client (false) */
    thread_handle_t server_thread;                  /**< Thread handle for the server accept thread (server mode only) */
    volatile bool should_stop;                      /**< Flag to signal the accept thread to stop */
    volatile bool server_ready;                     /**< Flag indicating the server is ready to accept connections */
    artie_can_receive_callback_t receive_callback;  /**< Callback function to call when a frame is received */
    artie_can_frame_t *receive_frame;               /**< Pointer to the frame struct where the received frame will be stored for the callback */
} artie_can_tcp_context_t;

/**
 * @brief Initialize an artie_can_tcp_context_t struct with the provided parameters.
 *
 * If is_server is true, the context will be set up for a server that listens for incoming connections on the specified host and port.
 * This will be set up in a separate thread when the backend is initialized.
 *
 * Please note that a server should be set up BEFORE any client nodes.
 *
 * @param context Pointer to the artie_can_tcp_context_t struct to initialize.
 * @param host Hostname or IP address of the TCP server.
 * @param port Port number of the TCP server.
 * @param is_server Boolean indicating whether the context is for a server (true) or client (false).
 * @return artie_can_error_t Error code indicating the result of the initialization.
 *
 */
artie_can_error_t artie_can_init_context_tcp(artie_can_tcp_context_t *context, const char *host, uint16_t port, bool is_server);

/**
 * @brief Initialize the Artie CAN backend struct with the TCP backend, using the provided context for configuration.
 *
 * Note that this function is not expected to call the node handle's init() function - that will be done
 * after this function returns.
 *
 * @param context Pointer to the artie_can_tcp_context_t struct.
 * @param handle Pointer to the artie_can_backend_t struct that will be populated with the function pointers and context for the TCP backend.
 * @return artie_can_error_t Error code indicating the result of the initialization.
 */
artie_can_error_t artie_can_init_tcp(artie_can_tcp_context_t *context, artie_can_backend_t *handle);
