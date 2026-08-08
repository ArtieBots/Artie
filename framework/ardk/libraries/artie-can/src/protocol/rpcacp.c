/**
 * @file rpcacp.c
 * @brief Implementation of RPCACP (Remote Procedure Call Artie CAN Protocol).
 *
 * See docs/specifications/CANProtocol.md for the wire protocol, docs/specifications/RPCSchema.md
 * for the RPC signature schema, docs/specifications/MsgPackSchema.md for the payload layout, and
 * docs/specifications/ByteStuffing.md for the byte-stuffing scheme used on the payload.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "backend.h"
#include "context.h"
#include "err.h"
#include "frame.h"
#include "log.h"
#include "rpcacp.h"
#include "rpcacp_context.h"
#include "util.h"

#define CRC16_INIT 0xFFFFU

typedef enum {
    BYTE_STUFF_SCAN_FOUND,
    BYTE_STUFF_SCAN_NEED_MORE,
    BYTE_STUFF_SCAN_MALFORMED,
} byte_stuff_scan_result_t;

// ---------------------------------------------------------------------------
// CRC16 (CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no final XOR)
// ---------------------------------------------------------------------------

static uint16_t _crc16_ccitt_update(uint16_t crc, const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (int bit = 0; bit < 8; bit++)
        {
            crc = (uint16_t)(((crc & 0x8000U) != 0U) ? ((uint16_t)(crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1));
        }
    }
    return crc;
}

// ---------------------------------------------------------------------------
// Byte stuffing (see docs/specifications/ByteStuffing.md)
// ---------------------------------------------------------------------------

/** Encode `in` (in_len bytes) into `out`. Returns false if out_cap is insufficient. */
static bool _byte_stuff(const uint8_t *in, uint32_t in_len, uint8_t *out, uint32_t out_cap, uint32_t *out_len)
{
    uint32_t consumed = 0;
    uint32_t produced = 0;

    for (;;)
    {
        uint32_t remaining = in_len - consumed;
        if (remaining == 0U)
        {
            if (produced >= out_cap) { return false; }
            out[produced++] = 0xFFU;
            break;
        }

        uint32_t chunk = (remaining > 254U) ? 254U : remaining;
        if ((produced + 1U + chunk) > out_cap) { return false; }
        out[produced++] = (uint8_t)chunk;
        memcpy(&out[produced], &in[consumed], chunk);
        produced += chunk;
        consumed += chunk;
    }

    *out_len = produced;
    return true;
}

/** Scan `buf` (buf_len bytes accumulated so far) for the terminal (0xFF) special byte. */
static byte_stuff_scan_result_t _byte_stuff_scan(const uint8_t *buf, uint32_t buf_len, uint32_t *out_total_len)
{
    uint32_t pos = 0;
    while (pos < buf_len)
    {
        uint8_t marker = buf[pos];
        if (marker == 0xFFU)
        {
            *out_total_len = pos + 1U;
            return BYTE_STUFF_SCAN_FOUND;
        }
        else if (marker == 0x00U)
        {
            return BYTE_STUFF_SCAN_MALFORMED;
        }

        uint32_t next_special = pos + 1U + marker;
        if (next_special >= buf_len)
        {
            return BYTE_STUFF_SCAN_NEED_MORE;
        }
        pos = next_special;
    }
    return BYTE_STUFF_SCAN_NEED_MORE;
}

/** Decode a complete stuffed buffer (stuffed_len bytes, ending with the terminal 0xFF) into `out`. */
static bool _byte_unstuff(const uint8_t *stuffed, uint32_t stuffed_len, uint8_t *out, uint32_t out_cap, uint32_t *out_len)
{
    uint32_t pos = 0;
    uint32_t produced = 0;

    while (pos < stuffed_len)
    {
        uint8_t marker = stuffed[pos];
        if (marker == 0xFFU)
        {
            break;
        }
        else if (marker == 0x00U)
        {
            return false;
        }

        if ((produced + marker) > out_cap) { return false; }
        memcpy(&out[produced], &stuffed[pos + 1U], marker);
        produced += marker;
        pos += 1U + marker;
    }

    *out_len = produced;
    return true;
}

// ---------------------------------------------------------------------------
// Wire-narrowed float conversions ("float" is 16-bit, "double" is 32-bit on the wire)
// ---------------------------------------------------------------------------

static uint16_t _float32_to_float16(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));

    uint32_t sign = (bits >> 16) & 0x8000U;
    int32_t exponent = (int32_t)((bits >> 23) & 0xFFU) - 127 + 15;
    uint32_t mantissa = bits & 0x7FFFFFU;

    if (((bits >> 23) & 0xFFU) == 0xFFU)
    {
        // Inf or NaN
        return (uint16_t)(sign | 0x7C00U | ((mantissa != 0U) ? 0x0200U : 0U));
    }
    else if (exponent <= 0)
    {
        // Underflow (including denormals); flush to zero for simplicity.
        return (uint16_t)sign;
    }
    else if (exponent >= 0x1F)
    {
        // Overflow to infinity
        return (uint16_t)(sign | 0x7C00U);
    }
    else
    {
        return (uint16_t)(sign | ((uint32_t)exponent << 10) | (mantissa >> 13));
    }
}

