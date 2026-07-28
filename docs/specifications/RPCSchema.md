# RPC Schema Specification

This document defines the schema for Remote Procedure Call (RPC) methods on the Artie platform.

## Overview

The RPC schema enables nodes on the CAN bus to discover what remote procedures are available
and how to call them correctly.

## RPC Signature Structure

Each RPC has a signature with the following structure:

```c
struct RpcSignature {
    uint16_t procedure_id;                // 7-bit field mapped to the procedure_id field in the RPCACP frames
    char *name;                           // Human-readable name (for debugging/logging); nul-terminated string
    bool synchronous;                     // true indicates this is a blocking method
    uint8_t param_count;                  // Number of parameters
    struct RpcParamDescriptor params[15]; // Array of parameter descriptors (up to 15 elements)
};
```

### Procedure ID Assignment

The 7-bit procedure ID is assigned per-device and must be unique within that device namespace.
The mapping follows these rules:

* 0x00-0x0F (0-31): Reserved RPCs (shared across all devices that conform to this specification):
    * `0x00`: WHOAMI - Returns whoami information
    * `0x01`: STATUS - Return status info
    * `0x02`: LIST - A list of RPC signatures that this node supports (in numerical order, starting with 0x00)
    * `0x03-0x0F`: Reserved for future standard RPCs

* 0x10-0x7F (16 - 127): Device-specific RPCs, assigned per-device by firmware configuration

### Parameter Descriptors

Each parameter in an RPC call is described with the following structure:

```c
struct RpcParamDescriptor {
    char *type_name;                 // Type name for serialization/deserialization - if not used, should be "NULL"
    uint8_t offset_in_msgpack;       // Offset within MsgPack-encoded data
    bool optional;                   // true = may not be present in all calls
};
```

Supported type names:
* `intX_t`, `uintX_t`, `float` (16-bit), `double` (32-bit) - Standard C primitive types
  (where 'X' is 8, 16, 32, or 64).
* `array<T, N>` - Fixed-size arrays (must specify type 'T' and size, 'N')
* `string` - Nul-terminated string
* `bool` - Boolean value
* `struct <name>` - Struct
* `NULL` - A param descriptor that is not used must have this type name.

## Standardized RPC Signatures

This section contains the details pertaining to each standard RPC signature.

### WHOAMI

* Procedure ID: 0x00
* Parameters: None
* Return: `RpcWhoamiResponse`
* Synchronous: Yes
* Description: Returns a standard set of information about the responding node.
               See below for the returned struct.

```c
struct RpcWhoamiResponse {
    char *node_name;        // Human-readable name of the Node
    uint8_t node_address;   // Artie CAN bus node address of this Node
    char *fw_version;       // FW version running on this Node
}
```

### Status

* Procedure ID: 0x01
* Parameters: None
* Return: `RpcStatusResponse`
* Synchronous: Yes
* Description: Returns current status of the responding node.
               See below for the returned struct.

```c
struct RpcStatusResponse {
    uint64_t uptime_ms;     // How long this Node has been running (ms)
    uint32_t err_flags;     // Bit mask of error flags. There is no standard definition
                            // of the error flags - each Node firmware is allowed to
                            // define whatever errors make the most sense for it.
};
```

### List

* Procedure ID: 0x02
* Parameters: `uint8_t page`: The page number to read.
* Return: `array<RpcSignature, 8>`: An array of 8 `RpcSignature` structs corresponding
          to the page that was requested. Pages range from 0 (indexes 0-7) to 15 (indexes 120-127).
* Synchronous: Yes
* Description: Returns an array of 8 `RpcSignature` structs corresponding to the page requested.
               If an RPC identifier is not used by that Node, the corresponding struct's
               `name` field must be set to `UNASSIGNED`, and the remaining data in the struct (other than
               the `procedure_id` can be set to anything, as it should be ignored).

## Blocking vs Non-Blocking

A blocking (synchronous) RPC call is expected to complete quickly, and the caller
is expected to block its thread of execution until a complete response is received.

A non-blocking (asynchronous) RPC call on the other hand, is expected to potentially
take a while. The caller should not block on the response. However, please note that
a remote node cannot execute any other RPC method while it is executing an asynchronous
one. A calling node *can* however execute multiple asynchronous RPC calls at the same time
- one for each node in the network.

## Error Handling

If a node fails to process an RPC request in a way that falls outside of the normal flow of
the requested function's failure modes, i.e., the node is unable to complete an RPC request
due to an error in the request itself, the node must return to the ArtieCAN library
an error code. Errors are returned using standard Linux errno codes.

The following errno values have specific meaning in this context:

* 0x00:            Something went wrong in transmission. Send whole request again.
* 0x01: EPERM:     The requested rpc ID is not one that this node has registered.
* 0x07: E2BIG:     Argument list is too long.
* 0x08: ENOEXEC:   We could not unpack the RPC correctly. It's possible, but unlikely that this is a transmission error. Most likely,
                   it is a bug in the serialization/deserialization of the RPC.
* 0x0b: EAGAIN:    Something transient went wrong, such as being overloaded with requests or just not able to get to it right now,
                   try again.
* 0x16: EINVAL:    At least one of the RPC arguments is invalid. The arguments were unpacked correctly, but there is an RPC
                   signature mismatch.
* 0x72: EALREADY:  We are already working on this RPC. This should only be sent if the RPC is identical and already being worked on.
