"""
Unit tests for RPCACP (Remote Procedure Call Addressed Communication Protocol).

Tests synchronous and asynchronous RPC calls, procedure IDs, and payload handling.
"""
import pytest
from artie_can import ArtieCAN, BackendType, Priority


class TestRPCACPBasicCalls:
    """Tests for basic RPCACP functionality."""

    def test_simple_rpc_call(self, mock_can_node):
        """Test making a simple RPC call."""
        mock_can_node.rpcacp_call(
            target_addr=0x02,
            procedure_id=1,
            payload=b"",
            priority=Priority.MED_LOW,
            synchronous=True
        )
        # Should not raise exceptions

    def test_rpc_with_payload(self, mock_can_node):
        """Test RPC call with data payload."""
        mock_can_node.rpcacp_call(
            target_addr=0x02,
            procedure_id=5,
            payload=b"Args",
            priority=Priority.MED_HIGH,
            synchronous=True
        )

    def test_rpc_with_empty_payload(self, mock_can_node):
        """Test RPC call with empty payload."""
        mock_can_node.rpcacp_call(
            target_addr=0x02,
            procedure_id=10,
            payload=b"",
            priority=Priority.HIGH,
            synchronous=True
        )

    def test_rpc_to_invalid_target(self, mock_can_node):
        """Test RPC call to invalid target address."""
        with pytest.raises((ValueError, OSError)):
            mock_can_node.rpcacp_call(
                target_addr=0x40,  # Invalid
                procedure_id=1,
                payload=b"",
                priority=Priority.MED_LOW,
                synchronous=True
            )


class TestRPCACPProcedureIDs:
    """Tests for RPC procedure ID handling."""

    def test_procedure_id_zero(self, mock_can_node):
        """Test RPC with procedure ID 0."""
        mock_can_node.rpcacp_call(
            target_addr=0x02,
            procedure_id=0,
            payload=b"",
            priority=Priority.MED_LOW,
            synchronous=True
        )

    def test_procedure_id_max(self, mock_can_node):
        """Test RPC with maximum procedure ID (127)."""
        mock_can_node.rpcacp_call(
            target_addr=0x02,
            procedure_id=127,
            payload=b"",
            priority=Priority.MED_LOW,
            synchronous=True
        )

    def test_procedure_id_mid_range(self, mock_can_node):
        """Test RPC with mid-range procedure IDs."""
        for proc_id in [1, 10, 50, 100]:
            mock_can_node.rpcacp_call(
                target_addr=0x02,
                procedure_id=proc_id,
                payload=b"",
                priority=Priority.MED_LOW,
                synchronous=True
            )

    def test_procedure_id_invalid_negative(self, mock_can_node):
        """Test RPC with negative procedure ID."""
        with pytest.raises((ValueError, OSError)):
            mock_can_node.rpcacp_call(
                target_addr=0x02,
                procedure_id=-1,
                payload=b"",
                priority=Priority.MED_LOW,
                synchronous=True
            )

    def test_procedure_id_invalid_too_large(self, mock_can_node):
        """Test RPC with procedure ID above maximum."""
        with pytest.raises((ValueError, OSError)):
            mock_can_node.rpcacp_call(
                target_addr=0x02,
                procedure_id=128,  # Max is 127
                payload=b"",
                priority=Priority.MED_LOW,
                synchronous=True
            )


class TestRPCACPSynchronous:
    """Tests for synchronous RPC calls."""

    def test_synchronous_call(self, mock_can_node):
        """Test synchronous RPC call."""
        mock_can_node.rpcacp_call(
            target_addr=0x02,
            procedure_id=1,
            payload=b"SyncCall",
            priority=Priority.MED_HIGH,
            synchronous=True
        )

    def test_synchronous_with_all_priorities(self, mock_can_node):
        """Test synchronous RPC with all priority levels."""
        priorities = [Priority.HIGH, Priority.MED_HIGH, Priority.MED_LOW, Priority.LOW]

        for i, priority in enumerate(priorities):
            mock_can_node.rpcacp_call(
                target_addr=0x02,
                procedure_id=i,
                payload=bytes([i]),
                priority=priority,
                synchronous=True
            )

    def test_synchronous_to_multiple_targets(self, mock_can_node):
        """Test synchronous RPC to different targets."""
        for target in [0x01, 0x02, 0x10, 0x20]:
            mock_can_node.rpcacp_call(
                target_addr=target,
                procedure_id=1,
                payload=b"Multi",
                priority=Priority.MED_LOW,
                synchronous=True
            )


