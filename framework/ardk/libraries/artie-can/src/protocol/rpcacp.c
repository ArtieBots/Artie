/**
 * @file rpcacp.c
 * @brief Implementation of RPCACP (Remote Procedure Call Artie CAN Protocol).
 */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "backend.h"
#include "context.h"
#include "err.h"
#include "frame.h"
#include "rpcacp.h"
#include "rpcacp_context.h"
#include "util.h"

/**
 * @brief Initializes the RPCACP context for a given node.
 */
artie_can_error_t artie_can_init_context_rpcacp(artie_can_context_t *ctx, uint8_t node_address, uint8_t node_class)
{
    if (ctx == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (node_address == 0U)
    {
        // Address 0 is reserved for broadcast/special use; nodes must have a non-zero address
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (node_address > (ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK >> ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION))
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    ctx->rpcacp_context.node_address = node_address;
    ctx->rpcacp_context.state = RPCACP_STATE_IDLE;
    ctx->protocol_flags |= ARTIE_CAN_PROTOCOL_FLAG_RPCACP;
    ctx->node_address = node_address;

    return ARTIE_CAN_ERR_NONE;
}

/**
 * @brief Sets the receive buffer for the RPCACP context.
 */
artie_can_error_t artie_can_rpcacp_set_receive_buffer(artie_can_context_t *ctx, uint8_t *buffer, uint32_t buffer_size)
{
    if (ctx == NULL || buffer == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    ctx->rpcacp_context.receive_buffer = buffer;
    ctx->rpcacp_context.receive_buffer_size = buffer_size;

    return ARTIE_CAN_ERR_NONE;
}

/**
 * @brief API function to initiate an RPC call from a node.
 */
artie_can_error_t artie_can_rpcacp_call(artie_can_backend_t *handle, uint16_t procedure_id, const artie_can_rpc_param_descriptor_t *params, uint8_t param_count, bool synchronous)
{
    if (handle == NULL || params == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Placeholder: Logic to prepare RpcSignature and encode MsgPack payload would go here.
    // For now, we just acknowledge the call was initiated successfully.
    ARTIE_CAN_LOG(handle->context, "RPCACP: Initiating procedure 0x%04X (%u params, sync=%d)\n", procedure_id, param_count, synchronous);

    return ARTIE_CAN_ERR_NONE;
}

/**
 * @brief Handle a received RPCACP frame within an ISR context.
 */
void rpcacp_receive_in_isr(artie_can_context_t *ctx, const artie_can_frame_t *frame)
{
    if (ctx == NULL || frame == NULL)
    {
        return;
    }

    if ((ctx->protocol_flags & ARTIE_CAN_PROTOCOL_FLAG_RPCACP) == 0)
    {
        // This node is not configured to use RPCACP, ignore the frame.
        return;
    }

    // Placeholder: Logic to parse RpcSignature and execute registered procedure would go here.
    // For now, we simply flag that a request was received.
    ctx->rpcacp_context.isr_flags |= RPCACP_ISR_FLAG_REQUEST_RECEIVED;
}

/**
 * @brief API function to check if a node is currently processing an asynchronous RPC.
 */
bool artie_can_rpcacp_is_busy(artie_can_backend_t *handle)
{
    if (handle == NULL)
    {
        return false;
    }

    // Placeholder: Check against rpcacp_context.state or busy_nodes_bitmap.
    return (handle->context->rpcacp_context.state == RPCACP_STATE_EXECUTING_PROC);
}

/**
 * @brief Tick function for the RPCACP protocol.
 */
artie_can_error_t rpcacp_tick(artie_can_backend_t *handle)
{
    if (handle == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Placeholder: Logic for processing timeouts, checking ack/nack status, and cleaning isr_flags.

    return ARTIE_CAN_ERR_NONE;
}

/**
 * @brief Register a new RPC procedure with the backend.
 */
artie_can_error_t artie_can_rpcacp_register_procedure(artie_can_backend_t *handle, const artie_can_rpc_signature_t *signature)
{
    if (handle == NULL || signature == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // Placeholder: Logic to add the procedure_id and its function pointer to the node's registry.
    ARTIE_CAN_LOG(handle->context, "RPCACP: Registered procedure 0x%04X (%s)\n", signature->procedure_id, signature->name);

    return ARTIE_CAN_ERR_NONE;
}
