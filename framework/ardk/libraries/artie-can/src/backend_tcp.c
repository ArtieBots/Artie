#ifndef _WIN32
#include <sys/time.h>
#endif
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "backend.h"
#include "backend_tcp.h"
#include "err.h"

// Platform-specific thread function signature
#ifdef _WIN32
    #define THREAD_RETURN DWORD WINAPI
    #define THREAD_RETURN_VALUE 0
#else
    #define THREAD_RETURN void*
    #define THREAD_RETURN_VALUE NULL
#endif

/** Server accept thread function */
static THREAD_RETURN _server_accept_thread(void *arg)
{
    artie_can_tcp_context_t *context = (artie_can_tcp_context_t *)arg;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (!context->should_stop)
    {
        // Accept a client connection
        socket_t client_fd = accept(context->listen_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd == INVALID_SOCKET)
        {
            // Check if we're stopping
            if (context->should_stop)
            {
                break;
            }
            // Otherwise, continue to try again
            continue;
        }

        // Store the accepted socket
        // Note: This simple implementation only supports one client at a time
        // Close any existing connection
        if (context->socket_fd != INVALID_SOCKET)
        {
            CLOSE_SOCKET(context->socket_fd);
        }
        context->socket_fd = client_fd;
    }

    return THREAD_RETURN_VALUE;
}

static artie_can_error_t _init_tcp(void *ctx)
{
    artie_can_tcp_context_t *context = (artie_can_tcp_context_t *)ctx;

    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

#ifdef _WIN32
    // Initialize Winsock on Windows
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0)
    {
        return ARTIE_CAN_ERR_INIT_FAIL;
    }
