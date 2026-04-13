/**
 * @file artie_can_core.c
 * @brief Core implementation for Artie CAN library.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include "artie_can.h"
#include "err.h"

static artie_can_error_t _init_mcp2515(artie_can_mcp2515_context_t *context, artie_can_backend_t *handle)
{
    artie_can_error_t err;

    err = artie_can_init_mcp2515(context, handle);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }
    else if (handle->init == NULL)
    {
        return ARTIE_CAN_ERR_INIT_FAIL;
    }
    else
    {
        return handle->init(handle->context);
    }
}

static artie_can_error_t _init_tcp(artie_can_tcp_context_t *context, artie_can_backend_t *handle)
{
    artie_can_error_t err;

    err = artie_can_init_tcp(context, handle);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }
    else if (handle->init == NULL)
    {
        return ARTIE_CAN_ERR_INIT_FAIL;
    }
    else
    {
        return handle->init(handle->context);
    }
}

artie_can_error_t artie_can_init(void *context, artie_can_backend_t *handle, artie_can_backend_type_t backend_type)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (handle == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    switch (backend_type)
    {
        case ARTIE_CAN_BACKEND_MCP2515:
            return artie_can_init_mcp2515((artie_can_mcp2515_context_t *)context, handle);
        case ARTIE_CAN_BACKEND_TCP:
            return artie_can_init_tcp((artie_can_tcp_context_t *)context, handle);
        default:
            return ARTIE_CAN_ERR_INVALID_ARG;
    }
}

artie_can_error_t artie_can_init_custom(artie_can_backend_t *handle)
{
    if (handle == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (handle->init == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    return handle->init(handle->context);
}

artie_can_error_t artie_can_close(artie_can_backend_t *handle)
{
    if (handle == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (handle->close == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    artie_can_error_t err = handle->close(handle->context);
    if (err == ARTIE_CAN_ERR_NONE)
    {
        // Zero out the handle so it can't be used again without reinitialization
        memset(handle, 0, sizeof(artie_can_backend_t));
    }
    return err;
}

artie_can_error_t artie_can_send(artie_can_backend_t *handle, const artie_can_frame_t *frame)
{
    if (handle == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (frame == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (handle->send == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    return handle->send(handle->context, frame);
}

artie_can_error_t artie_can_receive(artie_can_backend_t *handle, artie_can_frame_t *frame, uint32_t timeout_ms)
{
    if (handle == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (frame == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (handle->receive == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    return handle->receive(handle->context, frame, timeout_ms);
}

artie_can_error_t artie_can_receive_nonblocking(artie_can_backend_t *handle, artie_can_frame_t *frame, uint32_t timeout_ms, artie_can_receive_callback_t callback)
{
    if (handle == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (frame == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (callback == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (handle->receive_nonblocking == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    return handle->receive_nonblocking(handle->context, frame, timeout_ms, callback);
}
