/**
 * @file bwacp.c
 * @brief Implementation of BWACP (Block Write Artie CAN Protocol).
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "backend.h"
#include "bwacp.h"
#include "bwacp_context.h"
#include "context.h"
#include "err.h"
#include "frame.h"
#include "log.h"

// CRC24 algorithm constants
/** Initial value for CRC24 calculation */
#define BWACP_CRC24_INIT 0x000000U
/** CRC24 polynomial */
#define BWACP_CRC24_POLYNOMIAL 0x864CFBU
/** Bit shift for XOR operation in CRC24 */
#define BWACP_CRC24_XOR_SHIFT 16U
/** MSB bit position for CRC24 */
#define BWACP_CRC24_MSB_BIT 0x800000U
/** Mask to keep only 24 bits */
#define BWACP_CRC24_MASK 0xFFFFFFU

/**
 * @brief Calculate CRC24 over a buffer (simplified implementation).
 * For a real implementation, use a proper CRC24 algorithm.
 */
static uint32_t _calculate_crc24(const uint8_t *data, uint32_t length)
{
    uint32_t crc = BWACP_CRC24_INIT;

    for (uint32_t i = 0; i < length; i++)
    {
        crc ^= ((uint32_t)data[i] << BWACP_CRC24_XOR_SHIFT);
        for (int j = 0; j < 8; j++)
        {
            if (crc & BWACP_CRC24_MSB_BIT)
            {
                crc = (crc << 1) ^ BWACP_CRC24_POLYNOMIAL;
            }
            else
            {
                crc = crc << 1;
            }
        }
    }

    return crc & BWACP_CRC24_MASK;
}

/**
 * @brief Check if this node should accept a frame based on addressing.
 */
static bool _should_accept_frame(bwacp_context_t *ctx, uint8_t target_address, uint8_t target_class)
{
    // Check if addressed to us specifically
    if (target_address == ctx->node_address)
    {
        return true;
    }

    // Check if multicast and we match the class
    if (target_address == ARTIE_CAN_BWACP_MULTICAST_ADDRESS)
    {
        if ((target_class & ctx->node_class) != 0)
        {
            return true;
        }
    }

    return false;
}

/**
 * @brief Send a REPEAT frame to request full retransmission.
 */
static artie_can_error_t _send_repeat(artie_can_backend_t *handle)
{
    artie_can_frame_t frame = {0};
    bwacp_context_t *ctx = &handle->context->bwacp_context;

    // Build frame ID
    frame.id = ((uint32_t)ARTIE_CAN_BWACP_PROTOCOL_ID << ARTIE_CAN_FRAME_ID_PROTOCOL_LOCATION) |
               ((uint32_t)ARTIE_CAN_FRAME_TYPE_BWACP_REPEAT << ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION) |
               ((uint32_t)ARTIE_CAN_FRAME_PRIORITY_BWACP_HIGH << ARTIE_CAN_FRAME_ID_USER_PRIORITY_LOCATION) |
               ((uint32_t)ctx->node_address << ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION) |
               ((uint32_t)ctx->receive_sender_address << ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION);

    frame.dlc = 0; // REPEAT frames have no data

    ARTIE_CAN_LOG(handle->context, "BWACP: Sending REPEAT frame (full retransmission)\n");
    return handle->send(handle->context, &frame);
}

/**
 * @brief Send a READY frame.
 */
