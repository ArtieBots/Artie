/**
 * @file util.c
 * @brief General utility functions for Artie CAN library.
 *
 */

#include "util.h"
#include "log.h"
#include "translationlayer.h"

artie_can_error_t artie_can_send_with_retry(artie_can_backend_t *handle, const artie_can_frame_t *frame)
{
    if (handle == NULL || frame == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Try to send up to 10 times as long as the error is retriable
    artie_can_error_t err = ARTIE_CAN_ERR_NONE;
    for (int attempt = 0; attempt < 10; attempt++)
    {
        err = handle->send(handle->context, frame);
        if (err == ARTIE_CAN_ERR_NONE)
        {
            return ARTIE_CAN_ERR_NONE;
        }
        else if ((err == ARTIE_CAN_ERR_SEND_BUSY) || (err == ARTIE_CAN_ERR_TIMEOUT) || (err == ARTIE_CAN_ERR_SEND_FAIL))
        {
            // If the bus is busy, wait a bit and retry
            ARTIE_CAN_LOG(handle->context, "Send attempt %d failed with retriable error code %d, retrying...\n", attempt + 1, err);
            SLEEP_MS(1);
            continue;
        }
        else
        {
            // For other errors, log and return
            ARTIE_CAN_LOG(handle->context, "Failed to send frame (attempt %d): error code %d\n", attempt + 1, err);
            return err;
        }
    }

    return err;
}
