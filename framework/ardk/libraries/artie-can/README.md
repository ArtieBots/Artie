# Artie CAN Library

This document describes the Artie CAN library, which provides an interface for communication over the
Controller Area Network (CAN) bus used in Artie robots.

It also describes the mechanical and electrical design of the CAN bus system.

The protocol that we use for communication over CAN is described in the
[Artie CAN Protocol Specification](../../../../docs/specifications/CANProtocol.md).

## Library Overview

The Artie CAN library provides an API for serializing and deserializing messages
using the [Artie CAN Protocol](../../../../docs/specifications/CANProtocol.md).

This library is written in C but also provides a Python wrapper for ARM64 platforms.

In addition to the serialization and deserialization functions, the library also provides functions for
initializing the CAN bus, sending messages, and receiving messages. It provides an ARM64 backend,
a bare-metal backend, and two mock backends for testing. In all cases, no dynamic memory allocation is used.
In the ARM64 backend, the library uses the SocketCAN interface to communicate with the CAN bus. In the bare-metal
backend, the library assumes an MCP2515 CAN controller is used and provides functions for initializing the controller
and sending/receiving messages. The mock backends include a local queue-based implementation for single-process
testing and a TCP socket-based implementation for multi-container/multi-process testing. We also provide a call-back
interface for registering custom backends, which can be used to support other CAN controllers or platforms.

Raspberry Pi devices will need something like the following in their config.txt files:

`dtoverlay=mcp2515-can0,oscillator=16000000,interrupt=25`

which says to enable the MCP2515 CAN controller on the first CAN interface (can0) with a 16 MHz oscillator
and an interrupt on GPIO 25.

## Building

### Building the C Library

```bash
mkdir build
cd build
cmake ..
make
sudo make install
```

### Building the Python Package

The Python package will automatically build the C library during installation:

```bash
pip install .
```

Or for development:

```bash
pip install -e .
```

## Usage

### Python Example

```python
from artie_can import ArtieCAN, BackendType, Priority

# Initialize CAN with mock backend for testing (local queue)
with ArtieCAN(node_address=0x01, backend=BackendType.MOCK) as can:
    # Send a real-time message
    can.rtacp_send(target_addr=0x02, data=b"Hello", priority=Priority.HIGH)

    # Publish to a topic
    can.psacp_publish(topic=0x10, data=b"Sensor data", priority=Priority.MED_LOW)

    # Send RPC
    can.rpcacp_call(target_addr=0x02, procedure_id=5, payload=b"\x01\x02\x03")

# TCP Mock Backend (for inter-container/inter-process testing)
# Server mode (listens for connections)
with ArtieCAN(node_address=0x02, backend=BackendType.MOCK,
              mock_host="0.0.0.0", mock_port=5555, mock_server=True) as server:
    sender, target, data = server.rtacp_receive(timeout_ms=30000)
    print(f"Received: {data.hex()}")

# Client mode (connects to server)
with ArtieCAN(node_address=0x01, backend=BackendType.MOCK,
              mock_host="localhost", mock_port=5555, mock_server=False) as client:
    client.rtacp_send(target_addr=0x02, data=b"Hello from client", priority=Priority.HIGH)
```

### C Example

```c
#include "artie_can.h"

int main() {
    artie_can_context_t ctx;

    // Initialize with SocketCAN backend
    if (artie_can_init(&ctx, 0x01, ARTIE_CAN_BACKEND_SOCKETCAN) != 0) {
        return -1;
    }

    // Send RTACP message
    artie_can_rtacp_msg_t msg;
    msg.frame_type = ARTIE_CAN_RTACP_MSG;
    msg.priority = ARTIE_CAN_PRIORITY_HIGH;
    msg.sender_addr = 0x01;
    msg.target_addr = 0x02;
    msg.data_len = 5;
    memcpy(msg.data, "Hello", 5);

    artie_can_rtacp_send(&ctx, &msg, false);

    // Close
    artie_can_close(&ctx);
    return 0;
}
```

## Backends