static artie_can_error_t _send_ready(artie_can_backend_t *handle, const uint8_t *payload, uint32_t payload_size,
                                     uint32_t address, uint8_t target_address, uint8_t target_class,
                                     artie_can_frame_priority_bwacp_t priority, bool interrupt)
{
    artie_can_frame_t frame = {0};
    bwacp_context_t *ctx = &handle->context->bwacp_context;

    // Calculate CRC24 over the payload
    uint32_t crc24 = _calculate_crc24(payload, payload_size);
    ctx->send_crc24 = crc24;

    // Build frame ID
    frame.id = ((uint32_t)ARTIE_CAN_BWACP_PROTOCOL_ID << ARTIE_CAN_FRAME_ID_PROTOCOL_LOCATION) |
               ((uint32_t)ARTIE_CAN_FRAME_TYPE_BWACP_READY << ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION) |
               ((uint32_t)priority << ARTIE_CAN_FRAME_ID_USER_PRIORITY_LOCATION) |
               ((uint32_t)ctx->node_address << ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION) |
               ((uint32_t)target_address << ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION) |
               ((uint32_t)target_class << BWACP_FRAME_ID_CLASS_LOCATION);

    if (interrupt)
    {
        frame.id |= (1U << BWACP_FRAME_ID_REPEAT_INTERRUPT_LOCATION);
    }

    // Data: [3 bytes CRC24][4 bytes address][1 byte first stuffing byte]
    frame.data[BWACP_READY_DATA_CRC24_BYTE0] = (crc24 >> BWACP_CRC24_SHIFT_BYTE0) & 0xFF;
    frame.data[BWACP_READY_DATA_CRC24_BYTE1] = (crc24 >> BWACP_CRC24_SHIFT_BYTE1) & 0xFF;
    frame.data[BWACP_READY_DATA_CRC24_BYTE2] = (crc24 >> BWACP_CRC24_SHIFT_BYTE2) & 0xFF;
    frame.data[BWACP_READY_DATA_ADDRESS_BYTE0] = (address >> BWACP_SHIFT_BYTE0) & 0xFF;
    frame.data[BWACP_READY_DATA_ADDRESS_BYTE1] = (address >> BWACP_SHIFT_BYTE1) & 0xFF;
    frame.data[BWACP_READY_DATA_ADDRESS_BYTE2] = (address >> BWACP_SHIFT_BYTE2) & 0xFF;
    frame.data[BWACP_READY_DATA_ADDRESS_BYTE3] = (address >> BWACP_SHIFT_BYTE3) & 0xFF;
    frame.data[BWACP_READY_DATA_STUFFING] = 0x00; // First stuffing byte (simplified - not implementing full byte stuffing)
    frame.dlc = 8;

    ARTIE_CAN_LOG(handle->context, "BWACP: Sending READY frame (addr=0x%08X, size=%u)\n", address, payload_size);
    return handle->send(handle->context, &frame);
}

/**
 * @brief Send a DATA frame.
 */
static artie_can_error_t _send_data(artie_can_backend_t *handle, bool is_repeat)
{
    artie_can_frame_t frame = {0};
    bwacp_context_t *ctx = &handle->context->bwacp_context;

    // Build frame ID
    frame.id = ((uint32_t)ARTIE_CAN_BWACP_PROTOCOL_ID << ARTIE_CAN_FRAME_ID_PROTOCOL_LOCATION) |
               ((uint32_t)ARTIE_CAN_FRAME_TYPE_BWACP_DATA << ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION) |
               ((uint32_t)ARTIE_CAN_FRAME_PRIORITY_BWACP_LOW << ARTIE_CAN_FRAME_ID_USER_PRIORITY_LOCATION) |
               ((uint32_t)ctx->node_address << ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION) |
               ((uint32_t)ctx->send_target_address << ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION) |
               ((uint32_t)ctx->send_target_class << BWACP_FRAME_ID_CLASS_LOCATION);

    // Set repeat bit
    if (is_repeat)
    {
        frame.id |= (1U << BWACP_FRAME_ID_REPEAT_INTERRUPT_LOCATION);
    }

    // Set parity bit
    frame.id |= ((ctx->send_parity ? 1U : 0U) << BWACP_FRAME_ID_PARITY_LOCATION);

    // Copy up to 8 bytes of payload
    uint32_t bytes_remaining = ctx->send_payload_size - ctx->send_payload_offset;
    frame.dlc = (bytes_remaining > 8) ? 8 : bytes_remaining;
    memcpy(frame.data, &ctx->send_payload[ctx->send_payload_offset], frame.dlc);

    ARTIE_CAN_LOG(handle->context, "BWACP: Sending DATA frame (offset=%u, dlc=%u, parity=%d)\n",
                  ctx->send_payload_offset, frame.dlc, ctx->send_parity);

    return handle->send(handle->context, &frame);
}

/**
 * @brief Send a COMPLETE frame.
 */
