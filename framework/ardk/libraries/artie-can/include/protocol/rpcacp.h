/**
 * @file rpcacp.h
 * @brief Header file for Artie CAN RPCACP (Remote Procedure Call Artie CAN Protocol) implementation.
 *
 * This protocol allows nodes to invoke procedures on other nodes via the Artie CAN bus,
 * using a structured signature system and MsgPack-encoded payloads.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "backend.h"
#include "context.h"
#include "err.h"
#include "frame.h"

/** The RPCACP protocol ID. */
#define ARTIE_CAN_RPCACP_PROTOCOL_ID 0x02U

// RPC Signature Constants
/** Maximum number of parameters allowed in a single RPC signature. */
#define ARTIE_CAN_RPCACP_MAX_PARAMS 15U

// Standard Procedure IDs (Shared across all compliant devices)
#define ARTIE_CAN_RPC_ID_WHOAMI    0x00U
#define ARTIE_CAN_RPC_ID_STATUS    0x01U
#define ARTIE_CAN_RPC_ID_LIST      0x02U

// Void * return type signature. The void * return should be cast according to the return_descriptor in the signature.
typedef void *(*artie_can_rpc_function_t)(const void **params, uint8_t param_count, void *return_buffer, size_t return_buffer_size);

/**
 * @brief Describes a single parameter within an RPC call.
 *
 * Used to facilitate serialization/deserialization of the MsgPack payload.
 */
typedef struct {
    char *type_name;                 // Type name (e.g., "uint8_t", "string") or "NULL"
    uint8_t offset_in_msgpack;       // Offset within the encoded data stream
    bool optional;                   // true if parameter may be omitted
} artie_can_rpc_param_descriptor_t;

/**
 * @brief Defines a full RPC signature, allowing nodes to discover available procedures.
 */
typedef struct {
    uint16_t procedure_id;                // 7-bit field mapped to the protocol frame ID
    char *name;                           // Human-readable name for debugging
    bool synchronous;                     // true if the caller must block until response
    uint8_t param_count;                  // Number of parameters defined in this signature
    artie_can_rpc_param_descriptor_t params[ARTIE_CAN_RPCACP_MAX_PARAMS]; // Parameter array
    artie_can_rpc_function_t *function;   // Actual function to call when the RPC is invoked on this node (NULL if not implemented)
    artie_can_rpc_param_descriptor_t *return_descriptor; // Descriptor for the return value (NULL if void)
} artie_can_rpc_signature_t;

/**
 * @brief Response structure for the WHOAMI procedure.
 */
typedef struct {
    char *node_name;        // Human-readable name of the responding node
    uint8_t node_address;   // Artie CAN bus address of this Node
    char *fw_version;       // Firmware version running on this Node
} artie_can_whoami_response_t;

/**
 * @brief Response structure for the STATUS procedure.
 */
typedef struct {
    uint64_t uptime_ms;     // Total runtime in milliseconds
    uint32_t err_flags;     // Bit mask of node-specific error flags
} artie_can_status_response_t;

/**
 * @brief Initializes the RPCACP context for a given node.
 *
 * @param context Pointer to the artie_can_context_t struct representing the context.
 * @param node_address The Artie CAN bus address of the node.
 * @param node_class The class bitmask of the node.
 * @return artie_can_error_t Error code indicating the result of the initialization.
 */
artie_can_error_t artie_can_init_context_rpcacp(artie_can_context_t *context, uint8_t node_address, uint8_t node_class);

/**
 * @brief Sets the receive buffer for the RPCACP context.
 *
 * @param context Pointer to the artie_can_context_t struct representing the context.
 * @param buffer Pointer to the buffer to be used for receiving RPC data.
 * @param buffer_size Size of the receive buffer.
 * @return artie_can_error_t Error code indicating the result of the operation.
 */
artie_can_error_t artie_can_rpcacp_set_receive_buffer(artie_can_context_t *context, uint8_t *buffer, uint32_t buffer_size);

/**
 * @brief API function to initiate an RPC call from a node.
 *
 * This function prepares and transmits the request frame (containing the signature/params).
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @param procedure_id The ID of the procedure being invoked.
 * @param params Pointer to an array of artie_can_rpc_param_descriptor_t structs.
 * @param param_count Number of descriptors in the params array.
 * @param synchronous Indicates if the caller should block for a response.
 * @return artie_can_error_t Error code indicating the result of the call initiation.
 */
artie_can_error_t artie_can_rpcacp_call(artie_can_backend_t *handle, uint16_t procedure_id,
                                        const artie_can_rpc_param_descriptor_t *params, uint8_t param_count,
                                        bool synchronous);

/**
 * @brief Handle a received RPCACP frame within an ISR context.
 * This function will be called by the backend when a new frame is received that matches the RPCACP protocol.
 *
 * @param context Pointer to the artie_can_context_t struct representing the context.
 * @param frame Pointer to the artie_can_frame_t struct representing the received frame.
 */
void rpcacp_receive_in_isr(artie_can_context_t *context, const artie_can_frame_t *frame);

/**
 * @brief API function to check if a node is currently processing an asynchronous RPC.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @return true if the node is busy executing an async procedure, false otherwise.
 */
bool artie_can_rpcacp_is_busy(artie_can_backend_t *handle);

/**
 * @brief Tick function for the RPCACP protocol. This function should be called periodically
 * to allow the RPCACP state machine to process incoming frames, timeouts, and other protocol-specific tasks.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @return artie_can_error_t Error code indicating the result of the operation.
 */
artie_can_error_t rpcacp_tick(artie_can_backend_t *handle);

/**
 * @brief Register a new RPC procedure with the backend. When a frame is received
 * in an ISR that completes an RPC procedure request, the backend will call the registered procedure's
 * Note that all compliant devices should implement the standard procedures (WHOAMI, STATUS, LIST) with the specified signatures.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @param signature Pointer to the RPC signature descriptor.
 * @return artie_can_error_t Error code indicating the result of the registration.
 */
artie_can_error_t artie_can_rpcacp_register_procedure(artie_can_backend_t *handle, const artie_can_rpc_signature_t *signature);
