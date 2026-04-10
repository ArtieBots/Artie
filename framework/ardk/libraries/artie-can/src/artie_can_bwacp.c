/**
 * @file artie_can_bwacp.c
 * @brief Block Write Artie CAN Protocol (BWACP) implementation
 */

#include "artie_can.h"
#include <string.h>

/* BWACP ID bit field layout for extended CAN frame (29 bits total):
 * Bits 28-26: Protocol (101 for BWACP)
 * Bits 25-22: Frame type (0xx1: 0001=REPEAT, 0011=READY, 0111=DATA)
 * Bits 21-20: Priority (pp)
 * Bits 19-14: Sender address (ssssss)
 * Bits 13-8: Target address (tttttt)
 * Bits 7-2: Class mask (cccccc) for DATA/READY, or 000000 for REPEAT
 * Bit 1: If DATA: repeat flag; if READY: interrupt flag; if REPEAT: repeat all flag
 * Bit 0: If DATA: parity bit; otherwise 1
 */

/**
 * @brief Build BWACP CAN ID
 */
static uint32_t bwacp_build_can_id(uint8_t frame_type, uint8_t priority,
                                   uint8_t sender_addr, uint8_t target_addr,
                                   uint8_t class_mask, bool bit1, bool bit0)
{
    uint32_t can_id = 0;

    /* Protocol bits (28-26): 101 for BWACP */
    can_id |= (ARTIE_CAN_PROTOCOL_BWACP << ARTIE_CAN_ID_PROTOCOL_SHIFT);

    /* Frame type (25-22) */
    can_id |= (frame_type & ARTIE_CAN_MASK_FRAME_TYPE_4BIT) << ARTIE_CAN_ID_BWACP_FRAME_TYPE_SHIFT;

    /* Priority (21-20) */
    can_id |= (priority & ARTIE_CAN_MASK_PRIORITY) << ARTIE_CAN_ID_BWACP_PRIORITY_SHIFT;

    /* Sender address (19-14) */
    can_id |= (sender_addr & ARTIE_CAN_MASK_ADDRESS) << ARTIE_CAN_ID_BWACP_SENDER_SHIFT;

    /* Target address (13-8) */
    can_id |= (target_addr & ARTIE_CAN_MASK_ADDRESS) << ARTIE_CAN_ID_BWACP_TARGET_SHIFT;

    /* Class mask (7-2) */
    can_id |= (class_mask & ARTIE_CAN_MASK_CLASS) << ARTIE_CAN_ID_BWACP_CLASS_SHIFT;

    /* Bit 1 */
    if (bit1) {
        can_id |= ARTIE_CAN_MASK_REPEAT_BIT;
    }

    /* Bit 0 */
    if (bit0) {
        can_id |= ARTIE_CAN_MASK_PARITY_BIT;
    }

    return can_id;
}

/**
 * @brief Parse BWACP frame
 */
static void bwacp_parse_can_id(uint32_t can_id, artie_can_bwacp_msg_t *msg)
{
    msg->frame_type = (can_id >> ARTIE_CAN_ID_BWACP_FRAME_TYPE_SHIFT) & ARTIE_CAN_MASK_FRAME_TYPE_4BIT;
    msg->priority = (can_id >> ARTIE_CAN_ID_BWACP_PRIORITY_SHIFT) & ARTIE_CAN_MASK_PRIORITY;
    msg->sender_addr = (can_id >> ARTIE_CAN_ID_BWACP_SENDER_SHIFT) & ARTIE_CAN_MASK_ADDRESS;
    msg->target_addr = (can_id >> ARTIE_CAN_ID_BWACP_TARGET_SHIFT) & ARTIE_CAN_MASK_ADDRESS;
    msg->class_mask = (can_id >> ARTIE_CAN_ID_BWACP_CLASS_SHIFT) & ARTIE_CAN_MASK_CLASS;
    msg->is_repeat = (can_id & ARTIE_CAN_MASK_REPEAT_BIT) != 0;
    msg->parity = (can_id & ARTIE_CAN_MASK_PARITY_BIT) != 0;
}

/**
 * @brief Send a block write ready frame
 */
