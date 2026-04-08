/**
 * @file artie_can_backend_mock_tcp.c
 * @brief Mock backend for testing with TCP networking
 */

#include "artie_can.h"
#include "artie_can_backends.h"
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
/* MSVC doesn't have ssize_t, so define it */
#ifndef ssize_t
typedef SSIZE_T ssize_t;
#endif
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
typedef int SOCKET;
#endif

/* TCP-based mock context */
typedef struct {
    SOCKET sock;
    SOCKET client_sock;
    bool is_server;
    bool is_connected;
    struct sockaddr_in addr;
    char host[256];
    uint16_t port;
} mock_tcp_context_t;

static mock_tcp_context_t g_mock_tcp_ctx[ARTIE_CAN_N_MOCK_CONTEXTS] = {0};

static int mock_tcp_init(void *ctx)
{
    mock_tcp_context_t *mock = (mock_tcp_context_t *)ctx;

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return ARTIE_CAN_ERR_NETWORK;
    }
#endif

    /* Create socket */
    mock->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (mock->sock == INVALID_SOCKET) {
        return ARTIE_CAN_ERR_NETWORK;
    }

    /* Set socket to non-blocking mode */
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(mock->sock, FIONBIO, &mode);
#else
    int flags = fcntl(mock->sock, F_GETFL, 0);
    fcntl(mock->sock, F_SETFL, flags | O_NONBLOCK);
#endif

    /* Set up address */
    memset(&mock->addr, 0, sizeof(mock->addr));
    mock->addr.sin_family = AF_INET;
    mock->addr.sin_port = htons(mock->port);

    if (mock->is_server) {
        /* Server mode: bind and listen */
        mock->addr.sin_addr.s_addr = INADDR_ANY;

        /* Set SO_REUSEADDR */
        int opt = 1;
        setsockopt(mock->sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

        if (bind(mock->sock, (struct sockaddr *)&mock->addr, sizeof(mock->addr)) == SOCKET_ERROR) {
            closesocket(mock->sock);
            return ARTIE_CAN_ERR_NETWORK;
        }

        if (listen(mock->sock, 1) == SOCKET_ERROR) {
            closesocket(mock->sock);
            return ARTIE_CAN_ERR_NETWORK;
        }

        mock->client_sock = INVALID_SOCKET;
        mock->is_connected = false;
    } else {
        /* Client mode: connect */
        inet_pton(AF_INET, mock->host, &mock->addr.sin_addr);

        /* Non-blocking connect - will complete later */
        int result = connect(mock->sock, (struct sockaddr *)&mock->addr, sizeof(mock->addr));

#ifdef _WIN32
        if (result == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
            closesocket(mock->sock);
            return ARTIE_CAN_ERR_NETWORK;
        }
#else
        if (result == SOCKET_ERROR && errno != EINPROGRESS) {
            closesocket(mock->sock);
            return ARTIE_CAN_ERR_NETWORK;
        }
#endif

        mock->is_connected = (result == 0);
    }

    return 0;
}

static int mock_tcp_ensure_connection(mock_tcp_context_t *mock)
{
    if (mock->is_server) {
        if (mock->client_sock == INVALID_SOCKET) {
            /* Try to accept a connection */
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            mock->client_sock = accept(mock->sock, (struct sockaddr *)&client_addr, &addr_len);

            if (mock->client_sock != INVALID_SOCKET) {
                /* Set client socket to non-blocking */
#ifdef _WIN32
                u_long mode = 1;
                ioctlsocket(mock->client_sock, FIONBIO, &mode);
#else
                int flags = fcntl(mock->client_sock, F_GETFL, 0);
                fcntl(mock->client_sock, F_SETFL, flags | O_NONBLOCK);
#endif
                mock->is_connected = true;
            }
        }
    } else if (!mock->is_connected) {
        /* Check if connection is established */
        fd_set write_fds;
        struct timeval tv = {0, 0};
        FD_ZERO(&write_fds);
        FD_SET(mock->sock, &write_fds);

        if (select(((int)(mock->sock)) + 1, NULL, &write_fds, NULL, &tv) > 0) {
            mock->is_connected = true;
        }
    }

    return mock->is_connected ? 0 : -1;
}

