# Artie CAN Library Unit Tests

This directory contains comprehensive unit tests for the Artie CAN library. The tests use two types of mock backends for isolated, deterministic testing without requiring actual CAN hardware:
- **Dead-end backend**: For send-only tests (discards data, never receives)
- **TCP backend**: For send/receive communication tests between nodes

## Test Structure

The test suite is organized by protocol:

- **test_core.py**: Core functionality tests
  - Context initialization and management
  - Backend selection
  - Node addressing
  - Error handling

- **test_rtacp.py**: Real-Time Addressed Communication Protocol (RTACP) tests
  - Unicast and broadcast messaging
  - Message priorities (HIGH, MED_HIGH, MED_LOW, LOW)
  - ACK handling
  - Data payload variations
  - Send/receive operations

- **test_rpcacp.py**: Remote Procedure Call Addressed Communication Protocol (RPCACP) tests
  - Synchronous and asynchronous RPC calls
  - Procedure ID handling (0-127)
  - Payload transmission
  - Priority levels
  - Target addressing

- **test_psacp.py**: Publisher/Subscriber Addressed Communication Protocol (PSACP) tests
  - Topic-based publishing
  - Topic ID ranges (0x0B-0xF4, 0x00 for broadcast)
  - High-priority pub/sub mode
  - Message priorities
  - Subscriber operations

- **test_bwacp.py**: Block Write Addressed Communication Protocol (BWACP) tests
  - Multi-frame data transfers
  - Block ID management
  - Large data blocks (up to several KB)
  - Sequence numbering
  - Various data types

- **conftest.py**: Pytest configuration and shared fixtures
  - Mock CAN node fixtures
  - Test data fixtures
  - Shared setup/teardown logic

## Running the Tests

### Via Artie Tool (Recommended for CI/CD)

The recommended way to run tests in the Artie ecosystem is via Artie Tool, which runs tests in a containerized environment:

```bash
# From the Artie repository root
artie-tool test artie-can-unit-tests
```

This runs all 145+ unit tests inside a Docker container, ensuring consistency across different environments.

### Via pytest (For Development)

For rapid iteration during development, you can run tests directly with pytest.

#### Prerequisites

Install the development dependencies:

```bash
pip install -e ".[dev]"
```

This installs pytest and builds the CAN library.

#### Run All Tests

```bash
# From the artie-can directory
pytest

# Or more explicitly
python -m pytest tests/

# With verbose output
pytest -v

# With coverage report
pytest --cov=artie_can --cov-report=html
```

### Run Specific Test Files

```bash
# Test only RTACP
pytest tests/test_rtacp.py

# Test only RPC functionality
pytest tests/test_rpcacp.py

# Test core and RTACP
pytest tests/test_core.py tests/test_rtacp.py
```

### Run Specific Test Classes or Methods

```bash
# Run a specific test class
pytest tests/test_rtacp.py::TestRTACPBasicSending

# Run a specific test method
pytest tests/test_rtacp.py::TestRTACPBasicSending::test_send_unicast_message

# Run all tests matching a pattern
pytest -k "priority"
```

### Additional Options

```bash
# Stop on first failure
pytest -x

# Run last failed tests only
pytest --lf

# Show local variables on failure
pytest -l

# Parallel execution (requires pytest-xdist)
pip install pytest-xdist
pytest -n auto
```

## Test Coverage

The test suite provides comprehensive coverage of:

✅ **Protocol Operations**
- All four CAN protocols (RTACP, RPCACP, PSACP, BWACP)
- Message sending and receiving
- Priority handling
- Addressing modes (unicast, broadcast)

✅ **Data Handling**
- Empty payloads
- Small payloads (1 byte)
- Maximum single-frame payloads (8 bytes)
- Multi-frame transfers (for BWACP)
- Binary and ASCII data
- Structured data (using struct module)

✅ **Edge Cases**
- Boundary values for addresses, IDs, and sizes
- Rapid consecutive operations
- Interleaved operations
- Invalid inputs (negative values, out-of-range)
- Timeout scenarios

