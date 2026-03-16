/**
 * @file artie_can_psacp.c
 * @brief Pub/Sub Artie CAN Protocol (PSACP) implementation
 */

#include "artie_can.h"
#include <string.h>

/* PSACP ID bit field layout for extended CAN frame (29 bits total):
 * Bits 28-26: Protocol (100 for high priority, 110 for low priority)
 * Bits 25-22: Frame type (00x1: 0001=PUB, 0011=DATA)
 * Bits 21-20: Priority (pp)
 * Bits 19-14: Sender address (ssssss)
 * Bits 13-6: Topic (tttttttt)
 * Bits 5-0: All 1s (0x3F)
 */

/**
 * @brief Build PSACP CAN ID
 */
static uint32_t psacp_build_can_id(bool high_priority, uint8_t frame_type,
                                   uint8_t priority, uint8_t sender_addr, uint8_t topic)
{
    uint32_t can_id = 0;

    /* Protocol bits (28-26) */
    if (high_priority) {
        can_id |= (ARTIE_CAN_PROTOCOL_PSACP_HIGH << ARTIE_CAN_ID_PROTOCOL_SHIFT);
    } else {
        can_id |= (ARTIE_CAN_PROTOCOL_PSACP_LOW << ARTIE_CAN_ID_PROTOCOL_SHIFT);
    }

    /* Frame type (25-22) */
    can_id |= (frame_type & ARTIE_CAN_MASK_FRAME_TYPE_4BIT) << ARTIE_CAN_ID_PSACP_FRAME_TYPE_SHIFT;

    /* Priority (21-20) */
    can_id |= (priority & ARTIE_CAN_MASK_PRIORITY) << ARTIE_CAN_ID_PSACP_PRIORITY_SHIFT;

    /* Sender address (19-14) */
    can_id |= (sender_addr & ARTIE_CAN_MASK_ADDRESS) << ARTIE_CAN_ID_PSACP_SENDER_SHIFT;

    /* Topic (13-6) */
    can_id |= (topic & ARTIE_CAN_MASK_TOPIC) << ARTIE_CAN_ID_PSACP_TOPIC_SHIFT;

    /* Bottom 6 bits all 1s */
    can_id |= ARTIE_CAN_PSACP_PADDING;

    return can_id;
}

/**
 * @brief Parse PSACP frame
 */
static void psacp_parse_can_id(uint32_t can_id, artie_can_psacp_msg_t *msg)
{
    uint8_t protocol = (can_id >> ARTIE_CAN_ID_PROTOCOL_SHIFT) & ARTIE_CAN_MASK_PROTOCOL;
    msg->high_priority = (protocol == ARTIE_CAN_PROTOCOL_PSACP_HIGH);

    msg->frame_type = (can_id >> ARTIE_CAN_ID_PSACP_FRAME_TYPE_SHIFT) & ARTIE_CAN_MASK_FRAME_TYPE_4BIT;
    msg->priority = (can_id >> ARTIE_CAN_ID_PSACP_PRIORITY_SHIFT) & ARTIE_CAN_MASK_PRIORITY;
    msg->sender_addr = (can_id >> ARTIE_CAN_ID_PSACP_SENDER_SHIFT) & ARTIE_CAN_MASK_ADDRESS;
    msg->topic = (can_id >> ARTIE_CAN_ID_PSACP_TOPIC_SHIFT) & ARTIE_CAN_MASK_TOPIC;
}

/**
 * @brief Publish a message to a topic
 */
