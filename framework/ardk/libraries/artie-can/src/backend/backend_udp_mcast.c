/**
 * @file backend_udp_mcast.c
 * @brief Implementation of the UDP Multicast backend for Artie CAN.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "backend.h"
#include "backend_udp_mcast.h"
#include "bwacp.h"
#include "context.h"
#include "err.h"
#include "log.h"
#include "rtacp.h"
#include "translationlayer.h"

static void _complete_frame(artie_can_context_t *context, const char *recvbuf)
{
    // Convert from raw buffer to frame struct
    artie_can_frame_t *frame = (artie_can_frame_t *)recvbuf;

    // Ensure the frame isn't from us
    uint8_t source_address = (uint8_t)((frame->id & ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK) >> ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION);
    if (source_address == context->node_address)
    {
        ARTIE_CAN_LOG(context, "[UDP] Ignoring frame from self (source address 0x%X)\n", source_address);
        return;
    }

    // Check the frame's protocol against the context's protocol flags to see if we should
    // feed it back up the stack to its appropriate state machine.
    uint16_t protocol = (frame->id & ARTIE_CAN_FRAME_ID_PROTOCOL_MASK) >> ARTIE_CAN_FRAME_ID_PROTOCOL_LOCATION;
    switch (protocol)
    {
        case ARTIE_CAN_RTACP_PROTOCOL_ID:
            rtacp_receive_in_isr(context, frame);
            break;

        case ARTIE_CAN_BWACP_PROTOCOL_ID:
            bwacp_receive_in_isr(context, frame);
            break;

        default:
            // Unknown protocol, so ignore the frame
            break;
    }
}

#ifdef _WIN32
static DWORD WINAPI _receiver_thread_func(LPVOID arg)
#else
static void *_receiver_thread_func(void *arg)
#endif
{
    artie_can_context_t *context = (artie_can_context_t *)arg;
    artie_can_udp_mcast_context_t *mcast_ctx = (artie_can_udp_mcast_context_t *)(context->backend_context);

    ARTIE_CAN_LOG(context, "[UDP] Receiver thread started\n");

    // Alert the main thread we are ready
    mcast_ctx->receiver_ready = true;

    char recvbuf[sizeof(artie_can_frame_t)];
    sockaddr_in_t sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);

    while (!mcast_ctx->should_stop)
    {
        // Receive a packet (blocks until data is received or timeout)
        int recv_size = recvfrom(mcast_ctx->socket_fd, recvbuf, sizeof(recvbuf), 0, (sockaddr_t *)&sender_addr, &sender_addr_len);
        if (recv_size > 0)
        {
            if (recv_size == sizeof(artie_can_frame_t))
            {
                // We have received a full frame
                ARTIE_CAN_LOG(context, "[UDP] Received frame (%d bytes)\n", recv_size);
                _complete_frame(context, recvbuf);
            }
            else
            {
                // Partial or malformed frame
                ARTIE_CAN_LOG(context, "[UDP] Received malformed frame (%d bytes, expected %zu)\n", recv_size, sizeof(artie_can_frame_t));
            }
        }
        else if (recv_size == SOCKET_ERROR_VALUE)
        {
            if (is_socket_error_wouldblock())
            {
                // Timeout or interrupted - just continue
                continue;
            }
            else
            {
                ARTIE_CAN_LOG(context, "[UDP] Receive error: %d\n", get_socket_error());
            }
        }
    }

    ARTIE_CAN_LOG(context, "[UDP] Receiver thread exiting\n");
#ifdef _WIN32
    return (DWORD)ARTIE_CAN_ERR_NONE;
#else
    return (void *)(intptr_t)ARTIE_CAN_ERR_NONE;
#endif
}

static artie_can_error_t _init_udp_mcast(artie_can_context_t *context)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    artie_can_udp_mcast_context_t *mcast_ctx = (artie_can_udp_mcast_context_t *)(context->backend_context);

    // Initialize socket subsystem (Winsock on Windows, no-op on POSIX)
    if (!socket_subsystem_init())
    {
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    // Create UDP socket
    mcast_ctx->socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (mcast_ctx->socket_fd == INVALID_SOCKET_VALUE)
    {
        ARTIE_CAN_LOG(context, "[UDP] Failed to create socket\n");
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    // Allow multiple sockets to bind to the same port (required for multicast)
    int reuse = 1;
    if (setsockopt(mcast_ctx->socket_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) == SOCKET_ERROR_VALUE)
    {
        ARTIE_CAN_LOG(context, "[UDP] Failed to set SO_REUSEADDR\n");
        close_socket(mcast_ctx->socket_fd);
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    // On Unix-like systems, also set SO_REUSEPORT for better port sharing (no-op on Windows)
    if (set_socket_reuse_port(mcast_ctx->socket_fd) == SOCKET_ERROR_VALUE)
    {
        ARTIE_CAN_LOG(context, "[UDP] Warning: Failed to set SO_REUSEPORT\n");
        // Non-fatal on some systems
    }

    // Set receive timeout to allow checking should_stop flag periodically
    timeval_t tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100ms timeout
    if (setsockopt(mcast_ctx->socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv)) == SOCKET_ERROR_VALUE)
    {
        ARTIE_CAN_LOG(context, "[UDP] Warning: Failed to set receive timeout\n");
        // Non-fatal
    }

    // Bind to the multicast port (bind to INADDR_ANY to receive multicast)
    sockaddr_in_t local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(mcast_ctx->config.port);

    if (bind(mcast_ctx->socket_fd, (sockaddr_t *)&local_addr, sizeof(local_addr)) == SOCKET_ERROR_VALUE)
    {
        ARTIE_CAN_LOG(context, "[UDP] Failed to bind socket to port %d\n", mcast_ctx->config.port);
        close_socket(mcast_ctx->socket_fd);
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    // Join the multicast group
    ip_mreq_t mreq;
    memset(&mreq, 0, sizeof(mreq));
    if (inet_pton(AF_INET, mcast_ctx->config.group_addr, &mreq.imr_multiaddr) != 1)
    {
        ARTIE_CAN_LOG(context, "[UDP] Invalid multicast address: %s\n", mcast_ctx->config.group_addr);
        close_socket(mcast_ctx->socket_fd);
        return ARTIE_CAN_ERR_INIT_FAIL;
    }
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(mcast_ctx->socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) == SOCKET_ERROR_VALUE)
    {
        ARTIE_CAN_LOG(context, "[UDP] Failed to join multicast group %s\n", mcast_ctx->config.group_addr);
        close_socket(mcast_ctx->socket_fd);
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    ARTIE_CAN_LOG(context, "[UDP] Joined multicast group %s:%d\n", mcast_ctx->config.group_addr, mcast_ctx->config.port);

    // Start receiver thread
    if (!create_thread(&mcast_ctx->receiver_thread, _receiver_thread_func, (void *)context))
    {
        ARTIE_CAN_LOG(context, "[UDP] Failed to create receiver thread\n");
        close_socket(mcast_ctx->socket_fd);
        return ARTIE_CAN_ERR_INIT_FAIL;
    }

    // Wait until the receiver thread is ready
    while (!mcast_ctx->receiver_ready)
    {
        SLEEP_MS(10);
    }

    ARTIE_CAN_LOG(context, "[UDP] UDP multicast backend initialized\n");
    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _send_udp_mcast(void *ctx, const artie_can_frame_t *frame)
{
    if (ctx == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (frame == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    artie_can_context_t *context = (artie_can_context_t *)ctx;
    artie_can_udp_mcast_context_t *mcast_ctx = (artie_can_udp_mcast_context_t *)(context->backend_context);

    // Set up multicast destination address
    sockaddr_in_t dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(mcast_ctx->config.port);
    if (inet_pton(AF_INET, mcast_ctx->config.group_addr, &dest_addr.sin_addr) != 1)
    {
        ARTIE_CAN_LOG(context, "[UDP] Invalid multicast address\n");
        return ARTIE_CAN_ERR_SEND_FAIL;
    }

    // Send the frame to the multicast group
    ARTIE_CAN_LOG(context, "[UDP] Sending frame with ID 0x%X to multicast group\n", frame->id);
    int send_result = sendto(mcast_ctx->socket_fd, (const char *)frame, sizeof(artie_can_frame_t), 0, (sockaddr_t *)&dest_addr, sizeof(dest_addr));

    if (send_result == SOCKET_ERROR_VALUE)
    {
        ARTIE_CAN_LOG(context, "[UDP] Failed to send frame\n");
        return ARTIE_CAN_ERR_SEND_FAIL;
    }

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _close_udp_mcast(artie_can_context_t *context)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    artie_can_udp_mcast_context_t *mcast_ctx = (artie_can_udp_mcast_context_t *)(context->backend_context);

    // Signal the receiver thread to stop
    mcast_ctx->should_stop = true;

    // Wait for the receiver thread to finish
    if (mcast_ctx->receiver_thread != INVALID_THREAD_HANDLE)
    {
        join_thread(mcast_ctx->receiver_thread, 0);  // 0 = infinite wait
        mcast_ctx->receiver_thread = INVALID_THREAD_HANDLE;
    }

    // Leave the multicast group
    ip_mreq_t mreq;
    memset(&mreq, 0, sizeof(mreq));
    inet_pton(AF_INET, mcast_ctx->config.group_addr, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(mcast_ctx->socket_fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)&mreq, sizeof(mreq));

    // Close the socket
    close_socket(mcast_ctx->socket_fd);
    mcast_ctx->socket_fd = INVALID_SOCKET_VALUE;

    // Cleanup socket subsystem
    socket_subsystem_cleanup();

    ARTIE_CAN_LOG(context, "[UDP] UDP multicast backend closed\n");
    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_init_context_udp_mcast(artie_can_context_t *context, artie_can_udp_mcast_context_t *udp_mcast_context, const char *multicast_group, uint16_t port)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (udp_mcast_context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (multicast_group == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (strlen(multicast_group) == 0)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (strlen(multicast_group) >= ARTIE_CAN_UDP_MCAST_ADDR_MAX_LENGTH)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Initialize the UDP multicast context
    *udp_mcast_context = (artie_can_udp_mcast_context_t){
        .config = {
            .group_addr = {0},
            .port = port,
        },
        .receiver_thread = INVALID_THREAD_HANDLE,
        .receiver_ready = false,
        .should_stop = false,
        .socket_fd = INVALID_SOCKET_VALUE,
        .rx_callback = NULL,
    };

    // Copy the multicast group address
    strncpy(udp_mcast_context->config.group_addr, multicast_group, ARTIE_CAN_UDP_MCAST_ADDR_MAX_LENGTH - 1);
    udp_mcast_context->config.group_addr[ARTIE_CAN_UDP_MCAST_ADDR_MAX_LENGTH - 1] = '\0'; // Ensure null termination

    // Store the UDP multicast context in the main context
    context->backend_context = (void *)udp_mcast_context;

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t udp_mcast_init(artie_can_context_t *context, artie_can_backend_t *handle, artie_can_rx_callback_t rx_callback, artie_can_get_ms_t get_ms_fn)
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

    // Initialize the backend handle with UDP multicast function pointers
    handle->init = _init_udp_mcast;
    handle->send = _send_udp_mcast;
    handle->close = _close_udp_mcast;
    handle->context = context;
    handle->context->rx_callback = rx_callback;
    handle->context->isr_handler = NULL;
    handle->get_ms = get_ms_fn;

    return ARTIE_CAN_ERR_NONE;
}
