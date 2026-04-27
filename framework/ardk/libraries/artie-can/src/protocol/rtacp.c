#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "backend.h"
#include "context.h"
#include "err.h"
#include "frame.h"
#include "log.h"
#include "rtacp.h"

/** Check if we have timed out waiting for an ACK. */
static artie_can_error_t _check_ack_timeout(artie_can_backend_t *handle)
{
    uint64_t current_time_ms = handle->get_ms();
    if ((current_time_ms - handle->context->rtacp_context.ack_start_time_ms) >= (uint64_t)ARTIE_CAN_RTACP_ACK_TIMEOUT_MS)
    {
        // Timeout occurred, reset to idle and return an error
        ARTIE_CAN_LOG(handle->context, "ACK timeout occurred, resetting state to idle.\n");
        handle->context->rtacp_context.state = RTACP_STATE_IDLE;
        handle->context->rtacp_context.ack_start_time_ms = 0;
        memset(&handle->context->rtacp_context.in_flight_frame, 0, sizeof(artie_can_frame_t));
        return ARTIE_CAN_ERR_TIMEOUT;
    }

    return ARTIE_CAN_ERR_NONE;
}

/** Send the pending ACK and call back once we are sure it happened. */
static artie_can_error_t _send_pending_ack(artie_can_backend_t *handle)
{
    ARTIE_CAN_LOG(handle->context, "Attempting to send pending ACK frame.\n");

    artie_can_error_t err;
    err = handle->send(handle->context, &handle->context->rtacp_context.ack_frame);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        ARTIE_CAN_LOG(handle->context, "Failed to send ACK frame, error code %d.\n", err);
        return err;
    }
    else
    {
        // Reconstruct the original frame that we ACKed from the ACK frame buffer so that we can pass it to the callback function.
        artie_can_frame_t acked_frame;
        memcpy(&acked_frame, &handle->context->rtacp_context.ack_frame, sizeof(artie_can_frame_t));
        acked_frame.id &= ~(uint32_t)ARTIE_CAN_FRAME_ID_FRAME_TYPE_MASK; // Clear the ACK bit to get the original frame ID
        acked_frame.id &= ~(uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK; // Clear the sender address bits
        acked_frame.id &= ~(uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK; // Clear the target address bits
        acked_frame.id |= ((uint32_t)ARTIE_CAN_FRAME_TYPE_RTACP_DATA << (uint32_t)ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION);
        acked_frame.id |= ((((uint32_t)handle->context->rtacp_context.ack_frame.id & (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION) << ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION);
        acked_frame.id |= ((((uint32_t)handle->context->rtacp_context.ack_frame.id & (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION) << ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION);

        // Now call the callback to let the user know that we received a frame that we ACKed.
        ARTIE_CAN_LOG(handle->context, "Successfully sent ACK frame, calling callback with original frame.\n");
        handle->context->rx_callback(&acked_frame);

        // After sending the ACK, we are done with it, so we can clear the ACK frame buffer and reset to idle.
        memset(&handle->context->rtacp_context.ack_frame, 0, sizeof(artie_can_frame_t));
        handle->context->rtacp_context.state = RTACP_STATE_IDLE;
        return ARTIE_CAN_ERR_NONE;
    }
}

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

    out->id = ((uint32_t)(ARTIE_CAN_RTACP_PROTOCOL_ID) << (uint32_t)ARTIE_CAN_FRAME_ID_PROTOCOL_LOCATION) |
              ((uint32_t)(in->ack ? ARTIE_CAN_FRAME_TYPE_RTACP_ACK : ARTIE_CAN_FRAME_TYPE_RTACP_DATA) << (uint32_t)ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION) |
              ((uint32_t)(in->priority) << (uint32_t)ARTIE_CAN_FRAME_ID_USER_PRIORITY_LOCATION) |
              ((uint32_t)(in->source_address) << (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION) |
              ((uint32_t)(in->target_address) << (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION);
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

    out->ack = ((in->id & (uint32_t)ARTIE_CAN_FRAME_ID_FRAME_TYPE_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION) == ARTIE_CAN_FRAME_TYPE_RTACP_ACK;
    out->priority = (artie_can_frame_priority_rtacp_t)((in->id & (uint32_t)ARTIE_CAN_FRAME_ID_USER_PRIORITY_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_USER_PRIORITY_LOCATION);
    out->source_address = (uint8_t)((in->id & (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION);
    out->target_address = (uint8_t)((in->id & (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION);
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

    // If we are waiting to send an ACK, we need to send that first before we can send the new frame.
    if (handle->context->rtacp_context.state == RTACP_STATE_SENDING_ACK)
    {
        err = _send_pending_ack(handle);
        if (err != ARTIE_CAN_ERR_NONE)
        {
            return err;
        }
    }

    // Now send the requested frame
    ARTIE_CAN_LOG(handle->context, "Attempting to send frame with dest addr %u and priority %u\n", ((frame->id & (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION), ((frame->id & (uint32_t)ARTIE_CAN_FRAME_ID_USER_PRIORITY_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_USER_PRIORITY_LOCATION));
    err = handle->send(handle->context, frame);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        ARTIE_CAN_LOG(handle->context, "Failed to send frame, error code %d.\n", err);
        return err;
    }
    else if (((frame->id & (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION) == (uint32_t)ARTIE_CAN_RTACP_TARGET_ADDRESS_BROADCAST)
    {
        // If the frame is a broadcast frame, we are done.
        return ARTIE_CAN_ERR_NONE;
    }
    else
    {
        // Otherwise, we wait for the ACK.
        memcpy(&handle->context->rtacp_context.in_flight_frame, frame, sizeof(artie_can_frame_t));
        handle->context->rtacp_context.ack_start_time_ms = handle->get_ms();
        handle->context->rtacp_context.state = RTACP_STATE_WAITING_ACK;
        return ARTIE_CAN_ERR_NONE;
    }
}

void rtacp_receive_in_isr(artie_can_context_t *context, const artie_can_frame_t *frame)
{
    // We assume that the backend only calls this function for frames that match the RTACP protocol ID in their frame ID.
    // We further assume that the context and frame pointers are valid and that the frame data is well-formed.

    if ((context->protocol_flags & ARTIE_CAN_PROTOCOL_FLAG_RTACP) == 0)
    {
        // This node is not configured to use the RTACP protocol, ignore the frame.
        ARTIE_CAN_LOG(context, "Received RTACP frame but this node is not configured for RTACP, ignoring.\n");
        return;
    }

    // Get the frame type from the frame buffer
    artie_can_frame_type_rtacp_t frame_type = (artie_can_frame_type_rtacp_t)((frame->id & (uint32_t)ARTIE_CAN_FRAME_ID_FRAME_TYPE_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION);

    // If this is an ACK, do one thing; if it is a MSG, do another
    if ((frame_type == ARTIE_CAN_FRAME_TYPE_RTACP_ACK))
    {
        // Are we waiting for an ACK?
        if (context->rtacp_context.state != RTACP_STATE_WAITING_ACK)
        {
            ARTIE_CAN_LOG(context, "Received ACK frame but we are not waiting for an ACK, ignoring.\n");
            return;
        }

        // Check if we are the destination of the ACK.
        if (((frame->id & (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION) != context->rtacp_context.node_address)
        {
            ARTIE_CAN_LOG(context, "Received ACK frame but it is not addressed to us, ignoring.\n");
            return;
        }

        // Check if sender address is the destination address of the message we are waiting on.
        uint8_t dest_addr = ((context->rtacp_context.in_flight_frame.id & (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION);
        if (((frame->id & (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION) != dest_addr)
        {
            ARTIE_CAN_LOG(context, "Received ACK frame but sender address does not match destination address of in-flight message, ignoring.\n");
            return;
        }

        // Check if the data matches the data we are waiting on.
        if ((frame->dlc != context->rtacp_context.in_flight_frame.dlc) || (memcmp(frame->data, context->rtacp_context.in_flight_frame.data, sizeof(context->rtacp_context.in_flight_frame.dlc)) != 0))
        {
            ARTIE_CAN_LOG(context, "Received ACK frame but data does not match in-flight message, ignoring.\n");
            return;
        }

        // If we have made it through the gauntlet, we can reset our state, because the ACK checks out.
        ARTIE_CAN_LOG(context, "Received valid ACK frame, resetting state and calling callback.\n");
        memset(&context->rtacp_context.in_flight_frame, 0, sizeof(artie_can_frame_t));
        context->rtacp_context.ack_start_time_ms = 0;
        context->rtacp_context.state = RTACP_STATE_IDLE;
        return;
    }
    else
    {
        uint8_t address = (uint8_t)((frame->id & (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION);
        if (address == ARTIE_CAN_RTACP_TARGET_ADDRESS_BROADCAST)
        {
            // No need for ACK. Call the call-back function from ISR context with the frame.
            ARTIE_CAN_LOG(context, "Received broadcast frame, calling callback from ISR context.\n");
            context->rx_callback(frame);
            return;
        }
        else if (address == context->rtacp_context.node_address)
        {
            ARTIE_CAN_LOG(context, "Received frame addressed to us, preparing ACK to be sent at next tick.\n");
            // This frame is addressed to a specific node and that node is us. We need to ACK it.
            // Do so by copying the frame into the ACK frame buffer in our context,
            // setting the appropriate fields, and then setting our state machine to send the ACK from the main thread context.
            memcpy(&context->rtacp_context.ack_frame, frame, sizeof(artie_can_frame_t));
            // Swap the sender and the target in the frame ID for the ACK
            uint8_t sender_addr = (uint8_t)((frame->id & (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION);
            context->rtacp_context.ack_frame.id &= ~(uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK;
            context->rtacp_context.ack_frame.id &= ~(uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK;
            context->rtacp_context.ack_frame.id |= (((uint32_t)context->rtacp_context.node_address << (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION) & (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK);
            context->rtacp_context.ack_frame.id |= (((uint32_t)sender_addr << (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION) & (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK);
            // Set the ACK bit in the frame ID
            context->rtacp_context.ack_frame.id |= ((uint32_t)ARTIE_CAN_FRAME_TYPE_RTACP_ACK << (uint32_t)ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION);
            context->rtacp_context.state = RTACP_STATE_SENDING_ACK;
            return;
        }
    }
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

    // Act according to state machine state
    switch (handle->context->rtacp_context.state)
    {
        case RTACP_STATE_IDLE:
            // Nothing to do in the idle state
            return ARTIE_CAN_ERR_NONE;
        case RTACP_STATE_WAITING_ACK:
            // Check if we've timed out waiting for the ACK. If so, reset to idle and return an error.
            return _check_ack_timeout(handle);
        case RTACP_STATE_SENDING_ACK:
            // We need to send the ACK frame that we prepared in the ISR context. Do so and then reset to idle.
            return _send_pending_ack(handle);
        default:
            // Invalid state, reset to idle. Return an error.
            handle->context->rtacp_context.state = RTACP_STATE_IDLE;
            return ARTIE_CAN_ERR_INTERNAL;
    }
}
