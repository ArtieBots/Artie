"""
Pytest configuration and shared fixtures for Artie CAN library tests.
"""
import pytest
import artie_can


@pytest.fixture
def mock_can_node():
    """Create a CAN node with mock backend for testing.

    Uses dead-end mock backend that discards sends and never receives.
    """
    with artie_can.ArtieCAN(node_address=0x01, backend=artie_can.BackendType.MOCK_DEADEND) as can:
        yield can


@pytest.fixture
def mock_can_tcp_pair():
    """Create a pair of CAN nodes with TCP mock backend for network testing.

    Uses TCP sockets with one node as server and one as client.
    """
    with artie_can.ArtieCAN(node_address=0x01, backend=artie_can.BackendType.MOCK_TCP, mock_port=5556, mock_server=False) as node1:
        with artie_can.ArtieCAN(node_address=0x02, backend=artie_can.BackendType.MOCK_TCP, mock_port=5556, mock_server=True) as node2:
            yield node1, node2


@pytest.fixture
def valid_hex_data():
    """Sample valid hex data for testing."""
    return b"Hello"


@pytest.fixture
def valid_hex_string():
    """Sample valid hex string for testing."""
    return "48656c6c6f"  # "Hello" in hex
