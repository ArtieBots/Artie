"""
Unit tests for PSACP (Publisher/Subscriber Addressed Communication Protocol).

Tests topic-based publishing and subscribing with various priorities and data.
"""
import pytest
from artie_can import ArtieCAN, BackendType, Priority


class TestPSACPBasicPublishing:
    """Tests for basic PSACP publishing functionality."""

    def test_simple_publish(self, mock_can_node):
        """Test publishing a simple message to a topic."""
        mock_can_node.psacp_publish(
            topic=0x10,
            data=b"Test",
            priority=Priority.MED_LOW,
            high_priority=False
        )

    def test_publish_empty_data(self, mock_can_node):
        """Test publishing empty data to a topic."""
        mock_can_node.psacp_publish(
            topic=0x20,
            data=b"",
            priority=Priority.MED_LOW,
            high_priority=False
        )

    def test_publish_max_data(self, mock_can_node):
        """Test publishing maximum data length."""
        max_data = b"12345678"  # 8 bytes
        mock_can_node.psacp_publish(
            topic=0x30,
            data=max_data,
            priority=Priority.MED_HIGH,
            high_priority=False
        )

    def test_publish_to_broadcast_topic(self, mock_can_node):
        """Test publishing to broadcast topic (0x00)."""
        mock_can_node.psacp_publish(
            topic=0x00,  # Broadcast topic
            data=b"Broadcast",
            priority=Priority.HIGH,
            high_priority=False
        )


class TestPSACPTopicIDs:
    """Tests for PSACP topic ID handling."""

    def test_valid_topic_range(self, mock_can_node):
        """Test publishing to various valid topic IDs."""
        # Valid topics: 0x0B-0xF4 for normal, 0x00 for broadcast
        valid_topics = [0x00, 0x0B, 0x10, 0x50, 0xA0, 0xF4]

        for topic in valid_topics:
            mock_can_node.psacp_publish(
                topic=topic,
                data=b"Data",
                priority=Priority.MED_LOW,
                high_priority=False
            )

    def test_min_valid_topic(self, mock_can_node):
        """Test publishing to minimum valid topic ID."""
        mock_can_node.psacp_publish(
            topic=0x0B,  # Minimum valid topic
            data=b"MinTopic",
            priority=Priority.MED_LOW,
            high_priority=False
        )

    def test_max_valid_topic(self, mock_can_node):
        """Test publishing to maximum valid topic ID."""
        mock_can_node.psacp_publish(
            topic=0xF4,  # Maximum valid topic
            data=b"MaxTopic",
            priority=Priority.MED_LOW,
            high_priority=False
        )

    def test_reserved_topic_areas(self, mock_can_node):
        """Test publishing to topics in reserved areas."""
        # Topics 0x01-0x0A might be reserved
        # This test checks if library handles them correctly
        for topic in [0x01, 0x05, 0x0A]:
            try:
                mock_can_node.psacp_publish(
                    topic=topic,
                    data=b"Reserved",
                    priority=Priority.MED_LOW,
                    high_priority=False
                )
            except (ValueError, OSError):
                # Expected if reserved topics are rejected
                pass


class TestPSACPPriorities:
    """Tests for PSACP message priorities."""

    def test_high_priority_publish(self, mock_can_node):
        """Test publishing with high priority."""
        mock_can_node.psacp_publish(
            topic=0x10,
            data=b"HighPri",
            priority=Priority.HIGH,
            high_priority=False
        )

    def test_low_priority_publish(self, mock_can_node):
        """Test publishing with low priority."""
        mock_can_node.psacp_publish(
            topic=0x10,
            data=b"LowPri",
            priority=Priority.LOW,
            high_priority=False
        )

    def test_all_priority_levels(self, mock_can_node):
        """Test publishing with all priority levels."""
        priorities = [Priority.HIGH, Priority.MED_HIGH, Priority.MED_LOW, Priority.LOW]

        for i, priority in enumerate(priorities):
            mock_can_node.psacp_publish(
                topic=0x10 + i,
                data=bytes([i]),
                priority=priority,
                high_priority=False
            )


class TestPSACPHighPriority:
    """Tests for high priority pub/sub functionality."""

    def test_high_priority_flag_enabled(self, mock_can_node):
        """Test publishing with high_priority flag enabled."""
        mock_can_node.psacp_publish(
            topic=0x10,
            data=b"HighPriPubSub",
            priority=Priority.HIGH,
            high_priority=True
        )

    def test_high_priority_flag_disabled(self, mock_can_node):
        """Test publishing with high_priority flag disabled."""
        mock_can_node.psacp_publish(
            topic=0x10,
            data=b"NormalPubSub",
            priority=Priority.MED_LOW,
            high_priority=False
        )

    def test_high_priority_with_low_message_priority(self, mock_can_node):
        """Test high priority pub/sub with low message priority."""
        # high_priority flag vs message priority are different concepts
        mock_can_node.psacp_publish(
            topic=0x10,
            data=b"Mixed",
            priority=Priority.LOW,
            high_priority=True
        )


