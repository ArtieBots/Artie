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
#include <stddef.h>
#include "err.h"
#include "frame.h"
#include "translationlayer.h"

// The RPC signature schema (artie_can_rpc_signature_t and friends) is defined here rather than in
// rpcacp.h because rpcacp_context_t needs it directly (for its procedure registry), and rpcacp.h
// includes context.h, which includes this file -- defining it in rpcacp.h instead would create a
// circular #include dependency depending on which header a translation unit reaches first.
// rpcacp.h re-exposes these same types by including this header.

/** Maximum number of parameters allowed in a single RPC signature. */
#define ARTIE_CAN_RPCACP_MAX_PARAMS 15U

/** Maximum number of device-specific procedures (0x10-0x7F) a single node can register. */
#define ARTIE_CAN_RPCACP_MAX_REGISTERED_PROCEDURES 32U

/** Maximum length (not counting the nul terminator) of a node name or firmware version string. */
#define ARTIE_CAN_RPCACP_MAX_NAME_LENGTH 31U

/** Max size, in bytes, of a byte-stuffed RPCACP payload (request or return value). */
#define ARTIE_CAN_RPCACP_MAX_STUFFED_PAYLOAD_SIZE 1024U

/** Max size, in bytes, of a raw (un-stuffed) packed RPCACP payload (request or return value). */
#define ARTIE_CAN_RPCACP_MAX_PAYLOAD_SIZE 1018U

/**
 * @brief Describes a single parameter (or return value) within an RPC call.
 *
 * Used to facilitate serialization/deserialization of the MsgPack payload. Both sides of an
 * RPC call must agree on this schema out-of-band (e.g. shared header, or discovered via LIST
 * plus application-level knowledge of the resulting binary layout).
 */
typedef struct {
    char *type_name;                 ///< Type name (e.g., "uint8_t", "array<uint8_t, 4>", "struct foo") or "NULL"
    uint8_t offset_in_msgpack;       ///< Offset within the encoded data stream
    bool optional;                   ///< true if parameter may be omitted
} artie_can_rpc_param_descriptor_t;

/**
 * @brief Function signature for a registered device-specific RPC procedure.
 *
 * The void* return should be cast according to the return_descriptor in the signature. The
 * function should write its result into return_buffer (of return_buffer_size bytes) and return
 * return_buffer on success, or NULL to indicate the request could not be serviced (reported to
 * the caller as EINVAL).
 *
 * @param params Array of pointers to the decoded parameter values, one per entry in the signature's params array.
 * @param param_count Number of entries in params.
 * @param return_buffer Buffer the function should write its result into, if it has a non-NULL return_descriptor.
 * @param return_buffer_size Size of return_buffer, in bytes.
 * @return return_buffer on success, or NULL if the request could not be serviced.
 */
typedef void *(*artie_can_rpc_function_t)(const void **params, uint8_t param_count, void *return_buffer, size_t return_buffer_size);

/**
 * @brief Defines a full RPC signature, allowing nodes to discover available procedures.
 */
typedef struct {
    uint16_t procedure_id;                ///< 7-bit field mapped to the protocol frame ID
    char *name;                           ///< Human-readable name for debugging
    bool synchronous;                     ///< true if the caller must block until response
    uint8_t param_count;                  ///< Number of parameters defined in this signature
    artie_can_rpc_param_descriptor_t params[ARTIE_CAN_RPCACP_MAX_PARAMS]; ///< Parameter array
    artie_can_rpc_function_t function;    ///< Function to call when this RPC is invoked on this node (NULL for non-device-specific signatures used only for calling out)
    artie_can_rpc_param_descriptor_t *return_descriptor; ///< Descriptor for the return value (NULL if void)
    uint32_t return_size;                 ///< Native (local) size, in bytes, of the return value; ignored if return_descriptor is NULL
} artie_can_rpc_signature_t;

/**
 * @brief States that the RPCACP state machine can be in for a given node.
 *
 * RPCACP only allows a node to be doing one thing at a time (calling out to a remote
 * node, servicing an inbound request, or returning a value for a request it is servicing),
 * so a single state field is sufficient to track all of that.
 */
typedef enum {
    RPCACP_STATE_IDLE,                ///< The node is idle and not processing any RPC.
    RPCACP_STATE_SENDING_REQUEST,     ///< (Calling side) Sending StartRPC/TxData frames and waiting for the per-frame ACK/NACK.
    RPCACP_STATE_WAITING_RETURN,      ///< (Calling side) The request was accepted by the remote node; waiting for StartReturn/RxData frames.
    RPCACP_STATE_RECEIVING_REQUEST,   ///< (Responding side) Accumulating StartRPC/TxData frames from a requesting node.
    RPCACP_STATE_EXECUTING_PROC,      ///< (Responding side) Actively invoking the dispatched procedure's function.
    RPCACP_STATE_SENDING_RETURN,      ///< (Responding side) Sending StartReturn/RxData frames and waiting for the per-frame ACK/NACK.
} rpcacp_state_t;

/**
 * @brief ISR flags for RPCACP protocol.
 */
typedef enum {
    RPCACP_ISR_FLAG_NONE = 0,           ///< No special conditions for this ISR call
    RPCACP_ISR_FLAG_FRAME_RECEIVED = 1 << 0, ///< A frame relevant to RPCACP was received and needs main-thread processing
} rpcacp_isr_flags_t;

/**
 * @brief Context for RPCACP protocol handling within the Artie CAN library.
 *
 * Contains the state of the local node's RPC engine, its registry of device-specific
 * procedures, and the buffers used to build/accumulate MsgPack payloads for whichever
 * exchange (calling out, servicing a request, or returning a value) is currently active.
 */