static artie_can_error_t _send_complete(artie_can_backend_t *handle)
{
    artie_can_frame_t frame = {0};
    bwacp_context_t *ctx = &handle->context->bwacp_context;

    // Build frame ID
    frame.id = ((uint32_t)ARTIE_CAN_BWACP_PROTOCOL_ID << ARTIE_CAN_FRAME_ID_PROTOCOL_LOCATION) |
               ((uint32_t)ARTIE_CAN_FRAME_TYPE_BWACP_COMPLETE << ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION) |
               ((uint32_t)ARTIE_CAN_FRAME_PRIORITY_BWACP_MEDIUM << ARTIE_CAN_FRAME_ID_USER_PRIORITY_LOCATION) |
               ((uint32_t)ctx->node_address << ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION) |
               ((uint32_t)ctx->send_target_address << ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION) |
               ((uint32_t)ctx->send_target_class << BWACP_FRAME_ID_CLASS_LOCATION);

    frame.dlc = 0; // COMPLETE frames have no data

    ARTIE_CAN_LOG(handle->context, "BWACP: Sending COMPLETE frame\n");
    return handle->send(handle->context, &frame);
}

/**
 * @brief Process a READY frame received in ISR.
 */
static void _process_ready_frame_from_isr(artie_can_context_t *context, const artie_can_frame_t *frame)
{
    bwacp_context_t *ctx = &context->bwacp_context;

    // Check for ISR lockout
    if ((ctx->isr_flags & BWACP_ISR_FLAG_READY_RECEIVED) != 0)
    {
        return;
    }

    // If we are already receiving a bulk transfer from another source,
    // ignore this READY frame (BWACP does not support concurrent transfers to the same node)
    if (ctx->state == BWACP_STATE_RECEIVING)
    {
        return;
    }

    // Extract target address and class
    uint8_t target_address = (frame->id & ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK) >> ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION;
    uint8_t target_class = (frame->id & BWACP_FRAME_ID_CLASS_MASK) >> BWACP_FRAME_ID_CLASS_LOCATION;

    // Check if we should accept this frame
    if (!_should_accept_frame(ctx, target_address, target_class))
    {
        return;
    }

    // Copy frame to context for main thread processing
    memcpy(&ctx->received_ready_frame, frame, sizeof(artie_can_frame_t));

    // Set the address of the node we are receiving from
    ctx->sending_node_address = (uint8_t)((frame->id & ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK) >> ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION);

    // Set flag for main thread
    atomic_fetch_or(&ctx->isr_flags, BWACP_ISR_FLAG_READY_RECEIVED);
}

/**
 * @brief Process a DATA frame received in ISR.
 */
static void _process_data_frame_from_isr(artie_can_context_t *context, const artie_can_frame_t *frame)
{
    bwacp_context_t *ctx = &context->bwacp_context;

    // Check for ISR lockout
    if ((ctx->isr_flags & BWACP_ISR_FLAG_DATA_RECEIVED) != 0)
    {
        return;
    }

    // Check target address
    uint8_t target_address = (frame->id & ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK) >> ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION;
    uint8_t target_class = (frame->id & BWACP_FRAME_ID_CLASS_MASK) >> BWACP_FRAME_ID_CLASS_LOCATION;

    if (!_should_accept_frame(ctx, target_address, target_class))
    {
        return;
    }

    // Check that we are supposed to be receiving from this node
    uint8_t sending_node_address = (uint8_t)((frame->id & ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK) >> ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION);
    if (sending_node_address != ctx->sending_node_address)
    {
        return;
    }

    // Extract parity
    bool parity = (frame->id & BWACP_FRAME_ID_PARITY_MASK) != 0;

    // Check parity - if mismatch, drop the frame and continue
    // The CRC check at the end will catch the error and request full retransmission
    if (parity != ctx->receive_expected_parity)
    {
        ARTIE_CAN_LOG(context, "BWACP: Parity mismatch, dropping frame (will be caught by CRC check)\n");
        return;
    }

    // Toggle expected parity for next frame
    ctx->receive_expected_parity = !ctx->receive_expected_parity;

    // Check if circular buffer has space
    uint32_t pending = ctx->data_frames_pending;
    if (pending >= ARTIE_CAN_BWACP_DATA_FRAME_BUFFER_SIZE)
    {
        ARTIE_CAN_LOG(context, "BWACP: DATA frame buffer full\n");
        return;
    }

    // Copy frame to circular buffer
    memcpy(&ctx->data_frame_buffer[ctx->data_frame_write_index], frame, sizeof(artie_can_frame_t));

    // Update write index (wrap around)
    ctx->data_frame_write_index = (ctx->data_frame_write_index + 1) % ARTIE_CAN_BWACP_DATA_FRAME_BUFFER_SIZE;

    // Atomically increment pending count (this makes the frame visible to main thread)
    atomic_fetch_add(&ctx->data_frames_pending, 1);

    // Set flag
    atomic_fetch_or(&ctx->isr_flags, BWACP_ISR_FLAG_DATA_RECEIVED);
}

