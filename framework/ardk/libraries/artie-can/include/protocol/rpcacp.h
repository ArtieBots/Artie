/**
 * @file rpcacp.h
 * @brief Header file for Artie CAN RPCACP (Remote Procedure Call Artie CAN Protocol) implementation.
 *
 * This protocol allows nodes to invoke procedures on other nodes via the Artie CAN bus,
 * using a structured signature system and MsgPack-encoded payloads. See
 * docs/specifications/CANProtocol.md, docs/specifications/RPCSchema.md, and
 * docs/specifications/MsgPackSchema.md for the full specification.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "backend.h"
#include "context.h"
#include "err.h"
#include "frame.h"
#include "rpcacp_context.h"

/** The RPCACP protocol ID. */
#define ARTIE_CAN_RPCACP_PROTOCOL_ID 0x02U

// Note: ARTIE_CAN_RPCACP_MAX_PARAMS, ARTIE_CAN_RPCACP_MAX_REGISTERED_PROCEDURES,
// ARTIE_CAN_RPCACP_MAX_NAME_LENGTH, ARTIE_CAN_RPCACP_MAX_STUFFED_PAYLOAD_SIZE, and
// ARTIE_CAN_RPCACP_MAX_PAYLOAD_SIZE, along with artie_can_rpc_param_descriptor_t,
// artie_can_rpc_function_t, and artie_can_rpc_signature_t, are defined in rpcacp_context.h
// (included above) to avoid a circular #include between this file and context.h.

// Standard Procedure IDs (Shared across all compliant devices; 0x00-0x0F is reserved)
#define ARTIE_CAN_RPC_ID_WHOAMI    0x00U ///< Identify the node (name, address, firmware version). Answered internally.
#define ARTIE_CAN_RPC_ID_STATUS    0x01U ///< Report node uptime and error flags. Answered internally.
#define ARTIE_CAN_RPC_ID_LIST      0x02U ///< Page through this node's registered RPC signatures. Answered internally.

/** Highest procedure ID that is reserved for standard/future use; device-specific IDs start after this. */
#define ARTIE_CAN_RPCACP_RESERVED_ID_MAX 0x0FU

/** Number of RpcSignature entries returned per LIST page. */
#define ARTIE_CAN_RPCACP_LIST_PAGE_SIZE 8U

/** Number of pages of RpcSignature entries (16 pages * 8 entries = 128 procedure IDs). */
#define ARTIE_CAN_RPCACP_LIST_PAGE_COUNT 16U

/** Name reported by LIST for a procedure_id that has not been assigned/registered. */
#define ARTIE_CAN_RPCACP_LIST_UNASSIGNED_NAME "UNASSIGNED"

// Errno-style codes used in the data byte of a NACK frame (see CANProtocol.md / RPCSchema.md)
#define ARTIE_CAN_RPCACP_ERRNO_TRANSMISSION 0x00U ///< Something went wrong in transmission; resend.
#define ARTIE_CAN_RPCACP_ERRNO_EPERM        0x01U ///< The requested procedure ID is not registered on this node.
#define ARTIE_CAN_RPCACP_ERRNO_E2BIG        0x07U ///< Argument list is too long / too many parameters.
#define ARTIE_CAN_RPCACP_ERRNO_ENOEXEC      0x08U ///< The request could not be unpacked correctly.
#define ARTIE_CAN_RPCACP_ERRNO_EAGAIN       0x0BU ///< Something transient; this node is busy, try again later.
#define ARTIE_CAN_RPCACP_ERRNO_EINVAL       0x16U ///< At least one argument is invalid, or the procedure failed.
#define ARTIE_CAN_RPCACP_ERRNO_EALREADY     0x72U ///< This exact request is already being worked on.

/** Timeout, in ms, for waiting on the ACK/NACK of a single in-flight RPCACP frame. */
#define ARTIE_CAN_RPCACP_ACK_TIMEOUT_MS 100U

/** Maximum number of times a single frame will be retried before the exchange is aborted. */
#define ARTIE_CAN_RPCACP_MAX_RETRIES 5U

/** Timeout, in ms, for a synchronous call waiting on the StartReturn/RxData sequence after its request was accepted. */
#define ARTIE_CAN_RPCACP_RESPONSE_TIMEOUT_MS 5000U

/**
 * Delay, in ms, between ACKing a fully-received synchronous request and sending the StartReturn
 * frame, so the requesting node has time to process the ACK (and transition to waiting for the
 * return value) before the return data starts arriving.
 */
#define ARTIE_CAN_RPCACP_RETURN_START_DELAY_MS 2U

// Location of the RPCACP-specific "random" traceability bits in the ID field (occupies the low 8 bits).
/** Location of the random/traceability bits in the ID field. */
#define RPCACP_FRAME_ID_RANDOM_LOCATION 0U
/** Mask for the random/traceability bits in the ID field. */
#define RPCACP_FRAME_ID_RANDOM_MASK (0xFFU << RPCACP_FRAME_ID_RANDOM_LOCATION)

/**
 * @brief Enumeration for RPCACP frame types.
 */
