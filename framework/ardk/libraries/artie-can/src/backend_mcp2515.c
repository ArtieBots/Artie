#include <stdbool.h>
#include <stdint.h>
#include "backend.h"
#include "backend_mcp2515.h"
#include "err.h"

static artie_can_error_t _init_mcp2515(void *ctx)
{
    // TODO
    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _send_mcp2515(void *ctx, const artie_can_frame_t *frame)
{
    // TODO
    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _close_mcp2515(void *ctx)
{
    // TODO
    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_init_context_mcp2515(artie_can_mcp2515_context_t *context, uint8_t dummy)
{
    // TODO
    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_init_mcp2515(artie_can_mcp2515_context_t *context, artie_can_backend_t *handle, artie_can_rx_callback_t rx_callback, artie_can_get_ms_t get_ms_fn)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (handle == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Set up the function pointers in the handle
    handle->init = _init_mcp2515;
    handle->send = _send_mcp2515;
    handle->close = _close_mcp2515;
    handle->context = context;
    handle->receive_callback = rx_callback;
    handle->get_ms = get_ms_fn;

    return ARTIE_CAN_ERR_NONE;
}
