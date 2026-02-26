"""
Pytest configuration and shared fixtures for Artie CAN library tests.
"""
import pytest
from artie_can import ArtieCAN, BackendType


@pytest.fixture
def mock_can_node():
    """Create a CAN node with mock backend for testing."""
    with ArtieCAN(node_address=0x01, backend=BackendType.MOCK) as can:
        yield can


@pytest.fixture
def mock_can_pair():
    """Create a pair of CAN nodes with mock backend for testing communication."""
    with ArtieCAN(node_address=0x01, backend=BackendType.MOCK) as node1:
        with ArtieCAN(node_address=0x02, backend=BackendType.MOCK) as node2:
            yield node1, node2


@pytest.fixture
def valid_hex_data():
    """Sample valid hex data for testing."""
    return b"Hello"


@pytest.fixture
def valid_hex_string():
    """Sample valid hex string for testing."""
    return "48656c6c6f"  # "Hello" in hex
