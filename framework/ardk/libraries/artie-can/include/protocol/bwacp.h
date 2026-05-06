/**
 * @file bwacp.h
 * @brief Header file for Artie CAN BWACP (Block Write Artie CAN Protocol) implementation.
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "backend.h"
#include "context.h"
#include "err.h"
#include "frame.h"

/** The BWACP protocol ID. */
#define ARTIE_CAN_BWACP_PROTOCOL_ID 0x05U

/** Multicast address (target address = 0x3F) */
#define ARTIE_CAN_BWACP_MULTICAST_ADDRESS 0x3FU

/** Timeout in ms for waiting for the next packet */
#define ARTIE_CAN_BWACP_TIMEOUT_MS 5000U

/** Timeout in ms for waiting for REPEAT frames after transfer completes */
#define ARTIE_CAN_BWACP_REPEAT_REQUEST_TIMEOUT_MS 1000U

// BWACP READY frame data layout offsets
/** Offset of CRC24 byte 0 (MSB) in READY frame data */
#define BWACP_READY_DATA_CRC24_BYTE0 0U
/** Offset of CRC24 byte 1 in READY frame data */
#define BWACP_READY_DATA_CRC24_BYTE1 1U
/** Offset of CRC24 byte 2 (LSB) in READY frame data */
#define BWACP_READY_DATA_CRC24_BYTE2 2U
/** Offset of address byte 0 (MSB) in READY frame data */
#define BWACP_READY_DATA_ADDRESS_BYTE0 3U
/** Offset of address byte 1 in READY frame data */
#define BWACP_READY_DATA_ADDRESS_BYTE1 4U
/** Offset of address byte 2 in READY frame data */
#define BWACP_READY_DATA_ADDRESS_BYTE2 5U
/** Offset of address byte 3 (LSB) in READY frame data */
#define BWACP_READY_DATA_ADDRESS_BYTE3 6U
/** Offset of stuffing byte in READY frame data */
#define BWACP_READY_DATA_STUFFING 7U

// Bit shift amounts for multi-byte field extraction/packing
/** Bit shift for most significant byte of 32-bit value */
#define BWACP_SHIFT_BYTE0 24U
/** Bit shift for second byte of 32-bit value */
#define BWACP_SHIFT_BYTE1 16U
/** Bit shift for third byte of 32-bit value */
#define BWACP_SHIFT_BYTE2 8U
/** Bit shift for least significant byte */
#define BWACP_SHIFT_BYTE3 0U

// CRC24 specific shifts
/** Bit shift for CRC24 MSB (byte 0) */
#define BWACP_CRC24_SHIFT_BYTE0 16U
/** Bit shift for CRC24 middle byte (byte 1) */
#define BWACP_CRC24_SHIFT_BYTE1 8U
/** Bit shift for CRC24 LSB (byte 2) */
#define BWACP_CRC24_SHIFT_BYTE2 0U

// Location of BWACP-specific bits in ID field
/** Location of class bits in ID field (6 bits for target class) */
#define BWACP_FRAME_ID_CLASS_LOCATION 7U
/** Mask for class bits in ID field */
#define BWACP_FRAME_ID_CLASS_MASK (0x3FU << BWACP_FRAME_ID_CLASS_LOCATION)

/** Location of repeat/interrupt bit in ID field */
#define BWACP_FRAME_ID_REPEAT_INTERRUPT_LOCATION 1U
/** Mask for repeat/interrupt bit in ID field */
#define BWACP_FRAME_ID_REPEAT_INTERRUPT_MASK (0x01U << BWACP_FRAME_ID_REPEAT_INTERRUPT_LOCATION)

/** Location of parity bit in ID field */
#define BWACP_FRAME_ID_PARITY_LOCATION 0U
/** Mask for parity bit in ID field */
#define BWACP_FRAME_ID_PARITY_MASK (0x01U << BWACP_FRAME_ID_PARITY_LOCATION)

/**
 * @brief Enumeration for BWACP frame types.
 *
 */
