/**
 * @file artie_can_core.c
 * @brief Core implementation for Artie CAN library.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "artie_can.h"
#include "backend.h"
#include "err.h"
#include "rtacp.h"
#include "translationlayer.h"

static artie_can_error_t _init_mcp2515(artie_can_context_t *context, artie_can_backend_t *handle, artie_can_rx_callback_t rx_callback, artie_can_get_ms_t get_ms_fn)
{
    artie_can_error_t err;

    err = mcp2515_init(context, handle, rx_callback, get_ms_fn);
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

static artie_can_error_t _init_tcp(artie_can_context_t *context, artie_can_backend_t *handle, artie_can_rx_callback_t rx_callback, artie_can_get_ms_t get_ms_fn)
{
    artie_can_error_t err;

    err = tcp_init(context, handle, rx_callback, get_ms_fn);
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

artie_can_error_t artie_can_init(artie_can_context_t *context, artie_can_backend_t *handle, artie_can_backend_type_t backend_type, artie_can_rx_callback_t rx_callback, artie_can_get_ms_t get_ms_fn)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (handle == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (context->protocol_flags == 0)
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

    switch (backend_type)
    {
        case ARTIE_CAN_BACKEND_MCP2515:
            return _init_mcp2515(context, handle, rx_callback, get_ms_fn);
        case ARTIE_CAN_BACKEND_TCP:
            return _init_tcp(context, handle, rx_callback, get_ms_fn);
        default:
            return ARTIE_CAN_ERR_INVALID_ARG;
    }
}

artie_can_error_t artie_can_init_custom(artie_can_context_t *context, artie_can_backend_t *handle, artie_can_rx_callback_t rx_callback, artie_can_get_ms_t get_ms_fn)
{
    if (handle == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (handle->init == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    handle->context = context;
    handle->receive_callback = rx_callback;
    handle->get_ms = get_ms_fn;

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

    // Stop the event loop thread if it's running
    if (handle->context != NULL && handle->context->event_loop.thread != INVALID_THREAD_HANDLE)
    {
        // Signal the thread to stop
        handle->context->event_loop.running = false;

        // Wait for the thread to finish (with a reasonable timeout)
        if (!join_thread(handle->context->event_loop.thread, 5000))  // 5 second timeout
        {
            // Thread didn't exit cleanly within timeout
            // On Windows, we should still close the handle; on POSIX, pthread_join blocks indefinitely
#ifdef _WIN32
            CloseHandle(handle->context->event_loop.thread);
#endif
        }

        handle->context->event_loop.thread = INVALID_THREAD_HANDLE;
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

    // Depending on the type of frame, we route to different send functions.
    switch ((frame->id & ARTIE_CAN_FRAME_ID_FRAME_TYPE_MASK) >> ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION)
    {
        case ARTIE_CAN_RTACP_PROTOCOL_ID:
            rtacp_send(handle, frame);
            break;
        default:
            // Invalid frame ID type
            return ARTIE_CAN_ERR_INVALID_ARG;
    }
}

artie_can_error_t artie_can_tick(artie_can_backend_t *handle)
{
    if (handle == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (handle->context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else
    {
        // No valid protocol configured
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // For each protocol that this node is configured to use,
    // call that state machine's tick function.
    artie_can_error_t err = ARTIE_CAN_ERR_NONE;
    for (size_t i = 0; i < ARTIE_CAN_PROTOCOL_COUNT; i++)
    {
        if ((handle->context->protocol_flags & (1 << i)) != 0)
        {
            switch (i)
            {
                case ARTIE_CAN_PROTOCOL_FLAG_RTACP:
                    err |= rtacp_tick(handle);
                    break;
                case ARTIE_CAN_PROTOCOL_FLAG_RPCACP:
                    // TODO
                    break;
                case ARTIE_CAN_PROTOCOL_FLAG_PSACP:
                    // TODO
                    break;
                case ARTIE_CAN_PROTOCOL_FLAG_BWACP:
                    // TODO
                    break;
                default:
                    // Invalid protocol ID in flags
                    return ARTIE_CAN_ERR_INVALID_ARG;
            }
        }
    }

    return err;
}

/**
 * @brief Thread function for the event loop.
 * This function runs in a separate thread and calls artie_can_tick() periodically.
 */
#ifdef _WIN32
static DWORD WINAPI event_loop_thread_func(LPVOID arg)
#else
static void* event_loop_thread_func(void* arg)
#endif
{
    artie_can_backend_t *handle = (artie_can_backend_t *)arg;

    if (handle == NULL || handle->context == NULL)
    {
#ifdef _WIN32
        return 1;
#else
        return NULL;
#endif
    }

    artie_can_context_t *context = handle->context;

    // Run the event loop until signaled to stop
    while (context->event_loop.running)
    {
        // Call the tick function to process events
        artie_can_tick(handle);

        // Sleep for the specified interval
        SLEEP_US(context->event_loop.tick_interval_us);
    }

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

artie_can_error_t artie_can_start_event_loop(artie_can_backend_t *handle, uint32_t tick_interval_us)
{
    if (handle == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (handle->context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (tick_interval_us == 0)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Check if the event loop is already running
    if (handle->context->event_loop.thread != INVALID_THREAD_HANDLE)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;  // Already running
    }

    // Store the tick interval
    handle->context->event_loop.tick_interval_us = tick_interval_us;

    // Set the running flag before starting the thread
    handle->context->event_loop.running = true;

    // Create the event loop thread
    if (!create_thread(&handle->context->event_loop.thread, event_loop_thread_func, handle))
    {
        handle->context->event_loop.running = false;
        handle->context->event_loop.thread = INVALID_THREAD_HANDLE;
        return ARTIE_CAN_ERR_INIT_FAIL;  // Thread creation failed
    }

    return ARTIE_CAN_ERR_NONE;
}