typedef enum {
    ARTIE_CAN_FRAME_TYPE_RPCACP_ACK = 0x00,          ///< ACK frame
    ARTIE_CAN_FRAME_TYPE_RPCACP_NACK = 0x01,         ///< NACK frame
    ARTIE_CAN_FRAME_TYPE_RPCACP_START_RPC = 0x02,    ///< StartRPC frame
    ARTIE_CAN_FRAME_TYPE_RPCACP_START_RETURN = 0x03, ///< StartReturn frame
    ARTIE_CAN_FRAME_TYPE_RPCACP_TX_DATA = 0x04,      ///< TxData frame
    ARTIE_CAN_FRAME_TYPE_RPCACP_RX_DATA = 0x05,      ///< RxData frame
} artie_can_frame_type_rpcacp_t;

/**
 * @brief Enumeration for RPCACP frame priorities.
 */
typedef enum {
    ARTIE_CAN_FRAME_PRIORITY_RPCACP_LOW = 3,         ///< Low priority frame
    ARTIE_CAN_FRAME_PRIORITY_RPCACP_MEDIUM_LOW = 2,  ///< Medium-low priority frame
    ARTIE_CAN_FRAME_PRIORITY_RPCACP_MEDIUM_HIGH = 1, ///< Medium-high priority frame
    ARTIE_CAN_FRAME_PRIORITY_RPCACP_HIGH = 0,        ///< High priority frame
} artie_can_frame_priority_rpcacp_t;

/**
 * @brief A single argument (or return) value to pack into/unpack from an RPC's MsgPack payload.
 *
 * `data` points at the value as it is represented natively in memory on this node (e.g. a
 * `uint32_t*`, a `float*`, a pointer to a fixed-size array, or a pointer to a struct), and
 * `size` is that native representation's size in bytes. Wire-narrowed types ("float" is 16-bit,
 * "double" is 32-bit on the wire) are converted automatically based on the matching parameter
 * descriptor's type_name; every other type is copied byte-for-byte.
 */
typedef struct {
    const void *data; ///< Pointer to the value's native, in-memory representation on this node
    uint32_t size;     ///< Size, in bytes, of the value's native representation
} artie_can_rpc_value_t;

/**
 * @brief Response structure for the WHOAMI procedure.
 */
typedef struct {
    char *node_name;        ///< Human-readable name of the responding node
    uint8_t node_address;   ///< Artie CAN bus address of this Node
    char *fw_version;       ///< Firmware version running on this Node
} artie_can_whoami_response_t;

/**
 * @brief Response structure for the STATUS procedure.
 */
typedef struct {
    uint64_t uptime_ms;     ///< Total runtime in milliseconds
    uint32_t err_flags;     ///< Bit mask of node-specific error flags
} artie_can_status_response_t;

/**
 * @brief Initializes the RPCACP context for a given node.
 *
 * WHOAMI, STATUS, and LIST (procedure IDs 0x00-0x0F) are always answered internally by the
 * library and cannot be registered via artie_can_rpcacp_register_procedure().
 *
 * @param context Pointer to the artie_can_context_t struct representing the context.
 * @param node_address The Artie CAN bus address of the node.
 * @param node_class The class bitmask of the node.
 * @param node_name Human-readable name of this node, reported by WHOAMI (copied, truncated to ARTIE_CAN_RPCACP_MAX_NAME_LENGTH).
 * @param fw_version Firmware version string, reported by WHOAMI (copied, truncated to ARTIE_CAN_RPCACP_MAX_NAME_LENGTH).
 * @return artie_can_error_t Error code indicating the result of the initialization.
 */
artie_can_error_t artie_can_init_context_rpcacp(artie_can_context_t *context, uint8_t node_address, uint8_t node_class, const char *node_name, const char *fw_version);

/**
 * @brief Update the error flags this node reports in response to STATUS.
 *
 * @param context Pointer to the artie_can_context_t struct representing the context.
 * @param err_flags Device-specific error flags bitmask to report from now on.
 * @return artie_can_error_t Error code indicating the result of the operation.
 */
artie_can_error_t artie_can_rpcacp_set_status_err_flags(artie_can_context_t *context, uint32_t err_flags);

/**
 * @brief Register a device-specific RPC procedure (ID 0x10-0x7F) with the backend.
 * When a request for this procedure_id is received (and fully validated), the registered
 * function will be invoked to service it.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @param signature Pointer to the RPC signature descriptor. This is copied into the registry.
 * @return artie_can_error_t Error code indicating the result of the registration.
 */
artie_can_error_t artie_can_rpcacp_register_procedure(artie_can_backend_t *handle, const artie_can_rpc_signature_t *signature);

/**
 * @brief API function to initiate an RPC call from a node. Returns immediately; the request is
 * driven to completion by repeated calls to rpcacp_tick(). Use artie_can_rpcacp_is_busy() to
 * poll for completion, artie_can_rpcacp_get_last_error() to check the outcome, and (for
 * synchronous calls) artie_can_rpcacp_get_result() or one of its standard-RPC counterparts to
 * retrieve the return value.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @param target_address The address of the node to call (0 is not allowed; there is no broadcast in RPCACP).
 * @param signature The signature of the procedure being invoked. Must remain valid for the duration of the call.
 * @param args Array of values to pack as arguments, one per non-omitted parameter in the signature. May be NULL if arg_count is 0.
 * @param arg_count Number of entries in args. Trailing parameters not covered by args must be marked optional in the signature.
 * @return artie_can_error_t Error code indicating the result of the call initiation (not the eventual RPC outcome).
 */
