"""
Unit tests for core Artie CAN library functionality.

Tests initialization, context management, and basic operations.
"""
import pytest
from artie_can import ArtieCAN, BackendType


class TestInitialization:
    """Tests for CAN context initialization."""

    def test_init_with_mock_backend(self):
        """Test initialization with mock backend."""
        can = ArtieCAN(node_address=0x01, backend=BackendType.MOCK_DEADEND)
        assert can is not None
        can.close()

    def test_init_with_valid_address_range(self):
        """Test initialization with various valid node addresses."""
        for addr in [0x00, 0x01, 0x20, 0x3F]:
            can = ArtieCAN(node_address=addr, backend=BackendType.MOCK_DEADEND)
            assert can is not None
            can.close()

    def test_init_with_tcp_backend_server(self):
        """Test initialization with TCP mock backend as server."""
        can = ArtieCAN(node_address=0x01, backend=BackendType.MOCK_TCP, mock_host="localhost", mock_port=9999, mock_server=True)
        assert can is not None
        can.close()

    def test_init_with_tcp_backend_client(self):
        """Test initialization with TCP mock backend as client."""
        can = ArtieCAN(node_address=0x01, backend=BackendType.MOCK_TCP, mock_host="localhost", mock_port=9999, mock_server=False)
        assert can is not None
        can.close()

    def test_init_with_invalid_address_low(self):
        """Test initialization with address below valid range."""
        with pytest.raises((ValueError, OSError)):
            ArtieCAN(node_address=-1, backend=BackendType.MOCK_DEADEND)

    def test_init_with_invalid_address_high(self):
        """Test initialization with address above valid range."""
        with pytest.raises((ValueError, OSError)):
            ArtieCAN(node_address=0x40, backend=BackendType.MOCK_DEADEND)

    def test_context_manager(self):
        """Test using CAN context as a context manager."""
        with ArtieCAN(node_address=0x01, backend=BackendType.MOCK_DEADEND) as can:
            assert can is not None
        # Context should be closed after exiting with block

    def test_multiple_contexts(self):
        """Test creating multiple CAN contexts."""
        can1 = ArtieCAN(node_address=0x01, backend=BackendType.MOCK_DEADEND)
        can2 = ArtieCAN(node_address=0x02, backend=BackendType.MOCK_DEADEND)

        assert can1 is not None
        assert can2 is not None

        can1.close()
        can2.close()

class TestNodeAddressing:
    """Tests for node addressing functionality."""

    def test_broadcast_address(self):
        """Test using broadcast address (0x00)."""
        can = ArtieCAN(node_address=0x00, backend=BackendType.MOCK_DEADEND)
        assert can is not None
        can.close()

    def test_unicast_addresses(self):
        """Test using unicast addresses (0x01-0x3F)."""
        can = ArtieCAN(node_address=0x01, backend=BackendType.MOCK_DEADEND)
        assert can is not None
        can.close()

    def test_max_valid_address(self):
        """Test using maximum valid address (0x3F = 63)."""
        can = ArtieCAN(node_address=0x3F, backend=BackendType.MOCK_DEADEND)
        assert can is not None
        can.close()