class TestRPCACPAsynchronous:
    """Tests for asynchronous RPC calls."""

    def test_asynchronous_call(self, mock_can_node):
        """Test asynchronous RPC call."""
        mock_can_node.rpcacp_call(
            target_addr=0x02,
            procedure_id=2,
            payload=b"AsyncCall",
            priority=Priority.MED_LOW,
            synchronous=False
        )

    def test_asynchronous_fire_and_forget(self, mock_can_node):
        """Test asynchronous RPC as fire-and-forget."""
        mock_can_node.rpcacp_call(
            target_addr=0x02,
            procedure_id=99,
            payload=b"",
            priority=Priority.LOW,
            synchronous=False
        )
        # Should return immediately

    def test_multiple_asynchronous_calls(self, mock_can_node):
        """Test multiple consecutive asynchronous RPC calls."""
        for i in range(5):
            mock_can_node.rpcacp_call(
                target_addr=0x02,
                procedure_id=i,
                payload=bytes([i]),
                priority=Priority.MED_LOW,
                synchronous=False
            )


class TestRPCACPPriorities:
    """Tests for RPC call priorities."""

    def test_high_priority_rpc(self, mock_can_node):
        """Test high priority RPC call."""
        mock_can_node.rpcacp_call(
            target_addr=0x02,
            procedure_id=1,
            payload=b"HighPri",
            priority=Priority.HIGH,
            synchronous=True
        )

    def test_low_priority_rpc(self, mock_can_node):
        """Test low priority RPC call."""
        mock_can_node.rpcacp_call(
            target_addr=0x02,
            procedure_id=2,
            payload=b"LowPri",
            priority=Priority.LOW,
            synchronous=False
        )

    def test_priority_order(self, mock_can_node):
        """Test RPC calls with different priorities."""
        # Send RPCs with different priorities
        mock_can_node.rpcacp_call(
            target_addr=0x02, procedure_id=1,
            payload=b"Low", priority=Priority.LOW, synchronous=False
        )
        mock_can_node.rpcacp_call(
            target_addr=0x02, procedure_id=2,
            payload=b"High", priority=Priority.HIGH, synchronous=False
        )
        mock_can_node.rpcacp_call(
            target_addr=0x02, procedure_id=3,
            payload=b"Med", priority=Priority.MED_LOW, synchronous=False
        )


class TestRPCACPPayloads:
    """Tests for RPC payload handling."""

    def test_small_payload(self, mock_can_node):
        """Test RPC with small payload."""
        mock_can_node.rpcacp_call(
            target_addr=0x02,
            procedure_id=1,
            payload=b"X",
            priority=Priority.MED_LOW,
            synchronous=True
        )

    def test_medium_payload(self, mock_can_node):
        """Test RPC with medium payload."""
        mock_can_node.rpcacp_call(
            target_addr=0x02,
            procedure_id=1,
            payload=b"1234",
            priority=Priority.MED_LOW,
            synchronous=True
        )

    def test_max_payload(self, mock_can_node):
        """Test RPC with maximum payload size."""
        max_payload = b"12345678"  # 8 bytes
        mock_can_node.rpcacp_call(
            target_addr=0x02,
            procedure_id=1,
            payload=max_payload,
            priority=Priority.MED_HIGH,
            synchronous=True
        )

    def test_binary_payload(self, mock_can_node):
        """Test RPC with binary payload data."""
        binary_payload = bytes([0x00, 0x01, 0xAA, 0xFF])
        mock_can_node.rpcacp_call(
            target_addr=0x02,
            procedure_id=50,
            payload=binary_payload,
            priority=Priority.MED_LOW,
            synchronous=True
        )

    def test_structured_payload(self, mock_can_node):
        """Test RPC with structured binary payload."""
        import struct
        # Pack: uint8, uint16, uint8
        payload = struct.pack('<BHB', 1, 1000, 2)
        mock_can_node.rpcacp_call(
            target_addr=0x02,
            procedure_id=75,
            payload=payload,
            priority=Priority.MED_HIGH,
            synchronous=True
        )


class TestRPCACPEdgeCases:
    """Tests for RPC edge cases and error conditions."""

    def test_rpc_to_self(self, mock_can_node):
        """Test RPC call to self (same node address)."""
        # Assuming node_address is 0x01
        mock_can_node.rpcacp_call(
            target_addr=0x01,  # Same as sender
            procedure_id=1,
            payload=b"ToSelf",
            priority=Priority.MED_LOW,
            synchronous=False
        )

    def test_rapid_rpc_calls(self, mock_can_node):
        """Test rapid consecutive RPC calls."""
        for i in range(10):
            mock_can_node.rpcacp_call(
                target_addr=0x02,
                procedure_id=i % 128,
                payload=bytes([i]),
                priority=Priority.MED_LOW,
                synchronous=False
            )

    def test_alternating_sync_async(self, mock_can_node):
        """Test alternating synchronous and asynchronous calls."""
        for i in range(6):
            mock_can_node.rpcacp_call(
                target_addr=0x02,
                procedure_id=i,
                payload=bytes([i]),
                priority=Priority.MED_LOW,
                synchronous=(i % 2 == 0)  # Alternate sync/async
            )
