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

/** Timeout in ms for BWACP operations */
#define ARTIE_CAN_BWACP_TIMEOUT_MS 5000U

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
 * @param address Application-specific address (4 bytes).
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
