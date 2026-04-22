#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "backend.h"
#include "backend_tcp.h"
#include "circular_buffer.h"
#include "context.h"
#include "err.h"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
#else
    #include <sys/time.h>
#endif

static void _complete_frame(artie_can_tcp_context_t *context, const char *recvbuf)
{
    // Copy the received buffer into the circular buffer
    artie_can_error_t err = cb_write((const artie_can_frame_t *)recvbuf);
    if (context->receive_callback != NULL)
    {
        // Read from the circular buffer into a frame struct to pass to the callback
        cb_read(context->receive_frame);
        context->receive_callback((void *)context, err, context->receive_frame);

        // Clear the callback
        context->receive_callback = NULL;
    }
}

#ifdef _WIN32
// Windows implementation
static DWORD WINAPI _server_thread_func(void *arg)
{
    int err;
    artie_can_tcp_context_t *context = (artie_can_tcp_context_t *)arg;

    // Create a socket for the server to listen for client connections.
    context->listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (context->listen_fd == INVALID_SOCKET)
    {
        return (DWORD)ARTIE_CAN_ERR_INIT_FAIL;
    }

    // Allow quick reuse of the port
    int opt = 1;
    setsockopt(context->listen_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    // Set up server address structure
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, context->host, &server_addr.sin_addr);
    server_addr.sin_port = htons(context->port);

    // Bind the socket to the address
    err = bind(context->listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err == SOCKET_ERROR)
    {
        closesocket(context->listen_fd);
        return (DWORD)ARTIE_CAN_ERR_INIT_FAIL;
    }

    // Start listening for client connections
    err = listen(context->listen_fd, 1);
    if (err == SOCKET_ERROR)
    {
        closesocket(context->listen_fd);
        return (DWORD)ARTIE_CAN_ERR_INIT_FAIL;
    }

    // Alert the main thread we are ready
    context->server_ready = true;

    size_t bytes_received = 0;
    char recvbuf[sizeof(artie_can_frame_t)];
    bool connected = false;
    while (!context->should_stop)
    {
        // Accept a client socket (blocks until a client connects)
        if (!connected)
        {
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 100000; // 100ms

            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(context->listen_fd, &read_fds);

            int sel = select(0, &read_fds, NULL, NULL, &tv);
            if (sel == SOCKET_ERROR)
            {
                closesocket(context->listen_fd);
                return (DWORD)ARTIE_CAN_ERR_INIT_FAIL;
            }

            if (sel > 0)
            {
                context->socket_fd = accept(context->listen_fd, NULL, NULL);
                if (context->socket_fd == INVALID_SOCKET)
                {
                    closesocket(context->listen_fd);
                    return (DWORD)ARTIE_CAN_ERR_INIT_FAIL;
                }
                connected = true;
            }
        }

        // Receive a buffer from the client (blocks until data is received)
        bool close = false;
        int recv_size = recv(context->socket_fd, recvbuf, sizeof(recvbuf), 0);
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
            err = shutdown(context->socket_fd, SD_SEND);
            if (err == SOCKET_ERROR)
            {
                closesocket(context->socket_fd);
            }

            // Reset for next connection
            connected = false;
            bytes_received = 0;
        }
    }

    // Close the listening socket
    closesocket(context->listen_fd);

    return (DWORD)ARTIE_CAN_ERR_NONE;
}
#else
//Unix implementation
static void *_server_thread_func(void *arg)
{

}
#endif

