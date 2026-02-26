"""
Unit tests for BWACP (Block Write Addressed Communication Protocol).

Tests multi-frame data transfer, block IDs, and sequence numbering.
"""
import pytest
from artie_can import ArtieCAN, BackendType, Priority


class TestBWACPBasicBlockWrite:
    """Tests for basic BWACP block write functionality."""

    def test_small_block_write(self, mock_can_node):
        """Test writing a small block of data."""
        test_data = b"SmallBlock"
        mock_can_node.bwacp_write(
            target_addr=0x02,
            block_id=1,
            data=test_data,
            priority=Priority.MED_LOW
        )

    def test_empty_block_write(self, mock_can_node):
        """Test writing an empty block."""
        mock_can_node.bwacp_write(
            target_addr=0x02,
            block_id=1,
            data=b"",
            priority=Priority.MED_LOW
        )

    def test_single_byte_block(self, mock_can_node):
        """Test writing a single byte block."""
        mock_can_node.bwacp_write(
            target_addr=0x02,
            block_id=5,
            data=b"X",
            priority=Priority.MED_HIGH
        )

    def test_block_write_to_invalid_target(self, mock_can_node):
        """Test block write to invalid target address."""
        with pytest.raises((ValueError, OSError)):
            mock_can_node.bwacp_write(
                target_addr=0x40,  # Invalid
                block_id=1,
                data=b"Test",
                priority=Priority.MED_LOW
            )


class TestBWACPBlockIDs:
    """Tests for BWACP block ID handling."""

    def test_block_id_zero(self, mock_can_node):
        """Test block write with ID 0."""
        mock_can_node.bwacp_write(
            target_addr=0x02,
            block_id=0,
            data=b"BlockZero",
            priority=Priority.MED_LOW
        )

    def test_block_id_max(self, mock_can_node):
        """Test block write with maximum ID."""
        # Assuming 8-bit block ID (0-255)
        mock_can_node.bwacp_write(
            target_addr=0x02,
            block_id=255,
            data=b"BlockMax",
            priority=Priority.MED_LOW
        )

    def test_sequential_block_ids(self, mock_can_node):
        """Test writing blocks with sequential IDs."""
        for block_id in range(5):
            mock_can_node.bwacp_write(
                target_addr=0x02,
                block_id=block_id,
                data=bytes([block_id]),
                priority=Priority.MED_LOW
            )

    def test_random_block_ids(self, mock_can_node):
        """Test writing blocks with non-sequential IDs."""
        block_ids = [10, 5, 100, 1, 50]
        for block_id in block_ids:
            mock_can_node.bwacp_write(
                target_addr=0x02,
                block_id=block_id,
                data=b"Data",
                priority=Priority.MED_LOW
            )

    def test_block_id_invalid_negative(self, mock_can_node):
        """Test block write with negative ID."""
        with pytest.raises((ValueError, OSError)):
            mock_can_node.bwacp_write(
                target_addr=0x02,
                block_id=-1,
                data=b"Test",
                priority=Priority.MED_LOW
            )


class TestBWACPLargeData:
    """Tests for BWACP large data transfers."""

    def test_medium_block(self, mock_can_node):
        """Test writing a medium-sized block (64 bytes)."""
        medium_data = b"A" * 64
        mock_can_node.bwacp_write(
            target_addr=0x02,
            block_id=10,
            data=medium_data,
            priority=Priority.MED_HIGH
        )

    def test_large_block(self, mock_can_node):
        """Test writing a large block (256 bytes)."""
        large_data = b"B" * 256
        mock_can_node.bwacp_write(
            target_addr=0x02,
            block_id=20,
            data=large_data,
            priority=Priority.MED_LOW
        )

    def test_very_large_block(self, mock_can_node):
        """Test writing a very large block (1KB)."""
        very_large_data = b"C" * 1024
        mock_can_node.bwacp_write(
            target_addr=0x02,
            block_id=30,
            data=very_large_data,
            priority=Priority.MED_LOW
        )

    def test_firmware_size_block(self, mock_can_node):
        """Test writing a firmware-sized block (simulated, 4KB)."""
        # Smaller than actual firmware for test speed
        firmware_data = b"F" * 4096
        mock_can_node.bwacp_write(
            target_addr=0x02,
            block_id=99,
            data=firmware_data,
            priority=Priority.HIGH
        )


class TestBWACPPriorities:
    """Tests for BWACP message priorities."""

    def test_high_priority_block(self, mock_can_node):
        """Test block write with high priority."""
        mock_can_node.bwacp_write(
            target_addr=0x02,
            block_id=1,
            data=b"HighPriBlock",
            priority=Priority.HIGH
        )

    def test_low_priority_block(self, mock_can_node):
        """Test block write with low priority."""
        mock_can_node.bwacp_write(
            target_addr=0x02,
            block_id=2,
            data=b"LowPriBlock",
            priority=Priority.LOW
        )

    def test_all_priorities(self, mock_can_node):
        """Test block write with all priority levels."""
        priorities = [Priority.HIGH, Priority.MED_HIGH, Priority.MED_LOW, Priority.LOW]

        for i, priority in enumerate(priorities):
            mock_can_node.bwacp_write(
                target_addr=0x02,
                block_id=i,
                data=bytes([i]) * 10,
                priority=priority
            )


