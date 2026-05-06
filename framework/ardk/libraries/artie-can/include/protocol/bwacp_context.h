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

/**
 * @brief States that the BWACP state machine can be in for a given node.
 *
 */
typedef enum {
    BWACP_STATE_IDLE,              ///< The node is idle and not currently processing any block write.
    BWACP_STATE_SENDING,           ///< The node is sending a block write.
    BWACP_STATE_RECEIVING,         ///< The node is receiving a block write.
    BWACP_STATE_WAITING_COMPLETE,  ///< The node has finished sending and is waiting for receivers to acknowledge completion or request repeat.
} bwacp_state_t;

/**
 * @brief ISR flags for BWACP protocol.
 */
typedef enum {
    BWACP_ISR_FLAG_NONE = 0,                 ///< No special conditions for this ISR call
    BWACP_ISR_FLAG_PENDING_REPEAT = 1 << 0,  ///< A REPEAT frame needs to be sent from main thread
    BWACP_ISR_FLAG_READY_RECEIVED = 1 << 1,  ///< A READY frame was received in the ISR
    BWACP_ISR_FLAG_DATA_RECEIVED = 1 << 2,   ///< A DATA frame was received in the ISR
    BWACP_ISR_FLAG_COMPLETE_RECEIVED = 1 << 3, ///< A COMPLETE frame was received in the ISR
    BWACP_ISR_FLAG_REPEAT_RECEIVED = 1 << 4,  ///< A REPEAT frame was received in the ISR
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
    uint64_t transfer_start_time_ms;        ///< The time in milliseconds when the transfer started

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

    // Written to by ISR
    artie_can_frame_t received_frame;       ///< Most recently received frame (for processing in main thread)
    atomic_uint32_t isr_flags;              ///< Flags to indicate special conditions found during ISR; cleared by main thread (atomic operations required)
} bwacp_context_t;