int artie_can_psacp_publish(artie_can_context_t *ctx, uint8_t topic, uint8_t priority,
                            bool high_priority, const uint8_t *payload, size_t payload_len)
{
    if (!ctx || (payload_len > 0 && !payload)) {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    /* Byte stuff the payload */
    uint8_t stuffed_payload[ARTIE_CAN_MAX_STUFFED_PAYLOAD];
    size_t stuffed_len = 0;

    if (payload_len > 0) {
        int result = artie_can_byte_stuff(payload, payload_len, stuffed_payload,
                                         sizeof(stuffed_payload), &stuffed_len);
        if (result != 0) {
            return result;
        }
    }

    /* Compute CRC16 over stuffed payload */
    uint16_t crc = artie_can_crc16(stuffed_payload, stuffed_len);

    /* Send PUB frame */
    artie_can_frame_t frame;
    frame.extended = true;
    frame.can_id = psacp_build_can_id(high_priority, ARTIE_CAN_PSACP_PUB,
                                     priority, ctx->node_address, topic);

    /* First frame contains CRC16 + data */
    frame.data[0] = (crc >> ARTIE_CAN_BYTE1_SHIFT) & ARTIE_CAN_MASK_BYTE;
    frame.data[1] = crc & ARTIE_CAN_MASK_BYTE;

    size_t data_offset = 0;
    size_t frame_data_space = ARTIE_CAN_MAX_DATA_SIZE - 2;  /* 2 bytes for CRC */

    if (stuffed_len <= frame_data_space) {
        /* Single frame publish */
        memcpy(&frame.data[2], stuffed_payload, stuffed_len);
        frame.dlc = 2 + stuffed_len;

        if (!ctx->backend.send) {
            return ARTIE_CAN_ERR_NOT_INITIALIZED;
        }
        return ctx->backend.send(ctx->backend.context, &frame);
    } else {
        /* Multi-frame publish */
        memcpy(&frame.data[2], stuffed_payload, frame_data_space);
        frame.dlc = ARTIE_CAN_MAX_DATA_SIZE;
        data_offset = frame_data_space;

        if (!ctx->backend.send) {
            return ARTIE_CAN_ERR_NOT_INITIALIZED;
        }
        int result = ctx->backend.send(ctx->backend.context, &frame);
        if (result != 0) {
            return result;
        }

        /* Send DATA frames */
        while (data_offset < stuffed_len) {
            size_t remaining = stuffed_len - data_offset;
            size_t chunk_size = (remaining > ARTIE_CAN_MAX_DATA_SIZE) ?
                               ARTIE_CAN_MAX_DATA_SIZE : remaining;

            frame.can_id = psacp_build_can_id(high_priority, ARTIE_CAN_PSACP_DATA,
                                             priority, ctx->node_address, topic);
            memcpy(frame.data, &stuffed_payload[data_offset], chunk_size);
            frame.dlc = chunk_size;

            result = ctx->backend.send(ctx->backend.context, &frame);
            if (result != 0) {
                return result;
            }

            data_offset += chunk_size;
        }
    }

    return 0;
}

/**
 * @brief Receive a published message
 */
int artie_can_psacp_receive(artie_can_context_t *ctx, artie_can_psacp_msg_t *msg, uint32_t timeout_ms)
{
    if (!ctx || !msg) {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    if (!ctx->backend.receive) {
        return ARTIE_CAN_ERR_NOT_INITIALIZED;
    }

    /* Receive frames until we get a PSACP frame */
    artie_can_frame_t frame;
    int result = ctx->backend.receive(ctx->backend.context, &frame, timeout_ms);

    if (result != 0) {
        return result;
    }

    /* Check if this is a PSACP frame */
    uint8_t protocol = artie_can_get_protocol(&frame);
    if (protocol != ARTIE_CAN_PROTOCOL_PSACP_HIGH &&
        protocol != ARTIE_CAN_PROTOCOL_PSACP_LOW) {
        return ARTIE_CAN_ERR_PROTOCOL;  /* Not PSACP */
    }

    /* Parse CAN ID */
    psacp_parse_can_id(frame.can_id, msg);

    /* Handle different frame types */
    if (msg->frame_type == ARTIE_CAN_PSACP_PUB) {
        /* Parse CRC and collect payload */
        if (frame.dlc < 2) {
            return ARTIE_CAN_ERR_PROTOCOL;
        }

        msg->crc16 = (frame.data[0] << ARTIE_CAN_BYTE1_SHIFT) | frame.data[1];

        /* Collect stuffed payload */
        uint8_t stuffed_data[ARTIE_CAN_MAX_STUFFED_PAYLOAD];
        size_t stuffed_len = 0;

        /* Copy data from first frame */
        if (frame.dlc > 2) {
            memcpy(stuffed_data, &frame.data[2], frame.dlc - 2);
            stuffed_len = frame.dlc - 2;
        }

        /* For simplicity, assume single-frame for now */
        /* TODO: Handle multi-frame properly */

        /* Unstuff the payload */
        if (stuffed_len > 0) {
            result = artie_can_byte_unstuff(stuffed_data, stuffed_len,
                                           msg->payload, sizeof(msg->payload),
                                           &msg->payload_len);
            if (result != 0) {
                return result;
            }
        } else {
            msg->payload_len = 0;
        }

        /* TODO: Verify CRC */

        return 0;
    }

    return ARTIE_CAN_ERR_NOT_IMPLEMENTED;  /* Unsupported frame type or need continuation */
}