static float _float16_to_float32(uint16_t half)
{
    uint32_t sign = (uint32_t)(half & 0x8000U) << 16;
    uint32_t exponent = (half >> 10) & 0x1FU;
    uint32_t mantissa = half & 0x3FFU;
    uint32_t bits;

    if (exponent == 0U)
    {
        if (mantissa == 0U)
        {
            bits = sign;
        }
        else
        {
            // Denormal half; normalize into a full float exponent/mantissa.
            uint32_t norm_exp = 127U - 15U + 1U;
            while ((mantissa & 0x400U) == 0U)
            {
                mantissa <<= 1;
                norm_exp--;
            }
            mantissa &= 0x3FFU;
            bits = sign | (norm_exp << 23) | (mantissa << 13);
        }
    }
    else if (exponent == 0x1FU)
    {
        bits = sign | 0x7F800000U | (mantissa << 13); // Inf/NaN
    }
    else
    {
        bits = sign | ((exponent - 15U + 127U) << 23) | (mantissa << 13);
    }

    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

// ---------------------------------------------------------------------------
// Generic value packing / unpacking
//
// Every parameter/return value is packed at its descriptor's offset_in_msgpack by copying its
// native byte representation, EXCEPT "float" (wire size 16 bits) and "double" (wire size 32
// bits), which are narrowed/widened. Every other type (ints, bool, arrays, structs, strings) is
// treated as an opaque blob whose length the two ends of the RPC already agree on out-of-band;
// the receiving side hands the callee a raw pointer at the right offset rather than trying to
// know each type's length generically. Note: this means a variable-length field (string) should
// be the last parameter in a signature, since fields after it can't have a fixed static offset.
// ---------------------------------------------------------------------------

static bool _type_name_is(const char *type_name, const char *candidate)
{
    return (type_name != NULL) && (strcmp(type_name, candidate) == 0);
}

/**
 * Wire size, in bytes, of a fixed-size primitive type, or 0 for variable-length/unknown types
 * (string, array, struct, NULL). Note that "float" is 16 bits and "double" is 32 bits on the wire.
 */
static uint32_t _primitive_wire_size(const char *type_name)
{
    if (_type_name_is(type_name, "uint8_t") || _type_name_is(type_name, "int8_t") || _type_name_is(type_name, "bool"))
    {
        return 1U;
    }
    else if (_type_name_is(type_name, "uint16_t") || _type_name_is(type_name, "int16_t") || _type_name_is(type_name, "float"))
    {
        return 2U;
    }
    else if (_type_name_is(type_name, "uint32_t") || _type_name_is(type_name, "int32_t") || _type_name_is(type_name, "double"))
    {
        return 4U;
    }
    else if (_type_name_is(type_name, "uint64_t") || _type_name_is(type_name, "int64_t"))
    {
        return 8U;
    }
    return 0U;
}

static artie_can_error_t _pack_values(const artie_can_rpc_param_descriptor_t *descriptors, uint8_t descriptor_count,
                                       const artie_can_rpc_value_t *values, uint8_t value_count,
                                       uint8_t *out, uint32_t out_cap, uint32_t *out_len)
{
    if ((descriptors == NULL) && (descriptor_count > 0U)) { return ARTIE_CAN_ERR_INVALID_ARG; }
    if ((values == NULL) && (value_count > 0U)) { return ARTIE_CAN_ERR_INVALID_ARG; }
    if (value_count > descriptor_count) { return ARTIE_CAN_ERR_INVALID_ARG; }

    uint32_t max_offset = 0;
    for (uint8_t i = 0; i < descriptor_count; i++)
    {
        const artie_can_rpc_param_descriptor_t *desc = &descriptors[i];

        if (i >= value_count)
        {
            if (!desc->optional) { return ARTIE_CAN_ERR_INVALID_ARG; }
            continue;
        }

        const artie_can_rpc_value_t *val = &values[i];
        uint32_t offset = desc->offset_in_msgpack;
        uint32_t wire_size;

        if (_type_name_is(desc->type_name, "float"))
        {
            if (val->size != sizeof(float)) { return ARTIE_CAN_ERR_INVALID_ARG; }
            float f;
            memcpy(&f, val->data, sizeof(f));
            uint16_t half = _float32_to_float16(f);
            wire_size = 2U;
            if ((offset + wire_size) > out_cap) { return ARTIE_CAN_ERR_NO_SPACE; }
            out[offset] = (uint8_t)(half >> 8);
            out[offset + 1U] = (uint8_t)(half & 0xFFU);
        }
        else if (_type_name_is(desc->type_name, "double"))
        {
            if (val->size != sizeof(double)) { return ARTIE_CAN_ERR_INVALID_ARG; }
            double d;
            memcpy(&d, val->data, sizeof(d));
            float narrowed = (float)d;
            uint32_t bits;
            memcpy(&bits, &narrowed, sizeof(bits));
            wire_size = 4U;
            if ((offset + wire_size) > out_cap) { return ARTIE_CAN_ERR_NO_SPACE; }
            out[offset] = (uint8_t)(bits >> 24);
            out[offset + 1U] = (uint8_t)(bits >> 16);
            out[offset + 2U] = (uint8_t)(bits >> 8);
            out[offset + 3U] = (uint8_t)(bits & 0xFFU);
        }
        else
        {
            wire_size = val->size;
            if ((offset + wire_size) > out_cap) { return ARTIE_CAN_ERR_NO_SPACE; }
            if (wire_size > 0U)
            {
                memcpy(&out[offset], val->data, wire_size);
            }
        }

        if ((offset + wire_size) > max_offset) { max_offset = offset + wire_size; }
    }

    *out_len = max_offset;
    return ARTIE_CAN_ERR_NONE;
}

/** Build the pointer array handed to a dispatched procedure's function, widening float/double params into scratch. */
static artie_can_error_t _prepare_dispatch_params(const artie_can_rpc_signature_t *sig, const uint8_t *raw_payload, uint32_t raw_payload_len,
                                                   const void *param_ptrs[ARTIE_CAN_RPCACP_MAX_PARAMS], uint8_t scratch[ARTIE_CAN_RPCACP_MAX_PARAMS][8])
{
    for (uint8_t i = 0; i < sig->param_count; i++)
    {
        const artie_can_rpc_param_descriptor_t *desc = &sig->params[i];
        uint32_t offset = desc->offset_in_msgpack;

        if (_type_name_is(desc->type_name, "NULL"))
        {
            param_ptrs[i] = NULL;
            continue;
        }

        uint32_t wire_size = _primitive_wire_size(desc->type_name);
        bool missing = (wire_size > 0U) ? ((offset + wire_size) > raw_payload_len) : (offset >= raw_payload_len);
        if (missing)
        {
            if (desc->optional)
            {
                // Optional parameter not present in this call; hand the procedure a NULL.
                param_ptrs[i] = NULL;
                continue;
            }
            return ARTIE_CAN_ERR_INVALID_ARG;
        }

        if (_type_name_is(desc->type_name, "float"))
        {
            uint16_t half = (uint16_t)(((uint16_t)raw_payload[offset] << 8) | raw_payload[offset + 1U]);
            float f = _float16_to_float32(half);
            memcpy(scratch[i], &f, sizeof(f));
            param_ptrs[i] = scratch[i];
        }
        else if (_type_name_is(desc->type_name, "double"))
        {
            uint32_t bits = ((uint32_t)raw_payload[offset] << 24) | ((uint32_t)raw_payload[offset + 1U] << 16) |
                             ((uint32_t)raw_payload[offset + 2U] << 8) | (uint32_t)raw_payload[offset + 3U];
            float f;
            memcpy(&f, &bits, sizeof(f));
            double d = (double)f;
            memcpy(scratch[i], &d, sizeof(d));
            param_ptrs[i] = scratch[i];
        }
        else
        {
            param_ptrs[i] = &raw_payload[offset];
        }
    }
    return ARTIE_CAN_ERR_NONE;
}

/** Pack a procedure's return value (from its native return_buffer) into a raw MsgPack payload. */
static artie_can_error_t _pack_return_value(const artie_can_rpc_signature_t *sig, const uint8_t *return_buffer,
                                             uint8_t *out, uint32_t out_cap, uint32_t *out_len)
{
    if (sig->return_descriptor == NULL)
    {
        *out_len = 0;
        return ARTIE_CAN_ERR_NONE;
    }

    artie_can_rpc_value_t val;
    val.data = return_buffer;
    val.size = sig->return_size;
    return _pack_values(sig->return_descriptor, 1U, &val, 1U, out, out_cap, out_len);
}

// ---------------------------------------------------------------------------
// Bespoke wire formats for the always-internal standard RPCs (WHOAMI, STATUS, LIST)
// ---------------------------------------------------------------------------

static uint32_t _pack_whoami(const rpcacp_context_t *ctx, uint8_t *out, uint32_t out_cap)
{
    uint32_t pos = 0;
    size_t name_len = strlen(ctx->node_name);
    size_t fw_len = strlen(ctx->fw_version);

    if ((1U + name_len + 1U + fw_len + 1U) > out_cap) { return 0; }

    out[pos++] = ctx->node_address;
    memcpy(&out[pos], ctx->node_name, name_len + 1U);
    pos += (uint32_t)(name_len + 1U);
    memcpy(&out[pos], ctx->fw_version, fw_len + 1U);
    pos += (uint32_t)(fw_len + 1U);
    return pos;
}

static uint32_t _pack_status(uint64_t uptime_ms, uint32_t err_flags, uint8_t *out)
{
    for (int i = 0; i < 8; i++)
    {
        out[i] = (uint8_t)(uptime_ms >> (56 - (8 * i)));
    }
    for (int i = 0; i < 4; i++)
    {
        out[8 + i] = (uint8_t)(err_flags >> (24 - (8 * i)));
    }
    return 12U;
}

static bool _append_bytes(uint8_t *out, uint32_t out_cap, uint32_t *pos, const void *data, uint32_t len)
{
    if ((*pos + len) > out_cap) { return false; }
    memcpy(&out[*pos], data, len);
    *pos += len;
    return true;
}

static bool _append_cstr(uint8_t *out, uint32_t out_cap, uint32_t *pos, const char *str)
{
    if (str == NULL) { str = ""; }
    uint32_t len = (uint32_t)strlen(str) + 1U; // include nul
    return _append_bytes(out, out_cap, pos, str, len);
}

/** Fabricates the (non-registered, library-owned) signature for a reserved standard procedure ID. */
static bool _reserved_signature(uint16_t procedure_id, artie_can_rpc_signature_t *out)
{
    memset(out, 0, sizeof(*out));
    out->procedure_id = procedure_id;
    out->synchronous = true;

    switch (procedure_id)
    {
        case ARTIE_CAN_RPC_ID_WHOAMI:
            out->name = "WHOAMI";
            out->param_count = 0;
            return true;
        case ARTIE_CAN_RPC_ID_STATUS:
            out->name = "STATUS";
            out->param_count = 0;
            return true;
        case ARTIE_CAN_RPC_ID_LIST:
            out->name = "LIST";
            out->param_count = 1;
            out->params[0].type_name = "uint8_t";
            out->params[0].offset_in_msgpack = 0;
            out->params[0].optional = false;
            return true;
        default:
            return false;
    }
}

static const artie_can_rpc_signature_t *_find_registered(const rpcacp_context_t *ctx, uint8_t procedure_id)
{
    for (uint8_t i = 0; i < ctx->registered_procedure_count; i++)
    {
        if (ctx->registered_procedures[i].procedure_id == procedure_id)
        {
            return &ctx->registered_procedures[i];
        }
    }
    return NULL;
}

static bool _pack_list_page(const rpcacp_context_t *ctx, uint8_t page, uint8_t *out, uint32_t out_cap, uint32_t *out_len)
{
    uint32_t pos = 0;

    for (uint8_t i = 0; i < (uint8_t)ARTIE_CAN_RPCACP_LIST_PAGE_SIZE; i++)
    {
        uint16_t procedure_id = (uint16_t)(((uint16_t)page * ARTIE_CAN_RPCACP_LIST_PAGE_SIZE) + i);
        artie_can_rpc_signature_t reserved_sig;
        const artie_can_rpc_signature_t *sig = NULL;

        if (procedure_id <= ARTIE_CAN_RPCACP_RESERVED_ID_MAX)
        {
            if (_reserved_signature(procedure_id, &reserved_sig)) { sig = &reserved_sig; }
        }
        else
        {
            sig = _find_registered(ctx, (uint8_t)procedure_id);
        }

        uint8_t proc_id_byte = (uint8_t)procedure_id;
        if (!_append_bytes(out, out_cap, &pos, &proc_id_byte, 1U)) { return false; }

        if (sig == NULL)
        {
            uint8_t zero = 0;
            if (!_append_cstr(out, out_cap, &pos, ARTIE_CAN_RPCACP_LIST_UNASSIGNED_NAME)) { return false; }
            if (!_append_bytes(out, out_cap, &pos, &zero, 1U)) { return false; } // synchronous
            if (!_append_bytes(out, out_cap, &pos, &zero, 1U)) { return false; } // param_count
            continue;
        }

        if (!_append_cstr(out, out_cap, &pos, sig->name)) { return false; }
        uint8_t sync_byte = sig->synchronous ? 1U : 0U;
        if (!_append_bytes(out, out_cap, &pos, &sync_byte, 1U)) { return false; }
        if (!_append_bytes(out, out_cap, &pos, &sig->param_count, 1U)) { return false; }

        for (uint8_t p = 0; p < sig->param_count; p++)
        {
            if (!_append_cstr(out, out_cap, &pos, sig->params[p].type_name)) { return false; }
            if (!_append_bytes(out, out_cap, &pos, &sig->params[p].offset_in_msgpack, 1U)) { return false; }
            uint8_t opt_byte = sig->params[p].optional ? 1U : 0U;
            if (!_append_bytes(out, out_cap, &pos, &opt_byte, 1U)) { return false; }
        }
    }

    *out_len = pos;
    return true;
}

// ---------------------------------------------------------------------------
// Frame senders
// ---------------------------------------------------------------------------

static uint32_t _build_id(artie_can_frame_type_rpcacp_t frame_type, artie_can_frame_priority_rpcacp_t priority,
                           uint8_t sender_address, uint8_t target_address, uint8_t random_id)
{
    return ((uint32_t)ARTIE_CAN_RPCACP_PROTOCOL_ID << ARTIE_CAN_FRAME_ID_PROTOCOL_LOCATION) |
           ((uint32_t)frame_type << ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION) |
           ((uint32_t)priority << ARTIE_CAN_FRAME_ID_USER_PRIORITY_LOCATION) |
           ((uint32_t)sender_address << ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION) |
           ((uint32_t)target_address << ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION) |
           ((uint32_t)random_id << RPCACP_FRAME_ID_RANDOM_LOCATION);
}

static artie_can_error_t _send_ack_frame(artie_can_backend_t *handle, uint8_t target_address, uint8_t random_id)
{
    artie_can_frame_t frame = {0};
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;

    frame.id = _build_id(ARTIE_CAN_FRAME_TYPE_RPCACP_ACK, ARTIE_CAN_FRAME_PRIORITY_RPCACP_HIGH, ctx->node_address, target_address, random_id);
    frame.dlc = 0;

    ARTIE_CAN_LOG(handle->context, "RPCACP: Sending ACK to 0x%02X\n", target_address);
    return artie_can_send_with_retry(handle, &frame);
}

static artie_can_error_t _send_nack_frame(artie_can_backend_t *handle, uint8_t target_address, uint8_t random_id, uint8_t errno_code)
{
    artie_can_frame_t frame = {0};
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;

    frame.id = _build_id(ARTIE_CAN_FRAME_TYPE_RPCACP_NACK, ARTIE_CAN_FRAME_PRIORITY_RPCACP_HIGH, ctx->node_address, target_address, random_id);
    frame.data[0] = errno_code;
    frame.dlc = 1;

    ARTIE_CAN_LOG(handle->context, "RPCACP: Sending NACK (errno=0x%02X) to 0x%02X\n", errno_code, target_address);
    return artie_can_send_with_retry(handle, &frame);
}

/** Sends a StartRPC (is_return=false) or StartReturn (is_return=true) frame. */
static artie_can_error_t _send_start_frame(artie_can_backend_t *handle, bool is_return, uint8_t target_address, uint8_t random_id,
                                            uint8_t procedure_id, bool synchronous, uint16_t crc16,
                                            const uint8_t *stuffed_payload, uint32_t stuffed_payload_size, uint32_t *out_chunk_size)
{
    artie_can_frame_t frame = {0};
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;

    uint8_t header_byte = (uint8_t)(((is_return || synchronous) ? 0x80U : 0x00U) | (procedure_id & 0x7FU));

    frame.id = _build_id(is_return ? ARTIE_CAN_FRAME_TYPE_RPCACP_START_RETURN : ARTIE_CAN_FRAME_TYPE_RPCACP_START_RPC,
                          ARTIE_CAN_FRAME_PRIORITY_RPCACP_MEDIUM_HIGH, ctx->node_address, target_address, random_id);

    frame.data[0] = header_byte;
    frame.data[1] = (uint8_t)(crc16 >> 8);
    frame.data[2] = (uint8_t)(crc16 & 0xFFU);

    uint32_t chunk = (stuffed_payload_size > 5U) ? 5U : stuffed_payload_size;
    if (chunk > 0U)
    {
        memcpy(&frame.data[3], stuffed_payload, chunk);
    }
    frame.dlc = (uint8_t)(3U + chunk);

    ARTIE_CAN_LOG(handle->context, "RPCACP: Sending %s frame to 0x%02X (proc=0x%02X, sync=%d)\n", is_return ? "StartReturn" : "StartRPC", target_address, procedure_id, synchronous);

    artie_can_error_t err = artie_can_send_with_retry(handle, &frame);
    if (out_chunk_size != NULL) { *out_chunk_size = chunk; }
    return err;
}

/** Sends a TxData (is_return=false) or RxData (is_return=true) frame. */
static artie_can_error_t _send_data_frame(artie_can_backend_t *handle, bool is_return, uint8_t target_address, uint8_t random_id,
                                           const uint8_t *stuffed_payload, uint32_t offset, uint32_t total_size, uint32_t *out_chunk_size)
{
    artie_can_frame_t frame = {0};
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;

    frame.id = _build_id(is_return ? ARTIE_CAN_FRAME_TYPE_RPCACP_RX_DATA : ARTIE_CAN_FRAME_TYPE_RPCACP_TX_DATA,
                          ARTIE_CAN_FRAME_PRIORITY_RPCACP_MEDIUM_HIGH, ctx->node_address, target_address, random_id);

    uint32_t remaining = total_size - offset;
    uint32_t chunk = (remaining > 8U) ? 8U : remaining;
    memcpy(frame.data, &stuffed_payload[offset], chunk);
    frame.dlc = (uint8_t)chunk;

    ARTIE_CAN_LOG(handle->context, "RPCACP: Sending %s frame to 0x%02X (offset=%u, chunk=%u)\n", is_return ? "RxData" : "TxData", target_address, offset, chunk);

    artie_can_error_t err = artie_can_send_with_retry(handle, &frame);
    if (out_chunk_size != NULL) { *out_chunk_size = chunk; }
    return err;
}

static artie_can_error_t _send_next_request_chunk(artie_can_backend_t *handle)
{
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    uint32_t chunk_size = 0;
    artie_can_error_t err;

    if (ctx->call_stuffed_payload_offset == 0U)
    {
        err = _send_start_frame(handle, false, ctx->call_target_address, ctx->call_random_id,
                                 (uint8_t)ctx->call_signature->procedure_id, ctx->call_signature->synchronous,
                                 ctx->call_crc16, ctx->call_stuffed_payload, ctx->call_stuffed_payload_size, &chunk_size);
    }
    else
    {
        err = _send_data_frame(handle, false, ctx->call_target_address, ctx->call_random_id,
                                ctx->call_stuffed_payload, ctx->call_stuffed_payload_offset, ctx->call_stuffed_payload_size, &chunk_size);
    }

    if (err == ARTIE_CAN_ERR_NONE)
    {
        ctx->call_last_chunk_size = chunk_size;
        ctx->last_activity_ms = handle->get_ms();
    }
    return err;
}

static artie_can_error_t _send_next_return_chunk(artie_can_backend_t *handle)
{
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    uint32_t chunk_size = 0;
    artie_can_error_t err;

    if (ctx->return_stuffed_payload_offset == 0U)
    {
        err = _send_start_frame(handle, true, ctx->recv_sender_address, ctx->return_random_id,
                                 ctx->recv_procedure_id, true, ctx->return_crc16,
                                 ctx->return_stuffed_payload, ctx->return_stuffed_payload_size, &chunk_size);
    }
    else
    {
        err = _send_data_frame(handle, true, ctx->recv_sender_address, ctx->return_random_id,
                                ctx->return_stuffed_payload, ctx->return_stuffed_payload_offset, ctx->return_stuffed_payload_size, &chunk_size);
    }

    if (err == ARTIE_CAN_ERR_NONE)
    {
        ctx->return_last_chunk_size = chunk_size;
        ctx->last_activity_ms = handle->get_ms();
    }
    return err;
}

// ---------------------------------------------------------------------------
// Randomness (traceability field)
// ---------------------------------------------------------------------------

static uint8_t _generate_random_id(artie_can_backend_t *handle)
{
    static uint8_t counter = 0;
    counter = (uint8_t)(counter + 1U);
    uint64_t t = handle->get_ms();
    uint8_t mixed = (uint8_t)((t ^ (t >> 8) ^ (t >> 16)) & 0xFFU);
    return (uint8_t)(mixed ^ counter);
}

// ---------------------------------------------------------------------------
// Busy-node tracking
// ---------------------------------------------------------------------------

static void _set_node_busy(rpcacp_context_t *ctx, uint8_t node_address)
{
    if (node_address < 64U) { ctx->busy_nodes_bitmap |= (1ULL << node_address); }
}

static void _clear_node_busy(rpcacp_context_t *ctx, uint8_t node_address)
{
    if (node_address < 64U) { ctx->busy_nodes_bitmap &= ~(1ULL << node_address); }
}

// ---------------------------------------------------------------------------
// Outgoing call completion
// ---------------------------------------------------------------------------

static void _finish_call(rpcacp_context_t *ctx, artie_can_error_t error, uint8_t errno_code, bool has_result)
{
    ctx->call_last_error = error;
    ctx->call_last_errno = errno_code;
    ctx->call_result_ready = has_result;
    ctx->call_active = false;
    ctx->call_completed = true;
    ctx->state = RPCACP_STATE_IDLE;
}

// ---------------------------------------------------------------------------
// Servicing an inbound request (this node is the remote/executee)
// ---------------------------------------------------------------------------

static artie_can_error_t _finish_incoming_request(artie_can_backend_t *handle, uint8_t errno_code)
{
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    artie_can_error_t err = _send_nack_frame(handle, ctx->recv_sender_address, ctx->recv_random_id, errno_code);
    ctx->recv_stuffed_payload_size = 0;
    ctx->state = RPCACP_STATE_IDLE;
    return err;
}

static artie_can_error_t _finish_incoming_request_with_return(artie_can_backend_t *handle, const uint8_t *return_raw, uint32_t return_len)
{
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;

    if (!ctx->recv_synchronous)
    {
        // Asynchronous: accept the request. No return value is ever sent.
        artie_can_error_t err = _send_ack_frame(handle, ctx->recv_sender_address, ctx->recv_random_id);
        ctx->recv_stuffed_payload_size = 0;
        ctx->state = RPCACP_STATE_IDLE;
        return err;
    }

    if (!_byte_stuff(return_raw, return_len, ctx->return_stuffed_payload, sizeof(ctx->return_stuffed_payload), &ctx->return_stuffed_payload_size))
    {
        artie_can_error_t err = _send_nack_frame(handle, ctx->recv_sender_address, ctx->recv_random_id, ARTIE_CAN_RPCACP_ERRNO_E2BIG);
        ctx->recv_stuffed_payload_size = 0;
        ctx->state = RPCACP_STATE_IDLE;
        return err;
    }

    uint8_t header_byte = (uint8_t)(0x80U | (ctx->recv_procedure_id & 0x7FU));
    ctx->return_crc16 = _crc16_ccitt_update(CRC16_INIT, &header_byte, 1U);
    ctx->return_crc16 = _crc16_ccitt_update(ctx->return_crc16, ctx->return_stuffed_payload, ctx->return_stuffed_payload_size);

    // Accept the request with its final ACK. The requesting node transitions to waiting for the
    // return value only once it processes this ACK, so the StartReturn frame is deferred slightly
    // (see _handle_sending_return) rather than sent back-to-back with the ACK.
    artie_can_error_t err = _send_ack_frame(handle, ctx->recv_sender_address, ctx->recv_random_id);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        ctx->recv_stuffed_payload_size = 0;
        ctx->state = RPCACP_STATE_IDLE;
        return err;
    }

    ctx->recv_stuffed_payload_size = 0;
    ctx->return_random_id = _generate_random_id(handle);
    ctx->return_stuffed_payload_offset = 0;
    ctx->return_last_chunk_size = 0;
    ctx->retry_count = 0;
    ctx->last_activity_ms = handle->get_ms();
    ctx->state = RPCACP_STATE_SENDING_RETURN;
    return ARTIE_CAN_ERR_NONE;
}

