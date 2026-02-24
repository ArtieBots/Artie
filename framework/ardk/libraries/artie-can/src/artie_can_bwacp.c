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
    can_id |= (ARTIE_CAN_PROTOCOL_BWACP << 26);

    /* Frame type (25-22) */
    can_id |= (frame_type & 0x0F) << 22;

    /* Priority (21-20) */
    can_id |= (priority & 0x03) << 20;

    /* Sender address (19-14) */
    can_id |= (sender_addr & 0x3F) << 14;

    /* Target address (13-8) */
    can_id |= (target_addr & 0x3F) << 8;

    /* Class mask (7-2) */
    can_id |= (class_mask & 0x3F) << 2;

    /* Bit 1 */
    if (bit1) {
        can_id |= 0x02;
    }

    /* Bit 0 */
    if (bit0) {
        can_id |= 0x01;
    }

    return can_id;
}

/**
 * @brief Parse BWACP frame
 */
static void bwacp_parse_can_id(uint32_t can_id, artie_can_bwacp_msg_t *msg)
{
    msg->frame_type = (can_id >> 22) & 0x0F;
    msg->priority = (can_id >> 20) & 0x03;
    msg->sender_addr = (can_id >> 14) & 0x3F;
    msg->target_addr = (can_id >> 8) & 0x3F;
    msg->class_mask = (can_id >> 2) & 0x3F;
    msg->is_repeat = (can_id & 0x02) != 0;
    msg->parity = (can_id & 0x01) != 0;
}

/**
 * @brief Send a block write ready frame
 */
int artie_can_bwacp_send_ready(artie_can_context_t *ctx, uint8_t target_addr, uint8_t class_mask,
                               uint8_t priority, uint32_t address, const uint8_t *payload,
                               size_t payload_len, bool interrupt)
{
    if (!ctx) {
        return -1;
    }

    /* Byte stuff the payload */
    uint8_t stuffed_payload[ARTIE_CAN_MAX_STUFFED_PAYLOAD];
    size_t stuffed_len = 0;

    if (payload_len > 0 && payload) {
        int result = artie_can_byte_stuff(payload, payload_len, stuffed_payload,
                                         sizeof(stuffed_payload), &stuffed_len);
        if (result != 0) {
            return result;
        }
    }

    /* Build data for CRC: address + stuffed payload */
    uint8_t crc_data[ARTIE_CAN_MAX_STUFFED_PAYLOAD + 4];
    crc_data[0] = (address >> 24) & 0xFF;
    crc_data[1] = (address >> 16) & 0xFF;
    crc_data[2] = (address >> 8) & 0xFF;
    crc_data[3] = address & 0xFF;
    if (stuffed_len > 0) {
        memcpy(&crc_data[4], stuffed_payload, stuffed_len);
    }

    uint32_t crc24 = artie_can_crc24(crc_data, 4 + stuffed_len);

    /* Send READY frame */
    artie_can_frame_t frame;
    frame.extended = true;
    frame.can_id = bwacp_build_can_id(ARTIE_CAN_BWACP_READY, priority,
                                     ctx->node_address, target_addr,
                                     class_mask, interrupt, true);

    /* READY frame data: CRC24 (3 bytes) + address (4 bytes) + first stuffing byte */
    frame.data[0] = (crc24 >> 16) & 0xFF;
    frame.data[1] = (crc24 >> 8) & 0xFF;
    frame.data[2] = crc24 & 0xFF;
    frame.data[3] = (address >> 24) & 0xFF;
    frame.data[4] = (address >> 16) & 0xFF;
    frame.data[5] = (address >> 8) & 0xFF;
    frame.data[6] = address & 0xFF;

    if (stuffed_len > 0) {
        frame.data[7] = stuffed_payload[0];
        frame.dlc = 8;
    } else {
        frame.dlc = 7;
    }

    if (!ctx->backend.send) {
        return -1;
    }

    int result = ctx->backend.send(ctx->backend.context, &frame);
    if (result != 0) {
        return result;
    }

    /* Send remaining data if any */
    if (stuffed_len > 1) {
        return artie_can_bwacp_send_data(ctx, target_addr, class_mask, priority,
                                        &stuffed_payload[1], stuffed_len - 1);
    }

    return 0;
}

/**
 * @brief Send block write data
 */
int artie_can_bwacp_send_data(artie_can_context_t *ctx, uint8_t target_addr, uint8_t class_mask,
                              uint8_t priority, const uint8_t *payload, size_t payload_len)
{
    if (!ctx || !payload) {
        return -1;
    }

    if (!ctx->backend.send) {
        return -1;
    }

    size_t offset = 0;
    bool parity = false;

    while (offset < payload_len) {
        size_t remaining = payload_len - offset;
        size_t chunk_size = (remaining > ARTIE_CAN_MAX_DATA_SIZE) ?
                           ARTIE_CAN_MAX_DATA_SIZE : remaining;

        artie_can_frame_t frame;
        frame.extended = true;
        frame.can_id = bwacp_build_can_id(ARTIE_CAN_BWACP_DATA, priority,
                                         ctx->node_address, target_addr,
                                         class_mask, false, parity);

        memcpy(frame.data, &payload[offset], chunk_size);
        frame.dlc = chunk_size;

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
        return -1;
    }

    if (!ctx->backend.receive) {
        return -1;
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
        return -1;  /* Not BWACP */
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
            return -1;
        }

        msg->crc24 = (frame.data[0] << 16) | (frame.data[1] << 8) | frame.data[2];
        msg->address = (frame.data[3] << 24) | (frame.data[4] << 16) |
                      (frame.data[5] << 8) | frame.data[6];

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

    return -1;  /* Unsupported frame type */
}

/**
 * @brief Send a repeat request
 */
int artie_can_bwacp_send_repeat(artie_can_context_t *ctx, uint8_t target_addr,
                                uint8_t priority, bool repeat_all)
{
    if (!ctx) {
        return -1;
    }

    artie_can_frame_t frame;
    frame.extended = true;
    frame.can_id = bwacp_build_can_id(ARTIE_CAN_BWACP_REPEAT, priority,
                                     ctx->node_address, target_addr,
                                     0, repeat_all, false);
    frame.dlc = 0;  /* REPEAT has no data */

    if (!ctx->backend.send) {
        return -1;
    }

    return ctx->backend.send(ctx->backend.context, &frame);
}
