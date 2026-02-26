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
        can = ArtieCAN(node_address=0x01, backend=BackendType.MOCK)
        assert can is not None
        can.close()

    def test_init_with_valid_address_range(self):
        """Test initialization with various valid node addresses."""
        for addr in [0x00, 0x01, 0x20, 0x3F]:
            can = ArtieCAN(node_address=addr, backend=BackendType.MOCK)
            assert can is not None
            can.close()

    def test_init_with_invalid_address_low(self):
        """Test initialization with address below valid range."""
        with pytest.raises((ValueError, OSError)):
            ArtieCAN(node_address=-1, backend=BackendType.MOCK)

    def test_init_with_invalid_address_high(self):
        """Test initialization with address above valid range."""
        with pytest.raises((ValueError, OSError)):
            ArtieCAN(node_address=0x40, backend=BackendType.MOCK)

    def test_context_manager(self):
        """Test using CAN context as a context manager."""
        with ArtieCAN(node_address=0x01, backend=BackendType.MOCK) as can:
            assert can is not None
        # Context should be closed after exiting with block

    def test_multiple_contexts(self):
        """Test creating multiple CAN contexts."""
        can1 = ArtieCAN(node_address=0x01, backend=BackendType.MOCK)
        can2 = ArtieCAN(node_address=0x02, backend=BackendType.MOCK)

        assert can1 is not None
        assert can2 is not None

        can1.close()
        can2.close()

    def test_mock_tcp_initialization(self):
        """Test initialization with TCP mock backend parameters."""
        can = ArtieCAN(
            node_address=0x01,
            backend=BackendType.MOCK,
            mock_host="localhost",
            mock_port=9999,
            mock_server=False
        )
        assert can is not None
        can.close()


class TestBackendTypes:
    """Tests for different backend types."""

    def test_backend_enum_values(self):
        """Test that BackendType enum has expected values."""
        assert hasattr(BackendType, 'MOCK')
        assert hasattr(BackendType, 'SOCKETCAN')
        assert hasattr(BackendType, 'MCP2515')

    def test_mock_backend_available(self):
        """Test that mock backend is always available."""
        can = ArtieCAN(node_address=0x01, backend=BackendType.MOCK)
        assert can is not None
        can.close()


class TestNodeAddressing:
    """Tests for node addressing functionality."""

    def test_broadcast_address(self):
        """Test using broadcast address (0x00)."""
        can = ArtieCAN(node_address=0x00, backend=BackendType.MOCK)
        assert can is not None
        can.close()

    def test_unicast_addresses(self):
        """Test using unicast addresses (0x01-0x3F)."""
        can = ArtieCAN(node_address=0x01, backend=BackendType.MOCK)
        assert can is not None
        can.close()

    def test_max_valid_address(self):
        """Test using maximum valid address (0x3F = 63)."""
        can = ArtieCAN(node_address=0x3F, backend=BackendType.MOCK)
        assert can is not None
        can.close()
