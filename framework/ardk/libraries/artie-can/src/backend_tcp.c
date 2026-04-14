#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "backend.h"
#include "backend_tcp.h"
#include "err.h"

static artie_can_error_t _init_tcp(void *ctx, artie_can_backend_t *handle)
{
    // TODO
    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _send_tcp(void *ctx, const artie_can_frame_t *frame)
{
    // TODO
    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _receive_tcp(void *ctx, artie_can_frame_t *frame, uint32_t timeout_ms)
{
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
    // TODO
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
