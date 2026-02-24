/**
 * @file artie_can_rpcacp.c
 * @brief Remote Procedure Call Artie CAN Protocol (RPCACP) implementation
 */

#include "artie_can.h"
#include <string.h>
#include <stdlib.h>

/* RPCACP ID bit field layout for extended CAN frame (29 bits total):
 * Bits 28-26: Protocol (010 for RPCACP)
 * Bits 25-22: Frame type (0xxx)
 * Bits 21-20: Priority (pp)
 * Bits 19-14: Sender address (ssssss)
 * Bits 13-8: Target address (tttttt)
 * Bits 7-0: Random value (rrrr rrrr)
 */

/**
 * @brief Build RPCACP CAN ID
 */
static uint32_t rpcacp_build_can_id(uint8_t frame_type, uint8_t priority,
                                    uint8_t sender_addr, uint8_t target_addr,
                                    uint8_t random_value)
{
    uint32_t can_id = 0;

    /* Protocol bits (28-26): 010 for RPCACP */
    can_id |= (ARTIE_CAN_PROTOCOL_RPCACP << 26);

    /* Frame type (25-22) */
    can_id |= (frame_type & 0x0F) << 22;

    /* Priority (21-20) */
    can_id |= (priority & 0x03) << 20;

    /* Sender address (19-14) */
    can_id |= (sender_addr & 0x3F) << 14;

    /* Target address (13-8) */
    can_id |= (target_addr & 0x3F) << 8;

    /* Random value (7-0) */
    can_id |= random_value;

    return can_id;
}

/**
 * @brief Parse RPCACP frame
 */
static void rpcacp_parse_can_id(uint32_t can_id, artie_can_rpcacp_msg_t *msg)
{
    msg->frame_type = (can_id >> 22) & 0x0F;
    msg->priority = (can_id >> 20) & 0x03;
    msg->sender_addr = (can_id >> 14) & 0x3F;
    msg->target_addr = (can_id >> 8) & 0x3F;
    msg->random_value = can_id & 0xFF;
}

/**
 * @brief Generate random value for RPC
 */
static uint8_t rpcacp_generate_random(void)
{
    /* Simple pseudo-random for now - should use better RNG in production */
    static uint8_t seed = 1;
    seed = (seed * 75 + 74) % 256;
    if (seed == 0) seed = 1;
    return seed;
}

/**
 * @brief Send an RPC request
 */
