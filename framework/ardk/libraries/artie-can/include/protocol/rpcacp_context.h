/**
 * @file rpcacp_context.h
 * @brief Definitions for RPCACP context and related functions in the Artie CAN library.
 *
 * This header defines the state machine and data structures required to manage
 * Remote Procedure Calls (RPCs) across the Artie CAN network, supporting both
 * synchronous and asynchronous execution modes.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include "frame.h"
#include "translationlayer.h"
#include "rpcacp.h"

/**
 * @brief States that the RPCACP state machine can be in for a given node.
 *
 * Tracks whether the node is initiating a call, waiting for a result,
 * or currently executing a procedure requested by another node.
 */
typedef enum {
    RPCACP_STATE_IDLE,              ///< The node is idle and not processing any RPC.
    RPCACP_STATE_SENDING_START_REQUEST,   ///< The node is sending an RPC request to another node.
    RPCACP_STATE_WAITING_ACK,        ///< The node has sent a request and is waiting for an acknowledgment.
    RPCACP_STATE_SENDING_TXDATA,        ///< The node is sending the MsgPack-encoded payload for the RPC.
    RPC_STATE_WAITING_RESPONSE,     ///< The caller thread is blocked waiting for a synchronous result.
    RPCACP_STATE_EXECUTING_PROC,    ///< The node is actively processing a procedure requested by another node.
    RPCACP_STATE_SENDING_START_RETURN,
    RPCACP_STATE_SENDING_RXDATA,
} rpcacp_state_t;

/**
 * @brief ISR flags for RPCACP protocol.
 */
typedef enum {
    RPCACP_ISR_FLAG_NONE = 0,                   ///< No special conditions for this ISR call
    RPCACP_ISR_FLAG_REQUEST_RECEIVED = 1 << 0,  ///< An RPC request frame was received in the ISR
    RPCACP_ISR_FLAG_RESPONSE_RECEIVED = 1 << 1, ///< An RPC response frame was received in the ISR
    RPCACP_ISR_FLAG_TIMEOUT_OCCURRED = 1 << 2,  ///< A synchronous call timeout has occurred
} rpcacp_isr_flags_t;

/**
 * @brief Context for RPCACP protocol handling within the Artie CAN library.
 *
 * Contains the state of the local node's RPC engine and tracking for
 * network-wide asynchronous operations.
 */
typedef struct {
    // Written to by main thread
    uint8_t node_address;                    ///< The Artie CAN bus address of this Node
    uint8_t node_class;                      ///< The class bitmask of this Node
    rpcacp_state_t state;                    ///< The current state of the RPCACP protocol for this node
    uint64_t last_request_ms;                ///< Timestamp of the latest RPC event (request or response)

    // Calling/Initiating side (when the local node is the caller)
    uint16_t active_procedure_id;            ///< The ID of the procedure currently being called by this node
    bool is_synchronous;                     ///< true if the local node is blocking for a response
    const struct RpcSignature *active_signature; ///< Pointer to the signature being used for the current call
    uint64_t expected_response_timestamp;     ///< Timestamp when the response is expected (for sync calls)

    // Network-wide tracking
    uint64_t busy_nodes_bitmap;              ///< Bitmap of nodes in the network currently executing an async RPC (bit N = node address N)
    uint64_t active_requests_bitmap;         ///< Bitmap of nodes that have pending asynchronous requests directed to this node

    // Response/Execution side (when the local node is the responder)
    const uint8_t *response_payload_ptr;     ///< Pointer to the MsgPack-encoded response data (owned by caller)
    uint32_t response_payload_size;          ///< Size of the response payload
    uint32_t response_offset;                ///< Offset within the response stream
    uint8_t sender_address;                  ///< Address of the node that initiated the RPC

    // Error tracking
    artie_can_error_t last_rpc_error;        ///< The last error encountered during an RPC operation (e.g., EINVAL)

    // Written to by ISR (each ISR type has its own dedicated frame)
    artie_can_frame_t received_request_frame;   ///< Most recently received RPC request frame
    artie_can_frame_t received_response_frame;  ///< Most recently received RPC response frame
    atomic_uint32_t isr_flags;                  ///< Flags to indicate special conditions found during ISR; cleared by main thread (atomic operations required)
} rpcacp_context_t;