✅ **Error Handling**
- Invalid addresses
- Invalid procedure/topic/block IDs
- Timeout conditions
- Out-of-range values

## Test Configuration

### Mock Backends

Tests use two types of mock backends:

**Dead-end backend** (for send-only tests):
```python
can = ArtieCAN(node_address=0x01, backend=BackendType.MOCK)
```
- Discards sent data (sends always succeed)
- Never returns data on receive (always returns NO_DATA)
- No hardware dependencies
- Fast execution
- Ideal for validating send operations

**TCP backend** (for communication tests):
```python
node1 = ArtieCAN(node_address=0x01, backend=BackendType.MOCK, mock_port=5555, mock_server=True)
node2 = ArtieCAN(node_address=0x02, backend=BackendType.MOCK, mock_port=5555, mock_server=False)
```
- Enables actual communication between nodes
- Uses TCP sockets for inter-process communication
- One node acts as server, one as client
- Ideal for testing send/receive functionality

### Fixtures

Common test fixtures are defined in `conftest.py`:

- `mock_can_node`: Single CAN node with dead-end backend (send-only)
- `mock_can_tcp_pair`: Pair of CAN nodes with TCP backend for communication tests
- `valid_hex_data`: Sample binary data
- `valid_hex_string`: Sample hex string

## Expected Test Behavior

### Passing Tests

Most tests should pass, validating:
- Successful function calls without exceptions
- Proper parameter validation
- Expected error conditions raise appropriate exceptions

### Tests That May Timeout

Some receive tests expect timeouts (no messages available):
```python
with pytest.raises((TimeoutError, OSError)):
    can.rtacp_receive(timeout_ms=100)
```

This is expected behavior for the dead-end backend which never provides received data.

### Backend Behavior Notes

**Dead-end backend:**
- All sends succeed immediately (data is discarded)
- All receives return NO_DATA error
- Used for testing send operations in isolation

**TCP backend:**
- Requires both nodes to be initialized (server first, then client)
- Actual network communication between nodes
- Tests may fail if TCP connection is still establishing
- Does not simulate real CAN bus timing

These backends are ideal for unit testing. Integration tests with actual hardware provide full end-to-end validation.

## Continuous Integration

To integrate these tests into CI/CD:

```yaml
# Example GitHub Actions workflow
- name: Install dependencies
  run: pip install -e ".[dev]"

- name: Run unit tests
  run: pytest tests/ -v --cov=artie_can --cov-report=xml

- name: Upload coverage
  uses: codecov/codecov-action@v3
```

## Troubleshooting

### Import Errors

If you see import errors, ensure the library is installed:
```bash
pip install -e .
```

### Build Errors

If the C library fails to build:
```bash
# Rebuild from scratch
rm -rf build/
pip install -e . --force-reinstall
```

### Test Failures

If tests fail unexpectedly:
1. Check that the library is properly installed
2. Verify no other process is using CAN resources
3. Review recent code changes
4. Run with verbose output: `pytest -v -s`

## Adding New Tests

When adding new tests:

1. Follow the existing structure and naming conventions
2. Use appropriate fixtures from `conftest.py`
3. Group related tests in classes
4. Add docstrings explaining what each test validates
5. Test both success and error conditions
6. Consider edge cases and boundary values

Example:
```python
class TestNewFeature:
    """Tests for the new feature."""

    def test_basic_operation(self, mock_can_node):
        """Test that basic operation works correctly."""
        mock_can_node.new_feature(param=value)
        # Assertions or expected exceptions

    def test_error_condition(self, mock_can_node):
        """Test that invalid input raises appropriate error."""
        with pytest.raises(ValueError):
            mock_can_node.new_feature(param=invalid_value)
```

## Test Metrics

Current test coverage (approximate):
- **test_core.py**: 15 tests covering initialization and backend selection
- **test_rtacp.py**: 30+ tests covering RTACP protocol
- **test_rpcacp.py**: 35+ tests covering RPC functionality
- **test_psacp.py**: 35+ tests covering pub/sub operations
- **test_bwacp.py**: 30+ tests covering block writes

**Total: 145+ unit tests** providing comprehensive coverage of the CAN library.