/**
 * @brief Process a COMPLETE frame received in ISR.
 */
static void _process_complete_frame_from_isr(artie_can_context_t *context, const artie_can_frame_t *frame)
{
    bwacp_context_t *ctx = &context->bwacp_context;

    // Check for ISR lockout
    if ((ctx->isr_flags & BWACP_ISR_FLAG_COMPLETE_RECEIVED) != 0)
    {
        return;
    }

    // Check target address
    uint8_t target_address = (frame->id & ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK) >> ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION;
    uint8_t target_class = (frame->id & BWACP_FRAME_ID_CLASS_MASK) >> BWACP_FRAME_ID_CLASS_LOCATION;

    if (!_should_accept_frame(ctx, target_address, target_class))
    {
        return;
    }

    // Check that we are supposed to be receiving from this node
    uint8_t sending_node_address = (uint8_t)((frame->id & ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK) >> ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION);
    if (sending_node_address != ctx->sending_node_address)
    {
        return;
    }

    // Copy frame to context for main thread processing
    memcpy(&ctx->received_complete_frame, frame, sizeof(artie_can_frame_t));

    ARTIE_CAN_LOG(context, "BWACP: COMPLETE frame received\n");

    // Set flag
    atomic_fetch_or(&ctx->isr_flags, BWACP_ISR_FLAG_COMPLETE_RECEIVED);
}

/**
 * @brief Process a REPEAT frame (for senders) received in ISR.
 */
static void _process_repeat_frame_from_isr(artie_can_context_t *context, const artie_can_frame_t *frame)
{
    bwacp_context_t *ctx = &context->bwacp_context;

    // Check for ISR lockout
    if ((ctx->isr_flags & BWACP_ISR_FLAG_REPEAT_RECEIVED) != 0)
    {
        return;
    }

    // Check if addressed to us
    uint8_t target_address = (frame->id & ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK) >> ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION;
    if (target_address != ctx->node_address)
    {
        return;
    }

    // Copy frame to context for main thread processing
    memcpy(&ctx->received_repeat_frame, frame, sizeof(artie_can_frame_t));

    // Set flag for main thread
    atomic_fetch_or(&ctx->isr_flags, BWACP_ISR_FLAG_REPEAT_RECEIVED);
}

static artie_can_error_t _send_next_chunk(artie_can_backend_t *handle)
{
    bwacp_context_t *ctx = &handle->context->bwacp_context;

    if (ctx->send_payload_offset >= ctx->send_payload_size)
    {
        // All data sent, send COMPLETE frame
        ctx->state = BWACP_STATE_WAITING_COMPLETE;
        return _send_complete(handle);
    }
    else
    {
        // Send next DATA frame
        artie_can_error_t err = _send_data(handle, false);
        if (err != ARTIE_CAN_ERR_NONE)
        {
            return err;
        }

        // Update offset and toggle parity for next frame
        ctx->send_payload_offset += (ctx->send_payload_size - ctx->send_payload_offset > 8) ? 8 : (ctx->send_payload_size - ctx->send_payload_offset);
        ctx->send_parity = !ctx->send_parity;

        return ARTIE_CAN_ERR_NONE;
    }
}

/**
 * @brief Process READY frame received in ISR (main thread handler).
 */