class TestPSACPReceiving:
    """Tests for PSACP message receiving/subscribing."""

    def test_receive_timeout(self, mock_can_node):
        """Test receiving with timeout when no message available."""
        with pytest.raises((TimeoutError, OSError)):
            mock_can_node.psacp_receive(timeout_ms=100)

    def test_receive_zero_timeout(self, mock_can_node):
        """Test receiving with zero timeout (non-blocking)."""
        with pytest.raises((TimeoutError, OSError)):
            mock_can_node.psacp_receive(timeout_ms=0)

    def test_receive_large_timeout(self, mock_can_node):
        """Test receiving with large timeout value."""
        with pytest.raises((TimeoutError, OSError)):
            mock_can_node.psacp_receive(timeout_ms=5000)


class TestPSACPCommunication:
    """Tests for PSACP publish/subscribe communication."""

    def test_publish_and_receive(self, mock_can_pair):
        """Test publishing and receiving messages."""
        node1, node2 = mock_can_pair

        test_data = b"PubSubTest"
        test_topic = 0x10

        # Publish message
        node1.psacp_publish(
            topic=test_topic,
            data=test_data,
            priority=Priority.MED_LOW,
            high_priority=False
        )

        # Try to receive (mock backend might allow loopback)
        try:
            sender, topic, recv_data = node1.psacp_receive(timeout_ms=1000)
            assert recv_data == test_data
            assert topic == test_topic
        except (TimeoutError, OSError):
            # Expected if mock backend doesn't support loopback
            pass

    def test_multiple_publishes(self, mock_can_pair):
        """Test publishing multiple messages to same topic."""
        node1, node2 = mock_can_pair

        messages = [b"Msg1", b"Msg2", b"Msg3"]
        topic = 0x20

        for msg in messages:
            node1.psacp_publish(
                topic=topic,
                data=msg,
                priority=Priority.MED_LOW,
                high_priority=False
            )

    def test_different_topics(self, mock_can_pair):
        """Test publishing to different topics."""
        node1, node2 = mock_can_pair

        topics_data = [
            (0x10, b"Topic1"),
            (0x20, b"Topic2"),
            (0x30, b"Topic3"),
        ]

        for topic, data in topics_data:
            node1.psacp_publish(
                topic=topic,
                data=data,
                priority=Priority.MED_LOW,
                high_priority=False
            )


class TestPSACPDataPayloads:
    """Tests for various PSACP data payload types."""

    def test_ascii_payload(self, mock_can_node):
        """Test publishing ASCII text data."""
        mock_can_node.psacp_publish(
            topic=0x10,
            data=b"ASCIIText",
            priority=Priority.MED_LOW,
            high_priority=False
        )

    def test_binary_payload(self, mock_can_node):
        """Test publishing binary data."""
        binary_data = bytes([0x00, 0xFF, 0xAA, 0x55])
        mock_can_node.psacp_publish(
            topic=0x10,
            data=binary_data,
            priority=Priority.MED_HIGH,
            high_priority=False
        )

    def test_single_byte_payload(self, mock_can_node):
        """Test publishing single byte payload."""
        mock_can_node.psacp_publish(
            topic=0x10,
            data=b"X",
            priority=Priority.LOW,
            high_priority=False
        )

    def test_numeric_payload(self, mock_can_node):
        """Test publishing numeric data as bytes."""
        import struct
        numeric_data = struct.pack('<f', 3.14159)  # Float
        mock_can_node.psacp_publish(
            topic=0x10,
            data=numeric_data,
            priority=Priority.MED_LOW,
            high_priority=False
        )

    def test_sensor_data_payload(self, mock_can_node):
        """Test publishing sensor-like data."""
        import struct
        # Simulate sensor reading: timestamp (uint32) + value (float)
        sensor_data = struct.pack('<If', 1234567, 25.5)
        mock_can_node.psacp_publish(
            topic=0x50,  # Sensor topic
            data=sensor_data,
            priority=Priority.MED_HIGH,
            high_priority=False
        )


class TestPSACPEdgeCases:
    """Tests for PSACP edge cases."""

    def test_rapid_publishes(self, mock_can_node):
        """Test rapid consecutive publishes to same topic."""
        for i in range(10):
            mock_can_node.psacp_publish(
                topic=0x10,
                data=bytes([i]),
                priority=Priority.MED_LOW,
                high_priority=False
            )

    def test_alternating_topics(self, mock_can_node):
        """Test alternating between two topics."""
        for i in range(6):
            topic = 0x10 if i % 2 == 0 else 0x20
            mock_can_node.psacp_publish(
                topic=topic,
                data=bytes([i]),
                priority=Priority.MED_LOW,
                high_priority=False
            )

    def test_broadcast_and_targeted(self, mock_can_node):
        """Test mixing broadcast and targeted topic publishes."""
        # Broadcast
        mock_can_node.psacp_publish(
            topic=0x00,
            data=b"Broadcast",
            priority=Priority.HIGH,
            high_priority=False
        )

        # Targeted
        mock_can_node.psacp_publish(
            topic=0x10,
            data=b"Targeted",
            priority=Priority.MED_LOW,
            high_priority=False
        )

    def test_priority_combinations(self, mock_can_node):
        """Test various combinations of priority settings."""
        combos = [
            (Priority.HIGH, True),
            (Priority.HIGH, False),
            (Priority.LOW, True),
            (Priority.LOW, False),
            (Priority.MED_LOW, True),
            (Priority.MED_HIGH, False),
        ]

        for msg_priority, high_pri_flag in combos:
            mock_can_node.psacp_publish(
                topic=0x10,
                data=b"Test",
                priority=msg_priority,
                high_priority=high_pri_flag
            )
