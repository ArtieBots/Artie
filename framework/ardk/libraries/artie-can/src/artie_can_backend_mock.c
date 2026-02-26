/**
 * @file artie_can_backend_mock.c
 * @brief Mock backend for testing with TCP networking support
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

#define MOCK_QUEUE_SIZE 32

/* Local queue-based mock context (original implementation) */
typedef struct {
    artie_can_frame_t queue[MOCK_QUEUE_SIZE];
    size_t head;
    size_t tail;
    size_t count;
} mock_queue_context_t;

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

static mock_queue_context_t g_mock_queue_ctx = {0};
static mock_tcp_context_t g_mock_tcp_ctx = {0};

/* ===== Local Queue Mock Backend ===== */

static int mock_queue_init(void *ctx)
{
    mock_queue_context_t *mock = (mock_queue_context_t *)ctx;
    mock->head = 0;
    mock->tail = 0;
    mock->count = 0;
    return 0;
}

static int mock_queue_send(void *ctx, const artie_can_frame_t *frame)
{
    mock_queue_context_t *mock = (mock_queue_context_t *)ctx;

    if (mock->count >= MOCK_QUEUE_SIZE) {
        return -1;  /* Queue full */
    }

    /* Add to queue */
    memcpy(&mock->queue[mock->tail], frame, sizeof(artie_can_frame_t));
    mock->tail = (mock->tail + 1) % MOCK_QUEUE_SIZE;
    mock->count++;

    return 0;
}

static int mock_queue_receive(void *ctx, artie_can_frame_t *frame, uint32_t timeout_ms)
{
    mock_queue_context_t *mock = (mock_queue_context_t *)ctx;

    (void)timeout_ms;  /* Ignore timeout in local queue mock */

    if (mock->count == 0) {
        return -1;  /* Queue empty */
    }

    /* Remove from queue */
    memcpy(frame, &mock->queue[mock->head], sizeof(artie_can_frame_t));
    mock->head = (mock->head + 1) % MOCK_QUEUE_SIZE;
    mock->count--;

    return 0;
}

static int mock_queue_close(void *ctx)
{
    (void)ctx;
    return 0;
}

int artie_can_backend_mock_init(artie_can_backend_t *backend)
{
    if (!backend) {
        return -1;
    }

    backend->init = mock_queue_init;
    backend->send = mock_queue_send;
    backend->receive = mock_queue_receive;
    backend->close = mock_queue_close;
    backend->context = &g_mock_queue_ctx;

    return 0;
}

/* ===== TCP-based Mock Backend ===== */

static int mock_tcp_init(void *ctx)
{
    mock_tcp_context_t *mock = (mock_tcp_context_t *)ctx;

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return -1;
    }
#endif

    /* Create socket */
    mock->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (mock->sock == INVALID_SOCKET) {
        return -1;
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
            return -1;
        }

        if (listen(mock->sock, 1) == SOCKET_ERROR) {
            closesocket(mock->sock);
            return -1;
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
            return -1;
        }
#else
        if (result == SOCKET_ERROR && errno != EINPROGRESS) {
            closesocket(mock->sock);
            return -1;
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

        if (select(mock->sock + 1, NULL, &write_fds, NULL, &tv) > 0) {
            mock->is_connected = true;
        }
    }

    return mock->is_connected ? 0 : -1;
}

static int mock_tcp_send(void *ctx, const artie_can_frame_t *frame)
{
    mock_tcp_context_t *mock = (mock_tcp_context_t *)ctx;

    if (mock_tcp_ensure_connection(mock) != 0) {
        return -1;  /* Not connected */
    }

    SOCKET send_sock = mock->is_server ? mock->client_sock : mock->sock;

    /* Send frame size first (for framing) */
    uint32_t frame_size = sizeof(artie_can_frame_t);
    ssize_t sent = send(send_sock, (char *)&frame_size, sizeof(frame_size), 0);
    if (sent != sizeof(frame_size)) {
        return -1;
    }

    /* Send frame data */
    sent = send(send_sock, (char *)frame, sizeof(artie_can_frame_t), 0);
    if (sent != sizeof(artie_can_frame_t)) {
        return -1;
    }

    return 0;
}

static int mock_tcp_receive(void *ctx, artie_can_frame_t *frame, uint32_t timeout_ms)
{
    mock_tcp_context_t *mock = (mock_tcp_context_t *)ctx;

    if (mock_tcp_ensure_connection(mock) != 0) {
        return -1;  /* Not connected */
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
        return -1;
    }

    /* Receive frame data */
    received = recv(recv_sock, (char *)frame, sizeof(artie_can_frame_t), 0);
    if (received != sizeof(artie_can_frame_t)) {
        return -1;
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

int artie_can_backend_mock_tcp_init(artie_can_backend_t *backend, const artie_can_mock_config_t *config)
{
    if (!backend || !config) {
        return -1;
    }

    /* Configure TCP context */
    g_mock_tcp_ctx.port = config->port;
    g_mock_tcp_ctx.is_server = config->is_server;
    strncpy(g_mock_tcp_ctx.host, config->host, sizeof(g_mock_tcp_ctx.host) - 1);
    g_mock_tcp_ctx.host[sizeof(g_mock_tcp_ctx.host) - 1] = '\0';
    g_mock_tcp_ctx.sock = INVALID_SOCKET;
    g_mock_tcp_ctx.client_sock = INVALID_SOCKET;
    g_mock_tcp_ctx.is_connected = false;

    backend->init = mock_tcp_init;
    backend->send = mock_tcp_send;
    backend->receive = mock_tcp_receive;
    backend->close = mock_tcp_close;
    backend->context = &g_mock_tcp_ctx;

    return 0;
}
