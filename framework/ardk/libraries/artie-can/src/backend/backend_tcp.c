#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "backend.h"
#include "backend_tcp.h"
#include "circular_buffer.h"
#include "context.h"
#include "err.h"
#include "rtacp.h"
#include "translationlayer.h"

static void _complete_frame(artie_can_context_t *context, const char *recvbuf)
{
    // Convert from raw buffer to frame struct
    artie_can_frame_t *frame = (artie_can_frame_t *)recvbuf;

    // Check the frame's protocol against the context's protocol flags to see if we should
    // feed it back up the stack to its appropriate state machine.
    uint16_t protocol = (frame->id & ARTIE_CAN_FRAME_ID_PROTOCOL_MASK) >> ARTIE_CAN_FRAME_ID_PROTOCOL_LOCATION;
    switch (protocol)
    {
        case ARTIE_CAN_RTACP_PROTOCOL_ID:
            rtacp_receive_in_isr(context, frame);
            break;

        default:
            // Unknown protocol, so ignore the frame
            break;
    }
}

static artie_can_error_t _server_connect(artie_can_context_t *context)
{
    // Cast context
    tcp_context_t *tcp_ctx = (tcp_context_t *)(&(context->backend_context));

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100ms

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(tcp_ctx->rx_fd, &read_fds);

    int sel = select(0, &read_fds, NULL, NULL, &tv);
    if (sel == SOCKET_ERROR_VALUE)
    {
        close_socket(tcp_ctx->rx_fd);
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    if (sel > 0)
    {
        tcp_ctx->tx_fd = accept(tcp_ctx->rx_fd, NULL, NULL);
        if (tcp_ctx->tx_fd == INVALID_SOCKET_VALUE)
        {
            close_socket(tcp_ctx->rx_fd);
            return ARTIE_CAN_ERR_INIT_FAIL;
        }
    }
    else if (sel == 0)
    {
        // Timeout, no client connected within the timeout period
        return ARTIE_CAN_ERR_TIMEOUT;
    }

    return ARTIE_CAN_ERR_NONE;
}

#ifdef _WIN32
// Windows implementation
static DWORD WINAPI _server_thread_func(void *arg)
#else
// Unix implementation
static void *_server_thread_func(void *arg)
#endif
{
    int err;
    artie_can_context_t *context = (artie_can_context_t *)arg;
    tcp_context_t *tcp_ctx = (tcp_context_t *)(&(context->backend_context));

    // Create a socket for the server to listen for client connections.
    tcp_ctx->rx_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_ctx->rx_fd == INVALID_SOCKET_VALUE)
    {
#ifdef _WIN32
        return (DWORD)ARTIE_CAN_ERR_INIT_FAIL;
#else
        return (void *)(intptr_t)ARTIE_CAN_ERR_INIT_FAIL;
#endif
    }

    // Allow quick reuse of the port
    int opt = 1;
    setsockopt(tcp_ctx->rx_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    // Set up server address structure
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, tcp_ctx->address_mapping[tcp_ctx->address_index].host, &server_addr.sin_addr);
    server_addr.sin_port = htons(tcp_ctx->address_mapping[tcp_ctx->address_index].port);

    // Bind the socket to the address
    err = bind(tcp_ctx->rx_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err == SOCKET_ERROR_VALUE)
    {
        close_socket(tcp_ctx->rx_fd);
#ifdef _WIN32
        return (DWORD)ARTIE_CAN_ERR_INIT_FAIL;
#else
        return (void *)(intptr_t)ARTIE_CAN_ERR_INIT_FAIL;
#endif
    }

    // Start listening for client connections
    err = listen(tcp_ctx->rx_fd, 1);
    if (err == SOCKET_ERROR_VALUE)
    {
        close_socket(tcp_ctx->rx_fd);
#ifdef _WIN32
        return (DWORD)ARTIE_CAN_ERR_INIT_FAIL;
#else
        return (void *)(intptr_t)ARTIE_CAN_ERR_INIT_FAIL;
#endif
    }

    // Alert the main thread we are ready
    tcp_ctx->server_ready = true;

    size_t bytes_received = 0;
    char recvbuf[sizeof(artie_can_frame_t)];
    bool connected = false;
    while (!tcp_ctx->should_stop)
    {
        // Accept a client socket (blocks until a client connects)
        if (!connected)
        {
            artie_can_error_t artie_err = _server_connect(context);
            if (artie_err == ARTIE_CAN_ERR_TIMEOUT)
            {
                // No client connected within the timeout period. Just loop back and check if we should stop.
                continue;
            }
            else if (artie_err != ARTIE_CAN_ERR_NONE)
            {
                // Fatal error accepting client connection, so exit the thread
#ifdef _WIN32
                return (DWORD)artie_err;
#else
                return (void *)(intptr_t)artie_err;
#endif
            }
            else
            {
                // Client connected successfully
                connected = true;
            }
        }

        // Receive a buffer from the client (blocks until data is received)
        bool close = false;
        int recv_size = recv(tcp_ctx->tx_fd, recvbuf, sizeof(recvbuf), 0);
        if (recv_size > 0)
        {
            // We received some data
            bytes_received += recv_size;
        }
        else
        {
            // Error receiving from this client, or client disconnected.
            close = true;
        }

        if (bytes_received >= sizeof(artie_can_frame_t))
        {
            // We have received a full frame, so complete the frame and call the callback if applicable
            _complete_frame(context, recvbuf);

            // Close the connection. We only expect a single frame per client connection.
            close = true;
        }

        // Close the connection
        if (close)
        {
            err = shutdown_socket(tcp_ctx->tx_fd, SHUTDOWN_SEND);
            if (err == SOCKET_ERROR_VALUE)
            {
                close_socket(tcp_ctx->tx_fd);
            }

            // Reset for next connection
            connected = false;
            bytes_received = 0;
        }
    }

    // Close the listening socket
    close_socket(tcp_ctx->rx_fd);

#ifdef _WIN32
    return (DWORD)ARTIE_CAN_ERR_NONE;
#else
    return (void *)(intptr_t)ARTIE_CAN_ERR_NONE;
#endif
}

