#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "backend.h"
#include "backend_tcp.h"
#include "circular_buffer.h"
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
    }
}

#ifdef _WIN32
// Windows implementation
static DWORD WINAPI _server_thread_func(void *arg)
{
    artie_can_tcp_context_t *context = (artie_can_tcp_context_t *)arg;

    size_t bytes_received = 0;
    char recvbuf[sizeof(artie_can_frame_t)];
    bool connected = false;
    while (!context->should_stop)
    {
        // Accept a client socket (blocks until a client connects)
        if (!connected)
        {
            context->socket_fd = accept(context->listen_fd, NULL, NULL);
            if (context->socket_fd == INVALID_SOCKET)
            {
                closesocket(context->listen_fd);
                WSACleanup();
                return (DWORD)ARTIE_CAN_ERR_INIT_FAIL;
            }
            connected = true;
        }

        // Receive a buffer from the client (blocks until data is received)
        bool close = false;
        int err = recv(context->socket_fd, recvbuf, sizeof(recvbuf), 0);
        if (err > 0)
        {
            // We received some data
            bytes_received += err;
        }
        else
        {
            // Error receiving from this client. Ignore.
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
    WSACleanup();

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
    int err;

    // Initialize Winsock.
    // Every call to WSAStartup should have a corresponding call to WSACleanup, and we will
    // do so in the server thread at cleanup.
    // This is one reason why the server must be started before the client.
    WSADATA wsa_data;
    err = WSAStartup(MAKEWORD(2,2), &wsa_data);
    if (err != 0)
    {
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    // Resolve the server address and port
    char port_str[6]; // Max port number is 65535, which is 5 digits plus null terminator
    snprintf(port_str, sizeof(port_str), "%u", context->port);
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    err = getaddrinfo(context->host, port_str, &hints, &result);
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

    // We no longer need the addrinfo struct, so free it
    freeaddrinfo(result);

    // Start listening for client connections
    err = listen(context->listen_fd, SOMAXCONN);
    if (err == SOCKET_ERROR)
    {
        closesocket(context->listen_fd);
        WSACleanup();
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    // Start a thread that blocks until a client connects, then receives data until the client disconnects
    // Default security attributes, default stack size, default creation flags, and we don't need the thread identifier
    context->server_thread = CreateThread(NULL, 0, _server_thread_func, (void *)context, 0, NULL);
    if (context->server_thread == INVALID_THREAD_HANDLE)
    {
        closesocket(context->listen_fd);
        WSACleanup();
        return ARTIE_CAN_ERR_INIT_FAIL;
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
    // Nothing to do; all initialization is done in the send function.
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

    // Resolve the server address and port
    char port_str[6]; // Max port number is 65535, which is 5 digits plus null terminator
    snprintf(port_str, sizeof(port_str), "%u", context->port);
    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    struct addrinfo *result = NULL;
    err = getaddrinfo(context->host, port_str, &hints, &result);
    if (err != 0)
    {
        return ARTIE_CAN_ERR_SEND_FAIL;
    }

    // Create the socket for connecting to the server
    context->socket_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (context->socket_fd == INVALID_SOCKET)
    {
        freeaddrinfo(result);
        return ARTIE_CAN_ERR_SEND_FAIL;
    }

    // Connect to server
    err = connect(context->socket_fd, result->ai_addr, (int)result->ai_addrlen);
    if (err == SOCKET_ERROR)
    {
        freeaddrinfo(result);
        closesocket(context->socket_fd);
        context->socket_fd = INVALID_SOCKET;
        return ARTIE_CAN_ERR_SEND_FAIL;
    }

    freeaddrinfo(result);

    // Check the socket
    if (context->socket_fd == INVALID_SOCKET)
    {
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

    // Start waiting for data to be received until we get a frame or timeout
    if (timeout_ms > 0)
    {
        // Get the start time
        #ifdef _WIN32
        ULONGLONG start_time = GetTickCount64();
        #else
        struct timeval start_tv;
        gettimeofday(&start_tv, NULL);
        uint64_t start_time = (uint64_t)start_tv.tv_sec * 1000 + (uint64_t)start_tv.tv_usec / 1000;
        #endif

        while (true)
        {
            if (cb_get_count() > 0)
            {
                // Read a frame from the circular buffer into the provided frame pointer
                return cb_read(frame);
            }

            // Check if we've exceeded the timeout
            #ifdef _WIN32
            ULONGLONG current_time = GetTickCount64();
            ULONGLONG elapsed_ms = current_time - start_time;
            #else
            struct timeval current_tv;
            gettimeofday(&current_tv, NULL);
            uint64_t current_time = (uint64_t)current_tv.tv_sec * 1000 + (uint64_t)current_tv.tv_usec / 1000;
            uint64_t elapsed_ms = current_time - start_time;
            #endif

            if (elapsed_ms >= timeout_ms)
            {
                return ARTIE_CAN_ERR_TIMEOUT;
            }

            // Otherwise, sleep for a moment to avoid busy waiting
            // Sleep for the minimum of 100ms or remaining time
            uint32_t remaining_ms = timeout_ms - (uint32_t)elapsed_ms;
            uint32_t sleep_ms = (remaining_ms < 100) ? remaining_ms : 100;

            #ifdef _WIN32
            Sleep(sleep_ms);
            #else
            usleep(sleep_ms * 1000);
            #endif
        }
    }
    else
    {
        // Block forever until we receive a frame
        while (cb_get_count() == 0)
        {
            // Sleep for a moment to avoid busy waiting
            uint32_t sleep_ms = 100;
            #ifdef _WIN32
            Sleep(sleep_ms);
            #else
            usleep(sleep_ms * 1000);
            #endif
        }

        return cb_read(frame);
    }

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _receive_nonblocking_tcp(void *ctx, artie_can_frame_t *frame, artie_can_receive_callback_t callback)
{
    if (ctx == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    artie_can_tcp_context_t *context = (artie_can_tcp_context_t *)ctx;

    if (callback == NULL)
    {
        // Clear the callback
        context->receive_callback = NULL;
        context->receive_frame = NULL;
    }
    else
    {
        context->receive_frame = frame;
        context->receive_callback = callback;
    }

    return ARTIE_CAN_ERR_NONE;
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
    context->server_thread = INVALID_THREAD_HANDLE;
    context->should_stop = false;
    context->server_ready = false;
    context->receive_callback = NULL;
    context->receive_frame = NULL;

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