The library supports multiple backends for different use cases:

### SocketCAN (Linux)
Production backend for ARM64/x86_64 Linux systems. Uses the kernel's SocketCAN interface.

```python
can = ArtieCAN(node_address=0x01, backend=BackendType.SOCKETCAN)
```

### Mock (Local Queue)
Single-process testing backend using an in-memory queue. Messages are exchanged within the same process.

```python
can = ArtieCAN(node_address=0x01, backend=BackendType.MOCK)
```

### Mock (TCP Sockets)
Multi-process/multi-container testing backend using TCP sockets. Enables integration testing with Docker Compose.

**Server Mode** (listens for incoming connections):
```python
can = ArtieCAN(node_address=0x02, backend=BackendType.MOCK,
               mock_host="0.0.0.0", mock_port=5555, mock_server=True)
```

**Client Mode** (connects to server):
```python
can = ArtieCAN(node_address=0x01, backend=BackendType.MOCK,
               mock_host="server-hostname", mock_port=5555, mock_server=False)
```

Configuration via environment variables:
- `ARTIE_CAN_MOCK_HOST`: Server hostname/IP (default: localhost)
- `ARTIE_CAN_MOCK_PORT`: Server port (default: 5555)
- `ARTIE_CAN_MOCK_SERVER`: "true" for server mode (default: client)

### MCP2515 (Bare-Metal)
Bare-metal backend for embedded systems using the MCP2515 CAN controller via SPI. Currently a stub.

```c
artie_can_init(&ctx, node_address, ARTIE_CAN_BACKEND_MCP2515);
```

## Architecture

The library is organized into several layers:

1. **Protocol Layer**: Implements RTACP, RPCACP, PSACP, and BWACP protocols
2. **Core Layer**: Context management and initialization
3. **Backend Layer**: Hardware abstraction (SocketCAN, MCP2515, Mock)
4. **Utility Layer**: CRC calculation, byte stuffing

## Supported Protocols

- **RTACP** (Real-Time Artie CAN Protocol): Strict real-time message delivery (< 150us)
- **RPCACP** (Remote Procedure Call Artie CAN Protocol): Synchronous and asynchronous RPCs
- **PSACP** (Pub/Sub Artie CAN Protocol): Topic-based publish/subscribe messaging
- **BWACP** (Block Write Artie CAN Protocol): Large data transfers (firmware updates, etc.)

## Implementation Status

- ✅ Core API and context management
- ✅ RTACP implementation (basic, single-frame)
- ✅ RPCACP implementation (basic, single-frame)
- ✅ PSACP implementation (basic, single-frame)
- ✅ BWACP implementation (basic)
- ✅ Mock backend (local queue)
- ✅ Mock backend (TCP sockets for inter-container testing)
- ✅ SocketCAN backend
- ⚠️ MCP2515 backend (stub only)
- ✅ Python bindings
- ✅ Build system integration
- ✅ Artie CLI integration
- ✅ Integration tests (Docker Compose)
- ✅ Unit tests (145+ tests covering all protocols)
- 🔲 Multi-frame message handling (needs improvement)
- 🔲 Comprehensive error handling

## Testing

The library includes comprehensive test coverage:

### Unit Tests
Located in `tests/`, the unit test suite includes 145+ tests covering:
- Core functionality and initialization
- All four CAN protocols (RTACP, RPCACP, PSACP, BWACP)
- Message priorities and addressing
- Error handling and edge cases

**Run via Artie Tool** (recommended for CI/CD):
```bash
artie-tool test artie-can-unit-tests
```

**Run directly with pytest** (for development):
```bash
pip install -e ".[dev]"
pytest
```

See [tests/README.md](tests/README.md) for detailed testing documentation.

### Integration Tests
Docker Compose-based integration tests validate inter-container communication using the TCP mock backend.

**Run via Artie Tool**:
```bash
artie-tool test can-integration-tests
```

See the [integration test documentation](../../../../artietool/tasks/test-tasks/can/README.md) for details.

## Mechanical and Electrical Design

TODO
