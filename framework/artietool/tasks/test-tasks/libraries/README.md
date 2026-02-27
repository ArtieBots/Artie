# Artie CAN Library Tests

This directory contains test tasks for the Artie CAN library.

## Test Tasks

### Unit Tests

**Task:** `artie-can-unit-tests`
**Depends on:** `artie-can-test-image` (build task)

Runs the complete pytest suite (145+ tests) inside a container to validate all CAN protocols using the mock backend (local queue).

Run via Artie Tool:
```bash
artie-tool test artie-can-unit-tests
```

The test image is built from the artie-can library source code and includes all test files and dependencies. The unit test suite covers:
- Core functionality and initialization
- RTACP (Real-Time Addressed Communication Protocol)
- RPCACP (Remote Procedure Call Protocol)
- PSACP (Publisher/Subscriber Protocol)
- BWACP (Block Write Protocol)
- Error handling and edge cases

**How it works:**
1. The `artie-can-test-image` build task creates a Docker image from the library source
2. The image includes the library installed in development mode plus all test files
3. The test task uses the `single-container-pytest-suite` job type to run pytest inside this container
4. Pytest executes all 145+ tests and reports results
5. Exit code 0 = all tests passed; non-zero = test failures

The `single-container-pytest-suite` job type is specifically designed for Python projects with pytest-based unit tests,
providing a clean interface for running test suites inside containers.

For more details on the unit tests, see the [unit tests README](../../../ardk/libraries/artie-can/tests/README.md).

### Integration Tests

**Task:** `can-integration-tests`

Integration tests validate the TCP mock backend for inter-container communication using Docker Compose.

Run via Artie Tool:
```bash
artie-tool test can-integration-tests
```

## CAN Library Overview

The CAN library supports multiple backends:
- **SocketCAN**: Linux kernel CAN interface (production)
- **MCP2515**: Bare-metal SPI-based CAN controller (embedded systems)
- **Mock (local queue)**: In-process testing with queue-based message passing
- **Mock (TCP)**: Network-based testing using TCP sockets for inter-container/inter-process communication

## TCP Mock Backend

The TCP mock backend enables testing CAN communication between separate containers or processes. It uses TCP sockets to transmit CAN frames, with two modes:

### Server Mode
The server listens on a specified host and port, accepting connections from clients and exchanging CAN frames.

```bash
artie-cli can rtacp-receive \
  --node-address 0x02 \
  --mock \
  --mock-host 0.0.0.0 \
  --mock-port 5555 \
  --mock-server \
  --timeout 30000
```

### Client Mode
The client connects to a server at the specified host and port.

```bash
artie-cli can rtacp-send \
  --node-address 0x01 \
  --mock \
  --mock-host can-server \
  --mock-port 5555 \
  --target 0x02 \
  --data 48656c6c6f \
  --priority MED_LOW
```

### Environment Variables

TCP mock configuration can also be set via environment variables:
- `ARTIE_CAN_MOCK_HOST`: Server hostname/IP (default: localhost)
- `ARTIE_CAN_MOCK_PORT`: Server port (default: 5555)
- `ARTIE_CAN_MOCK_SERVER`: Set to "true" for server mode (default: client)

## Integration Test Architecture

The integration tests use Docker Compose to create two containers that communicate via TCP:

```
┌─────────────────────┐         TCP Socket        ┌─────────────────────┐
│   can-itest-server  │◄──────────────────────────►│   artie-cli         │
│   (Node 0x02)       │       (port 5555)          │   (Node 0x01)       │
│   Listens & echoes  │                             │   Sends commands    │
└─────────────────────┘                             └─────────────────────┘
```

## Running Tests

### Via Artie Tool

```bash
artie-tool test can-integration-tests
```

### Manual Docker Compose

1. Create the network:
   ```bash
   docker network create can-itest
   ```

2. Start the compose stack:
   ```bash
   cd framework/artietool/compose-files
   docker-compose -f compose.can.yaml up
   ```

3. In another terminal, send commands:
   ```bash
   docker run --rm --network can-itest artie-cli \
     artie-cli can rtacp-send \
       --node-address 0x01 \
       --mock \
       --mock-host can-itest-server \
       --mock-port 5555 \
       --target 0x02 \
       --data 48656c6c6f
   ```

## Test Cases

The integration tests cover four main protocols:

1. **RTACP (Real-Time Addressed Communication Protocol)**
   - Targeted message sending
   - Broadcast messages
   - Priority handling
   - ACK waiting (optional)

2. **PSACP (Publisher/Subscriber Addressed Communication Protocol)**
   - Topic publishing
   - Topic subscription
   - High-priority pub/sub

3. **RPCACP (Remote Procedure Call Addressed Communication Protocol)**
   - Synchronous RPC calls
   - Asynchronous RPC calls
   - Procedure ID routing

4. **BWACP (Block Write Addressed Communication Protocol)**
   - Multi-frame data transfer
   - Block ID management
   - Sequence numbering

## TCP Wire Protocol

CAN frames are transmitted over TCP using a simple framing protocol:

1. **Frame Size Header** (4 bytes, network byte order): Size of the CAN frame
2. **CAN Frame Data** (variable): The actual CAN frame structure

Example:
```
[0x00, 0x00, 0x00, 0x10]  ← 16 bytes follow
[CAN frame data: 16 bytes]
```

This prevents message boundary issues when multiple frames are sent in quick succession.

## Platform Support

The TCP mock backend is cross-platform:
- **Linux**: Uses POSIX sockets
- **Windows**: Uses WinSock2
- **macOS**: Uses POSIX sockets

Socket operations are non-blocking with configurable timeouts via `select()`.

## Troubleshooting

### Connection Refused
- Ensure server container is healthy before client connects
- Check firewall rules and network connectivity
- Verify port 5555 is not in use by another process

### Timeout Errors
- Increase timeout values for slow systems
- Check that both containers are on the same Docker network
- Ensure server is in server mode (`--mock-server` flag)

### Frame Decoding Errors
- Verify both nodes use compatible CAN library versions
- Check that data payloads are valid hex strings
- Ensure target addresses are in valid range (0x00-0x3F)

## Implementation Notes

The TCP mock backend is implemented in:
- C library: `artie-common/firmware/artie-can/src/artie_can_backend_mock.c`
- Python bindings: `artie-common/firmware/artie-can/src/artie_can/__init__.py`
- CLI integration: `framework/cli/artiecli/modules/can.py`

Key features:
- Zero dynamic memory allocation (C code)
- Cross-platform socket compatibility
- Non-blocking I/O with timeouts
- Automatic reconnection handling
- Frame size prefixing for message boundaries