/** Called once a full request payload has been accumulated (byte-stuffing terminal found). Sends the final ACK/NACK itself. */
static artie_can_error_t _service_request(artie_can_backend_t *handle)
{
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;

    uint8_t header_byte = (uint8_t)((ctx->recv_synchronous ? 0x80U : 0x00U) | (ctx->recv_procedure_id & 0x7FU));
    uint16_t crc = _crc16_ccitt_update(CRC16_INIT, &header_byte, 1U);
    crc = _crc16_ccitt_update(crc, ctx->recv_stuffed_payload, ctx->recv_stuffed_payload_size);

    if (crc != ctx->recv_expected_crc16)
    {
        ARTIE_CAN_LOG(handle->context, "RPCACP: CRC mismatch servicing request from 0x%02X\n", ctx->recv_sender_address);
        return _finish_incoming_request(handle, ARTIE_CAN_RPCACP_ERRNO_TRANSMISSION);
    }

    uint8_t raw_payload[ARTIE_CAN_RPCACP_MAX_PAYLOAD_SIZE];
    uint32_t raw_len = 0;
    if (!_byte_unstuff(ctx->recv_stuffed_payload, ctx->recv_stuffed_payload_size, raw_payload, sizeof(raw_payload), &raw_len))
    {
        return _finish_incoming_request(handle, ARTIE_CAN_RPCACP_ERRNO_ENOEXEC);
    }

    if (ctx->recv_procedure_id == ARTIE_CAN_RPC_ID_WHOAMI)
    {
        uint8_t return_raw[ARTIE_CAN_RPCACP_MAX_PAYLOAD_SIZE];
        uint32_t return_len = _pack_whoami(ctx, return_raw, sizeof(return_raw));
        return _finish_incoming_request_with_return(handle, return_raw, return_len);
    }
    else if (ctx->recv_procedure_id == ARTIE_CAN_RPC_ID_STATUS)
    {
        uint8_t return_raw[16];
        uint64_t uptime = handle->get_ms() - ctx->boot_time_ms;
        uint32_t return_len = _pack_status(uptime, ctx->status_err_flags, return_raw);
        return _finish_incoming_request_with_return(handle, return_raw, return_len);
    }
    else if (ctx->recv_procedure_id == ARTIE_CAN_RPC_ID_LIST)
    {
        if ((raw_len < 1U) || (raw_payload[0] >= ARTIE_CAN_RPCACP_LIST_PAGE_COUNT))
        {
            return _finish_incoming_request(handle, ARTIE_CAN_RPCACP_ERRNO_EINVAL);
        }

        uint8_t return_raw[ARTIE_CAN_RPCACP_MAX_PAYLOAD_SIZE];
        uint32_t return_len = 0;
        if (!_pack_list_page(ctx, raw_payload[0], return_raw, sizeof(return_raw), &return_len))
        {
            return _finish_incoming_request(handle, ARTIE_CAN_RPCACP_ERRNO_E2BIG);
        }
        return _finish_incoming_request_with_return(handle, return_raw, return_len);
    }
    else if (ctx->recv_procedure_id <= ARTIE_CAN_RPCACP_RESERVED_ID_MAX)
    {
        return _finish_incoming_request(handle, ARTIE_CAN_RPCACP_ERRNO_EPERM);
    }

    const artie_can_rpc_signature_t *sig = _find_registered(ctx, ctx->recv_procedure_id);
    if (sig == NULL)
    {
        return _finish_incoming_request(handle, ARTIE_CAN_RPCACP_ERRNO_EPERM);
    }
    if (sig->synchronous != ctx->recv_synchronous)
    {
        return _finish_incoming_request(handle, ARTIE_CAN_RPCACP_ERRNO_EINVAL);
    }
    if (sig->function == NULL)
    {
        return _finish_incoming_request(handle, ARTIE_CAN_RPCACP_ERRNO_EPERM);
    }

    // If every parameter has a fixed wire size, we can detect an argument list that is too long
    // for this signature (i.e., the two nodes' signatures do not match).
    bool all_params_fixed_size = true;
    uint32_t expected_max_len = 0;
    for (uint8_t i = 0; i < sig->param_count; i++)
    {
        const artie_can_rpc_param_descriptor_t *desc = &sig->params[i];
        if (_type_name_is(desc->type_name, "NULL")) { continue; }

        uint32_t wire_size = _primitive_wire_size(desc->type_name);
        if (wire_size == 0U)
        {
            all_params_fixed_size = false;
            break;
        }
        if (((uint32_t)desc->offset_in_msgpack + wire_size) > expected_max_len)
        {
            expected_max_len = (uint32_t)desc->offset_in_msgpack + wire_size;
        }
    }
    if (all_params_fixed_size && (raw_len > expected_max_len))
    {
        return _finish_incoming_request(handle, ARTIE_CAN_RPCACP_ERRNO_E2BIG);
    }

    const void *param_ptrs[ARTIE_CAN_RPCACP_MAX_PARAMS] = {0};
    uint8_t scratch[ARTIE_CAN_RPCACP_MAX_PARAMS][8];
    if (_prepare_dispatch_params(sig, raw_payload, raw_len, param_ptrs, scratch) != ARTIE_CAN_ERR_NONE)
    {
        return _finish_incoming_request(handle, ARTIE_CAN_RPCACP_ERRNO_EINVAL);
    }

    ctx->state = RPCACP_STATE_EXECUTING_PROC;
    uint8_t return_buffer[ARTIE_CAN_RPCACP_MAX_PAYLOAD_SIZE];
    void *result = sig->function(param_ptrs, sig->param_count, return_buffer, sizeof(return_buffer));
    if (result == NULL)
    {
        return _finish_incoming_request(handle, ARTIE_CAN_RPCACP_ERRNO_EINVAL);
    }

    if (sig->return_descriptor == NULL)
    {
        return _finish_incoming_request_with_return(handle, NULL, 0U);
    }

    uint8_t return_raw[ARTIE_CAN_RPCACP_MAX_PAYLOAD_SIZE];
    uint32_t return_len = 0;
    if (_pack_return_value(sig, return_buffer, return_raw, sizeof(return_raw), &return_len) != ARTIE_CAN_ERR_NONE)
    {
        return _finish_incoming_request(handle, ARTIE_CAN_RPCACP_ERRNO_E2BIG);
    }
    return _finish_incoming_request_with_return(handle, return_raw, return_len);
}