static artie_can_error_t _process_ready_received(artie_can_backend_t *handle)
{
    bwacp_context_t *ctx = &handle->context->bwacp_context;

    // Extract data from received READY frame
    artie_can_frame_t *frame = &ctx->received_ready_frame;

    // Extract sender address
    ctx->receive_sender_address = (frame->id & ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK) >> ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION;

    // Extract CRC24
    ctx->receive_crc24 = ((uint32_t)frame->data[BWACP_READY_DATA_CRC24_BYTE0] << BWACP_CRC24_SHIFT_BYTE0) |
                         ((uint32_t)frame->data[BWACP_READY_DATA_CRC24_BYTE1] << BWACP_CRC24_SHIFT_BYTE1) |
                         ((uint32_t)frame->data[BWACP_READY_DATA_CRC24_BYTE2] << BWACP_CRC24_SHIFT_BYTE2);

    // Extract address
    ctx->receive_address = ((uint32_t)frame->data[BWACP_READY_DATA_ADDRESS_BYTE0] << BWACP_SHIFT_BYTE0) |
                           ((uint32_t)frame->data[BWACP_READY_DATA_ADDRESS_BYTE1] << BWACP_SHIFT_BYTE1) |
                           ((uint32_t)frame->data[BWACP_READY_DATA_ADDRESS_BYTE2] << BWACP_SHIFT_BYTE2) |
                           ((uint32_t)frame->data[BWACP_READY_DATA_ADDRESS_BYTE3] << BWACP_SHIFT_BYTE3);

    // Check if this is the same transfer we just completed (within cooldown period)
    // This prevents receiving duplicate transfers when a sibling node requests REPEAT
    uint64_t time_since_last_complete = handle->get_ms() - ctx->last_completed_timestamp_ms;
    bool same_sender = ctx->receive_sender_address == ctx->last_completed_sender_address;
    bool same_address = ctx->receive_address == ctx->last_completed_receive_address;
    if ((time_since_last_complete < ARTIE_CAN_BWACP_REPEAT_REQUEST_TIMEOUT_MS) && same_sender && same_address)
    {
        ARTIE_CAN_LOG(handle->context, "BWACP: Ignoring duplicate transfer within cooldown (sender=0x%02X, addr=0x%08X)\n", ctx->receive_sender_address, ctx->receive_address);
        atomic_fetch_and(&ctx->isr_flags, ~BWACP_ISR_FLAG_READY_RECEIVED);
        return ARTIE_CAN_ERR_NONE;
    }

    // Check interrupt bit
    ctx->receive_ready_interrupt = (frame->id & BWACP_FRAME_ID_REPEAT_INTERRUPT_MASK) != 0;

    // Reset receive state
    ctx->receive_bytes_written = 0;
    ctx->receive_expected_parity = false;
    ctx->state = BWACP_STATE_RECEIVING;
    ctx->last_packet_ms = handle->get_ms();

    ARTIE_CAN_LOG(handle->context, "BWACP: READY frame received (addr=0x%08X, CRC=0x%06X)\n", ctx->receive_address, ctx->receive_crc24);

    atomic_fetch_and(&ctx->isr_flags, ~BWACP_ISR_FLAG_READY_RECEIVED);
    return ARTIE_CAN_ERR_NONE;
}

/**
 * @brief Process DATA frame received in ISR (main thread handler).
 */
static artie_can_error_t _process_data_received(artie_can_backend_t *handle)
{
    bwacp_context_t *ctx = &handle->context->bwacp_context;

    // Only process if we're in receiving state
    if (ctx->state != BWACP_STATE_RECEIVING)
    {
        atomic_fetch_and(&ctx->isr_flags, ~BWACP_ISR_FLAG_DATA_RECEIVED);
        return ARTIE_CAN_ERR_NONE;
    }

    // Process all pending DATA frames from circular buffer
    while (ctx->data_frames_pending > 0)
    {
        // Read frame from circular buffer
        artie_can_frame_t *frame = &ctx->data_frame_buffer[ctx->data_frame_read_index];

        // Check if this is a repeat frame
        bool is_repeat = (frame->id & BWACP_FRAME_ID_REPEAT_INTERRUPT_MASK) != 0;

        // Check buffer space
        if (ctx->receive_bytes_written + frame->dlc <= ctx->receive_buffer_size)
        {
            // Copy data to receive buffer
            if (ctx->receive_buffer != NULL && !is_repeat)
            {
                memcpy(&ctx->receive_buffer[ctx->receive_bytes_written], frame->data, frame->dlc);
                ctx->receive_bytes_written += frame->dlc;
            }
        }
        else
        {
            ARTIE_CAN_LOG(handle->context, "BWACP: Receive buffer overflow\n");
        }

        // Update read index (wrap around)
        ctx->data_frame_read_index = (ctx->data_frame_read_index + 1) % ARTIE_CAN_BWACP_DATA_FRAME_BUFFER_SIZE;

        // Atomically decrement pending count
        atomic_fetch_sub(&ctx->data_frames_pending, 1);
    }

    // Update timeout counter
    ctx->last_packet_ms = handle->get_ms();

    atomic_fetch_and(&ctx->isr_flags, ~BWACP_ISR_FLAG_DATA_RECEIVED);
    return ARTIE_CAN_ERR_NONE;
}

