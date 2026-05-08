/**
 * @file bwacp_context.h
 * @brief Definitions for BWACP context and related functions in the Artie CAN library.
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "frame.h"
#include "translationlayer.h"

/** Maximum payload size for BWACP (64 KB) */
#define ARTIE_CAN_BWACP_MAX_PAYLOAD_SIZE (65536U)

/** Size of the circular buffer for DATA frames */
#define ARTIE_CAN_BWACP_DATA_FRAME_BUFFER_SIZE (16U)

/**
 * @brief States that the BWACP state machine can be in for a given node.
 *
 */
typedef enum {
    BWACP_STATE_IDLE,              ///< The node is idle and not currently processing any block write.
    BWACP_STATE_SENDING,           ///< The node is sending a block write.
    BWACP_STATE_RECEIVING,         ///< The node is receiving a block write.
    BWACP_STATE_WAITING_COMPLETE,  ///< The node has finished sending and is waiting for receivers to acknowledge completion or request repeat.
    BWACP_STATE_RECEIVE_COOLDOWN   ///< The node has finished receiving and is in a cooldown period before it can receive another block write from the same sender to the same target address (to allow for REPEAT requests to be received after COMPLETE).
} bwacp_state_t;

/**
 * @brief ISR flags for BWACP protocol.
 */
typedef enum {
    BWACP_ISR_FLAG_NONE = 0,                 ///< No special conditions for this ISR call
    BWACP_ISR_FLAG_READY_RECEIVED = 1 << 0,  ///< A READY frame was received in the ISR
    BWACP_ISR_FLAG_DATA_RECEIVED = 1 << 1,   ///< A DATA frame was received in the ISR
    BWACP_ISR_FLAG_COMPLETE_RECEIVED = 1 << 2, ///< A COMPLETE frame was received in the ISR
    BWACP_ISR_FLAG_REPEAT_RECEIVED = 1 << 3,  ///< A REPEAT frame was received in the ISR
} bwacp_isr_flags_t;

/**
 * @brief Context for BWACP protocol handling within the Artie CAN library.
 *
 */
typedef struct {
    // Written to by main thread
    uint8_t node_address;                   ///< The BWACP address of this node on the CAN bus
    uint8_t node_class;                     ///< The class bitmask of this node (bit 0=SBC, bit 1=MCU, bit 2=Sensor, bit 3=Motor, bits 4-5=Reserved)
    bwacp_state_t state;                    ///< The current state of the BWACP protocol for this node
    uint64_t last_packet_ms;                ///< The time in milliseconds when the latest packet has been received

    // Sending state
    const uint8_t *send_payload;            ///< Pointer to the payload being sent (owned by caller)
    uint32_t send_payload_size;             ///< Size of the payload being sent
    uint32_t send_payload_offset;           ///< Current offset in the send payload
    uint32_t send_address;                  ///< Application-specific address for the write
    uint8_t send_target_address;            ///< Target node address (or 0x3F for multicast)
    uint8_t send_target_class;              ///< Target class bitmask (for multicast)
    bool send_parity;                       ///< Current parity bit for DATA frames
    uint32_t send_crc24;                    ///< CRC24 over the payload

    // Receiving state
    uint8_t *receive_buffer;                ///< Buffer for receiving data (owned by caller, must be provided before receiving)
    uint32_t receive_buffer_size;           ///< Size of the receive buffer
    uint32_t receive_bytes_written;         ///< Number of bytes written to the receive buffer so far
    uint32_t receive_address;               ///< Application-specific address for the received write
    uint8_t receive_sender_address;         ///< Address of the sender
    bool receive_expected_parity;           ///< Expected parity bit for next DATA frame
    uint32_t receive_crc24;                 ///< Expected CRC24 for the received data
    bool receive_ready_interrupt;           ///< Whether the READY frame had the interrupt bit set
    uint8_t sending_node_address;           ///< The address of the node we are receiving from

    // Circular buffer for DATA frames (written by ISR, read by main thread)
    artie_can_frame_t data_frame_buffer[ARTIE_CAN_BWACP_DATA_FRAME_BUFFER_SIZE]; ///< Circular buffer for received DATA frames
    uint32_t data_frame_read_index;         ///< Read index for circular buffer (modified only by main thread)
    uint32_t data_frame_write_index;        ///< Write index for circular buffer (modified only by ISR)
    atomic_uint32_t data_frames_pending;    ///< Number of DATA frames pending processing (atomic: incremented by ISR, decremented by main thread)

    // Written to by ISR (each ISR type has its own dedicated frame)
    artie_can_frame_t received_ready_frame;     ///< Most recently received READY frame (for processing in main thread)
    artie_can_frame_t received_complete_frame;  ///< Most recently received COMPLETE frame (for processing in main thread)
    artie_can_frame_t received_repeat_frame;    ///< Most recently received REPEAT frame (for processing in main thread)
    atomic_uint32_t isr_flags;              ///< Flags to indicate special conditions found during ISR; cleared by main thread (atomic operations required)
} bwacp_context_t;