static artie_can_error_t _check_request_terminal(artie_can_backend_t *handle)
{
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    uint32_t total_len = 0;
    byte_stuff_scan_result_t scan = _byte_stuff_scan(ctx->recv_stuffed_payload, ctx->recv_stuffed_payload_size, &total_len);

    if (scan == BYTE_STUFF_SCAN_MALFORMED)
    {
        return _finish_incoming_request(handle, ARTIE_CAN_RPCACP_ERRNO_ENOEXEC);
    }
    else if (scan == BYTE_STUFF_SCAN_NEED_MORE)
    {
        return _send_ack_frame(handle, ctx->recv_sender_address, ctx->recv_random_id);
    }

    ctx->recv_stuffed_payload_size = total_len;
    return _service_request(handle);
}

static artie_can_error_t _handle_start_rpc(artie_can_backend_t *handle, uint8_t sender, uint8_t random, const artie_can_frame_t *frame)
{
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;

    if (frame->dlc < 3U) { return ARTIE_CAN_ERR_NONE; } // malformed; sender will time out and retry

    uint8_t header_byte = frame->data[0];
    bool synchronous = (header_byte & 0x80U) != 0U;
    uint8_t procedure_id = (uint8_t)(header_byte & 0x7FU);

    if (ctx->state != RPCACP_STATE_IDLE)
    {
        bool same_request = (ctx->state == RPCACP_STATE_RECEIVING_REQUEST) &&
                             (ctx->recv_sender_address == sender) && (ctx->recv_procedure_id == procedure_id);
        return _send_nack_frame(handle, sender, random, same_request ? ARTIE_CAN_RPCACP_ERRNO_EALREADY : ARTIE_CAN_RPCACP_ERRNO_EAGAIN);
    }

    ctx->recv_sender_address = sender;
    ctx->recv_random_id = random;
    ctx->recv_procedure_id = procedure_id;
    ctx->recv_synchronous = synchronous;
    ctx->recv_expected_crc16 = (uint16_t)(((uint16_t)frame->data[1] << 8) | frame->data[2]);
    ctx->recv_stuffed_payload_size = 0;

    uint32_t chunk = (uint32_t)frame->dlc - 3U;
    if (chunk > 0U)
    {
        memcpy(ctx->recv_stuffed_payload, &frame->data[3], chunk);
        ctx->recv_stuffed_payload_size = chunk;
    }

    ctx->state = RPCACP_STATE_RECEIVING_REQUEST;
    ctx->last_activity_ms = handle->get_ms();
    return _check_request_terminal(handle);
}