/**
 * @brief Process REPEAT frame received in ISR (main thread handler).
 */
static artie_can_error_t _process_repeat_received(artie_can_backend_t *handle)
{
    bwacp_context_t *ctx = &handle->context->bwacp_context;

    ARTIE_CAN_LOG(handle->context, "BWACP: REPEAT all requested\n");

    // Reset to beginning for full retransmission
    ctx->send_payload_offset = 0;
    ctx->send_parity = false;
    ctx->state = BWACP_STATE_SENDING;

    atomic_fetch_and(&ctx->isr_flags, ~BWACP_ISR_FLAG_REPEAT_RECEIVED);
    return ARTIE_CAN_ERR_NONE;
}

/**
 * @brief Process COMPLETE frame received in ISR (main thread handler).
 */
static artie_can_error_t _process_complete_received(artie_can_backend_t *handle)
{
    bwacp_context_t *ctx = &handle->context->bwacp_context;
    artie_can_error_t err = ARTIE_CAN_ERR_NONE;

    // Verify CRC
    if ((ctx->receive_buffer != NULL) && (ctx->receive_bytes_written > 0))
    {
        uint32_t calculated_crc = _calculate_crc24(ctx->receive_buffer, ctx->receive_bytes_written);
        if (calculated_crc != ctx->receive_crc24)
        {
            // Set up the receive state
            ctx->receive_bytes_written = 0;
            ctx->receive_crc24 = 0;
            ctx->state = BWACP_STATE_RECEIVING;

            ARTIE_CAN_LOG(handle->context, "BWACP: CRC mismatch, requesting full retransmission\n");
            err = _send_repeat(handle);
        }
        else
        {
            ARTIE_CAN_LOG(handle->context, "BWACP: Transfer complete and verified\n");

            // Save this transfer info for cooldown period
            ctx->last_completed_sender_address = ctx->receive_sender_address;
            ctx->last_completed_receive_address = ctx->receive_address;
            ctx->last_completed_timestamp_ms = handle->get_ms();

            // Return to IDLE - we can receive new transfers immediately
            ctx->sending_node_address = 0xFF;
            ctx->state = BWACP_STATE_IDLE;
        }
    }
    atomic_fetch_and(&ctx->isr_flags, ~BWACP_ISR_FLAG_COMPLETE_RECEIVED);
    return err;
}

