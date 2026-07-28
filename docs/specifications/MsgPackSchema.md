# MsgPack Schema

This document describes the usage of MsgPack for serializing and deserializing C data types within
the Artie Framework. MsgPack provides a compact binary format that allows for efficient transmission
of complex structures.

## Overview

In the Artie ecosystem, MsgPack is primarily used to serialize the payload of Remote Procedure Call Artie CAN Protocol (RPCACP)
messages.

## Integration with RPCACP

When an RPC request or return is prepared, the following serialization flow is followed:

1. **Serialization**: The application data is packed into a binary blob using the MsgPack schema.
2. **Byte Stuffing**: The resulting MsgPack blob is passed through the [Byte Stuffing](./ByteStuffing.md) layer to ensure it can be safely transmitted over CAN.
3. **CRC Calculation**: A CRC16 is calculated over the byte-stuffed payload.
4. **Framing**: The data is sharded into `StartRPC` and `TxData`, or `StartReturn` and `RxData` frames.

On the receiving end, the process is reversed:
1. **De-framing & CRC Check**: CAN frames are reassembled, byte stuffing is removed, and the CRC is verified.
2. **MsgPack Decoding**: If the CRC is valid, the remaining payload is decoded using MsgPack and mapped to the expected C structures.

## Supported Types

The following types are supported for serialization. When defining an RPC signature,
the `type_name` in the `RpcParamDescriptor` must match one of these:

### Primitives

* `int8_t`, `uint8_t`
* `int16_t`, `uint16_t`
* `int32_t`, `uint32_t`
* `int64_t`, `uint64_t`
* `float` (16-bit)
* `double` (32-bit)
* `bool` - Boolean value

### Strings and Arrays

* `string` - Nul-terminated string.
* `array<T, N>` - A fixed-size array of type `T` with `N` elements. For example,
                  `array<uint8_t, 4>` represents a 4-byte array of unsigned 8-bit integers.

### Complex Types

* `struct <name>` - A nested structure. The internal fields of the structure must also follow the MsgPack schema.
* `NULL` - A parameter descriptor that is not used. This is required for descriptors that are not mapped to the binary payload.

## Parameter Mapping

Each RPC call is associated with a set of `RpcParamDescriptor` structures.
These descriptors define how the RPC library interprets the MsgPack blob:

* `type_name`: The supported type (as listed above).
* `offset_in_msgpack`: The byte offset from the beginning of the MsgPack blob where the parameter data begins.
* `optional`: If `true`, the parameter may not be present in the MsgPack blob.
              If the parameter is optional and not provided, the library should treat it as the default value or a null value depending on the type.

## Example

If an RPC has a signature with two parameters: a `uint32_t` ID and a `string` name.

**MsgPack Blob Layout:**
| Offset | Data | Description |
| :--- | :--- | :--- |
| 0 | `[4 bytes]` | `uint32_t` ID |
| 4 | `[var]` | `string` Name |

**RPC Signature Descriptors:**
1. `type_name: "uint32_t"`, `offset_in_msgpack: 0`, `optional: false`
2. `type_name: "string"`, `offset_in_msgpack: 4`, `optional: true`
