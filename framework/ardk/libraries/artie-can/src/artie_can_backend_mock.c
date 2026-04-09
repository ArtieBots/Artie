/**
 * @file artie_can_backend_mock.c
 * @brief Mock backend for testing with dead-end support
 */

#include "artie_can.h"
#include "artie_can_backends.h"
#include <string.h>
#include <stdlib.h>

static int mock_deadend_init(void *ctx)
{
    (void)ctx;
    return 0;
}

static int mock_deadend_send(void *ctx, const artie_can_frame_t *frame)
{
    (void)ctx;
    (void)frame;
    /* Discard the data - successful send to nowhere */
    return 0;
}

static int mock_deadend_receive(void *ctx, artie_can_frame_t *frame, uint32_t timeout_ms)
{
    (void)ctx;
    (void)frame;
    (void)timeout_ms;
    /* Always return no data available */
    return ARTIE_CAN_ERR_NO_DATA;
}

static int mock_deadend_close(void *ctx)
{
    (void)ctx;
    return 0;
}

int artie_can_backend_mock_init(artie_can_context_t *ctx, const void *config)
{
    if (!ctx) {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    ctx->backend.init = mock_deadend_init;
    ctx->backend.send = mock_deadend_send;
    ctx->backend.receive = mock_deadend_receive;
    ctx->backend.close = mock_deadend_close;
    ctx->backend.context = NULL;  /* No context needed for dead-end backend */

    return 0;
}
