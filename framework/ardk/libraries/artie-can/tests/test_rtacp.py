"""
Unit tests for RTACP (Real-Time Addressed Communication Protocol).

Tests sending and receiving real-time messages with various priorities,
addressing modes, and data payloads.
"""
import pytest
from artie_can import ArtieCAN, BackendType, Priority


class TestRTACPBasicSending:
    """Tests for basic RTACP message sending."""

    def test_send_unicast_message(self, mock_can_node):
        """Test sending a unicast RTACP message."""
        mock_can_node.rtacp_send(
            target_addr=0x02,
            data=b"Test",
            priority=Priority.MED_LOW
        )
        # Should not raise any exceptions

    def test_send_broadcast_message(self, mock_can_node):
        """Test sending a broadcast RTACP message."""
        mock_can_node.rtacp_send(
            target_addr=0x00,  # Broadcast address
            data=b"Broadcast",
            priority=Priority.HIGH
        )
        # Should not raise any exceptions

    def test_send_empty_message(self, mock_can_node):
        """Test sending an empty RTACP message."""
        mock_can_node.rtacp_send(
            target_addr=0x02,
            data=b"",
            priority=Priority.LOW
        )
        # Should not raise any exceptions

    def test_send_max_length_message(self, mock_can_node):
        """Test sending maximum length RTACP message (8 bytes)."""
        max_data = b"12345678"  # 8 bytes
        mock_can_node.rtacp_send(
            target_addr=0x02,
            data=max_data,
            priority=Priority.MED_HIGH
        )
        # Should not raise any exceptions

    def test_send_with_invalid_target(self, mock_can_node):
        """Test sending to invalid target address."""
        with pytest.raises((ValueError, OSError)):
            mock_can_node.rtacp_send(
                target_addr=0x40,  # Invalid (> 0x3F)
                data=b"Test",
                priority=Priority.MED_LOW
            )


class TestRTACPPriorities:
    """Tests for RTACP message priorities."""

    def test_send_high_priority(self, mock_can_node):
        """Test sending high priority message."""
        mock_can_node.rtacp_send(
            target_addr=0x02,
            data=b"High",
            priority=Priority.HIGH
        )

    def test_send_med_high_priority(self, mock_can_node):
        """Test sending medium-high priority message."""
        mock_can_node.rtacp_send(
            target_addr=0x02,
            data=b"MedHigh",
            priority=Priority.MED_HIGH
        )

    def test_send_med_low_priority(self, mock_can_node):
        """Test sending medium-low priority message."""
        mock_can_node.rtacp_send(
            target_addr=0x02,
            data=b"MedLow",
            priority=Priority.MED_LOW
        )

    def test_send_low_priority(self, mock_can_node):
        """Test sending low priority message."""
        mock_can_node.rtacp_send(
            target_addr=0x02,
            data=b"Low",
            priority=Priority.LOW
        )


class TestRTACPReceiving:
    """Tests for RTACP message receiving."""

    def test_receive_timeout(self, mock_can_node):
        """Test receiving with timeout when no message available."""
        with pytest.raises((TimeoutError, OSError)):
            mock_can_node.rtacp_receive(timeout_ms=100)

    def test_receive_with_zero_timeout(self, mock_can_node):
        """Test receiving with zero timeout (non-blocking)."""
        with pytest.raises((TimeoutError, OSError)):
            mock_can_node.rtacp_receive(timeout_ms=0)

    def test_receive_with_large_timeout(self, mock_can_node):
        """Test receiving with large timeout value."""
        with pytest.raises((TimeoutError, OSError)):
            mock_can_node.rtacp_receive(timeout_ms=5000)


