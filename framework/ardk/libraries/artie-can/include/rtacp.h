/**
 * @file rtacp.h
 * @brief Header file for Artie CAN RTACP (Real Time Artie CAN Protocol) implementation.
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "backend.h"
#include "context.h"
#include "err.h"
#include "frame.h"

/** Address to use for broadcast messages in RTACP */
#define ARTIE_CAN_RTACP_TARGET_ADDRESS_BROADCAST 0U

/** Maximum number of data bytes in an RTACP frame */
#define ARTIE_CAN_RTACP_MAX_DATA_BYTES 8U

/** The RTACP protocol ID */
#define ARTIE_CAN_RTACP_PROTOCOL_ID 0x00U

/**
 * @brief Enumeration for RTACP frame priorities.
 *
 */
typedef enum {
    ARTIE_CAN_FRAME_PRIORITY_RTACP_LOW = 3,       ///< Low priority frame
    ARTIE_CAN_FRAME_PRIORITY_RTACP_MEDIUM = 2,    ///< Medium priority frame
    ARTIE_CAN_FRAME_PRIORITY_RTACP_HIGH = 1,      ///< High priority frame
    ARTIE_CAN_FRAME_PRIORITY_RTACP_HIGHEST = 0,   ///< Highest priority frame
} artie_can_frame_priority_rtacp_t;

/**
 * @brief Structure representing an RTACP frame, as opposed to the more general artie_can_frame_t.
 *
 */
typedef struct {
    bool ack;                                       ///< Whether this is an ACK frame
    artie_can_frame_priority_rtacp_t priority;      ///< Priority of the frame within the RTACP protocol
    uint8_t source_address;                         ///< Source address of the frame
    uint8_t target_address;                         ///< Target address of the frame (or ARTIE_CAN_RTACP_TARGET_ADDRESS_BROADCAST for broadcast)
    uint8_t nbytes;                                 ///< Number of bytes of data in the message (0-8)
    uint8_t data[ARTIE_CAN_RTACP_MAX_DATA_BYTES];   ///< Data bytes of the message (up to 8 bytes)
} artie_can_frame_rtacp_t;

/**
 * @brief Initialize an RTACP context with the specified source address.
 *
 * @param ctx Pointer to the artie_can_context_t struct to initialize.
 * @param source_address Source address to use for the RTACP context.
 * @return artie_can_error_t Error code indicating the result of the initialization.
 */
artie_can_error_t artie_can_init_context_rtacp(artie_can_context_t *ctx, uint8_t source_address);

/**
 * @brief Initialize an RTACP frame with the appropriate headers and metadata for a given backend and frame.
 *
 * @param out Pointer to the artie_can_frame_t struct representing the frame to initialize.
 * @param in Pointer to the artie_can_frame_rtacp_t struct representing the RTACP frame to use for initialization.
 * All data in the 'in' struct will be copied into the 'out' struct appropriately,
 * and the data in the 'in' struct will no longer be needed after this function returns, so the caller can free or reuse it if desired.
 * @return Error code indicating the result of the operation.
 *
 */
artie_can_error_t artie_can_rtacp_init_frame(artie_can_frame_t *out, const artie_can_frame_rtacp_t *in);

/**
 * @brief Parse a received CAN frame into the RTACP format, extracting the relevant metadata and data bytes.
 *
 * @param in Pointer to the artie_can_frame_t struct representing the received CAN frame.
 * @param out Pointer to the artie_can_frame_rtacp_t struct where the parsed RTACP frame will be stored.
 * All data in the 'in' struct will be parsed and copied into the 'out' struct appropriately, and the data
 * in the 'in' struct will no longer be needed after this function returns, so the caller can free or reuse it if desired.
 * @return Error code indicating the result of the operation.
 */
artie_can_error_t artie_can_rtacp_parse_frame(const artie_can_frame_t *in, artie_can_frame_rtacp_t *out);

/**
 * @brief Send an RTACP frame using the specified backend. Puts the frame into the backend's
 * RTACP state machine, which will handle the ACK and retransmission logic.
 * This function will block until the frame is sent and acknowledged (if it's not a broadcast frame).
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @param frame Pointer to the artie_can_frame_t struct representing the frame to send.
 * @return Error code indicating the result of the operation.
 */
artie_can_error_t rtacp_send(artie_can_backend_t *handle, const artie_can_frame_t *frame);

/**
 * @brief Handle a received RTACP frame within an ISR context.
 * This function will be called by the backend when a new frame is received that matches the RTACP protocol.
 *
 * @param context Pointer to the artie_can_context_t struct representing the context.
 * @param frame Pointer to the artie_can_frame_t struct representing the received frame.
 * This should be copied out of the backend's receive buffer and into the RTACP state machine for processing.
 */
void rtacp_receive_in_isr(artie_can_context_t *context, const artie_can_frame_t *frame);