typedef struct {
    // ---- Node identity ----
    uint8_t node_address;                    ///< The Artie CAN bus address of this Node
    uint8_t node_class;                      ///< The class bitmask of this Node
    rpcacp_state_t state;                    ///< The current state of the RPCACP protocol for this node
    uint64_t last_activity_ms;               ///< Timestamp of the last frame sent/received for the active exchange (used for ACK timeouts)
    uint8_t retry_count;                     ///< Number of retries attempted for the in-flight frame of the active exchange

    // ---- WHOAMI / STATUS data (always answered internally, never via the registry) ----
    char node_name[ARTIE_CAN_RPCACP_MAX_NAME_LENGTH + 1U];  ///< Human-readable node name, used to answer WHOAMI
    char fw_version[ARTIE_CAN_RPCACP_MAX_NAME_LENGTH + 1U]; ///< Firmware version string, used to answer WHOAMI
    uint64_t boot_time_ms;                   ///< Timestamp (per get_ms()) this context was initialized; used to compute STATUS uptime
    uint32_t status_err_flags;               ///< Device-specific error flags reported by STATUS

    // ---- Registry of device-specific procedures (0x10-0x7F); this node acting as the remote/executee ----
    artie_can_rpc_signature_t registered_procedures[ARTIE_CAN_RPCACP_MAX_REGISTERED_PROCEDURES]; ///< Registered procedure signatures
    uint8_t registered_procedure_count;      ///< Number of valid entries in registered_procedures

    // ---- Outgoing call state (this node acting as requester) ----
    uint8_t call_target_address;             ///< Target node address of the active outgoing call
    const artie_can_rpc_signature_t *call_signature; ///< Signature of the procedure being called (owned by caller)
    uint8_t call_random_id;                  ///< Random traceability value for the active outgoing exchange
    bool call_active;                        ///< true from the moment artie_can_rpcacp_call() is invoked until the exchange fully resolves
    uint8_t call_stuffed_payload[ARTIE_CAN_RPCACP_MAX_STUFFED_PAYLOAD_SIZE]; ///< Byte-stuffed MsgPack payload of the outgoing request
    uint32_t call_stuffed_payload_size;      ///< Total size of the stuffed outgoing payload
    uint32_t call_stuffed_payload_offset;    ///< Number of stuffed payload bytes already sent
    uint32_t call_last_chunk_size;           ///< Size of the most recently sent chunk (for rewinding offset on NACK)
    uint16_t call_crc16;                     ///< CRC16 over [header byte][stuffed payload] for the outgoing request
    bool call_completed;                     ///< true once a call has fully resolved (successfully or not) at least once
    artie_can_error_t call_last_error;       ///< Result of the most recently completed call (ARTIE_CAN_ERR_NONE on success)
    uint8_t call_last_errno;                 ///< Valid when call_last_error indicates the remote NACKed the request
    uint8_t call_return_raw[ARTIE_CAN_RPCACP_MAX_PAYLOAD_SIZE]; ///< Decoded (un-stuffed) return payload, wire-sized (pre float/double widening)
    uint32_t call_return_size;               ///< Number of valid bytes in call_return_raw
    bool call_result_ready;                  ///< true once a completed synchronous call's return value is available to read

    // ---- Incoming request state (this node acting as remote/executee) ----
    uint8_t recv_sender_address;             ///< Address of the node that sent the request currently being accumulated/serviced
    uint8_t recv_random_id;                  ///< Random traceability value of the inbound exchange
    uint8_t recv_procedure_id;               ///< Procedure ID requested by the inbound exchange
    bool recv_synchronous;                   ///< Whether the inbound request expects a return value
    uint16_t recv_expected_crc16;            ///< CRC16 supplied by the requester, to validate once the payload is fully received
    uint8_t recv_stuffed_payload[ARTIE_CAN_RPCACP_MAX_STUFFED_PAYLOAD_SIZE]; ///< Accumulated byte-stuffed MsgPack payload of the inbound request
    uint32_t recv_stuffed_payload_size;      ///< Number of bytes accumulated so far in recv_stuffed_payload

    // ---- Outgoing return state (this node acting as remote/executee, after executing a synchronous procedure) ----
    uint8_t return_random_id;                ///< Random traceability value chosen for the outgoing return exchange
    uint8_t return_stuffed_payload[ARTIE_CAN_RPCACP_MAX_STUFFED_PAYLOAD_SIZE]; ///< Byte-stuffed MsgPack payload of the outgoing return value
    uint32_t return_stuffed_payload_size;    ///< Total size of the stuffed outgoing return payload
    uint32_t return_stuffed_payload_offset;  ///< Number of stuffed return payload bytes already sent
    uint32_t return_last_chunk_size;         ///< Size of the most recently sent return chunk (for rewinding offset on NACK)
    uint16_t return_crc16;                   ///< CRC16 over [header byte][stuffed payload] for the outgoing return value

    // ---- Network-wide tracking (calling side keeping tabs on remote nodes it has issued async calls to) ----
    uint64_t busy_nodes_bitmap;               ///< Bitmap of nodes believed to be busy executing an async RPC (bit N = node address N)

    // ---- Written to by ISR ----
    artie_can_frame_t received_frame;         ///< Most recently received RPCACP frame, for processing in the main thread
    atomic_uint32_t isr_flags;                ///< Flags to indicate special conditions found during ISR; cleared by main thread (atomic operations required)
} rpcacp_context_t;
