/**
 * @file artie_can_backend_mock.c
 * @brief Mock backend for testing
 */

#include "artie_can.h"
#include "artie_can_backends.h"
#include <string.h>

#define MOCK_QUEUE_SIZE 32

typedef struct {
    artie_can_frame_t queue[MOCK_QUEUE_SIZE];
    size_t head;
    size_t tail;
    size_t count;
} mock_context_t;

static mock_context_t g_mock_ctx = {0};

static int mock_init(void *ctx)
{
    mock_context_t *mock = (mock_context_t *)ctx;
    mock->head = 0;
    mock->tail = 0;
    mock->count = 0;
    return 0;
}

static int mock_send(void *ctx, const artie_can_frame_t *frame)
{
    mock_context_t *mock = (mock_context_t *)ctx;

    if (mock->count >= MOCK_QUEUE_SIZE) {
        return -1;  /* Queue full */
    }

    /* Add to queue */
    memcpy(&mock->queue[mock->tail], frame, sizeof(artie_can_frame_t));
    mock->tail = (mock->tail + 1) % MOCK_QUEUE_SIZE;
    mock->count++;

    return 0;
}

static int mock_receive(void *ctx, artie_can_frame_t *frame, uint32_t timeout_ms)
{
    mock_context_t *mock = (mock_context_t *)ctx;

    (void)timeout_ms;  /* Ignore timeout in mock */

    if (mock->count == 0) {
        return -1;  /* Queue empty */
    }

    /* Remove from queue */
    memcpy(frame, &mock->queue[mock->head], sizeof(artie_can_frame_t));
    mock->head = (mock->head + 1) % MOCK_QUEUE_SIZE;
    mock->count--;

    return 0;
}

static int mock_close(void *ctx)
{
    (void)ctx;
    return 0;
}

int artie_can_backend_mock_init(artie_can_backend_t *backend)
{
    if (!backend) {
        return -1;
    }

    backend->init = mock_init;
    backend->send = mock_send;
    backend->receive = mock_receive;
    backend->close = mock_close;
    backend->context = &g_mock_ctx;

    return 0;
}
