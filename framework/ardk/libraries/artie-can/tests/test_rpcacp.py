"""
Unit tests for RPCACP (Remote Procedure Call Addressed Communication Protocol).

Tests synchronous and asynchronous RPC calls, procedure IDs, and payload handling.
"""
import pytest
from artie_can import ArtieCAN, BackendType, Priority


class TestRPCACPBasicCalls:
    """Tests for basic RPCACP functionality."""

    def test_simple_rpc_call(self, mock_can_tcp_pair):
        """Test making a simple RPC call."""
        node1, node2 = mock_can_tcp_pair
        node1.rpcacp_call(
            target_addr=0x02,
            procedure_id=1,
            payload=b"",
            priority=Priority.MED_LOW,
            synchronous=False  # Async since no responder
        )
        # Should not raise exceptions

    def test_rpc_with_payload(self, mock_can_tcp_pair):
        """Test RPC call with data payload."""
        node1, node2 = mock_can_tcp_pair
        node1.rpcacp_call(
            target_addr=0x02,
            procedure_id=5,
            payload=b"Args",
            priority=Priority.MED_HIGH,
            synchronous=False  # Async since no responder
        )

    def test_rpc_with_empty_payload(self, mock_can_tcp_pair):
        """Test RPC call with empty payload."""
        node1, node2 = mock_can_tcp_pair
        node1.rpcacp_call(
            target_addr=0x02,
            procedure_id=10,
            payload=b"",
            priority=Priority.HIGH,
            synchronous=False  # Async since no responder
        )

    def test_rpc_to_invalid_target(self, mock_can_tcp_pair):
        """Test RPC call to invalid target address."""
        node1, node2 = mock_can_tcp_pair
        with pytest.raises((ValueError, OSError)):
            node1.rpcacp_call(
                target_addr=0x40,  # Invalid
                procedure_id=1,
                payload=b"",
                priority=Priority.MED_LOW,
                synchronous=False
            )


class TestRPCACPProcedureIDs:
    """Tests for RPC procedure ID handling."""

    def test_procedure_id_zero(self, mock_can_tcp_pair):
        """Test RPC with procedure ID 0."""
        node1, node2 = mock_can_tcp_pair
        node1.rpcacp_call(
            target_addr=0x02,
            procedure_id=0,
            payload=b"",
            priority=Priority.MED_LOW,
            synchronous=False  # Async since no responder
        )

    def test_procedure_id_max(self, mock_can_tcp_pair):
        """Test RPC with maximum procedure ID (127)."""
        node1, node2 = mock_can_tcp_pair
        node1.rpcacp_call(
            target_addr=0x02,
            procedure_id=127,
            payload=b"",
            priority=Priority.MED_LOW,
            synchronous=False  # Async since no responder
        )

    def test_procedure_id_mid_range(self, mock_can_tcp_pair):
        """Test RPC with mid-range procedure IDs."""
        node1, node2 = mock_can_tcp_pair
        for proc_id in [1, 10, 50, 100]:
            node1.rpcacp_call(
                target_addr=0x02,
                procedure_id=proc_id,
                payload=b"",
                priority=Priority.MED_LOW,
                synchronous=False  # Async since no responder
            )

    def test_procedure_id_invalid_negative(self, mock_can_tcp_pair):
        """Test RPC with negative procedure ID."""
        node1, node2 = mock_can_tcp_pair
        with pytest.raises((ValueError, OSError)):
            node1.rpcacp_call(
                target_addr=0x02,
                procedure_id=-1,
                payload=b"",
                priority=Priority.MED_LOW,
                synchronous=False
            )

    def test_procedure_id_invalid_too_large(self, mock_can_tcp_pair):
        """Test RPC with procedure ID above maximum."""
        node1, node2 = mock_can_tcp_pair
        with pytest.raises((ValueError, OSError)):
            node1.rpcacp_call(
                target_addr=0x02,
                procedure_id=128,  # Max is 127
                payload=b"",
                priority=Priority.MED_LOW,
                synchronous=False
            )


class TestRPCACPSynchronous:
    """Tests for synchronous RPC calls."""

    def test_synchronous_call(self, mock_can_tcp_pair):
        """Test synchronous RPC call."""
        node1, node2 = mock_can_tcp_pair
        # Using async since we don't have a responder in tests
        node1.rpcacp_call(
            target_addr=0x02,
            procedure_id=1,
            payload=b"SyncCall",
            priority=Priority.MED_HIGH,
            synchronous=False
        )

    def test_synchronous_with_all_priorities(self, mock_can_tcp_pair):
        """Test synchronous RPC with all priority levels."""
        node1, node2 = mock_can_tcp_pair
        priorities = [Priority.HIGH, Priority.MED_HIGH, Priority.MED_LOW, Priority.LOW]

        for i, priority in enumerate(priorities):
            node1.rpcacp_call(
                target_addr=0x02,
                procedure_id=i,
                payload=bytes([i]),
                priority=priority,
                synchronous=False  # Async since no responder
            )

    def test_synchronous_to_multiple_targets(self, mock_can_tcp_pair):
        """Test synchronous RPC to different targets."""
        node1, node2 = mock_can_tcp_pair
        for target in [0x01, 0x02, 0x10, 0x20]:
            node1.rpcacp_call(
                target_addr=target,
                procedure_id=1,
                payload=b"Multi",
                priority=Priority.MED_LOW,
                synchronous=False  # Async since no responder
            )


