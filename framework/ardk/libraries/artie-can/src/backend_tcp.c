#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "backend.h"
#include "backend_tcp.h"
#include "err.h"

#ifdef _WIN32
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/time.h>
#endif

// Buffer for frames we have received.
#define _ARTIE_CAN_TCP_N_RX_MAX (10)
static artie_can_frame_t _rx_buffer[_ARTIE_CAN_TCP_N_RX_MAX] = {0};

#ifdef _WIN32
// Windows implementation
static DWORD WINAPI _server_thread_func(void *arg)
{
    artie_can_tcp_context_t *context = (artie_can_tcp_context_t *)arg;

    // Accept a client socket (blocks until a client connects)
    context->socket_fd = accept(context->listen_fd, NULL, NULL);
    if (context->socket_fd == INVALID_SOCKET)
    {
        closesocket(context->listen_fd);
        WSACleanup();
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    // Receive a buffer from the client (blocks until data is received)
    char recvbuf[sizeof(artie_can_frame_t)];
    int err = recv(context->socket_fd, recvbuf, sizeof(recvbuf), 0);
    if (err > 0)
    {
        // Received 'err' bytes, which should be the size of an artie_can_frame_t
        if (err != sizeof(artie_can_frame_t))
        {
            return ARTIE_CAN_ERR_RECEIVE_FAIL;
        }
    }
    else if  (err == 0)
    {
        // No more bytes to receive
    }
    else
    {
        closesocket(context->socket_fd);
        WSACleanup();
        return ARTIE_CAN_ERR_RECEIVE_FAIL;
    }

    // Close the connection
    err = shutdown(context->socket_fd, SD_SEND);
    if (err == SOCKET_ERROR)
    {
        closesocket(context->socket_fd);
        WSACleanup();
        return ARTIE_CAN_ERR_RECEIVE_FAIL;
    }

    // Copy the received buffer into the static rx buffer at the right index
    static size_t rx_buffer_index = 0;
    if (rx_buffer_index < _ARTIE_CAN_TCP_N_RX_MAX)
    {
        memcpy(&_rx_buffer[rx_buffer_index], recvbuf, sizeof(artie_can_frame_t));
        rx_buffer_index++;
    }
    else
    {
        // Buffer overflow, we received more frames than we have space for
        return ARTIE_CAN_ERR_RECEIVE_FAIL;
    }

    return ARTIE_CAN_ERR_NONE;
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
    int err;

    // Initialize Winsock
    WSADATA wsa_data;
    err = WSAStartup(MAKEWORD(2,2), &wsa_data);
    if (err != 0)
    {
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    // Resolve the server address and port
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    // TODO: NULL should be the host
    err = getaddrinfo(NULL, context->port, &hints, &result);
    if (err != 0)
    {
        WSACleanup();
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    // Create a SOCKET for the server to listen for client connections.
    context->listen_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (context->listen_fd == INVALID_SOCKET)
    {
        freeaddrinfo(result);
        WSACleanup();
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    // Setup the TCP listening socket
    err = bind(context->listen_fd, result->ai_addr, (int)result->ai_addrlen);
    if (err == SOCKET_ERROR)
    {
        freeaddrinfo(result);
        closesocket(context->listen_fd);
        WSACleanup();
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    freeaddrinfo(result);

    err = listen(context->listen_fd, SOMAXCONN);
    if (err == SOCKET_ERROR)
    {
        closesocket(context->listen_fd);
        WSACleanup();
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    // Start a thread that blocks until a client connects, then receives data until the client disconnects
    // Default security attributes, default stack size, default creation flags, and we don't need the thread identifier
    context->accept_thread = CreateThread(NULL, 0, _server_thread_func, (void *)context, 0, NULL);
    if (context->accept_thread == INVALID_THREAD_HANDLE)
    {
        closesocket(context->listen_fd);
        WSACleanup();
        return ARTIE_CAN_ERR_INIT_FAIL;
    }
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

    if (context->socket_fd == INVALID_SOCKET)
    {
        return ARTIE_CAN_ERR_CLOSED;
    }

    // TODO

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

    // TODO

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _receive_nonblocking_tcp(void *ctx, artie_can_frame_t *frame, uint32_t timeout_ms, artie_can_receive_callback_t callback)
{
    // TODO

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _close_tcp(void *ctx)
{
    artie_can_tcp_context_t *context = (artie_can_tcp_context_t *)ctx;

    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // TODO
    closesocket(context->listen_fd);

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
    context->server_ready = false;

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
