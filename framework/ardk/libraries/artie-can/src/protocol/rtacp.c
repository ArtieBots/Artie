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
        ARTIE_CAN_LOG(handle->context, "RTACP: ACK timeout occurred, resetting state to idle.\n");
        handle->context->rtacp_context.state = RTACP_STATE_IDLE;
        handle->context->rtacp_context.ack_start_time_ms = 0;
        memset(&handle->context->rtacp_context.in_flight_frame, 0, sizeof(artie_can_frame_t));
        return ARTIE_CAN_ERR_TIMEOUT;
    }

    return ARTIE_CAN_ERR_NONE;
}

/** Send the pending ACK and call back once we are sure it happened. */
static artie_can_error_t _send_pending_ack(artie_can_backend_t *handle, size_t ack_buffer_index)
{
    ARTIE_CAN_LOG(handle->context, "RTACP: Attempting to send pending ACK frame from buffer index %zu.\n", ack_buffer_index);

    artie_can_error_t err;
    artie_can_frame_t *ack_frame = (ack_buffer_index == 0) ? &handle->context->rtacp_context.ack_frame0 : &handle->context->rtacp_context.ack_frame1;

    err = handle->send(handle->context, ack_frame);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        ARTIE_CAN_LOG(handle->context, "RTACP: Failed to send ACK frame, error code %d.\n", err);
        return err;
    }
    else
    {
        // After sending the ACK, we are done with it, so we can clear the ACK frame buffer.
        // Even though the ISR owns the ack frame buffer, it is locked out of it by the interrupt flag
        // in the context struct until we clear that flag, which we don't do until we return from this function.
        ARTIE_CAN_LOG(handle->context, "RTACP: Clearing ACK frame buffer and resetting state to idle.\n");
        memset(ack_frame, 0, sizeof(artie_can_frame_t));
        return ARTIE_CAN_ERR_NONE;
    }
}