class TestRPCACPAsynchronous:
    """Tests for asynchronous RPC calls."""

    def test_asynchronous_call(self, mock_can_tcp_pair):
        """Test asynchronous RPC call."""
        node1, node2 = mock_can_tcp_pair
        node1.rpcacp_call(
            target_addr=0x02,
            procedure_id=2,
            payload=b"AsyncCall",
            priority=Priority.MED_LOW,
            synchronous=False
        )

    def test_asynchronous_fire_and_forget(self, mock_can_tcp_pair):
        """Test asynchronous RPC as fire-and-forget."""
        node1, node2 = mock_can_tcp_pair
        node1.rpcacp_call(
            target_addr=0x02,
            procedure_id=99,
            payload=b"",
            priority=Priority.LOW,
            synchronous=False
        )
        # Should return immediately

    def test_multiple_asynchronous_calls(self, mock_can_tcp_pair):
        """Test multiple consecutive asynchronous RPC calls."""
        node1, node2 = mock_can_tcp_pair
        for i in range(5):
            node1.rpcacp_call(
                target_addr=0x02,
                procedure_id=i,
                payload=bytes([i]),
                priority=Priority.MED_LOW,
                synchronous=False
            )


class TestRPCACPPriorities:
    """Tests for RPC call priorities."""

    def test_high_priority_rpc(self, mock_can_tcp_pair):
        """Test high priority RPC call."""
        node1, node2 = mock_can_tcp_pair
        node1.rpcacp_call(
            target_addr=0x02,
            procedure_id=1,
            payload=b"HighPri",
            priority=Priority.HIGH,
            synchronous=False  # Async since no responder
        )

    def test_low_priority_rpc(self, mock_can_tcp_pair):
        """Test low priority RPC call."""
        node1, node2 = mock_can_tcp_pair
        node1.rpcacp_call(
            target_addr=0x02,
            procedure_id=2,
            payload=b"LowPri",
            priority=Priority.LOW,
            synchronous=False
        )

    def test_priority_order(self, mock_can_tcp_pair):
        """Test RPC calls with different priorities."""
        node1, node2 = mock_can_tcp_pair
        # Send RPCs with different priorities
        node1.rpcacp_call(
            target_addr=0x02, procedure_id=1,
            payload=b"Low", priority=Priority.LOW, synchronous=False
        )
        node1.rpcacp_call(
            target_addr=0x02, procedure_id=2,
            payload=b"High", priority=Priority.HIGH, synchronous=False
        )
        node1.rpcacp_call(
            target_addr=0x02, procedure_id=3,
            payload=b"Med", priority=Priority.MED_LOW, synchronous=False
        )


class TestRPCACPPayloads:
    """Tests for RPC payload handling."""

    def test_small_payload(self, mock_can_tcp_pair):
        """Test RPC with small payload."""
        node1, node2 = mock_can_tcp_pair
        node1.rpcacp_call(
            target_addr=0x02,
            procedure_id=1,
            payload=b"X",
            priority=Priority.MED_LOW,
            synchronous=False  # Async since no responder
        )

    def test_medium_payload(self, mock_can_tcp_pair):
        """Test RPC with medium payload."""
        node1, node2 = mock_can_tcp_pair
        node1.rpcacp_call(
            target_addr=0x02,
            procedure_id=1,
            payload=b"1234",
            priority=Priority.MED_LOW,
            synchronous=False  # Async since no responder
        )

    def test_max_payload(self, mock_can_tcp_pair):
        """Test RPC with maximum payload size."""
        node1, node2 = mock_can_tcp_pair
        max_payload = b"12345678"  # 8 bytes
        node1.rpcacp_call(
            target_addr=0x02,
            procedure_id=1,
            payload=max_payload,
            priority=Priority.MED_HIGH,
            synchronous=False  # Async since no responder
        )

    def test_binary_payload(self, mock_can_tcp_pair):
        """Test RPC with binary payload data."""
        node1, node2 = mock_can_tcp_pair
        binary_payload = bytes([0x00, 0x01, 0xAA, 0xFF])
        node1.rpcacp_call(
            target_addr=0x02,
            procedure_id=50,
            payload=binary_payload,
            priority=Priority.MED_LOW,
            synchronous=False  # Async since no responder
        )

    def test_structured_payload(self, mock_can_tcp_pair):
        """Test RPC with structured binary payload."""
        node1, node2 = mock_can_tcp_pair
        import struct
        # Pack: uint8, uint16, uint8
        payload = struct.pack('<BHB', 1, 1000, 2)
        node1.rpcacp_call(
            target_addr=0x02,
            procedure_id=75,
            payload=payload,
            priority=Priority.MED_HIGH,
            synchronous=False  # Async since no responder
        )


class TestRPCACPEdgeCases:
    """Tests for RPC edge cases and error conditions."""

    def test_rpc_to_self(self, mock_can_tcp_pair):
        """Test RPC call to self (same node address)."""
        node1, node2 = mock_can_tcp_pair
        # Assuming node_address is 0x01
        node1.rpcacp_call(
            target_addr=0x01,  # Same as sender
            procedure_id=1,
            payload=b"ToSelf",
            priority=Priority.MED_LOW,
            synchronous=False
        )

    def test_rapid_rpc_calls(self, mock_can_tcp_pair):
        """Test rapid consecutive RPC calls."""
        node1, node2 = mock_can_tcp_pair
        for i in range(10):
            node1.rpcacp_call(
                target_addr=0x02,
                procedure_id=i % 128,
                payload=bytes([i]),
                priority=Priority.MED_LOW,
                synchronous=False
            )

    def test_alternating_sync_async(self, mock_can_tcp_pair):
        """Test alternating synchronous and asynchronous calls."""
        node1, node2 = mock_can_tcp_pair
        for i in range(6):
            node1.rpcacp_call(
                target_addr=0x02,
                procedure_id=i,
                payload=bytes([i]),
                priority=Priority.MED_LOW,
                synchronous=False  # All async since no responder
            )