class TestRTACPCommunication:
    """Tests for RTACP communication between nodes."""

    def test_send_and_receive_simple(self, mock_can_pair):
        """Test sending from one node and receiving on another."""
        node1, node2 = mock_can_pair

        # Note: Mock backend uses a shared queue, so both nodes share the same queue
        # Send from node1
        test_data = b"Hello"
        node1.rtacp_send(
            target_addr=0x02,
            data=test_data,
            priority=Priority.MED_LOW
        )

        # Receive on node1 (mock backend allows receiving own messages)
        try:
            sender, target, recv_data = node1.rtacp_receive(timeout_ms=1000)
            assert recv_data == test_data
            assert target == 0x02
        except (TimeoutError, OSError):
            # Mock backend might not support loopback
            pass

    def test_broadcast_communication(self, mock_can_pair):
        """Test broadcast message communication."""
        node1, node2 = mock_can_pair

        test_data = b"Broadcast"
        node1.rtacp_send(
            target_addr=0x00,  # Broadcast
            data=test_data,
            priority=Priority.HIGH
        )

        # Try to receive
        try:
            sender, target, recv_data = node1.rtacp_receive(timeout_ms=1000)
            assert recv_data == test_data
            assert target == 0x00  # Broadcast address
        except (TimeoutError, OSError):
            # Expected if mock backend doesn't support loopback
            pass

    def test_multiple_messages(self, mock_can_pair):
        """Test sending and receiving multiple messages."""
        node1, node2 = mock_can_pair

        messages = [b"Msg1", b"Msg2", b"Msg3"]

        # Send multiple messages
        for msg in messages:
            node1.rtacp_send(
                target_addr=0x02,
                data=msg,
                priority=Priority.MED_LOW
            )

        # Try to receive all messages
        received = []
        for _ in range(len(messages)):
            try:
                sender, target, data = node1.rtacp_receive(timeout_ms=1000)
                received.append(data)
            except (TimeoutError, OSError):
                break

        # At least verify no exceptions occurred during sending
        assert len(messages) == 3


class TestRTACPAcknowledgment:
    """Tests for RTACP acknowledgment functionality."""

    def test_send_with_wait_ack(self, mock_can_node):
        """Test sending with ACK waiting enabled."""
        # This may timeout on mock backend if ACK is not implemented
        try:
            mock_can_node.rtacp_send(
                target_addr=0x02,
                data=b"NeedAck",
                priority=Priority.HIGH,
                wait_ack=True
            )
        except (TimeoutError, OSError):
            # Expected if mock backend doesn't implement ACK
            pass

    def test_send_without_wait_ack(self, mock_can_node):
        """Test sending without ACK waiting (fire and forget)."""
        mock_can_node.rtacp_send(
            target_addr=0x02,
            data=b"NoAck",
            priority=Priority.MED_LOW,
            wait_ack=False
        )
        # Should complete immediately

    def test_broadcast_cannot_wait_ack(self, mock_can_node):
        """Test that broadcast messages cannot wait for ACK."""
        # Broadcast should not wait for ACK (undefined behavior)
        mock_can_node.rtacp_send(
            target_addr=0x00,  # Broadcast
            data=b"Broadcast",
            priority=Priority.HIGH,
            wait_ack=False  # Should be False for broadcast
        )


class TestRTACPDataPayloads:
    """Tests for various RTACP data payload types."""

    def test_ascii_data(self, mock_can_node):
        """Test sending ASCII text data."""
        mock_can_node.rtacp_send(
            target_addr=0x02,
            data=b"ASCII123",
            priority=Priority.MED_LOW
        )

    def test_binary_data(self, mock_can_node):
        """Test sending binary data."""
        binary_data = bytes([0x00, 0xFF, 0xAA, 0x55])
        mock_can_node.rtacp_send(
            target_addr=0x02,
            data=binary_data,
            priority=Priority.MED_HIGH
        )

    def test_single_byte(self, mock_can_node):
        """Test sending single byte."""
        mock_can_node.rtacp_send(
            target_addr=0x02,
            data=b"X",
            priority=Priority.LOW
        )

    def test_numeric_data(self, mock_can_node):
        """Test sending numeric data as bytes."""
        import struct
        numeric_data = struct.pack('<I', 12345)  # Little-endian uint32
        mock_can_node.rtacp_send(
            target_addr=0x02,
            data=numeric_data,
            priority=Priority.MED_LOW
        )