static artie_can_error_t _check_timeout_receiving(artie_can_backend_t *handle)
{
    bwacp_context_t *ctx = &handle->context->bwacp_context;
    uint64_t elapsed = handle->get_ms() - ctx->last_packet_ms;
    if (elapsed >= ARTIE_CAN_BWACP_TIMEOUT_MS)
    {
        ARTIE_CAN_LOG(handle->context, "BWACP: Transfer timeout\n");
        ctx->sending_node_address = 0xFF;
        ctx->state = BWACP_STATE_IDLE;
        return ARTIE_CAN_ERR_TIMEOUT;
    }
    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _check_timeout_waiting_complete(artie_can_backend_t *handle)
{
    bwacp_context_t *ctx = &handle->context->bwacp_context;
    uint64_t elapsed = handle->get_ms() - ctx->last_packet_ms;
    if (elapsed >= ARTIE_CAN_BWACP_REPEAT_REQUEST_TIMEOUT_MS)
    {
        ARTIE_CAN_LOG(handle->context, "BWACP: Finished waiting for REPEATs. Transfer is complete.\n");
        ctx->state = BWACP_STATE_IDLE;

        // No error here - the timeout is a normal part of operation
    }
    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_init_context_bwacp(artie_can_context_t *context, uint8_t node_address, uint8_t node_class)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    if (node_address >= ARTIE_CAN_BWACP_MULTICAST_ADDRESS)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Initialize BWACP context
    memset(&context->bwacp_context, 0, sizeof(bwacp_context_t));
    context->bwacp_context.node_address = node_address;
    context->bwacp_context.node_class = node_class;
    context->bwacp_context.state = BWACP_STATE_IDLE;
    context->bwacp_context.sending_node_address = 0xFF;

    // Enable BWACP protocol
    context->protocol_flags |= ARTIE_CAN_PROTOCOL_FLAG_BWACP;
    context->node_address = node_address;

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_bwacp_set_receive_buffer(artie_can_context_t *context, uint8_t *buffer, uint32_t buffer_size)
{
    if (context == NULL || buffer == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    context->bwacp_context.receive_buffer = buffer;
    context->bwacp_context.receive_buffer_size = buffer_size;

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_bwacp_send(artie_can_backend_t *handle, const uint8_t *payload, uint32_t payload_size, uint32_t address, uint8_t target_address, uint8_t target_class, artie_can_frame_priority_bwacp_t priority)
{
    if (handle == NULL || payload == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    bwacp_context_t *ctx = &handle->context->bwacp_context;

    // Check if we're already busy
    if (ctx->state != BWACP_STATE_IDLE)
    {
        return ARTIE_CAN_ERR_SEND_BUSY;
    }

    // Initialize send state
    ctx->send_payload = payload;
    ctx->send_payload_size = payload_size;
    ctx->send_payload_offset = 0;
    ctx->send_address = address;
    ctx->send_target_address = target_address;
    ctx->send_target_class = target_class;
    ctx->send_parity = false;
    ctx->state = BWACP_STATE_SENDING;

    // Send READY frame
    artie_can_error_t err = _send_ready(handle, payload, payload_size, address, target_address, target_class, priority, false);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        ctx->state = BWACP_STATE_IDLE;
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

void bwacp_receive_in_isr(artie_can_context_t *context, const artie_can_frame_t *frame)
{
    if ((context->protocol_flags & ARTIE_CAN_PROTOCOL_FLAG_BWACP) == 0)
    {
        return;
    }

    // Extract frame type
    uint8_t frame_type = (frame->id & ARTIE_CAN_FRAME_ID_FRAME_TYPE_MASK) >> ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION;

    // Dispatch based on frame type
    switch (frame_type)
    {
        case ARTIE_CAN_FRAME_TYPE_BWACP_READY:
            _process_ready_frame_from_isr(context, frame);
            break;

        case ARTIE_CAN_FRAME_TYPE_BWACP_DATA:
            _process_data_frame_from_isr(context, frame);
            break;

        case ARTIE_CAN_FRAME_TYPE_BWACP_COMPLETE:
            _process_complete_frame_from_isr(context, frame);
            break;

        case ARTIE_CAN_FRAME_TYPE_BWACP_REPEAT:
            _process_repeat_frame_from_isr(context, frame);
            break;

        default:
            break;
    }
}

artie_can_error_t bwacp_tick(artie_can_backend_t *handle)
{
    if ((handle == NULL) || (handle->context == NULL))
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    bwacp_context_t *ctx = &handle->context->bwacp_context;
    artie_can_error_t err = ARTIE_CAN_ERR_NONE;

    // Process ISR flags
    if ((ctx->isr_flags & BWACP_ISR_FLAG_READY_RECEIVED) != 0)
    {
        err |= _process_ready_received(handle);
    }

    if ((ctx->isr_flags & BWACP_ISR_FLAG_DATA_RECEIVED) != 0)
    {
        err |= _process_data_received(handle);
    }

    if ((ctx->isr_flags & BWACP_ISR_FLAG_REPEAT_RECEIVED) != 0)
    {
        err |= _process_repeat_received(handle);
    }

    if ((ctx->isr_flags & BWACP_ISR_FLAG_COMPLETE_RECEIVED) != 0)
    {
        err |= _process_complete_received(handle);
    }

    // State machine
    switch (ctx->state)
    {
        case BWACP_STATE_SENDING:
            // Continue sending DATA frames
            err |= _send_next_chunk(handle);
            break;
        case BWACP_STATE_WAITING_COMPLETE:
            err |= _check_timeout_waiting_complete(handle);
            break;
        case BWACP_STATE_RECEIVING:
            err |= _check_timeout_receiving(handle);
            break;
        case BWACP_STATE_IDLE:
            break;
        default:
            break;
    }

    return err;
}