class TestBWACPMultiFrame:
    """Tests for multi-frame BWACP transfers."""

    def test_data_requiring_multiple_frames(self, mock_can_node):
        """Test data that requires multiple CAN frames."""
        # CAN frame payload is typically 8 bytes, so > 8 bytes needs multiple frames
        multi_frame_data = b"ThisDataNeedsMultipleFramesToTransmit"
        mock_can_node.bwacp_write(
            target_addr=0x02,
            block_id=42,
            data=multi_frame_data,
            priority=Priority.MED_HIGH
        )

    def test_exactly_one_frame(self, mock_can_node):
        """Test data that fits exactly in one frame."""
        one_frame_data = b"12345678"  # Exactly 8 bytes
        mock_can_node.bwacp_write(
            target_addr=0x02,
            block_id=1,
            data=one_frame_data,
            priority=Priority.MED_LOW
        )

    def test_just_over_one_frame(self, mock_can_node):
        """Test data just over one frame size."""
        over_one_frame = b"123456789"  # 9 bytes, needs 2 frames
        mock_can_node.bwacp_write(
            target_addr=0x02,
            block_id=2,
            data=over_one_frame,
            priority=Priority.MED_LOW
        )


class TestBWACPDataTypes:
    """Tests for various BWACP data types."""

    def test_binary_data(self, mock_can_node):
        """Test writing binary data."""
        binary_data = bytes(range(256))  # All byte values 0-255
        mock_can_node.bwacp_write(
            target_addr=0x02,
            block_id=100,
            data=binary_data,
            priority=Priority.MED_LOW
        )

    def test_ascii_data(self, mock_can_node):
        """Test writing ASCII text data."""
        ascii_data = b"The quick brown fox jumps over the lazy dog"
        mock_can_node.bwacp_write(
            target_addr=0x02,
            block_id=50,
            data=ascii_data,
            priority=Priority.MED_LOW
        )

    def test_structured_binary_data(self, mock_can_node):
        """Test writing structured binary data."""
        import struct
        # Create a structured data block: header + payload
        header = struct.pack('<IHH', 0x12345678, 0x100, 0x200)
        payload = b"A" * 100
        structured_data = header + payload

        mock_can_node.bwacp_write(
            target_addr=0x02,
            block_id=75,
            data=structured_data,
            priority=Priority.MED_HIGH
        )

    def test_null_bytes_in_data(self, mock_can_node):
        """Test writing data containing null bytes."""
        null_data = b"Data\x00with\x00null\x00bytes"
        mock_can_node.bwacp_write(
            target_addr=0x02,
            block_id=25,
            data=null_data,
            priority=Priority.MED_LOW
        )


class TestBWACPTargetAddressing:
    """Tests for BWACP target addressing."""

    def test_write_to_multiple_targets(self, mock_can_node):
        """Test writing same block to different targets."""
        test_data = b"MultiTarget"
        targets = [0x01, 0x02, 0x10, 0x20]

        for target in targets:
            mock_can_node.bwacp_write(
                target_addr=target,
                block_id=1,
                data=test_data,
                priority=Priority.MED_LOW
            )

    def test_write_to_broadcast(self, mock_can_node):
        """Test writing to broadcast address."""
        # Broadcast might not be valid for BWACP (targeted protocol)
        try:
            mock_can_node.bwacp_write(
                target_addr=0x00,  # Broadcast
                block_id=1,
                data=b"Broadcast",
                priority=Priority.HIGH
            )
        except (ValueError, OSError):
            # Expected if broadcast not supported for BWACP
            pass


class TestBWACPEdgeCases:
    """Tests for BWACP edge cases."""

    def test_consecutive_blocks_same_target(self, mock_can_node):
        """Test writing consecutive blocks to same target."""
        for i in range(5):
            mock_can_node.bwacp_write(
                target_addr=0x02,
                block_id=i,
                data=b"Block" + bytes([i]),
                priority=Priority.MED_LOW
            )

    def test_interleaved_targets(self, mock_can_node):
        """Test writing blocks interleaved between targets."""
        for i in range(4):
            target = 0x02 if i % 2 == 0 else 0x03
            mock_can_node.bwacp_write(
                target_addr=target,
                block_id=i // 2,
                data=bytes([i]),
                priority=Priority.MED_LOW
            )

    def test_same_block_id_different_targets(self, mock_can_node):
        """Test using same block ID for different targets."""
        same_block_id = 42

        mock_can_node.bwacp_write(
            target_addr=0x02,
            block_id=same_block_id,
            data=b"TargetTwo",
            priority=Priority.MED_LOW
        )

        mock_can_node.bwacp_write(
            target_addr=0x03,
            block_id=same_block_id,
            data=b"TargetThree",
            priority=Priority.MED_LOW
        )

    def test_rapid_block_writes(self, mock_can_node):
        """Test rapid consecutive block writes."""
        for i in range(10):
            mock_can_node.bwacp_write(
                target_addr=0x02,
                block_id=i,
                data=bytes([i]) * 20,
                priority=Priority.MED_LOW
            )

    def test_varying_block_sizes(self, mock_can_node):
        """Test writing blocks of varying sizes."""
        sizes = [1, 8, 16, 32, 64, 128, 256]

        for i, size in enumerate(sizes):
            mock_can_node.bwacp_write(
                target_addr=0x02,
                block_id=i,
                data=b"X" * size,
                priority=Priority.MED_LOW
            )
