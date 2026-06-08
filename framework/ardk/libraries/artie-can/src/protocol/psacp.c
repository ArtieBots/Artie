/**
 * @file psacp.c
 * @brief Implementation of PSACP (Pub/Sub Artie CAN Protocol).
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "backend.h"
#include "context.h"
#include "err.h"
#include "frame.h"
#include "log.h"
#include "psacp.h"
#include "psacp_context.h"
#include "util.h"

/**
 * @brief Check whether this node is subscribed to the given topic.
 * Broadcast (topic 0x00) always returns true.
 */
static bool _is_subscribed(const psacp_context_t *ctx, uint8_t topic)
{
    if (topic == ARTIE_CAN_PSACP_TOPIC_BROADCAST)
    {
        return true;
    }

    for (uint8_t i = 0; i < ctx->subscribed_topic_count; i++)
    {
        if (ctx->subscribed_topics[i] == topic)
        {
            return true;
        }
    }

    return false;
}

artie_can_error_t artie_can_init_context_psacp(artie_can_context_t *ctx, uint8_t node_address)
{
    if (ctx == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (node_address == 0U)
    {
        // Address 0 is reserved for broadcast; nodes must have a non-zero address
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (node_address > (ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK >> ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION))
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    ctx->psacp_context.node_address = node_address;
    ctx->psacp_context.subscribed_topic_count = 0;
    memset(ctx->psacp_context.subscribed_topics, 0, sizeof(ctx->psacp_context.subscribed_topics));

    ctx->protocol_flags |= ARTIE_CAN_PROTOCOL_FLAG_PSACP;
    ctx->node_address = node_address;

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_psacp_subscribe(artie_can_context_t *ctx, uint8_t topic)
{
    if (ctx == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Broadcast is implicit - no need to subscribe explicitly
    if (topic == ARTIE_CAN_PSACP_TOPIC_BROADCAST)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Check if already subscribed
    for (uint8_t i = 0; i < ctx->psacp_context.subscribed_topic_count; i++)
    {
        if (ctx->psacp_context.subscribed_topics[i] == topic)
        {
            // Already subscribed, nothing to do
            return ARTIE_CAN_ERR_NONE;
        }
    }

    // Check capacity
    if (ctx->psacp_context.subscribed_topic_count >= ARTIE_CAN_PSACP_MAX_SUBSCRIPTIONS)
    {
        return ARTIE_CAN_ERR_NO_SPACE;
    }

    ctx->psacp_context.subscribed_topics[ctx->psacp_context.subscribed_topic_count] = topic;
    ctx->psacp_context.subscribed_topic_count++;

    ARTIE_CAN_LOG(ctx, "PSACP: Subscribed to topic 0x%02X (total subscriptions: %u)\n", topic, ctx->psacp_context.subscribed_topic_count);

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_psacp_unsubscribe(artie_can_context_t *ctx, uint8_t topic)
{
    if (ctx == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    for (uint8_t i = 0; i < ctx->psacp_context.subscribed_topic_count; i++)
    {
        if (ctx->psacp_context.subscribed_topics[i] == topic)
        {
            // Remove by swapping with the last element
            ctx->psacp_context.subscribed_topics[i] = ctx->psacp_context.subscribed_topics[ctx->psacp_context.subscribed_topic_count - 1U];
            ctx->psacp_context.subscribed_topic_count--;

            ARTIE_CAN_LOG(ctx, "PSACP: Unsubscribed from topic 0x%02X (total subscriptions: %u)\n", topic, ctx->psacp_context.subscribed_topic_count);

            return ARTIE_CAN_ERR_NONE;
        }
    }

    return ARTIE_CAN_ERR_NO_DATA;
}

artie_can_error_t artie_can_psacp_init_frame(artie_can_frame_t *out, const artie_can_frame_psacp_t *in)
{
    if (out == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (in == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (in->nbytes > ARTIE_CAN_PSACP_MAX_DATA_BYTES)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    uint8_t protocol_id = in->high_priority ? ARTIE_CAN_PSACP_HIGH_PROTOCOL_ID : ARTIE_CAN_PSACP_LOW_PROTOCOL_ID;

    out->id = ((uint32_t)protocol_id << (uint32_t)ARTIE_CAN_FRAME_ID_PROTOCOL_LOCATION) |
              ((uint32_t)ARTIE_CAN_PSACP_FRAME_TYPE_PUB << (uint32_t)ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION) |
              ((uint32_t)in->priority << (uint32_t)ARTIE_CAN_FRAME_ID_USER_PRIORITY_LOCATION) |
              ((uint32_t)in->source_address << (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION) |
              ((uint32_t)in->topic << (uint32_t)PSACP_FRAME_ID_TOPIC_LOCATION);

    out->dlc = in->nbytes;
    memcpy(out->data, in->data, in->nbytes);

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_psacp_parse_frame(const artie_can_frame_t *in, artie_can_frame_psacp_t *out)
{
    if (in == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (out == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (in->dlc > ARTIE_CAN_PSACP_MAX_DATA_BYTES)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    uint8_t protocol_id = (uint8_t)((in->id & (uint32_t)ARTIE_CAN_FRAME_ID_PROTOCOL_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_PROTOCOL_LOCATION);
    if ((protocol_id != ARTIE_CAN_PSACP_HIGH_PROTOCOL_ID) && (protocol_id != ARTIE_CAN_PSACP_LOW_PROTOCOL_ID))
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    out->high_priority = (protocol_id == ARTIE_CAN_PSACP_HIGH_PROTOCOL_ID);
    out->priority = (artie_can_frame_priority_psacp_t)((in->id & (uint32_t)ARTIE_CAN_FRAME_ID_USER_PRIORITY_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_USER_PRIORITY_LOCATION);
    out->source_address = (uint8_t)((in->id & (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK) >> (uint32_t)ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION);
    out->topic = (uint8_t)((in->id & (uint32_t)PSACP_FRAME_ID_TOPIC_MASK) >> (uint32_t)PSACP_FRAME_ID_TOPIC_LOCATION);
    out->nbytes = in->dlc;
    memcpy(out->data, in->data, in->dlc);

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_psacp_publish(artie_can_backend_t *handle, const artie_can_frame_t *frame)
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
    else if (handle->context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    uint8_t topic = (uint8_t)((frame->id & (uint32_t)PSACP_FRAME_ID_TOPIC_MASK) >> (uint32_t)PSACP_FRAME_ID_TOPIC_LOCATION);

    ARTIE_CAN_LOG(handle->context, "PSACP: Publishing to topic 0x%02X (%u bytes)\n", topic, frame->dlc);

    artie_can_error_t err = artie_can_send_with_retry(handle, frame);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        ARTIE_CAN_LOG(handle->context, "PSACP: Failed to publish frame, error code %d.\n", err);
        return err;
    }

    // The UDP backend (and hardware backends) do not echo frames back to the sender,
    // so we handle local delivery here: if we are subscribed to this topic (or it is broadcast),
    // invoke the rx_callback as if we had received the message.
    if ((handle->context->protocol_flags & ARTIE_CAN_PROTOCOL_FLAG_PSACP) != 0)
    {
        if (_is_subscribed(&handle->context->psacp_context, topic))
        {
            ARTIE_CAN_LOG(handle->context, "PSACP: Local delivery for topic 0x%02X (node is subscribed)\n", topic);
            handle->context->rx_callback(frame);
        }
    }

    return ARTIE_CAN_ERR_NONE;
}

// !! Make sure all code in this function interacts with the rest of the code in a re-entrant manner !!
void psacp_receive_in_isr(artie_can_context_t *context, const artie_can_frame_t *frame)
{
    if ((context->protocol_flags & ARTIE_CAN_PROTOCOL_FLAG_PSACP) == 0)
    {
        // This node is not configured to use PSACP, ignore the frame.
        ARTIE_CAN_LOG(context, "PSACP: Received PSACP frame but this node is not configured for PSACP, ignoring.\n");
        return;
    }

    uint8_t topic = (uint8_t)((frame->id & (uint32_t)PSACP_FRAME_ID_TOPIC_MASK) >> (uint32_t)PSACP_FRAME_ID_TOPIC_LOCATION);

    if (_is_subscribed(&context->psacp_context, topic))
    {
        ARTIE_CAN_LOG(context, "PSACP: Received frame for topic 0x%02X, calling callback.\n", topic);
        context->rx_callback(frame);
    }
    else
    {
        ARTIE_CAN_LOG(context, "PSACP: Received frame for topic 0x%02X but not subscribed, ignoring.\n", topic);
    }
}

artie_can_error_t psacp_tick(artie_can_backend_t *handle)
{
    if (handle == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // PSACP is fire-and-forget with no state machine, nothing to do here.
    return ARTIE_CAN_ERR_NONE;
}