int artie_can_rpcacp_call(artie_can_context_t *ctx, uint8_t target_addr, uint8_t priority,
                          bool is_synchronous, uint8_t procedure_id,
                          const uint8_t *payload, size_t payload_len)
{
    if (!ctx || (payload_len > 0 && !payload)) {
        return -1;
    }

    if (target_addr == ARTIE_CAN_BROADCAST_ADDRESS) {
        return -1;  /* Broadcast not allowed for RPC */
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

    /* Build header for CRC calculation */
    uint8_t crc_data[ARTIE_CAN_MAX_STUFFED_PAYLOAD + 2];
    crc_data[0] = (is_synchronous ? 0x80 : 0x00) | (procedure_id & 0x7F);
    if (stuffed_len > 0) {
        memcpy(&crc_data[1], stuffed_payload, stuffed_len);
    }
    uint16_t crc = artie_can_crc16(crc_data, 1 + stuffed_len);

    /* Generate random value */
    uint8_t random_value = rpcacp_generate_random();

    /* Send StartRPC frame(s) */
    artie_can_frame_t frame;
    frame.extended = true;
    frame.can_id = rpcacp_build_can_id(ARTIE_CAN_RPCACP_START_RPC, priority,
                                      ctx->node_address, target_addr, random_value);

    /* First frame contains: sync bit + procedure ID + CRC16 + remaining data */
    frame.data[0] = crc_data[0];  /* sync bit + procedure ID */
    frame.data[1] = (crc >> 8) & 0xFF;  /* CRC high byte */
    frame.data[2] = crc & 0xFF;  /* CRC low byte */

    size_t data_offset = 0;
    size_t frame_data_space = ARTIE_CAN_MAX_DATA_SIZE - 3;  /* 3 bytes for header */

    if (stuffed_len <= frame_data_space) {
        /* Single frame RPC */
        memcpy(&frame.data[3], stuffed_payload, stuffed_len);
        frame.dlc = 3 + stuffed_len;

        if (!ctx->backend.send) {
            return -1;
        }
        return ctx->backend.send(ctx->backend.context, &frame);
    } else {
        /* Multi-frame RPC */
        memcpy(&frame.data[3], stuffed_payload, frame_data_space);
        frame.dlc = ARTIE_CAN_MAX_DATA_SIZE;
        data_offset = frame_data_space;

        if (!ctx->backend.send) {
            return -1;
        }
        int result = ctx->backend.send(ctx->backend.context, &frame);
        if (result != 0) {
            return result;
        }

        /* Send TxData frames */
        while (data_offset < stuffed_len) {
            size_t remaining = stuffed_len - data_offset;
            size_t chunk_size = (remaining > ARTIE_CAN_MAX_DATA_SIZE) ?
                               ARTIE_CAN_MAX_DATA_SIZE : remaining;

            frame.can_id = rpcacp_build_can_id(ARTIE_CAN_RPCACP_TX_DATA, priority,
                                              ctx->node_address, target_addr, random_value);
            memcpy(frame.data, &stuffed_payload[data_offset], chunk_size);
            frame.dlc = chunk_size;

            result = ctx->backend.send(ctx->backend.context, &frame);
            if (result != 0) {
                return result;
            }

            data_offset += chunk_size;
        }
    }

    /* Wait for ACK/NACK */
    artie_can_rpcacp_msg_t response;
    int result = artie_can_rpcacp_receive(ctx, &response, 30);  /* 30ms timeout */

    if (result != 0) {
        return result;
    }

    if (response.frame_type == ARTIE_CAN_RPCACP_NACK) {
        return -response.nack_error_code;  /* Return negative error code */
    }

    if (response.frame_type != ARTIE_CAN_RPCACP_ACK) {
        return -1;  /* Unexpected response */
    }

    return 0;
}

/**
 * @brief Wait for RPC response (for synchronous RPCs)
 */
int artie_can_rpcacp_wait_response(artie_can_context_t *ctx, uint8_t *response,
                                   size_t max_len, size_t *actual_len, uint32_t timeout_ms)
{
    if (!ctx || !response || !actual_len) {
        return -1;
    }

    artie_can_rpcacp_msg_t msg;
    int result = artie_can_rpcacp_receive(ctx, &msg, timeout_ms);

    if (result != 0) {
        return result;
    }

    if (msg.frame_type != ARTIE_CAN_RPCACP_START_RETURN) {
        return -1;  /* Wrong frame type */
    }

    if (msg.payload_len > max_len) {
        return -1;  /* Response too large */
    }

    memcpy(response, msg.payload, msg.payload_len);
    *actual_len = msg.payload_len;

    return 0;
}

/**
 * @brief Receive and handle an RPC request
 */
int artie_can_rpcacp_receive(artie_can_context_t *ctx, artie_can_rpcacp_msg_t *msg, uint32_t timeout_ms)
{
    if (!ctx || !msg) {
        return -1;
    }

    if (!ctx->backend.receive) {
        return -1;
    }

    /* Receive frames until we get an RPCACP frame */
    artie_can_frame_t frame;
    int result = ctx->backend.receive(ctx->backend.context, &frame, timeout_ms);

    if (result != 0) {
        return result;
    }

    /* Check if this is an RPCACP frame */
    uint8_t protocol = artie_can_get_protocol(&frame);
    if (protocol != ARTIE_CAN_PROTOCOL_RPCACP) {
        return -1;  /* Not RPCACP */
    }

    /* Parse CAN ID */
    rpcacp_parse_can_id(frame.can_id, msg);

    /* Handle different frame types */
    if (msg->frame_type == ARTIE_CAN_RPCACP_ACK) {
        /* ACK frame - no payload */
        msg->payload_len = 0;
        return 0;
    }

    if (msg->frame_type == ARTIE_CAN_RPCACP_NACK) {
        /* NACK frame - 1 byte error code */
        if (frame.dlc < 1) {
            return -1;
        }
        msg->nack_error_code = frame.data[0];
        msg->payload_len = 0;
        return 0;
    }

    if (msg->frame_type == ARTIE_CAN_RPCACP_START_RPC ||
        msg->frame_type == ARTIE_CAN_RPCACP_START_RETURN) {
        /* Parse header */
        if (frame.dlc < 3) {
            return -1;
        }

        uint8_t sync_and_proc = frame.data[0];
        msg->is_synchronous = (sync_and_proc & 0x80) != 0;
        msg->procedure_id = sync_and_proc & 0x7F;
        msg->crc16 = (frame.data[1] << 8) | frame.data[2];

        /* Collect payload data */
        uint8_t stuffed_data[ARTIE_CAN_MAX_STUFFED_PAYLOAD];
        size_t stuffed_len = 0;

        /* Copy data from first frame */
        if (frame.dlc > 3) {
            memcpy(stuffed_data, &frame.data[3], frame.dlc - 3);
            stuffed_len = frame.dlc - 3;
        }

        /* Check for continuation frames */
        /* For simplicity, we'll assume single-frame for now */
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

    return -1;  /* Unsupported frame type */
}

/**
 * @brief Send an RPC response
 */
int artie_can_rpcacp_respond(artie_can_context_t *ctx, uint8_t target_addr, uint8_t priority,
                             uint8_t procedure_id, uint8_t random_value,
                             const uint8_t *payload, size_t payload_len)
{
    if (!ctx) {
        return -1;
    }

    /* Similar to artie_can_rpcacp_call but sends StartReturn frames */
    /* For brevity, implementing simplified version */

    uint8_t stuffed_payload[ARTIE_CAN_MAX_STUFFED_PAYLOAD];
    size_t stuffed_len = 0;

    if (payload_len > 0) {
        int result = artie_can_byte_stuff(payload, payload_len, stuffed_payload,
                                         sizeof(stuffed_payload), &stuffed_len);
        if (result != 0) {
            return result;
        }
    }

    /* Build header for CRC */
    uint8_t crc_data[ARTIE_CAN_MAX_STUFFED_PAYLOAD + 2];
    crc_data[0] = 0x80 | (procedure_id & 0x7F);  /* First bit always 1 for return */
    if (stuffed_len > 0) {
        memcpy(&crc_data[1], stuffed_payload, stuffed_len);
    }
    uint16_t crc = artie_can_crc16(crc_data, 1 + stuffed_len);

    /* Send StartReturn frame */
    artie_can_frame_t frame;
    frame.extended = true;
    frame.can_id = rpcacp_build_can_id(ARTIE_CAN_RPCACP_START_RETURN, priority,
                                      ctx->node_address, target_addr, random_value);

    frame.data[0] = crc_data[0];
    frame.data[1] = (crc >> 8) & 0xFF;
    frame.data[2] = crc & 0xFF;

    size_t frame_data_space = ARTIE_CAN_MAX_DATA_SIZE - 3;

    if (stuffed_len <= frame_data_space) {
        memcpy(&frame.data[3], stuffed_payload, stuffed_len);
        frame.dlc = 3 + stuffed_len;

        if (!ctx->backend.send) {
            return -1;
        }
        return ctx->backend.send(ctx->backend.context, &frame);
    }

    /* TODO: Handle multi-frame returns */
    return -1;  /* Not implemented yet */
}

/**
 * @brief Send an ACK for an RPC request
 */
int artie_can_rpcacp_send_ack(artie_can_context_t *ctx, uint8_t target_addr,
                              uint8_t priority, uint8_t random_value)
{
    if (!ctx) {
        return -1;
    }

    artie_can_frame_t frame;
    frame.extended = true;
    frame.can_id = rpcacp_build_can_id(ARTIE_CAN_RPCACP_ACK, priority,
                                      ctx->node_address, target_addr, random_value);
    frame.dlc = 0;  /* ACK has no data */

    if (!ctx->backend.send) {
        return -1;
    }

    return ctx->backend.send(ctx->backend.context, &frame);
}

/**
 * @brief Send a NACK for an RPC request
 */
int artie_can_rpcacp_send_nack(artie_can_context_t *ctx, uint8_t target_addr,
                               uint8_t priority, uint8_t random_value, uint8_t error_code)
{
    if (!ctx) {
        return -1;
    }

    artie_can_frame_t frame;
    frame.extended = true;
    frame.can_id = rpcacp_build_can_id(ARTIE_CAN_RPCACP_NACK, priority,
                                      ctx->node_address, target_addr, random_value);
    frame.data[0] = error_code;
    frame.dlc = 1;

    if (!ctx->backend.send) {
        return -1;
    }

    return ctx->backend.send(ctx->backend.context, &frame);
}