static void _process_received_ack(artie_can_backend_t *handle)
{
    // Check if we are waiting for an ACK
    if (handle->context->rtacp_context.state != RTACP_STATE_WAITING_ACK)
    {
        ARTIE_CAN_LOG(handle->context, "RTACP: Received ACK frame but we are not waiting for an ACK, ignoring.\n");
        return;
    }

    // Check if the ACK is for the frame we are waiting for:
    // Sender is destination of in-flight frame?
    uint8_t dest_addr = ((handle->context->rtacp_context.in_flight_frame.id & (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION);
    if (((handle->context->rtacp_context.received_ack.id & (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION) != dest_addr)
    {
        ARTIE_CAN_LOG(handle->context, "RTACP: Received ACK frame but sender address does not match destination address of in-flight message, ignoring.\n");
        return;
    }

    // Data matches what we are waiting on?
    if ((handle->context->rtacp_context.received_ack.dlc != handle->context->rtacp_context.in_flight_frame.dlc) || (memcmp(handle->context->rtacp_context.received_ack.data, handle->context->rtacp_context.in_flight_frame.data, sizeof(handle->context->rtacp_context.in_flight_frame.dlc)) != 0))
    {
        ARTIE_CAN_LOG(handle->context, "RTACP: Received ACK frame but data does not match in-flight message, ignoring.\n");
        return;
    }

    // Valid and expected ACK frame
    ARTIE_CAN_LOG(handle->context, "RTACP: Received valid ACK frame, resetting state and calling callback.\n");
    memset(&handle->context->rtacp_context.in_flight_frame, 0, sizeof(artie_can_frame_t));
    handle->context->rtacp_context.ack_start_time_ms = 0;
    handle->context->rtacp_context.state = RTACP_STATE_IDLE;

    // Memset the received ACK buffer. Normally the ISR owns this buffer, but we have locked them out
    // by means of the interrupt flag until we clear that flag when we return from this function.
    memset(&handle->context->rtacp_context.received_ack, 0, sizeof(artie_can_frame_t));
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
    context->node_address = node_address;

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

artie_can_error_t artie_can_rtacp_send(artie_can_backend_t *handle, const artie_can_frame_t *frame)
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
        ARTIE_CAN_LOG(handle->context, "RTACP: Cannot send frame, already waiting for ACK.\n");
        return ARTIE_CAN_ERR_SEND_BUSY;
    }

    artie_can_error_t err;

    // Now send the requested frame
    ARTIE_CAN_LOG(handle->context, "RTACP: Attempting to send frame with dest addr %u and priority %u; setting state to sending\n", ((frame->id & (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION), ((frame->id & (uint32_t)ARTIE_CAN_FRAME_ID_USER_PRIORITY_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_USER_PRIORITY_LOCATION));
    memcpy(&handle->context->rtacp_context.in_flight_frame, frame, sizeof(artie_can_frame_t)); // Set this before we send, in case we get the ACK before we are ready for it
    err = handle->send(handle->context, frame);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        ARTIE_CAN_LOG(handle->context, "RTACP: Failed to send frame, error code %d.\n", err);
        return err;
    }
    else if (((frame->id & (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION) == (uint32_t)ARTIE_CAN_RTACP_TARGET_ADDRESS_BROADCAST)
    {
        // If the frame is a broadcast frame, we are done.
        return ARTIE_CAN_ERR_NONE;
    }
    else
    {
        ARTIE_CAN_LOG(handle->context, "RTACP: Sent frame, now waiting for ACK.\n");
        handle->context->rtacp_context.state = RTACP_STATE_WAITING_ACK;
        handle->context->rtacp_context.ack_start_time_ms = handle->get_ms();
        return ARTIE_CAN_ERR_NONE;
    }
}

// !! Make sure all code in this function interacts with the rest of the code in a re-entrant manner !!
// In general we do this by only writing to items in the context that are meant to be owned by the ISR.
// Also, don't read anything from the context that might be changed on the main thread at any moment, such as the state machine state.
void rtacp_receive_in_isr(artie_can_context_t *context, const artie_can_frame_t *frame)
{
    // We assume that the backend only calls this function for frames that match the RTACP protocol ID in their frame ID.
    // We further assume that the context and frame pointers are valid and that the frame data is well-formed.

    if ((context->protocol_flags & ARTIE_CAN_PROTOCOL_FLAG_RTACP) == 0)
    {
        // This node is not configured to use the RTACP protocol, ignore the frame.
        ARTIE_CAN_LOG(context, "RTACP: Received RTACP frame but this node is not configured for RTACP, ignoring.\n");
        return;
    }

    // Get the frame type from the frame buffer
    artie_can_frame_type_rtacp_t frame_type = (artie_can_frame_type_rtacp_t)((frame->id & (uint32_t)ARTIE_CAN_FRAME_ID_FRAME_TYPE_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION);

    // If this is an ACK, do one thing; if it is a MSG, do another
    if ((frame_type == ARTIE_CAN_FRAME_TYPE_RTACP_ACK))
    {
        // Check if we are the destination of the ACK.
        if (((frame->id & (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION) != context->rtacp_context.node_address)
        {
            ARTIE_CAN_LOG(context, "RTACP: Received ACK frame but it is not addressed to us, ignoring.\n");
            return;
        }
        else if ((context->rtacp_context.isr_flags & RTACP_ISR_FLAG_PENDING_ACK_RX) != 0)
        {
            // We already have a pending ACK that we received and haven't processed yet,
            // so we shouldn't overwrite it with a new one until we process the first one.
            // Also, this generally means that one of the ACKs is wrongly addressed to us.
            ARTIE_CAN_LOG(context, "RTACP: Received ACK frame but we already have a pending ACK to process, ignoring.\n");
            return;
        }
        else
        {
            // Got an ACK that is addressed to us. Handle from the main thread.
            memcpy(&context->rtacp_context.received_ack, frame, sizeof(artie_can_frame_t));

            // Atomically set the pending ACK RX flag
            atomic_fetch_or(&context->rtacp_context.isr_flags, (uint32_t)RTACP_ISR_FLAG_PENDING_ACK_RX);
            return;
        }
    }
    else
    {
        uint8_t address = (uint8_t)((frame->id & (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION);
        if (address == ARTIE_CAN_RTACP_TARGET_ADDRESS_BROADCAST)
        {
            // No need for ACK. Call the call-back function from ISR context with the frame.
            ARTIE_CAN_LOG(context, "RTACP: Received broadcast frame, calling callback from ISR context.\n");
            context->rx_callback(frame);
            return;
        }
        else if (address == context->rtacp_context.node_address)
        {
            // Only receive this message into the ack frame buffer if we don't already have a message we are pending on.
            artie_can_frame_t *ack_buffer = NULL;
            rtacp_isr_flags_t flag = 0;
            if ((context->rtacp_context.isr_flags & RTACP_ISR_FLAG_PENDING_ACK_TX0) && (context->rtacp_context.isr_flags & RTACP_ISR_FLAG_PENDING_ACK_TX1))
            {
                ARTIE_CAN_LOG(context, "RTACP: Received frame addressed to us but we already have two pending ACKs to send, ignoring.\n");
                return;
            }
            else if ((context->rtacp_context.isr_flags & RTACP_ISR_FLAG_PENDING_ACK_TX0) == 0)
            {
                ARTIE_CAN_LOG(context, "RTACP: Received frame addressed to us. Assembling an ACK in buffer 0.\n");
                ack_buffer = &context->rtacp_context.ack_frame0;
                flag = RTACP_ISR_FLAG_PENDING_ACK_TX0;
            }
            else if ((context->rtacp_context.isr_flags & RTACP_ISR_FLAG_PENDING_ACK_TX1) == 0)
            {
                ARTIE_CAN_LOG(context, "RTACP: Received frame addressed to us. Assembling an ACK in buffer 1.\n");
                ack_buffer = &context->rtacp_context.ack_frame1;
                flag = RTACP_ISR_FLAG_PENDING_ACK_TX1;
            }

            if (ack_buffer != NULL)
            {
                // This frame is addressed to a specific node and that node is us. We need to ACK it.
                // Do so by copying the frame into the ACK frame buffer in our context,
                // setting the appropriate fields, and then setting our state machine to send the ACK from the main thread context.
                memcpy(ack_buffer, frame, sizeof(artie_can_frame_t));
                uint8_t sender_addr = (uint8_t)((frame->id & (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION);
                ack_buffer->id &= ~(uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK;
                ack_buffer->id &= ~(uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK;
                ack_buffer->id |= (((uint32_t)context->rtacp_context.node_address << (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION) & (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK);
                ack_buffer->id |= (((uint32_t)sender_addr << (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION) & (uint32_t)ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK);
                ack_buffer->id |= ((uint32_t)ARTIE_CAN_FRAME_TYPE_RTACP_ACK << (uint32_t)ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION);

                // Atomically set the pending ACK TX flag
                atomic_fetch_or(&context->rtacp_context.isr_flags, (uint32_t)flag);

                // Call the callback from ISR
                ARTIE_CAN_LOG(context, "RTACP: Calling callback from ISR context with received frame.\n");
                context->rx_callback(frame);
                return;
            }
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

    artie_can_error_t err = ARTIE_CAN_ERR_NONE;

    // Check the ISR flags
    if ((handle->context->rtacp_context.isr_flags & RTACP_ISR_FLAG_PENDING_ACK_RX) != 0)
    {
        // We received an ACK in the ISR that we need to process from the main thread context.
        // Clear the flag and process the ACK.
        ARTIE_CAN_LOG(handle->context, "RTACP: Processing received ACK from main thread context.\n");
        _process_received_ack(handle);

        // Atomically clear the pending ACK RX flag
        atomic_fetch_and(&handle->context->rtacp_context.isr_flags, (uint32_t)~RTACP_ISR_FLAG_PENDING_ACK_RX);
    }

    if ((handle->context->rtacp_context.isr_flags & RTACP_ISR_FLAG_PENDING_ACK_TX0) != 0)
    {
        // We have an ACK in buffer 0 that we need to send from the main thread context.
        // We clear the flag only once we have sent the ACK
        ARTIE_CAN_LOG(handle->context, "RTACP: Sending pending ACK from buffer 0 from main thread context.\n");
        err = _send_pending_ack(handle, 0);
        if (err != ARTIE_CAN_ERR_NONE)
        {
            return err;
        }

        // Atomically clear the pending ACK TX0 flag
        atomic_fetch_and(&handle->context->rtacp_context.isr_flags, (uint32_t)~RTACP_ISR_FLAG_PENDING_ACK_TX0);
    }

    if ((handle->context->rtacp_context.isr_flags & RTACP_ISR_FLAG_PENDING_ACK_TX1) != 0)
    {
        // We have an ACK in buffer 1 that we need to send from the main thread context.
        // We clear the flag only once we have sent the ACK
        ARTIE_CAN_LOG(handle->context, "RTACP: Sending pending ACK from buffer 1 from main thread context.\n");
        err = _send_pending_ack(handle, 1);
        if (err != ARTIE_CAN_ERR_NONE)
        {
            return err;
        }

        // Atomically clear the pending ACK TX1 flag
        atomic_fetch_and(&handle->context->rtacp_context.isr_flags, (uint32_t)~RTACP_ISR_FLAG_PENDING_ACK_TX1);
    }

    // Act according to state machine state
    switch (handle->context->rtacp_context.state)
    {
        case RTACP_STATE_IDLE:
            // Nothing to do in the idle state
            err = ARTIE_CAN_ERR_NONE;
            break;
        case RTACP_STATE_WAITING_ACK:
            // Check if we've timed out waiting for the ACK. If so, reset to idle and return an error.
            err = _check_ack_timeout(handle);
            break;
        default:
            // Invalid state
            handle->context->rtacp_context.state = RTACP_STATE_IDLE;
            err = ARTIE_CAN_ERR_INTERNAL;
            break;
    }

    return err;
}