static artie_can_error_t _handle_tx_data(artie_can_backend_t *handle, uint8_t sender, uint8_t random, const artie_can_frame_t *frame)
{
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    if (ctx->state != RPCACP_STATE_RECEIVING_REQUEST) { return ARTIE_CAN_ERR_NONE; }
    if ((sender != ctx->recv_sender_address) || (random != ctx->recv_random_id)) { return ARTIE_CAN_ERR_NONE; }

    uint32_t chunk = frame->dlc;
    if ((ctx->recv_stuffed_payload_size + chunk) > sizeof(ctx->recv_stuffed_payload))
    {
        return _finish_incoming_request(handle, ARTIE_CAN_RPCACP_ERRNO_ENOEXEC);
    }

    memcpy(&ctx->recv_stuffed_payload[ctx->recv_stuffed_payload_size], frame->data, chunk);
    ctx->recv_stuffed_payload_size += chunk;
    ctx->last_activity_ms = handle->get_ms();
    return _check_request_terminal(handle);
}

// ---------------------------------------------------------------------------
// Receiving a return value (this node is the original caller)
// ---------------------------------------------------------------------------

static artie_can_error_t _check_return_terminal(artie_can_backend_t *handle)
{
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    uint32_t total_len = 0;
    byte_stuff_scan_result_t scan = _byte_stuff_scan(ctx->recv_stuffed_payload, ctx->recv_stuffed_payload_size, &total_len);

    if (scan == BYTE_STUFF_SCAN_MALFORMED)
    {
        artie_can_error_t err = _send_nack_frame(handle, ctx->call_target_address, ctx->recv_random_id, ARTIE_CAN_RPCACP_ERRNO_ENOEXEC);
        _finish_call(ctx, ARTIE_CAN_ERR_RECEIVE_FAIL, 0, false);
        return err;
    }
    else if (scan == BYTE_STUFF_SCAN_NEED_MORE)
    {
        return _send_ack_frame(handle, ctx->call_target_address, ctx->recv_random_id);
    }

    ctx->recv_stuffed_payload_size = total_len;
    uint8_t header_byte = (uint8_t)(0x80U | (ctx->call_signature->procedure_id & 0x7FU));
    uint16_t crc = _crc16_ccitt_update(CRC16_INIT, &header_byte, 1U);
    crc = _crc16_ccitt_update(crc, ctx->recv_stuffed_payload, ctx->recv_stuffed_payload_size);

    if (crc != ctx->recv_expected_crc16)
    {
        // Ask the remote node to resend the whole return value; keep waiting.
        artie_can_error_t err = _send_nack_frame(handle, ctx->call_target_address, ctx->recv_random_id, ARTIE_CAN_RPCACP_ERRNO_TRANSMISSION);
        ctx->recv_stuffed_payload_size = 0;
        ctx->last_activity_ms = handle->get_ms();
        return err;
    }

    if (!_byte_unstuff(ctx->recv_stuffed_payload, ctx->recv_stuffed_payload_size, ctx->call_return_raw, sizeof(ctx->call_return_raw), &ctx->call_return_size))
    {
        artie_can_error_t err = _send_nack_frame(handle, ctx->call_target_address, ctx->recv_random_id, ARTIE_CAN_RPCACP_ERRNO_ENOEXEC);
        _finish_call(ctx, ARTIE_CAN_ERR_RECEIVE_FAIL, 0, false);
        return err;
    }

    artie_can_error_t err = _send_ack_frame(handle, ctx->call_target_address, ctx->recv_random_id);
    _finish_call(ctx, ARTIE_CAN_ERR_NONE, 0, true);
    return err;
}