#ifdef _WIN32
// Windows implementation
static artie_can_error_t _init_server(artie_can_tcp_context_t *context)
{
    // Start a thread that blocks until a client connects, then receives data until the client disconnects
    // Default security attributes, default stack size, default creation flags, and we don't need the thread identifier
    context->server_thread = CreateThread(NULL, 0, _server_thread_func, (void *)context, 0, NULL);
    if (context->server_thread == INVALID_THREAD_HANDLE)
    {
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    // Wait until the server thread has set up the listening socket and is ready to accept connections before returning
    while (!context->server_ready)
    {
        Sleep(10);
    }

    return ARTIE_CAN_ERR_NONE;
}
#else
// Unix implementation
static artie_can_error_t _init_server(artie_can_tcp_context_t *context)
{

}
#endif

#ifdef _WIN32
// Windows implementation
static artie_can_error_t _init_client(artie_can_tcp_context_t *context)
{
    return ARTIE_CAN_ERR_NONE;
}
#else
// Unix implementation
static artie_can_error_t _init_client(artie_can_tcp_context_t *context)
{

}
#endif

static artie_can_error_t _init_tcp(void *ctx)
{
    artie_can_tcp_context_t *context = (artie_can_tcp_context_t *)ctx;

    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

#if defined(_WIN32)
    // Initialize Winsock library
    WSADATA wsa_data;
    int err = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (err != 0)
    {
        return ARTIE_CAN_ERR_INIT_FAIL;
    }
#endif

    if (context->is_server)
    {
        return _init_server(context);
    }
    else
    {
        return _init_client(context);
    }
}

static artie_can_error_t _send_tcp(void *ctx, const artie_can_frame_t *frame)
{
    artie_can_tcp_context_t *context = (artie_can_tcp_context_t *)ctx;

    if (context == NULL || frame == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

#ifdef _WIN32
    int err;
    // Create the socket for connecting to the server
    context->socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (context->socket_fd == INVALID_SOCKET)
    {
        return ARTIE_CAN_ERR_SEND_FAIL;
    }

    // Connect to server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, context->host, &server_addr.sin_addr);
    server_addr.sin_port = htons(context->port);
    err = connect(context->socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err == SOCKET_ERROR)
    {
        closesocket(context->socket_fd);
        context->socket_fd = INVALID_SOCKET;
        return ARTIE_CAN_ERR_SEND_FAIL;
    }

    // Send the frame
    err = send(context->socket_fd, (const char *)frame, sizeof(artie_can_frame_t), 0);
    if (err == SOCKET_ERROR)
    {
        closesocket(context->socket_fd);
        context->socket_fd = INVALID_SOCKET;
        return ARTIE_CAN_ERR_SEND_FAIL;
    }

    // Shutdown the connection since no more data will be sent
    err = shutdown(context->socket_fd, SD_SEND);
    if (err == SOCKET_ERROR)
    {
        closesocket(context->socket_fd);
        context->socket_fd = INVALID_SOCKET;
        return ARTIE_CAN_ERR_SEND_FAIL;
    }

    return ARTIE_CAN_ERR_NONE;
#else // Unix implementation
    // TODO

    return ARTIE_CAN_ERR_NONE;
#endif
}

static artie_can_error_t _close_tcp(void *ctx)
{
    artie_can_tcp_context_t *context = (artie_can_tcp_context_t *)ctx;

    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Alert the server thread that we should stop
    context->should_stop = true;

    // Wait until the server thread has stopped
    if (context->server_thread != INVALID_THREAD_HANDLE)
    {
        #ifdef _WIN32
        WaitForSingleObject(context->server_thread, INFINITE);
        CloseHandle(context->server_thread);
        #else
        pthread_join(context->server_thread, NULL);
        #endif

        context->server_thread = INVALID_THREAD_HANDLE;
    }

    // Cleanup Winsock library
    #ifdef _WIN32
    WSACleanup();
    #endif

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_init_context_tcp(artie_can_context_t *context, const char *host, uint16_t port)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (host == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (strlen(host) >= ARTIE_CAN_TCP_HOSTNAME_MAX_LENGTH)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Initialize the TCP context within the provided artie_can_context_t
    context->backend_context.tcp = (tcp_context_t){
         .host = {0},
         .port = port,
    };

    // Copy the hostname
    strncpy(context->backend_context.tcp.host, host, ARTIE_CAN_TCP_HOSTNAME_MAX_LENGTH);
    context->backend_context.tcp.host[ARTIE_CAN_TCP_HOSTNAME_MAX_LENGTH - 1] = '\0'; // Ensure null termination

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_init_tcp(artie_can_tcp_context_t *context, artie_can_backend_t *handle, artie_can_rx_callback_t rx_callback, artie_can_get_ms_t get_ms_fn)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (handle == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Initialize the backend function pointers and context
    handle->init = _init_tcp;
    handle->send = _send_tcp;
    handle->close = _close_tcp;
    handle->context = context;
    handle->receive_callback = rx_callback;
    handle->get_ms = get_ms_fn;

    return ARTIE_CAN_ERR_NONE;
}
