"""
Artie CAN Library - Python bindings

This module provides Python bindings for the Artie CAN library, which implements
the Artie CAN Protocol for communication over CAN bus.
"""

import ctypes
import os
import pathlib
from typing import Optional, Tuple, List
from enum import IntEnum

# Find the shared library
_lib_name = "libartie_can.so"
_lib_path = None

# Search in common locations
_search_paths = [
    pathlib.Path(__file__).parent / "lib",
    pathlib.Path("/usr/local/lib"),
    pathlib.Path("/usr/lib"),
]

for path in _search_paths:
    candidate = path / _lib_name
    if candidate.exists():
        _lib_path = str(candidate)
        break

if _lib_path is None:
    # Try to load from system path
    _lib_path = _lib_name

try:
    _lib = ctypes.CDLL(_lib_path)
except OSError:
    # Library not found - this might be OK if we're just importing for documentation
    _lib = None


# ===== Enums =====

class BackendType(IntEnum):
    """CAN backend types"""
    SOCKETCAN = 0
    MCP2515 = 1
    MOCK = 2


class Priority(IntEnum):
    """Message priority levels"""
    HIGH = 0
    MED_HIGH = 1
    MED_LOW = 2
    LOW = 3


class RTACPFrameType(IntEnum):
    """RTACP frame types"""
    ACK = 0
    MSG = 1


class RPCACPFrameType(IntEnum):
    """RPCACP frame types"""
    ACK = 0
    NACK = 1
    START_RPC = 2
    START_RETURN = 3
    TX_DATA = 4
    RX_DATA = 5


class PSACPFrameType(IntEnum):
    """PSACP frame types"""
    PUB = 1
    DATA = 3


class BWACPFrameType(IntEnum):
    """BWACP frame types"""
    REPEAT = 1
    READY = 3
    DATA = 7


# ===== C Structure Definitions =====

class CANFrame(ctypes.Structure):
    """CAN frame structure"""
    _fields_ = [
        ("can_id", ctypes.c_uint32),
        ("dlc", ctypes.c_uint8),
        ("data", ctypes.c_uint8 * 8),
        ("extended", ctypes.c_bool),
    ]


class CANBackend(ctypes.Structure):
    """CAN backend interface"""
    _fields_ = [
        ("init", ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p)),
        ("send", ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.POINTER(CANFrame))),
        ("receive", ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.POINTER(CANFrame), ctypes.c_uint32)),
        ("close", ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p)),
        ("context", ctypes.c_void_p),
    ]


class CANContext(ctypes.Structure):
    """Main CAN context structure"""
    _fields_ = [
        ("node_address", ctypes.c_uint8),
        ("backend", CANBackend),
    ]


class RTACPMessage(ctypes.Structure):
    """RTACP message structure"""
    _fields_ = [
        ("priority", ctypes.c_uint8),
        ("sender_addr", ctypes.c_uint8),
        ("target_addr", ctypes.c_uint8),
        ("frame_type", ctypes.c_int),
        ("data", ctypes.c_uint8 * 8),
        ("data_len", ctypes.c_uint8),
    ]


class RPCACPMessage(ctypes.Structure):
    """RPCACP message structure"""
    _fields_ = [
        ("priority", ctypes.c_uint8),
        ("sender_addr", ctypes.c_uint8),
        ("target_addr", ctypes.c_uint8),
        ("random_value", ctypes.c_uint8),
        ("frame_type", ctypes.c_int),
        ("is_synchronous", ctypes.c_bool),
        ("procedure_id", ctypes.c_uint8),
        ("crc16", ctypes.c_uint16),
        ("payload", ctypes.c_uint8 * 1024),
        ("payload_len", ctypes.c_size_t),
        ("nack_error_code", ctypes.c_uint8),
    ]


class PSACPMessage(ctypes.Structure):
    """PSACP message structure"""
    _fields_ = [
        ("priority", ctypes.c_uint8),
        ("sender_addr", ctypes.c_uint8),
        ("topic", ctypes.c_uint8),
        ("high_priority", ctypes.c_bool),
        ("frame_type", ctypes.c_int),
        ("crc16", ctypes.c_uint16),
        ("payload", ctypes.c_uint8 * 2048),
        ("payload_len", ctypes.c_size_t),
    ]


class BWACPMessage(ctypes.Structure):
    """BWACP message structure"""
    _fields_ = [
        ("priority", ctypes.c_uint8),
        ("sender_addr", ctypes.c_uint8),
        ("target_addr", ctypes.c_uint8),
        ("class_mask", ctypes.c_uint8),
        ("frame_type", ctypes.c_int),
        ("is_repeat", ctypes.c_bool),
        ("parity", ctypes.c_bool),
        ("crc24", ctypes.c_uint32),
        ("address", ctypes.c_uint32),
        ("payload", ctypes.c_uint8 * 2048),
        ("payload_len", ctypes.c_size_t),
    ]