static artie_can_error_t _handle_start_return(artie_can_backend_t *handle, uint8_t sender, uint8_t random, const artie_can_frame_t *frame)
{
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    if (ctx->state != RPCACP_STATE_WAITING_RETURN) { return ARTIE_CAN_ERR_NONE; }
    if (sender != ctx->call_target_address) { return ARTIE_CAN_ERR_NONE; }
    if (frame->dlc < 3U) { return ARTIE_CAN_ERR_NONE; }

    ctx->recv_random_id = random;
    ctx->recv_expected_crc16 = (uint16_t)(((uint16_t)frame->data[1] << 8) | frame->data[2]);
    ctx->recv_stuffed_payload_size = 0;

    uint32_t chunk = (uint32_t)frame->dlc - 3U;
    if (chunk > 0U)
    {
        memcpy(ctx->recv_stuffed_payload, &frame->data[3], chunk);
        ctx->recv_stuffed_payload_size = chunk;
    }

    ctx->last_activity_ms = handle->get_ms();
    return _check_return_terminal(handle);
}

static artie_can_error_t _handle_rx_data(artie_can_backend_t *handle, uint8_t sender, uint8_t random, const artie_can_frame_t *frame)
{
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    if (ctx->state != RPCACP_STATE_WAITING_RETURN) { return ARTIE_CAN_ERR_NONE; }
    if ((sender != ctx->call_target_address) || (random != ctx->recv_random_id)) { return ARTIE_CAN_ERR_NONE; }

    uint32_t chunk = frame->dlc;
    if ((ctx->recv_stuffed_payload_size + chunk) > sizeof(ctx->recv_stuffed_payload))
    {
        artie_can_error_t err = _send_nack_frame(handle, ctx->call_target_address, random, ARTIE_CAN_RPCACP_ERRNO_ENOEXEC);
        _finish_call(ctx, ARTIE_CAN_ERR_RECEIVE_FAIL, 0, false);
        return err;
    }

    memcpy(&ctx->recv_stuffed_payload[ctx->recv_stuffed_payload_size], frame->data, chunk);
    ctx->recv_stuffed_payload_size += chunk;
    ctx->last_activity_ms = handle->get_ms();
    return _check_return_terminal(handle);
}

// ---------------------------------------------------------------------------
// ACK / NACK handling for our own outgoing frames (either a request or a return)
// ---------------------------------------------------------------------------

static artie_can_error_t _handle_ack(artie_can_backend_t *handle, uint8_t sender, uint8_t random)
{
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    artie_can_error_t err = ARTIE_CAN_ERR_NONE;

    if ((ctx->state == RPCACP_STATE_SENDING_REQUEST) && (sender == ctx->call_target_address) && (random == ctx->call_random_id))
    {
        ctx->call_stuffed_payload_offset += ctx->call_last_chunk_size;
        ctx->retry_count = 0;

        if (ctx->call_stuffed_payload_offset >= ctx->call_stuffed_payload_size)
        {
            _clear_node_busy(ctx, ctx->call_target_address);
            if (ctx->call_signature->synchronous)
            {
                ctx->recv_stuffed_payload_size = 0;
                ctx->state = RPCACP_STATE_WAITING_RETURN;
                ctx->last_activity_ms = handle->get_ms();
            }
            else
            {
                _set_node_busy(ctx, ctx->call_target_address);
                _finish_call(ctx, ARTIE_CAN_ERR_NONE, 0, false);
            }
        }
        else
        {
            err = _send_next_request_chunk(handle);
            if (err != ARTIE_CAN_ERR_NONE)
            {
                _finish_call(ctx, ARTIE_CAN_ERR_SEND_FAIL, 0, false);
            }
        }
    }
    else if ((ctx->state == RPCACP_STATE_SENDING_RETURN) && (sender == ctx->recv_sender_address) && (random == ctx->return_random_id))
    {
        ctx->return_stuffed_payload_offset += ctx->return_last_chunk_size;
        ctx->retry_count = 0;

        if (ctx->return_stuffed_payload_offset >= ctx->return_stuffed_payload_size)
        {
            ctx->state = RPCACP_STATE_IDLE;
        }
        else
        {
            err = _send_next_return_chunk(handle);
            if (err != ARTIE_CAN_ERR_NONE)
            {
                ctx->state = RPCACP_STATE_IDLE;
            }
        }
    }
    // else: stale/unrelated ACK; ignore.

    return err;
}

static artie_can_error_t _handle_nack(artie_can_backend_t *handle, uint8_t sender, uint8_t random, uint8_t errno_code)
{
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    artie_can_error_t err = ARTIE_CAN_ERR_NONE;

    if ((ctx->state == RPCACP_STATE_SENDING_REQUEST) && (sender == ctx->call_target_address) && (random == ctx->call_random_id))
    {
        bool is_final_chunk = (ctx->call_stuffed_payload_offset + ctx->call_last_chunk_size) >= ctx->call_stuffed_payload_size;

        if (is_final_chunk && (errno_code != ARTIE_CAN_RPCACP_ERRNO_TRANSMISSION))
        {
            if ((errno_code == ARTIE_CAN_RPCACP_ERRNO_EAGAIN) || (errno_code == ARTIE_CAN_RPCACP_ERRNO_EALREADY))
            {
                _set_node_busy(ctx, ctx->call_target_address);
            }
            else
            {
                _clear_node_busy(ctx, ctx->call_target_address);
            }
            _finish_call(ctx, ARTIE_CAN_ERR_INVALID_ARG, errno_code, false);
            return err;
        }

        if (ctx->retry_count >= ARTIE_CAN_RPCACP_MAX_RETRIES)
        {
            _finish_call(ctx, ARTIE_CAN_ERR_TIMEOUT, 0, false);
            return err;
        }
        ctx->retry_count++;
        if (is_final_chunk) { ctx->call_stuffed_payload_offset = 0; } // full resend requested
        err = _send_next_request_chunk(handle);
        if (err != ARTIE_CAN_ERR_NONE)
        {
            _finish_call(ctx, ARTIE_CAN_ERR_SEND_FAIL, 0, false);
        }
    }
    else if ((ctx->state == RPCACP_STATE_SENDING_RETURN) && (sender == ctx->recv_sender_address) && (random == ctx->return_random_id))
    {
        if (ctx->retry_count >= ARTIE_CAN_RPCACP_MAX_RETRIES)
        {
            ctx->state = RPCACP_STATE_IDLE;
            return err;
        }
        ctx->retry_count++;

        bool is_final_chunk = (ctx->return_stuffed_payload_offset + ctx->return_last_chunk_size) >= ctx->return_stuffed_payload_size;
        if (is_final_chunk) { ctx->return_stuffed_payload_offset = 0; } // full resend requested
        err = _send_next_return_chunk(handle);
        if (err != ARTIE_CAN_ERR_NONE)
        {
            ctx->state = RPCACP_STATE_IDLE;
        }
    }
    // else: stale/unrelated NACK; ignore.

    return err;
}

// ---------------------------------------------------------------------------
// State machine handlers
// ---------------------------------------------------------------------------

/**
 * @brief Handle SENDING_REQUEST state - retry the outgoing request chunk on ACK timeout.
 */
static artie_can_error_t _handle_sending_request(artie_can_backend_t *handle)
{
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    uint64_t now = handle->get_ms();
    artie_can_error_t err = ARTIE_CAN_ERR_NONE;

    if ((now - ctx->last_activity_ms) >= ARTIE_CAN_RPCACP_ACK_TIMEOUT_MS)
    {
        if (ctx->retry_count >= ARTIE_CAN_RPCACP_MAX_RETRIES)
        {
            _finish_call(ctx, ARTIE_CAN_ERR_NO_RESPONSE, 0, false);
            err = ARTIE_CAN_ERR_NO_RESPONSE;
        }
        else
        {
            ctx->retry_count++;
            err = _send_next_request_chunk(handle);
            if (err != ARTIE_CAN_ERR_NONE)
            {
                _finish_call(ctx, ARTIE_CAN_ERR_SEND_FAIL, 0, false);
            }
        }
    }

    return err;
}

/**
 * @brief Handle WAITING_RETURN state - give up on the call if the return value never arrives.
 */
static artie_can_error_t _handle_waiting_return(artie_can_backend_t *handle)
{
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    uint64_t now = handle->get_ms();
    artie_can_error_t err = ARTIE_CAN_ERR_NONE;

    if ((now - ctx->last_activity_ms) >= ARTIE_CAN_RPCACP_RESPONSE_TIMEOUT_MS)
    {
        _finish_call(ctx, ARTIE_CAN_ERR_TIMEOUT, 0, false);
        err = ARTIE_CAN_ERR_TIMEOUT;
    }

    return err;
}

/**
 * @brief Handle SENDING_RETURN state - retry the outgoing return chunk on ACK timeout.
 */
