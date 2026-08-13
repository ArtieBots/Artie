#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "backend.h"
#include "backend_mcp2515_context.h"
#include "backend_mcp2515.h"
#include "driver_mcp2515.h"
#include "err.h"

static artie_can_error_t _init_mcp2515(artie_can_context_t *context)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Set up the function pointers in the handle
    context->isr_handler = driver_mcp2515_isr;

    // Initialize the driver
    artie_can_error_t err;
    err = driver_mcp2515_init(context);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // Initialize the device itself
    err = driver_mcp2515_config(context);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _send_mcp2515(artie_can_context_t *context, const artie_can_frame_t *frame)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (frame == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Send the frame using the MCP2515 driver
    return driver_mcp2515_send(context, frame);
}

static artie_can_error_t _close_mcp2515(artie_can_context_t *context)
{
    return driver_mcp2515_deinit(context);
}

artie_can_error_t artie_can_init_context_mcp2515(artie_can_context_t *context, artie_can_mcp2515_context_t *mcp2515_ctx, driver_mcp2515_config_t *driver_config, artie_can_write_byte_t write_byte_fn, artie_can_write_cs_pin_t write_cs_pin_fn)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (mcp2515_ctx == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (driver_config == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (write_byte_fn == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (write_cs_pin_fn == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Copy the driver config into the context
    memcpy(&mcp2515_ctx->mcp2515_config, driver_config, sizeof(driver_mcp2515_config_t));

    // Set the function pointers in the context
    mcp2515_ctx->write_byte = write_byte_fn;
    mcp2515_ctx->write_cs_pin = write_cs_pin_fn;

    // Now that the mcp2515 context is initialized, add it into the main context's backend context pointer
    context->backend_context = (void *)mcp2515_ctx;

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t mcp2515_init(artie_can_context_t *context, artie_can_backend_t *handle, artie_can_rx_callback_t rx_callback, artie_can_get_ms_t get_ms_fn)
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

    // Set up the function pointers in the handle
    handle->init = _init_mcp2515;
    handle->send = _send_mcp2515;
    handle->close = _close_mcp2515;
    handle->context = context;
    handle->context->rx_callback = rx_callback;
    handle->context->isr_handler = driver_mcp2515_isr;
    handle->get_ms = get_ms_fn;

    return ARTIE_CAN_ERR_NONE;
}
