/**
 * @file circular_buffer.c
 * @brief Implementation of circular buffer for Artie CAN frames.
 *
 */

#include "circular_buffer.h"
#include <string.h>

/** Static buffer storage for frames */
static artie_can_frame_t _buffer[ARTIE_CAN_TCP_BUFFER_N_FRAMES];

/** Read index (head of the buffer) */
static uint32_t _read_index = 0;

/** Write index (tail of the buffer) */
static uint32_t _write_index = 0;

/** Number of frames currently in the buffer */
static uint32_t _count = 0;

size_t cb_get_count(void)
{
    return _count;
}

artie_can_error_t cb_read(artie_can_frame_t *out)
{
    if (out == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Check if buffer is empty
    if (_count == 0)
    {
        return ARTIE_CAN_ERR_NO_DATA;
    }

    // Byte-by-byte copy the frame from the buffer
    memcpy(out, &_buffer[_read_index], sizeof(artie_can_frame_t));

    // Update read index (wrap around if necessary)
    _read_index = (_read_index + 1) % ARTIE_CAN_TCP_BUFFER_N_FRAMES;

    // Decrement count
    _count--;

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t cb_write(const artie_can_frame_t *in)
{
    if (in == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Check if buffer is full
    if (_count >= ARTIE_CAN_TCP_BUFFER_N_FRAMES)
    {
        return ARTIE_CAN_NO_SPACE;
    }

    // Byte-by-byte copy the frame into the buffer
    memcpy(&_buffer[_write_index], in, sizeof(artie_can_frame_t));

    // Update write index (wrap around if necessary)
    _write_index = (_write_index + 1) % ARTIE_CAN_TCP_BUFFER_N_FRAMES;

    // Increment count
    _count++;

    return ARTIE_CAN_ERR_NONE;
}
