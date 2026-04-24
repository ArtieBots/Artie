#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "backend.h"
#include "context.h"
#include "err.h"
#include "frame.h"
#include "rtacp.h"

artie_can_error_t artie_can_init_context_rtacp(artie_can_context_t *context, uint8_t node_address)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (node_address > (ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK >> ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION))
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (node_address == ARTIE_CAN_RTACP_TARGET_ADDRESS_BROADCAST)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Fill in the RTACP details and enable the RTACP protocol for this node
    context->rtacp_context.node_address = node_address;
    context->protocol_flags |= ARTIE_CAN_PROTOCOL_FLAG_RTACP;

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_rtacp_init_frame(artie_can_frame_t *out, const artie_can_frame_rtacp_t *in)
{
    if (out == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (in == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (in->nbytes > ARTIE_CAN_RTACP_MAX_DATA_BYTES)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if ((in->nbytes > 0) && (in->data == NULL))
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (out->data == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    out->id = ((uint32_t)(ARTIE_CAN_RTACP_PROTOCOL_ID) << ARTIE_CAN_FRAME_ID_PROTOCOL_LOCATION) |
              ((uint32_t)(in->ack ? 1 : 0) << ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION) |
              ((uint32_t)(in->priority) << ARTIE_CAN_FRAME_ID_USER_PRIORITY_LOCATION) |
              ((uint32_t)(in->source_address) << ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION) |
              ((uint32_t)(in->target_address) << ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION);
    out->dlc = in->nbytes;
    memcpy(out->data, in->data, in->nbytes);

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_rtacp_parse_frame(const artie_can_frame_t *in, artie_can_frame_rtacp_t *out)
{
    if (in == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (out == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (in->dlc > ARTIE_CAN_RTACP_MAX_DATA_BYTES)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if ((in->dlc > 0) && (in->data == NULL))
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (out->data == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    out->ack = ((in->id & ARTIE_CAN_FRAME_ID_FRAME_TYPE_MASK) >> ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION) == 1 ? true : false;
    out->priority = (artie_can_frame_priority_rtacp_t)((in->id & ARTIE_CAN_FRAME_ID_USER_PRIORITY_MASK) >> ARTIE_CAN_FRAME_ID_USER_PRIORITY_LOCATION);
    out->source_address = (uint8_t)((in->id & ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK) >> ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION);
    out->target_address = (uint8_t)((in->id & ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK) >> ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION);
    out->nbytes = in->dlc;
    memcpy(out->data, in->data, in->dlc);

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t rtacp_send(artie_can_backend_t *handle, const artie_can_frame_t *frame)
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
    else if (handle->context->rtacp_context.state == RTACP_STATE_WAITING_ACK)
    {
        // We are already waiting for an ACK for a previously sent frame,
        // we can't send another frame until we get the ACK back or timeout
        return ARTIE_CAN_ERR_SEND_BUSY;
    }

    artie_can_error_t err;
    err = handle->send(handle->context, frame);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }
    else if (((frame->id & ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK) >> ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION) == ARTIE_CAN_RTACP_TARGET_ADDRESS_BROADCAST)
    {
        // If the frame is a broadcast frame, we are done.
        return ARTIE_CAN_ERR_NONE;
    }
    else
    {
        // Otherwise, we wait for the ACK.
        handle->context->rtacp_context.state = RTACP_STATE_WAITING_ACK;
        return ARTIE_CAN_ERR_NONE;
    }
}

void rtacp_receive_in_isr(artie_can_context_t *context, const artie_can_frame_t *frame)
{
    // TODO
}

artie_can_error_t rtacp_tick(artie_can_backend_t *handle)
{
    if (handle == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (handle->context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
}