static artie_can_error_t _handle_sending_return(artie_can_backend_t *handle)
{
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    uint64_t now = handle->get_ms();
    artie_can_error_t err = ARTIE_CAN_ERR_NONE;

    if ((ctx->return_stuffed_payload_offset == 0U) && (ctx->return_last_chunk_size == 0U))
    {
        // The StartReturn frame has not been sent yet; it is deferred until shortly after the
        // request's final ACK so the requester processes that ACK first (see
        // _finish_incoming_request_with_return).
        if ((now - ctx->last_activity_ms) >= ARTIE_CAN_RPCACP_RETURN_START_DELAY_MS)
        {
            err = _send_next_return_chunk(handle);
            if (err != ARTIE_CAN_ERR_NONE)
            {
                ctx->state = RPCACP_STATE_IDLE;
            }
        }
        return err;
    }

    if ((now - ctx->last_activity_ms) >= ARTIE_CAN_RPCACP_ACK_TIMEOUT_MS)
    {
        if (ctx->retry_count >= ARTIE_CAN_RPCACP_MAX_RETRIES)
        {
            ctx->state = RPCACP_STATE_IDLE;
            err = ARTIE_CAN_ERR_TIMEOUT;
        }
        else
        {
            ctx->retry_count++;
            err = _send_next_return_chunk(handle);
            if (err != ARTIE_CAN_ERR_NONE)
            {
                ctx->state = RPCACP_STATE_IDLE;
            }
        }
    }

    return err;
}

/**
 * @brief Handle RECEIVING_REQUEST state - abandon the inbound request if it stalls.
 */
static artie_can_error_t _handle_receiving_request(artie_can_backend_t *handle)
{
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    uint64_t now = handle->get_ms();
    artie_can_error_t err = ARTIE_CAN_ERR_NONE;

    if ((now - ctx->last_activity_ms) >= ARTIE_CAN_RPCACP_RESPONSE_TIMEOUT_MS)
    {
        ctx->recv_stuffed_payload_size = 0;
        ctx->state = RPCACP_STATE_IDLE;
        err = ARTIE_CAN_ERR_TIMEOUT;
    }

    return err;
}

// ---------------------------------------------------------------------------
// Top-level frame dispatch
// ---------------------------------------------------------------------------