typedef enum {
    ARTIE_CAN_FRAME_TYPE_BWACP_REPEAT = 0x01,    ///< REPEAT frame (0001)
    ARTIE_CAN_FRAME_TYPE_BWACP_READY = 0x03,     ///< READY frame (0011)
    ARTIE_CAN_FRAME_TYPE_BWACP_COMPLETE = 0x05,  ///< COMPLETE frame (0101)
    ARTIE_CAN_FRAME_TYPE_BWACP_DATA = 0x07,      ///< DATA frame (0111)
} artie_can_frame_type_bwacp_t;

/**
 * @brief Enumeration for BWACP frame priorities.
 *
 */
typedef enum {
    ARTIE_CAN_FRAME_PRIORITY_BWACP_LOW = 3,       ///< Low priority frame
    ARTIE_CAN_FRAME_PRIORITY_BWACP_MEDIUM = 2,    ///< Medium priority frame
    ARTIE_CAN_FRAME_PRIORITY_BWACP_HIGH = 1,      ///< High priority frame
    ARTIE_CAN_FRAME_PRIORITY_BWACP_HIGHEST = 0,   ///< Highest priority frame
} artie_can_frame_priority_bwacp_t;

/**
 * @brief Node class bit definitions for BWACP multicast.
 *
 */
typedef enum {
    ARTIE_CAN_BWACP_CLASS_SBC = 1 << 0,     ///< Single Board Computer
    ARTIE_CAN_BWACP_CLASS_MCU = 1 << 1,     ///< Microcontroller Unit
    ARTIE_CAN_BWACP_CLASS_SENSOR = 1 << 2,  ///< Sensor Node
    ARTIE_CAN_BWACP_CLASS_MOTOR = 1 << 3,   ///< Motor Node
    // Bits 4-5 reserved
} artie_can_bwacp_class_t;

/**
 * @brief Initialize a BWACP context with the specified node address and class.
 *
 * @param context Pointer to the artie_can_context_t struct to initialize.
 * @param node_address Node address to use for the BWACP context (0-62, 63 is reserved for multicast).
 * @param node_class Class bitmask for this node (combination of artie_can_bwacp_class_t values).
 * @return artie_can_error_t Error code indicating the result of the initialization.
 */
artie_can_error_t artie_can_init_context_bwacp(artie_can_context_t *context, uint8_t node_address, uint8_t node_class);

/**
 * @brief Set the receive buffer for BWACP. This must be called before receiving any block writes.
 * The buffer must remain valid for the duration of any block write reception.
 *
 * @param context Pointer to the artie_can_context_t struct.
 * @param buffer Pointer to the buffer where received data will be stored.
 * @param buffer_size Size of the buffer in bytes.
 * @return artie_can_error_t Error code indicating the result of the operation.
 */
artie_can_error_t artie_can_bwacp_set_receive_buffer(artie_can_context_t *context, uint8_t *buffer, uint32_t buffer_size);

/**
 * @brief Start a BWACP block write to a single node or multicast to a class of nodes.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @param payload Pointer to the payload data to send. Must remain valid until the block write completes.
 * @param payload_size Size of the payload in bytes.
 * @param address Buffer offset (0 to buffer_size - payload_size) where the receiving node(s) should write the data.
 * @param target_address Target node address (0-62), or ARTIE_CAN_BWACP_MULTICAST_ADDRESS for multicast.
 * @param target_class If multicast, the class bitmask to target. Otherwise, ignored.
 * @param priority Priority for the block write frames.
 * @return artie_can_error_t Error code indicating the result of the operation.
 */
artie_can_error_t artie_can_bwacp_send(artie_can_backend_t *handle, const uint8_t *payload, uint32_t payload_size,
                                       uint32_t address, uint8_t target_address, uint8_t target_class,
                                       artie_can_frame_priority_bwacp_t priority);

/**
 * @brief Handle a received BWACP frame within an ISR context.
 * This function will be called by the backend when a new frame is received that matches the BWACP protocol.
 *
 * @param context Pointer to the artie_can_context_t struct representing the context.
 * @param frame Pointer to the artie_can_frame_t struct representing the received frame.
 */
void bwacp_receive_in_isr(artie_can_context_t *context, const artie_can_frame_t *frame);

/**
 * @brief Tick function for the BWACP protocol. This function should be called periodically
 * to allow the BWACP state machine to process incoming frames, timeouts, and other protocol-specific tasks.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @return artie_can_error_t Error code indicating the result of the operation.
 */
artie_can_error_t bwacp_tick(artie_can_backend_t *handle);