static artie_can_error_t _init_server(artie_can_context_t *context)
{
    // Cast context
    tcp_context_t *tcp_ctx = (tcp_context_t *)(&(context->backend_context));

    // Start a thread that blocks until a client connects, then receives data until the client disconnects
    if (!create_thread(&tcp_ctx->server_thread, _server_thread_func, (void *)context))
    {
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    // Wait until the server thread has set up the listening socket and is ready to accept connections before returning
    while (!tcp_ctx->server_ready)
    {
        SLEEP_MS(10);
    }

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _init_client(artie_can_context_t *context)
{
    // Currently, we don't do anything
    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _send_tcp_to_node(artie_can_context_t *context, const artie_can_frame_t *frame, size_t node_index)
{
    // Cast context
    tcp_context_t *tcp_ctx = (tcp_context_t *)(&(context->backend_context));

    // Create the socket for connecting to the server
    socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET_VALUE)
    {
        return ARTIE_CAN_ERR_SEND_FAIL;
    }

    // Connect to server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, tcp_ctx->address_mapping[node_index].host, &server_addr.sin_addr);
    server_addr.sin_port = htons(tcp_ctx->address_mapping[node_index].port);
    int err = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err == SOCKET_ERROR_VALUE)
    {
        return ARTIE_CAN_ERR_SEND_FAIL;
    }

    // Send the frame
    err = send(sock, (const char *)frame, sizeof(artie_can_frame_t), 0);
    if (err == SOCKET_ERROR_VALUE)
    {
        return ARTIE_CAN_ERR_SEND_FAIL;
    }

    // Shutdown the connection since no more data will be sent
    err = shutdown_socket(sock, SHUTDOWN_SEND);
    if (err == SOCKET_ERROR_VALUE)
    {
        return ARTIE_CAN_ERR_SEND_FAIL;
    }

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _init_tcp(artie_can_context_t *context)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Initialize socket subsystem (Winsock on Windows, no-op on POSIX)
    if (!socket_subsystem_init())
    {
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    artie_can_error_t err;
    err = _init_server(context);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    err = _init_client(context);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _send_tcp(void *ctx, const artie_can_frame_t *frame)
{
    if (ctx == NULL || frame == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Cast context
    artie_can_context_t *context = (artie_can_context_t *)ctx;
    tcp_context_t *tcp_ctx = (tcp_context_t *)(&(context->backend_context));

    // Create the socket for connecting to the server
    tcp_ctx->tx_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_ctx->tx_fd == INVALID_SOCKET_VALUE)
    {
        return ARTIE_CAN_ERR_SEND_FAIL;
    }

    // For each tcp node on the 'bus' (other than us), connect to the node and send the frame
    artie_can_error_t err = ARTIE_CAN_ERR_NONE;
    for (size_t i = 0; i < tcp_ctx->num_nodes; i++)
    {
        if (i == tcp_ctx->address_index)
        {
            // Don't connect to ourselves
            continue;
        }

        artie_can_error_t send_err = _send_tcp_to_node(context, frame, i);
        if (send_err != ARTIE_CAN_ERR_NONE)
        {
            // We don't want to stop sending to other nodes if one fails.
            err |= send_err;
        }
    }

    close_socket(tcp_ctx->tx_fd);
    tcp_ctx->tx_fd = INVALID_SOCKET_VALUE;

    return err;
}

static artie_can_error_t _close_tcp(artie_can_context_t *context)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Cast context
    tcp_context_t *tcp_ctx = (tcp_context_t *)(&(context->backend_context));

    // Alert the server thread that we should stop
    tcp_ctx->should_stop = true;

    // Wait until the server thread has stopped
    if (tcp_ctx->server_thread != INVALID_THREAD_HANDLE)
    {
        join_thread(tcp_ctx->server_thread, 0);  // 0 = infinite wait
        tcp_ctx->server_thread = INVALID_THREAD_HANDLE;
    }

    // Cleanup socket subsystem (Winsock on Windows, no-op on POSIX)
    socket_subsystem_cleanup();

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_init_context_tcp(artie_can_context_t *context, const artie_can_tcp_addr_t *own_address, const artie_can_tcp_addr_t *all_node_addresses, size_t num_nodes)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (own_address == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (all_node_addresses == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (num_nodes == 0)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (num_nodes > ARTIE_CAN_MAX_TCP_NODES)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (strlen(own_address->host) == 0)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (strlen(own_address->host) >= ARTIE_CAN_TCP_HOSTNAME_MAX_LENGTH)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // We are TCP backend
    tcp_context_t *tcp_ctx = (tcp_context_t *)(&(context->backend_context));

    // Initialize the TCP context within the provided artie_can_context_t
    *tcp_ctx = (tcp_context_t){
         .address_index = 0, // Will be set properly below
         .server_thread = INVALID_THREAD_HANDLE,
         .server_ready = false,
         .should_stop = false,
         .rx_fd = INVALID_SOCKET_VALUE,
         .tx_fd = INVALID_SOCKET_VALUE,
         .rx_callback = NULL,
         .num_nodes = num_nodes,
         .address_mapping = {0},
    };

    // Copy the address mapping information into the context's TCP context
    for (size_t i = 0; i < num_nodes; i++)
    {
        tcp_ctx->address_mapping[i] = {
            .host = {0},
            .port = all_node_addresses[i].port,
        };
        strncpy(tcp_ctx->address_mapping[i].host, all_node_addresses[i].host, ARTIE_CAN_TCP_HOSTNAME_MAX_LENGTH - 1);
        tcp_ctx->address_mapping[i].host[ARTIE_CAN_TCP_HOSTNAME_MAX_LENGTH - 1] = '\0'; // Ensure null termination

        // If this address matches our own address, set the address index in the context
        if ((strcmp(all_node_addresses[i].host, own_address->host) == 0) && (all_node_addresses[i].port == own_address->port))
        {
            tcp_ctx->address_index = i;
        }
    }

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t tcp_init(artie_can_context_t *context, artie_can_backend_t *handle, artie_can_rx_callback_t rx_callback, artie_can_get_ms_t get_ms_fn)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (handle == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (rx_callback == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (get_ms_fn == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Initialize the backend function pointers and context
    handle->init = _init_tcp;
    handle->send = _send_tcp;
    handle->close = _close_tcp;
    handle->context = context;
    handle->get_ms = get_ms_fn;

    // Initialize the rx callback in the backend context pointer
    tcp_context_t *tcp_ctx = (tcp_context_t *)(&(context->backend_context));
    tcp_ctx->rx_callback = rx_callback;

    return ARTIE_CAN_ERR_NONE;
}