static artie_can_error_t _process_received_frame(artie_can_backend_t *handle)
{
    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    const artie_can_frame_t *frame = &ctx->received_frame;
    artie_can_error_t err = ARTIE_CAN_ERR_NONE;

    uint8_t frame_type = (uint8_t)((frame->id & ARTIE_CAN_FRAME_ID_FRAME_TYPE_MASK) >> ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION);
    uint8_t sender = (uint8_t)((frame->id & ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK) >> ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION);
    uint8_t random = (uint8_t)((frame->id & RPCACP_FRAME_ID_RANDOM_MASK) >> RPCACP_FRAME_ID_RANDOM_LOCATION);

    switch (frame_type)
    {
        case ARTIE_CAN_FRAME_TYPE_RPCACP_ACK:
            err = _handle_ack(handle, sender, random);
            break;
        case ARTIE_CAN_FRAME_TYPE_RPCACP_NACK:
        {
            uint8_t errno_code = (frame->dlc >= 1U) ? frame->data[0] : ARTIE_CAN_RPCACP_ERRNO_TRANSMISSION;
            err = _handle_nack(handle, sender, random, errno_code);
            break;
        }
        case ARTIE_CAN_FRAME_TYPE_RPCACP_START_RPC:
            err = _handle_start_rpc(handle, sender, random, frame);
            break;
        case ARTIE_CAN_FRAME_TYPE_RPCACP_TX_DATA:
            err = _handle_tx_data(handle, sender, random, frame);
            break;
        case ARTIE_CAN_FRAME_TYPE_RPCACP_START_RETURN:
            err = _handle_start_return(handle, sender, random, frame);
            break;
        case ARTIE_CAN_FRAME_TYPE_RPCACP_RX_DATA:
            err = _handle_rx_data(handle, sender, random, frame);
            break;
        default:
            break;
    }

    return err;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

artie_can_error_t artie_can_init_context_rpcacp(artie_can_context_t *ctx, uint8_t node_address, uint8_t node_class, const char *node_name, const char *fw_version)
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

    // Initialize RPCACP context
    memset(&ctx->rpcacp_context, 0, sizeof(rpcacp_context_t));
    ctx->rpcacp_context.node_address = node_address;
    ctx->rpcacp_context.node_class = node_class;
    ctx->rpcacp_context.state = RPCACP_STATE_IDLE;

    if (node_name != NULL)
    {
        strncpy(ctx->rpcacp_context.node_name, node_name, ARTIE_CAN_RPCACP_MAX_NAME_LENGTH);
    }
    if (fw_version != NULL)
    {
        strncpy(ctx->rpcacp_context.fw_version, fw_version, ARTIE_CAN_RPCACP_MAX_NAME_LENGTH);
    }

    // Enable RPCACP protocol
    ctx->protocol_flags |= ARTIE_CAN_PROTOCOL_FLAG_RPCACP;
    ctx->node_address = node_address;

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_rpcacp_set_status_err_flags(artie_can_context_t *context, uint32_t err_flags)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    context->rpcacp_context.status_err_flags = err_flags;
    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_rpcacp_register_procedure(artie_can_backend_t *handle, const artie_can_rpc_signature_t *signature)
{
    if ((handle == NULL) || (handle->context == NULL) || (signature == NULL))
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (signature->procedure_id <= ARTIE_CAN_RPCACP_RESERVED_ID_MAX)
    {
        // WHOAMI/STATUS/LIST (and the rest of the reserved range) are always answered internally.
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (signature->procedure_id > 0x7FU)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (signature->param_count > (uint8_t)ARTIE_CAN_RPCACP_MAX_PARAMS)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    rpcacp_context_t *ctx = &handle->context->rpcacp_context;

    for (uint8_t i = 0; i < ctx->registered_procedure_count; i++)
    {
        if (ctx->registered_procedures[i].procedure_id == signature->procedure_id)
        {
            ctx->registered_procedures[i] = *signature;
            ARTIE_CAN_LOG(handle->context, "RPCACP: Updated registered procedure 0x%04X (%s)\n", signature->procedure_id, signature->name);
            return ARTIE_CAN_ERR_NONE;
        }
    }

    if (ctx->registered_procedure_count >= ARTIE_CAN_RPCACP_MAX_REGISTERED_PROCEDURES)
    {
        return ARTIE_CAN_ERR_NO_SPACE;
    }

    ctx->registered_procedures[ctx->registered_procedure_count] = *signature;
    ctx->registered_procedure_count++;

    ARTIE_CAN_LOG(handle->context, "RPCACP: Registered procedure 0x%04X (%s)\n", signature->procedure_id, signature->name);
    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_rpcacp_call(artie_can_backend_t *handle, uint8_t target_address, const artie_can_rpc_signature_t *signature,
                                         const artie_can_rpc_value_t *args, uint8_t arg_count)
{
    if ((handle == NULL) || (handle->context == NULL) || (signature == NULL))
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (target_address == 0U)
    {
        // There is no broadcast in RPCACP.
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (signature->procedure_id > 0x7FU)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (arg_count > signature->param_count)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    if (ctx->state != RPCACP_STATE_IDLE)
    {
        return ARTIE_CAN_ERR_SEND_BUSY;
    }

    uint8_t raw_payload[ARTIE_CAN_RPCACP_MAX_PAYLOAD_SIZE];
    uint32_t raw_len = 0;
    artie_can_error_t err = _pack_values(signature->params, signature->param_count, args, arg_count, raw_payload, sizeof(raw_payload), &raw_len);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    if (!_byte_stuff(raw_payload, raw_len, ctx->call_stuffed_payload, sizeof(ctx->call_stuffed_payload), &ctx->call_stuffed_payload_size))
    {
        return ARTIE_CAN_ERR_NO_SPACE;
    }

    uint8_t header_byte = (uint8_t)((signature->synchronous ? 0x80U : 0x00U) | (signature->procedure_id & 0x7FU));
    ctx->call_crc16 = _crc16_ccitt_update(CRC16_INIT, &header_byte, 1U);
    ctx->call_crc16 = _crc16_ccitt_update(ctx->call_crc16, ctx->call_stuffed_payload, ctx->call_stuffed_payload_size);

    ctx->call_target_address = target_address;
    ctx->call_signature = signature;
    ctx->call_random_id = _generate_random_id(handle);
    ctx->call_stuffed_payload_offset = 0;
    ctx->call_last_chunk_size = 0;
    ctx->call_active = true;
    ctx->call_completed = false;
    ctx->call_result_ready = false;
    ctx->retry_count = 0;
    ctx->state = RPCACP_STATE_SENDING_REQUEST;

    err = _send_next_request_chunk(handle);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        _finish_call(ctx, err, 0, false);
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_rpcacp_get_last_error(artie_can_backend_t *handle, uint8_t *out_errno)
{
    if ((handle == NULL) || (handle->context == NULL))
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    if (!ctx->call_completed || ctx->call_active)
    {
        return ARTIE_CAN_ERR_NO_DATA;
    }

    if (out_errno != NULL)
    {
        *out_errno = ctx->call_last_errno;
    }
    return ctx->call_last_error;
}

artie_can_error_t artie_can_rpcacp_get_result(artie_can_backend_t *handle, const artie_can_rpc_signature_t *signature, void *out, uint32_t out_size)
{
    if ((handle == NULL) || (handle->context == NULL) || (signature == NULL) || (out == NULL))
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }
    else if (signature->return_descriptor == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    if (!ctx->call_result_ready)
    {
        return ARTIE_CAN_ERR_NO_DATA;
    }
    else if (out_size < signature->return_size)
    {
        return ARTIE_CAN_ERR_NO_SPACE;
    }

    uint32_t offset = signature->return_descriptor->offset_in_msgpack;

    if (_type_name_is(signature->return_descriptor->type_name, "float"))
    {
        if ((offset + 2U) > ctx->call_return_size) { return ARTIE_CAN_ERR_NO_DATA; }
        uint16_t half = (uint16_t)(((uint16_t)ctx->call_return_raw[offset] << 8) | ctx->call_return_raw[offset + 1U]);
        float f = _float16_to_float32(half);
        memcpy(out, &f, sizeof(f));
    }
    else if (_type_name_is(signature->return_descriptor->type_name, "double"))
    {
        if ((offset + 4U) > ctx->call_return_size) { return ARTIE_CAN_ERR_NO_DATA; }
        uint32_t bits = ((uint32_t)ctx->call_return_raw[offset] << 24) | ((uint32_t)ctx->call_return_raw[offset + 1U] << 16) |
                        ((uint32_t)ctx->call_return_raw[offset + 2U] << 8) | (uint32_t)ctx->call_return_raw[offset + 3U];
        float f;
        memcpy(&f, &bits, sizeof(f));
        double d = (double)f;
        memcpy(out, &d, sizeof(d));
    }
    else
    {
        if ((offset + signature->return_size) > ctx->call_return_size) { return ARTIE_CAN_ERR_NO_DATA; }
        memcpy(out, &ctx->call_return_raw[offset], signature->return_size);
    }

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_rpcacp_get_whoami_result(artie_can_backend_t *handle, artie_can_whoami_response_t *out)
{
    if ((handle == NULL) || (handle->context == NULL) || (out == NULL))
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    if (!ctx->call_result_ready || (ctx->call_return_size < 2U))
    {
        return ARTIE_CAN_ERR_NO_DATA;
    }

    out->node_address = ctx->call_return_raw[0];
    out->node_name = (char *)&ctx->call_return_raw[1];
    size_t name_len = strlen(out->node_name);
    out->fw_version = (char *)&ctx->call_return_raw[1U + name_len + 1U];

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_rpcacp_get_status_result(artie_can_backend_t *handle, artie_can_status_response_t *out)
{
    if ((handle == NULL) || (handle->context == NULL) || (out == NULL))
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    if (!ctx->call_result_ready || (ctx->call_return_size < 12U))
    {
        return ARTIE_CAN_ERR_NO_DATA;
    }

    uint64_t uptime = 0;
    for (int i = 0; i < 8; i++) { uptime = (uptime << 8) | ctx->call_return_raw[i]; }
    uint32_t flags = 0;
    for (int i = 0; i < 4; i++) { flags = (flags << 8) | ctx->call_return_raw[8 + i]; }

    out->uptime_ms = uptime;
    out->err_flags = flags;
    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_rpcacp_get_list_result(artie_can_backend_t *handle, artie_can_rpc_signature_t out[ARTIE_CAN_RPCACP_LIST_PAGE_SIZE])
{
    if ((handle == NULL) || (handle->context == NULL) || (out == NULL))
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    if (!ctx->call_result_ready)
    {
        return ARTIE_CAN_ERR_NO_DATA;
    }

    uint32_t pos = 0;
    for (uint8_t i = 0; i < (uint8_t)ARTIE_CAN_RPCACP_LIST_PAGE_SIZE; i++)
    {
        if (pos >= ctx->call_return_size) { return ARTIE_CAN_ERR_NO_DATA; }

        memset(&out[i], 0, sizeof(out[i]));
        out[i].procedure_id = ctx->call_return_raw[pos++];

        out[i].name = (char *)&ctx->call_return_raw[pos];
        pos += (uint32_t)strlen((const char *)&ctx->call_return_raw[pos]) + 1U;
        if ((pos + 2U) > ctx->call_return_size) { return ARTIE_CAN_ERR_NO_DATA; }

        out[i].synchronous = ctx->call_return_raw[pos++] != 0U;
        out[i].param_count = ctx->call_return_raw[pos++];

        for (uint8_t p = 0; (p < out[i].param_count) && (p < (uint8_t)ARTIE_CAN_RPCACP_MAX_PARAMS); p++)
        {
            if (pos >= ctx->call_return_size) { return ARTIE_CAN_ERR_NO_DATA; }
            out[i].params[p].type_name = (char *)&ctx->call_return_raw[pos];
            pos += (uint32_t)strlen((const char *)&ctx->call_return_raw[pos]) + 1U;
            if ((pos + 2U) > ctx->call_return_size) { return ARTIE_CAN_ERR_NO_DATA; }
            out[i].params[p].offset_in_msgpack = ctx->call_return_raw[pos++];
            out[i].params[p].optional = ctx->call_return_raw[pos++] != 0U;
        }
    }

    return ARTIE_CAN_ERR_NONE;
}

bool artie_can_rpcacp_is_busy(artie_can_backend_t *handle)
{
    if ((handle == NULL) || (handle->context == NULL))
    {
        return false;
    }
    return handle->context->rpcacp_context.state != RPCACP_STATE_IDLE;
}

bool artie_can_rpcacp_is_node_busy(artie_can_backend_t *handle, uint8_t node_address)
{
    if ((handle == NULL) || (handle->context == NULL) || (node_address >= 64U))
    {
        return false;
    }
    return (handle->context->rpcacp_context.busy_nodes_bitmap & (1ULL << node_address)) != 0U;
}

void rpcacp_receive_in_isr(artie_can_context_t *context, const artie_can_frame_t *frame)
{
    if ((context == NULL) || (frame == NULL))
    {
        return;
    }
    else if ((context->protocol_flags & ARTIE_CAN_PROTOCOL_FLAG_RPCACP) == 0)
    {
        // This node is not configured to use RPCACP, ignore the frame.
        return;
    }

    rpcacp_context_t *ctx = &context->rpcacp_context;

    uint8_t target = (uint8_t)((frame->id & ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK) >> ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION);
    if (target != ctx->node_address)
    {
        return;
    }

    if ((ctx->isr_flags & RPCACP_ISR_FLAG_FRAME_RECEIVED) != 0U)
    {
        // Previous frame not yet processed by the main thread; drop this one. Given the
        // stop-and-wait nature of RPCACP, the sender will time out and retry.
        return;
    }

    memcpy(&ctx->received_frame, frame, sizeof(artie_can_frame_t));
    atomic_fetch_or(&ctx->isr_flags, (uint32_t)RPCACP_ISR_FLAG_FRAME_RECEIVED);
}

artie_can_error_t rpcacp_tick(artie_can_backend_t *handle)
{
    if ((handle == NULL) || (handle->context == NULL))
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    rpcacp_context_t *ctx = &handle->context->rpcacp_context;
    artie_can_error_t err = ARTIE_CAN_ERR_NONE;

    if (ctx->boot_time_ms == 0U)
    {
        ctx->boot_time_ms = handle->get_ms();
    }

    if ((ctx->isr_flags & RPCACP_ISR_FLAG_FRAME_RECEIVED) != 0U)
    {
        err |= _process_received_frame(handle);
        atomic_fetch_and(&ctx->isr_flags, ~((uint32_t)RPCACP_ISR_FLAG_FRAME_RECEIVED));
    }

    switch (ctx->state)
    {
        case RPCACP_STATE_SENDING_REQUEST:
            err |= _handle_sending_request(handle);
            break;

        case RPCACP_STATE_WAITING_RETURN:
            err |= _handle_waiting_return(handle);
            break;

        case RPCACP_STATE_SENDING_RETURN:
            err |= _handle_sending_return(handle);
            break;

        case RPCACP_STATE_RECEIVING_REQUEST:
            err |= _handle_receiving_request(handle);
            break;

        case RPCACP_STATE_IDLE: // fall-through
        case RPCACP_STATE_EXECUTING_PROC: // transient: resolved synchronously within _service_request()
        default:
            break;
    }

    return err;
}