int artie_can_bwacp_send_ready(artie_can_context_t *ctx, uint8_t target_addr, uint8_t class_mask,
                               uint8_t priority, uint32_t address, const uint8_t *payload,
                               size_t payload_len, bool interrupt)
{
    if (!ctx) {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    /* Validate payload size (must fit after byte stuffing) */
    if (payload_len > ARTIE_CAN_MAX_UNSTUFFED_BWACP_PAYLOAD) {
        return ARTIE_CAN_ERR_PAYLOAD_TOO_LARGE;
    }

    /* Byte stuff the payload */
    static uint8_t stuffed_payload[ARTIE_CAN_MAX_STUFFED_PAYLOAD];
    memset(stuffed_payload, 0, sizeof(stuffed_payload));
    size_t stuffed_len = 0;

    if (payload_len > 0 && payload) {
        int result = artie_can_byte_stuff(payload, payload_len, stuffed_payload, sizeof(stuffed_payload), &stuffed_len);
        if (result != 0) {
            return result;
        }
    }

    /* Build data for CRC: address + stuffed payload */
    static uint8_t crc_data[ARTIE_CAN_MAX_STUFFED_PAYLOAD + 4];
    memset(crc_data, 0, sizeof(crc_data));
    crc_data[0] = (address >> ARTIE_CAN_BYTE3_SHIFT) & ARTIE_CAN_MASK_BYTE;
    crc_data[1] = (address >> ARTIE_CAN_BYTE2_SHIFT) & ARTIE_CAN_MASK_BYTE;
    crc_data[2] = (address >> ARTIE_CAN_BYTE1_SHIFT) & ARTIE_CAN_MASK_BYTE;
    crc_data[3] = address & ARTIE_CAN_MASK_BYTE;
    if (stuffed_len > 0) {
        memcpy(&crc_data[4], stuffed_payload, stuffed_len);
    }

    uint32_t crc24 = artie_can_crc24(crc_data, 4 + stuffed_len);

    /* Send READY frame */
    artie_can_frame_t frame;
    frame.extended = true;
    frame.can_id = bwacp_build_can_id(ARTIE_CAN_BWACP_READY, priority, ctx->node_address, target_addr, class_mask, interrupt, true);

    /* READY frame data: CRC24 (3 bytes) + address (4 bytes) + first stuffing byte */
    frame.data[0] = (crc24 >> ARTIE_CAN_BYTE2_SHIFT) & ARTIE_CAN_MASK_BYTE;
    frame.data[1] = (crc24 >> ARTIE_CAN_BYTE1_SHIFT) & ARTIE_CAN_MASK_BYTE;
    frame.data[2] = crc24 & ARTIE_CAN_MASK_BYTE;
    frame.data[3] = (address >> ARTIE_CAN_BYTE3_SHIFT) & ARTIE_CAN_MASK_BYTE;
    frame.data[4] = (address >> ARTIE_CAN_BYTE2_SHIFT) & ARTIE_CAN_MASK_BYTE;
    frame.data[5] = (address >> ARTIE_CAN_BYTE1_SHIFT) & ARTIE_CAN_MASK_BYTE;
    frame.data[6] = address & ARTIE_CAN_MASK_BYTE;

    if (stuffed_len > 0) {
        frame.data[7] = stuffed_payload[0];
        frame.dlc = 8;
    } else {
        frame.dlc = 7;
    }

    if (!ctx->backend.send) {
        return ARTIE_CAN_ERR_NOT_INITIALIZED;
    }

    int result = ctx->backend.send(ctx->backend.context, &frame);
    if (result != 0) {
        return result;
    }

    /* Send remaining data if any */
    if (stuffed_len > 1) {
        return artie_can_bwacp_send_data(ctx, target_addr, class_mask, priority, &stuffed_payload[1], stuffed_len - 1);
    }

    return 0;
}

/**
 * @brief Send block write data
 */
int artie_can_bwacp_send_data(artie_can_context_t *ctx, uint8_t target_addr, uint8_t class_mask, uint8_t priority, const uint8_t *payload, size_t payload_len)
{
    if (!ctx || !payload) {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    if (!ctx->backend.send) {
        return ARTIE_CAN_ERR_NOT_INITIALIZED;
    }

    size_t offset = 0;
    bool parity = false;

    while (offset < payload_len) {
        size_t remaining = payload_len - offset;
        size_t chunk_size = (remaining > ARTIE_CAN_MAX_DATA_SIZE) ? ARTIE_CAN_MAX_DATA_SIZE : remaining;

        artie_can_frame_t frame;
        frame.extended = true;
        frame.can_id = bwacp_build_can_id(ARTIE_CAN_BWACP_DATA, priority, ctx->node_address, target_addr, class_mask, false, parity);

        memcpy(frame.data, &payload[offset], chunk_size);
        frame.dlc = (uint8_t)chunk_size;

        int result = ctx->backend.send(ctx->backend.context, &frame);
        if (result != 0) {
            return result;
        }

        offset += chunk_size;
        parity = !parity;  /* Toggle parity */
    }

    return 0;
}

/**
 * @brief Receive block write messages
 */
int artie_can_bwacp_receive(artie_can_context_t *ctx, artie_can_bwacp_msg_t *msg, uint32_t timeout_ms)
{
    if (!ctx || !msg) {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    if (!ctx->backend.receive) {
        return ARTIE_CAN_ERR_NOT_INITIALIZED;
    }

    /* Receive frames until we get a BWACP frame */
    artie_can_frame_t frame;
    int result = ctx->backend.receive(ctx->backend.context, &frame, timeout_ms);

    if (result != 0) {
        return result;
    }

    /* Check if this is a BWACP frame */
    uint8_t protocol = artie_can_get_protocol(&frame);
    if (protocol != ARTIE_CAN_PROTOCOL_BWACP) {
        return ARTIE_CAN_ERR_PROTOCOL;  /* Not BWACP */
    }

    /* Parse CAN ID */
    bwacp_parse_can_id(frame.can_id, msg);

    /* Handle different frame types */
    if (msg->frame_type == ARTIE_CAN_BWACP_REPEAT) {
        /* REPEAT frame has no payload */
        msg->payload_len = 0;
        return 0;
    }

    if (msg->frame_type == ARTIE_CAN_BWACP_READY) {
        /* Parse READY frame */
        if (frame.dlc < 7) {
            return ARTIE_CAN_ERR_PROTOCOL;
        }

        msg->crc24 = (frame.data[0] << ARTIE_CAN_BYTE2_SHIFT) | (frame.data[1] << ARTIE_CAN_BYTE1_SHIFT) | frame.data[2];
        msg->address = (frame.data[3] << ARTIE_CAN_BYTE3_SHIFT) | (frame.data[4] << ARTIE_CAN_BYTE2_SHIFT) |
                      (frame.data[5] << ARTIE_CAN_BYTE1_SHIFT) | frame.data[6];

        /* Collect stuffed payload */
        if (frame.dlc > 7) {
            msg->payload[0] = frame.data[7];
            msg->payload_len = 1;
        } else {
            msg->payload_len = 0;
        }

        /* TODO: Handle multi-frame properly */

        return 0;
    }

    if (msg->frame_type == ARTIE_CAN_BWACP_DATA) {
        /* DATA frame - just return the stuffed payload */
        memcpy(msg->payload, frame.data, frame.dlc);
        msg->payload_len = frame.dlc;
        return 0;
    }

    return ARTIE_CAN_ERR_NOT_IMPLEMENTED;  /* Unsupported frame type */
}

/**
 * @brief Send a repeat request
 */
int artie_can_bwacp_send_repeat(artie_can_context_t *ctx, uint8_t target_addr,
                                uint8_t priority, bool repeat_all)
{
    if (!ctx) {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    artie_can_frame_t frame;
    frame.extended = true;
    frame.can_id = bwacp_build_can_id(ARTIE_CAN_BWACP_REPEAT, priority,
                                     ctx->node_address, target_addr,
                                     0, repeat_all, false);
    frame.dlc = 0;  /* REPEAT has no data */

    if (!ctx->backend.send) {
        return ARTIE_CAN_ERR_NOT_INITIALIZED;
    }

    return ctx->backend.send(ctx->backend.context, &frame);
}