artie_can_error_t artie_can_rpcacp_call(artie_can_backend_t *handle, uint8_t target_address, const artie_can_rpc_signature_t *signature, const artie_can_rpc_value_t *args, uint8_t arg_count);

/**
 * @brief Retrieve the outcome of the most recently completed call.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @param out_errno If the call failed due to a NACK from the remote node, populated with the errno byte it reported. May be NULL.
 * @return artie_can_error_t ARTIE_CAN_ERR_NONE if the last call succeeded, ARTIE_CAN_ERR_NO_DATA if no call has completed yet,
 * or the error encountered otherwise.
 */
artie_can_error_t artie_can_rpcacp_get_last_error(artie_can_backend_t *handle, uint8_t *out_errno);

/**
 * @brief Retrieve the return value of the most recently completed synchronous call, generically.
 * Applies float(16-bit)/double(32-bit) wire-widening back to native size based on the signature's return_descriptor.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @param signature The signature that was used to make the call (must have a non-NULL return_descriptor).
 * @param out Buffer to receive the decoded return value.
 * @param out_size Size of out, in bytes; must be at least signature->return_size.
 * @return artie_can_error_t ARTIE_CAN_ERR_NONE on success, ARTIE_CAN_ERR_NO_DATA if no result is available.
 */
artie_can_error_t artie_can_rpcacp_get_result(artie_can_backend_t *handle, const artie_can_rpc_signature_t *signature, void *out, uint32_t out_size);

/**
 * @brief Retrieve the decoded result of a completed WHOAMI call. The node_name/fw_version pointers in
 * `out` point into library-owned storage that remains valid until the next RPCACP call is made.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @param out Populated with the decoded WHOAMI response.
 * @return artie_can_error_t ARTIE_CAN_ERR_NONE on success, ARTIE_CAN_ERR_NO_DATA if no result is available.
 */
artie_can_error_t artie_can_rpcacp_get_whoami_result(artie_can_backend_t *handle, artie_can_whoami_response_t *out);

/**
 * @brief Retrieve the decoded result of a completed STATUS call.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @param out Populated with the decoded STATUS response.
 * @return artie_can_error_t ARTIE_CAN_ERR_NONE on success, ARTIE_CAN_ERR_NO_DATA if no result is available.
 */
artie_can_error_t artie_can_rpcacp_get_status_result(artie_can_backend_t *handle, artie_can_status_response_t *out);

/**
 * @brief Retrieve the decoded result of a completed LIST call. The name/type_name pointers in each
 * entry of `out` point into library-owned storage that remains valid until the next RPCACP call is made.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @param out Array of ARTIE_CAN_RPCACP_LIST_PAGE_SIZE signatures to populate.
 * @return artie_can_error_t ARTIE_CAN_ERR_NONE on success, ARTIE_CAN_ERR_NO_DATA if no result is available.
 */
artie_can_error_t artie_can_rpcacp_get_list_result(artie_can_backend_t *handle, artie_can_rpc_signature_t out[ARTIE_CAN_RPCACP_LIST_PAGE_SIZE]);

/**
 * @brief API function to check if this node's RPCACP state machine is currently busy, i.e. it is
 * calling out to a remote node, servicing an inbound request, or returning a value for one.
 * A node cannot do more than one of these at a time.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @return true if the local RPCACP state machine is not idle, false otherwise.
 */
bool artie_can_rpcacp_is_busy(artie_can_backend_t *handle);

/**
 * @brief Check whether a specific remote node is believed to still be executing an asynchronous
 * RPC that this node previously called. This is a best-effort, locally-tracked belief: it is set
 * when an async call to that node is accepted, and cleared the next time that node accepts (or
 * definitively rejects, for a reason other than being busy) another request from this node.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @param node_address The remote node address to check.
 * @return true if node_address is believed to be busy with an async RPC from this node.
 */
bool artie_can_rpcacp_is_node_busy(artie_can_backend_t *handle, uint8_t node_address);

/**
 * @brief Handle a received RPCACP frame within an ISR context.
 * This function will be called by the backend when a new frame is received that matches the RPCACP protocol.
 *
 * @param context Pointer to the artie_can_context_t struct representing the context.
 * @param frame Pointer to the artie_can_frame_t struct representing the received frame.
 */
void rpcacp_receive_in_isr(artie_can_context_t *context, const artie_can_frame_t *frame);

/**
 * @brief Tick function for the RPCACP protocol. This function should be called periodically
 * to allow the RPCACP state machine to process incoming frames, timeouts, and other protocol-specific tasks.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend.
 * @return artie_can_error_t Error code indicating the result of the operation.
 */
artie_can_error_t rpcacp_tick(artie_can_backend_t *handle);