# ===== Function Definitions =====

if _lib is not None:
    # Core functions
    _lib.artie_can_init.argtypes = [ctypes.POINTER(CANContext), ctypes.c_uint8, ctypes.c_int]
    _lib.artie_can_init.restype = ctypes.c_int

    _lib.artie_can_close.argtypes = [ctypes.POINTER(CANContext)]
    _lib.artie_can_close.restype = ctypes.c_int

    # RTACP functions
    _lib.artie_can_rtacp_send.argtypes = [ctypes.POINTER(CANContext), ctypes.POINTER(RTACPMessage), ctypes.c_bool]
    _lib.artie_can_rtacp_send.restype = ctypes.c_int

    _lib.artie_can_rtacp_receive.argtypes = [ctypes.POINTER(CANContext), ctypes.POINTER(RTACPMessage), ctypes.c_uint32]
    _lib.artie_can_rtacp_receive.restype = ctypes.c_int

    # RPCACP functions
    _lib.artie_can_rpcacp_call.argtypes = [
        ctypes.POINTER(CANContext), ctypes.c_uint8, ctypes.c_uint8, ctypes.c_bool,
        ctypes.c_uint8, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t
    ]
    _lib.artie_can_rpcacp_call.restype = ctypes.c_int

    _lib.artie_can_rpcacp_receive.argtypes = [ctypes.POINTER(CANContext), ctypes.POINTER(RPCACPMessage), ctypes.c_uint32]
    _lib.artie_can_rpcacp_receive.restype = ctypes.c_int

    _lib.artie_can_rpcacp_send_ack.argtypes = [ctypes.POINTER(CANContext), ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8]
    _lib.artie_can_rpcacp_send_ack.restype = ctypes.c_int

    _lib.artie_can_rpcacp_send_nack.argtypes = [
        ctypes.POINTER(CANContext), ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8
    ]
    _lib.artie_can_rpcacp_send_nack.restype = ctypes.c_int

    # PSACP functions
    _lib.artie_can_psacp_publish.argtypes = [
        ctypes.POINTER(CANContext), ctypes.c_uint8, ctypes.c_uint8, ctypes.c_bool,
        ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t
    ]
    _lib.artie_can_psacp_publish.restype = ctypes.c_int

    _lib.artie_can_psacp_receive.argtypes = [ctypes.POINTER(CANContext), ctypes.POINTER(PSACPMessage), ctypes.c_uint32]
    _lib.artie_can_psacp_receive.restype = ctypes.c_int

    # BWACP functions
    _lib.artie_can_bwacp_send_ready.argtypes = [
        ctypes.POINTER(CANContext), ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8,
        ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t, ctypes.c_bool
    ]
    _lib.artie_can_bwacp_send_ready.restype = ctypes.c_int

    _lib.artie_can_bwacp_receive.argtypes = [ctypes.POINTER(CANContext), ctypes.POINTER(BWACPMessage), ctypes.c_uint32]
    _lib.artie_can_bwacp_receive.restype = ctypes.c_int

    # Utility functions
    _lib.artie_can_crc16.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]
    _lib.artie_can_crc16.restype = ctypes.c_uint16

    _lib.artie_can_crc24.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]
    _lib.artie_can_crc24.restype = ctypes.c_uint32


# ===== Python Wrapper Classes =====

class ArtieCANException(Exception):
    """Base exception for Artie CAN library"""
    pass