static int mock_tcp_send(void *ctx, const artie_can_frame_t *frame)
{
    mock_tcp_context_t *mock = (mock_tcp_context_t *)ctx;

    if (mock_tcp_ensure_connection(mock) != 0) {
        return ARTIE_CAN_ERR_NOT_CONNECTED;  /* Not connected */
    }

    SOCKET send_sock = mock->is_server ? mock->client_sock : mock->sock;

    /* Send frame size first (for framing) */
    uint32_t frame_size = sizeof(artie_can_frame_t);
    ssize_t sent = send(send_sock, (char *)&frame_size, sizeof(frame_size), 0);
    if (sent != sizeof(frame_size)) {
        return ARTIE_CAN_ERR_SEND_FAILED;
    }

    /* Send frame data */
    sent = send(send_sock, (char *)frame, sizeof(artie_can_frame_t), 0);
    if (sent != sizeof(artie_can_frame_t)) {
        return ARTIE_CAN_ERR_SEND_FAILED;
    }

    return 0;
}

static int mock_tcp_receive(void *ctx, artie_can_frame_t *frame, uint32_t timeout_ms)
{
    mock_tcp_context_t *mock = (mock_tcp_context_t *)ctx;

    if (mock_tcp_ensure_connection(mock) != 0) {
        return ARTIE_CAN_ERR_NOT_CONNECTED;  /* Not connected */
    }

    SOCKET recv_sock = mock->is_server ? mock->client_sock : mock->sock;

    /* Use select for timeout */
    fd_set read_fds;
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    FD_ZERO(&read_fds);
    FD_SET(recv_sock, &read_fds);

    int result = select(recv_sock + 1, &read_fds, NULL, NULL, &tv);
    if (result <= 0) {
        return -2;  /* Timeout or error */
    }

    /* Receive frame size */
    uint32_t frame_size;
    ssize_t received = recv(recv_sock, (char *)&frame_size, sizeof(frame_size), 0);
    if (received != sizeof(frame_size)) {
        return ARTIE_CAN_ERR_RECV_FAILED;
    }

    /* Receive frame data */
    received = recv(recv_sock, (char *)frame, sizeof(artie_can_frame_t), 0);
    if (received != sizeof(artie_can_frame_t)) {
        return ARTIE_CAN_ERR_RECV_FAILED;
    }

    return 0;
}

static int mock_tcp_close(void *ctx)
{
    mock_tcp_context_t *mock = (mock_tcp_context_t *)ctx;

    if (mock->is_server && mock->client_sock != INVALID_SOCKET) {
        closesocket(mock->client_sock);
        mock->client_sock = INVALID_SOCKET;
    }

    if (mock->sock != INVALID_SOCKET) {
        closesocket(mock->sock);
        mock->sock = INVALID_SOCKET;
    }

#ifdef _WIN32
    WSACleanup();
#endif

    mock->is_connected = false;
    return 0;
}

int artie_can_backend_mock_tcp_init(artie_can_context_t *ctx, const artie_can_mock_config_t *config)
{
    if (!ctx || !config) {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    /* Configure TCP context */
    mock_tcp_context_t *backend_ctx = &g_mock_tcp_ctx[ARTIE_CAN_MAP_NODE_ADDRESS_TO_INDEX(ctx->node_address, ARTIE_CAN_N_MOCK_CONTEXTS)];
    backend_ctx->port = config->port;
    backend_ctx->is_server = config->is_server;
    strncpy(backend_ctx->host, config->host, sizeof(backend_ctx->host) - 1);
    backend_ctx->host[sizeof(backend_ctx->host) - 1] = '\0';
    backend_ctx->sock = INVALID_SOCKET;
    backend_ctx->client_sock = INVALID_SOCKET;
    backend_ctx->is_connected = false;

    ctx->backend.init = mock_tcp_init;
    ctx->backend.send = mock_tcp_send;
    ctx->backend.receive = mock_tcp_receive;
    ctx->backend.close = mock_tcp_close;
    ctx->backend.context = backend_ctx;

    return 0;
}