#endif

    if (context->is_server)
    {
        // Server mode: Create listening socket and start accept thread
        struct sockaddr_in server_addr;

        // Create listening socket
        context->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (context->listen_fd == INVALID_SOCKET)
        {
            return ARTIE_CAN_ERR_INIT_FAIL;
        }

        // Enable SO_REUSEADDR to avoid "address already in use" errors
        int opt = 1;
        if (setsockopt(context->listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) < 0)
        {
            CLOSE_SOCKET(context->listen_fd);
            return ARTIE_CAN_ERR_INIT_FAIL;
        }

        // Bind to the specified port
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(context->port);

        if (bind(context->listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            CLOSE_SOCKET(context->listen_fd);
            return ARTIE_CAN_ERR_INIT_FAIL;
        }

        // Start listening
        if (listen(context->listen_fd, 1) < 0)
        {
            CLOSE_SOCKET(context->listen_fd);
            return ARTIE_CAN_ERR_INIT_FAIL;
        }

        // Start accept thread
#ifdef _WIN32
        context->accept_thread = CreateThread(NULL, 0, _server_accept_thread, context, 0, NULL);
        if (context->accept_thread == NULL)
        {
            CLOSE_SOCKET(context->listen_fd);
            return ARTIE_CAN_ERR_INIT_FAIL;
        }
#else
        if (pthread_create(&context->accept_thread, NULL, _server_accept_thread, context) != 0)
        {
            CLOSE_SOCKET(context->listen_fd);
            return ARTIE_CAN_ERR_INIT_FAIL;
        }
#endif
    }

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _send_tcp(void *ctx, const artie_can_frame_t *frame)
{
    artie_can_tcp_context_t *context = (artie_can_tcp_context_t *)ctx;

    if (context == NULL || frame == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Client mode: Connect to server
    struct sockaddr_in server_addr;

    context->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (context->socket_fd == INVALID_SOCKET)
    {
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(context->port);

    // Convert hostname to IP address
    if (inet_pton(AF_INET, context->host, &server_addr.sin_addr) <= 0)
    {
        CLOSE_SOCKET(context->socket_fd);
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    // Connect to server
    if (connect(context->socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        CLOSE_SOCKET(context->socket_fd);
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    if (context->socket_fd == INVALID_SOCKET)
    {
        return ARTIE_CAN_ERR_CLOSED;
    }

    // Send the frame structure over the socket
    int bytes_sent = send(context->socket_fd, (const char *)frame, sizeof(artie_can_frame_t), 0);
    if (bytes_sent != sizeof(artie_can_frame_t))
    {
        return ARTIE_CAN_ERR_SEND_FAIL;
    }

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _receive_tcp(void *ctx, artie_can_frame_t *frame, uint32_t timeout_ms)
{
    artie_can_tcp_context_t *context = (artie_can_tcp_context_t *)ctx;

    if (context == NULL || frame == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    if (context->socket_fd == INVALID_SOCKET)
    {
        return ARTIE_CAN_ERR_CLOSED;
    }

    // Set socket timeout
    if (timeout_ms > 0)
    {
#ifdef _WIN32
        DWORD timeout = timeout_ms;
        if (setsockopt(context->socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0)
        {
            return ARTIE_CAN_ERR_RECEIVE_FAIL;
        }
#else
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (setsockopt(context->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        {
            return ARTIE_CAN_ERR_RECEIVE_FAIL;
        }
#endif
    }

    // Receive the frame structure from the socket
    int bytes_received = recv(context->socket_fd, (char *)frame, sizeof(artie_can_frame_t), 0);
    if (bytes_received != sizeof(artie_can_frame_t))
    {
        if (bytes_received == 0)
        {
            // Connection closed
            return ARTIE_CAN_ERR_CLOSED;
        }
#ifdef _WIN32
        if (WSAGetLastError() == WSAETIMEDOUT)
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK)
#endif
        {
            return ARTIE_CAN_ERR_TIMEOUT;
        }
        return ARTIE_CAN_ERR_RECEIVE_FAIL;
    }

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _receive_nonblocking_tcp(void *ctx, artie_can_frame_t *frame, uint32_t timeout_ms, artie_can_receive_callback_t callback)
{
    // For simplicity, implement as blocking receive with callback
    // A full implementation might use select/poll or a separate thread
    artie_can_error_t err = _receive_tcp(ctx, frame, timeout_ms);

    if (callback != NULL)
    {
        callback(ctx, err, (err == ARTIE_CAN_ERR_NONE) ? frame : NULL);
    }

    return err;
}

static artie_can_error_t _close_tcp(void *ctx)
{
    artie_can_tcp_context_t *context = (artie_can_tcp_context_t *)ctx;

    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Signal the accept thread to stop (if server mode)
    if (context->is_server && context->accept_thread != INVALID_THREAD_HANDLE)
    {
        context->should_stop = true;

        // Close the listening socket to unblock accept()
        if (context->listen_fd != INVALID_SOCKET)
        {
            CLOSE_SOCKET(context->listen_fd);
            context->listen_fd = INVALID_SOCKET;
        }

        // Wait for the accept thread to finish
#ifdef _WIN32
        WaitForSingleObject(context->accept_thread, INFINITE);
        CloseHandle(context->accept_thread);
#else
        pthread_join(context->accept_thread, NULL);
#endif
        context->accept_thread = INVALID_THREAD_HANDLE;
    }

    // Close the data socket
    if (context->socket_fd != INVALID_SOCKET)
    {
        CLOSE_SOCKET(context->socket_fd);
        context->socket_fd = INVALID_SOCKET;
    }

#ifdef _WIN32
    WSACleanup();
#endif

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_init_context_tcp(artie_can_tcp_context_t *context, const char *host, uint16_t port, bool is_server)
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

    // Copy the args into the struct
    strncpy(context->host, host, ARTIE_CAN_TCP_HOSTNAME_MAX_LENGTH);
    context->host[ARTIE_CAN_TCP_HOSTNAME_MAX_LENGTH - 1] = '\0'; // Ensure null termination
    context->port = port;
    context->is_server = is_server;
    context->socket_fd = INVALID_SOCKET;
    context->listen_fd = INVALID_SOCKET;
    context->accept_thread = INVALID_THREAD_HANDLE;
    context->should_stop = false;

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_init_tcp(artie_can_tcp_context_t *context, artie_can_backend_t *handle)
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
    handle->receive = _receive_tcp;
    handle->receive_nonblocking = _receive_nonblocking_tcp;
    handle->close = _close_tcp;
    handle->context = context;

    return ARTIE_CAN_ERR_NONE;
}