class ArtieCAN:
    """
    Artie CAN context wrapper

    This class provides a Pythonic interface to the Artie CAN library.
    """

    def __init__(self, node_address: int, backend: BackendType = BackendType.SOCKETCAN):
        """
        Initialize Artie CAN context

        Args:
            node_address: This node's CAN address (0-63)
            backend: Backend type to use
        """
        if _lib is None:
            raise ArtieCANException("Artie CAN library not found")

        if not (0 <= node_address <= 63):
            raise ValueError("Node address must be 0-63")

        self._ctx = CANContext()
        result = _lib.artie_can_init(ctypes.byref(self._ctx), node_address, backend.value)
        if result != 0:
            raise ArtieCANException(f"Failed to initialize CAN context: {result}")

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def close(self):
        """Close the CAN context"""
        if _lib is not None and hasattr(self, '_ctx'):
            _lib.artie_can_close(ctypes.byref(self._ctx))

    def rtacp_send(self, target_addr: int, data: bytes, priority: Priority = Priority.MED_LOW, wait_ack: bool = False):
        """
        Send an RTACP message

        Args:
            target_addr: Target address (0 for broadcast)
            data: Data to send (max 8 bytes)
            priority: Message priority
            wait_ack: Wait for ACK if targeted (not broadcast)
        """
        if len(data) > 8:
            raise ValueError("RTACP data must be <= 8 bytes")

        msg = RTACPMessage()
        msg.frame_type = RTACPFrameType.MSG
        msg.priority = priority.value
        msg.sender_addr = self._ctx.node_address
        msg.target_addr = target_addr
        msg.data_len = len(data)
        for i, b in enumerate(data):
            msg.data[i] = b

        result = _lib.artie_can_rtacp_send(ctypes.byref(self._ctx), ctypes.byref(msg), wait_ack)
        if result != 0:
            raise ArtieCANException(f"RTACP send failed: {result}")

    def rtacp_receive(self, timeout_ms: int = 0) -> Tuple[int, int, bytes]:
        """
        Receive an RTACP message

        Args:
            timeout_ms: Timeout in milliseconds (0 for non-blocking)

        Returns:
            Tuple of (sender_addr, target_addr, data)
        """
        msg = RTACPMessage()
        result = _lib.artie_can_rtacp_receive(ctypes.byref(self._ctx), ctypes.byref(msg), timeout_ms)
        if result != 0:
            raise ArtieCANException(f"RTACP receive failed: {result}")

        data = bytes(msg.data[:msg.data_len])
        return (msg.sender_addr, msg.target_addr, data)

    def rpcacp_call(self, target_addr: int, procedure_id: int, payload: bytes,
                    priority: Priority = Priority.MED_LOW, synchronous: bool = True):
        """
        Send an RPC request

        Args:
            target_addr: Target node address
            procedure_id: RPC procedure ID (0-127)
            payload: Serialized arguments
            priority: Message priority
            synchronous: True for synchronous (blocking) RPC
        """
        if not (0 < target_addr <= 63):
            raise ValueError("Target address must be 1-63 (0 is broadcast, not allowed for RPC)")

        if not (0 <= procedure_id <= 127):
            raise ValueError("Procedure ID must be 0-127")

        payload_arr = (ctypes.c_uint8 * len(payload))(*payload)
        result = _lib.artie_can_rpcacp_call(
            ctypes.byref(self._ctx), target_addr, priority.value,
            synchronous, procedure_id, payload_arr, len(payload)
        )
        if result != 0:
            raise ArtieCANException(f"RPC call failed: {result}")

    def psacp_publish(self, topic: int, data: bytes, priority: Priority = Priority.MED_LOW,
                     high_priority: bool = False):
        """
        Publish a message to a topic

        Args:
            topic: Topic ID (0x0B-0xF4, or 0x00 for broadcast)
            data: Message data
            priority: Message priority
            high_priority: Use high priority pub/sub
        """
        data_arr = (ctypes.c_uint8 * len(data))(*data)
        result = _lib.artie_can_psacp_publish(
            ctypes.byref(self._ctx), topic, priority.value,
            high_priority, data_arr, len(data)
        )
        if result != 0:
            raise ArtieCANException(f"Publish failed: {result}")

    def psacp_receive(self, timeout_ms: int = 0) -> Tuple[int, int, bytes]:
        """
        Receive a published message

        Args:
            timeout_ms: Timeout in milliseconds (0 for non-blocking)

        Returns:
            Tuple of (sender_addr, topic, data)
        """
        msg = PSACPMessage()
        result = _lib.artie_can_psacp_receive(ctypes.byref(self._ctx), ctypes.byref(msg), timeout_ms)
        if result != 0:
            raise ArtieCANException(f"Receive failed: {result}")

        data = bytes(msg.payload[:msg.payload_len])
        return (msg.sender_addr, msg.topic, data)


# ===== Utility Functions =====

def crc16(data: bytes) -> int:
    """Compute CRC16 over data"""
    if _lib is None:
        raise ArtieCANException("Artie CAN library not found")

    data_arr = (ctypes.c_uint8 * len(data))(*data)
    return _lib.artie_can_crc16(data_arr, len(data))


def crc24(data: bytes) -> int:
    """Compute CRC24 over data"""
    if _lib is None:
        raise ArtieCANException("Artie CAN library not found")

    data_arr = (ctypes.c_uint8 * len(data))(*data)
    return _lib.artie_can_crc24(data_arr, len(data))


__all__ = [
    'ArtieCAN',
    'ArtieCANException',
    'BackendType',
    'Priority',
    'RTACPFrameType',
    'RPCACPFrameType',
    'PSACPFrameType',
    'BWACPFrameType',
    'crc16',
    'crc24',
]
