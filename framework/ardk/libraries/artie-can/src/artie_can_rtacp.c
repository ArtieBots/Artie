/**
 * @file artie_can_rtacp.c
 * @brief Real Time Artie CAN Protocol (RTACP) implementation
 */

#include "artie_can.h"
#include <string.h>

/* RTACP ID bit field layout for extended CAN frame (29 bits total):
 * Bits 28-26: Protocol (000 for RTACP)
 * Bits 25-22: Frame type and priority
 *   - Bit 25 (000x): Frame type (0=ACK, 1=MSG)
 *   - Bits 24-23 (pp): Priority (00=HIGH, 01=MED_HIGH, 10=MED_LOW, 11=LOW)
 * Bits 21-16: Sender address (ssssss)
 * Bits 15-10: Target address (tttttt)
 * Bits 9-0: All 1s (0x3FF)
 */

/**
 * @brief Build RTACP CAN ID from message parameters
 */
static uint32_t rtacp_build_can_id(const artie_can_rtacp_msg_t *msg)
{
    uint32_t can_id = 0;

    /* Protocol bits (28-26): 000 for RTACP */
    can_id |= (ARTIE_CAN_PROTOCOL_RTACP << 26);

    /* Frame type bit (25) */
    can_id |= (msg->frame_type & 0x01) << 25;

    /* Priority bits (24-23) */
    can_id |= (msg->priority & 0x03) << 23;

    /* Sender address (21-16) */
    can_id |= (msg->sender_addr & 0x3F) << 16;

    /* Target address (15-10) */
    can_id |= (msg->target_addr & 0x3F) << 10;

    /* Bottom 10 bits all 1s */
    can_id |= 0x3FF;

    return can_id;
}

/**
 * @brief Parse RTACP message from CAN frame
 */
static int rtacp_parse_frame(const artie_can_frame_t *frame, artie_can_rtacp_msg_t *msg)
{
    uint32_t can_id = frame->can_id;

    /* Extract frame type */
    msg->frame_type = (can_id >> 25) & 0x01;

    /* Extract priority */
    msg->priority = (can_id >> 23) & 0x03;

    /* Extract sender address */
    msg->sender_addr = (can_id >> 16) & 0x3F;

    /* Extract target address */
    msg->target_addr = (can_id >> 10) & 0x3F;

    /* Copy data */
    msg->data_len = frame->dlc;
    if (msg->data_len > ARTIE_CAN_MAX_DATA_SIZE) {
        return -1;
    }
    memcpy(msg->data, frame->data, msg->data_len);

    return 0;
}

/**
 * @brief Send an RTACP message
 */
int artie_can_rtacp_send(artie_can_context_t *ctx, const artie_can_rtacp_msg_t *msg, bool wait_ack)
{
    if (!ctx || !msg) {
        return -1;
    }

    /* Build CAN frame */
    artie_can_frame_t frame;
    frame.extended = true;
    frame.can_id = rtacp_build_can_id(msg);
    frame.dlc = msg->data_len;

    if (msg->data_len > ARTIE_CAN_MAX_DATA_SIZE) {
        return -1;
    }

    memcpy(frame.data, msg->data, msg->data_len);

    /* Send frame */
    if (!ctx->backend.send) {
        return -1;
    }

    int result = ctx->backend.send(ctx->backend.context, &frame);
    if (result != 0) {
        return result;
    }

    /* If this is a targeted message and we need to wait for ACK */
    if (wait_ack && msg->frame_type == ARTIE_CAN_RTACP_MSG &&
        msg->target_addr != ARTIE_CAN_BROADCAST_ADDRESS) {

        /* Wait for ACK with 1ms timeout */
        artie_can_rtacp_msg_t ack_msg;
        uint32_t start_time = 0;  /* TODO: Implement timing */
        const uint32_t timeout_ms = 1;

        while (1) {
            result = artie_can_rtacp_receive(ctx, &ack_msg, timeout_ms);

            if (result == 0) {
                /* Check if this is the ACK we're waiting for */
                if (ack_msg.frame_type == ARTIE_CAN_RTACP_ACK &&
                    ack_msg.sender_addr == msg->target_addr &&
                    ack_msg.target_addr == msg->sender_addr &&
                    ack_msg.data_len == msg->data_len &&
                    memcmp(ack_msg.data, msg->data, msg->data_len) == 0) {
                    return 0;  /* Got correct ACK */
                }
            }

            /* TODO: Check timeout properly */
            /* For now, just try once */
            break;
        }

        /* Timeout or error - should resend, but for now just return error */
        return -1;
    }

    return 0;
}

/**
 * @brief Receive an RTACP message
 */
int artie_can_rtacp_receive(artie_can_context_t *ctx, artie_can_rtacp_msg_t *msg, uint32_t timeout_ms)
{
    if (!ctx || !msg) {
        return -1;
    }

    if (!ctx->backend.receive) {
        return -1;
    }

    /* Receive frames until we get an RTACP frame */
    while (1) {
        artie_can_frame_t frame;
        int result = ctx->backend.receive(ctx->backend.context, &frame, timeout_ms);

        if (result != 0) {
            return result;
        }

        /* Check if this is an RTACP frame */
        uint8_t protocol = artie_can_get_protocol(&frame);
        if (protocol != ARTIE_CAN_PROTOCOL_RTACP) {
            continue;  /* Not RTACP, keep looking */
        }

        /* Parse the frame */
        result = rtacp_parse_frame(&frame, msg);
        if (result != 0) {
            return result;
        }

        /* If this is a MSG frame targeted at us, send ACK */
        if (msg->frame_type == ARTIE_CAN_RTACP_MSG &&
            msg->target_addr == ctx->node_address) {

            artie_can_rtacp_msg_t ack;
            ack.frame_type = ARTIE_CAN_RTACP_ACK;
            ack.priority = msg->priority;
            ack.sender_addr = ctx->node_address;
            ack.target_addr = msg->sender_addr;
            ack.data_len = msg->data_len;
            memcpy(ack.data, msg->data, msg->data_len);

            /* Send ACK (don't wait for ACK of ACK) */
            artie_can_rtacp_send(ctx, &ack, false);
        }

        return 0;
    }
}
