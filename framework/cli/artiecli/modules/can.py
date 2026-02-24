"""
CLI code for CAN bus interfaces.
"""
from .. import common
import argparse
import json

def _cmd_can_send_rtacp(args):
    """Send an RTACP message"""
    try:
        from artie_can import ArtieCAN, BackendType, Priority

        # Determine backend
        backend = BackendType.MOCK if args.mock else BackendType.SOCKETCAN

        # Convert hex string to bytes
        data = bytes.fromhex(args.data) if args.data else b""

        with ArtieCAN(node_address=args.node_address, backend=backend) as can:
            can.rtacp_send(
                target_addr=args.target,
                data=data,
                priority=Priority[args.priority.upper()],
                wait_ack=args.wait_ack
            )
            print(f"✓ Sent RTACP message to 0x{args.target:02X}")
    except Exception as e:
        print(f"✗ Failed to send RTACP message: {e}")
        return 1

    return 0

def _cmd_can_receive_rtacp(args):
    """Receive RTACP messages"""
    try:
        from artie_can import ArtieCAN, BackendType

        # Determine backend
        backend = BackendType.MOCK if args.mock else BackendType.SOCKETCAN

        with ArtieCAN(node_address=args.node_address, backend=backend) as can:
            print(f"Listening for RTACP messages (timeout: {args.timeout}ms)...")

            sender, target, data = can.rtacp_receive(timeout_ms=args.timeout)
            print(f"✓ Received from 0x{sender:02X} to 0x{target:02X}: {data.hex()}")

    except Exception as e:
        print(f"✗ Failed to receive RTACP message: {e}")
        return 1

    return 0

def _cmd_can_publish(args):
    """Publish to a topic"""
    try:
        from artie_can import ArtieCAN, BackendType, Priority

        # Determine backend
        backend = BackendType.MOCK if args.mock else BackendType.SOCKETCAN

        # Convert hex string to bytes
        data = bytes.fromhex(args.data) if args.data else b""

        with ArtieCAN(node_address=args.node_address, backend=backend) as can:
            can.psacp_publish(
                topic=args.topic,
                data=data,
                priority=Priority[args.priority.upper()],
                high_priority=args.high_priority
            )
            print(f"✓ Published to topic 0x{args.topic:02X}")
    except Exception as e:
        print(f"✗ Failed to publish: {e}")
        return 1

    return 0

def _cmd_can_subscribe(args):
    """Subscribe to topics"""
    try:
        from artie_can import ArtieCAN, BackendType

        # Determine backend
        backend = BackendType.MOCK if args.mock else BackendType.SOCKETCAN

        with ArtieCAN(node_address=args.node_address, backend=backend) as can:
            print(f"Listening for published messages (timeout: {args.timeout}ms)...")

            sender, topic, data = can.psacp_receive(timeout_ms=args.timeout)
            print(f"✓ Received from 0x{sender:02X} on topic 0x{topic:02X}: {data.hex()}")

    except Exception as e:
        print(f"✗ Failed to receive message: {e}")
        return 1

    return 0

def _cmd_can_rpc_call(args):
    """Call an RPC"""
    try:
        from artie_can import ArtieCAN, BackendType, Priority

        # Determine backend
        backend = BackendType.MOCK if args.mock else BackendType.SOCKETCAN

        # Convert hex string to bytes
        payload = bytes.fromhex(args.payload) if args.payload else b""

        with ArtieCAN(node_address=args.node_address, backend=backend) as can:
            can.rpcacp_call(
                target_addr=args.target,
                procedure_id=args.procedure_id,
                payload=payload,
                priority=Priority[args.priority.upper()],
                synchronous=args.synchronous
            )
            print(f"✓ Called RPC {args.procedure_id} on node 0x{args.target:02X}")
    except Exception as e:
        print(f"✗ Failed to call RPC: {e}")
        return 1

    return 0

def _cmd_can_info(args):
    """Display CAN bus information"""
    try:
        from artie_can import ArtieCAN, BackendType

        # Try to open CAN interface
        try:
            backend = BackendType.SOCKETCAN
            with ArtieCAN(node_address=args.node_address, backend=backend) as can:
                info = {
                    "backend": "SocketCAN",
                    "node_address": f"0x{args.node_address:02X}",
                    "status": "Connected"
                }
        except:
            info = {
                "backend": "SocketCAN",
                "node_address": f"0x{args.node_address:02X}",
                "status": "Not available"
            }

        print(json.dumps(info, indent=2))
    except Exception as e:
        print(f"✗ Failed to get CAN info: {e}")
        return 1

    return 0

def fill_subparser(parser: argparse.ArgumentParser, parent: argparse.ArgumentParser):
    subparsers = parser.add_subparsers(title="Commands", description="The CAN module's commands")

    # Common options for all CAN commands
    option_parser = argparse.ArgumentParser(parents=[parent], add_help=False)
    group = option_parser.add_argument_group("CAN Module", "CAN Module Options")
    group.add_argument("--node-address", type=lambda x: int(x, 0), default=0x01, help="This node's CAN address (0-63, default: 0x01)")
    group.add_argument("--mock", action="store_true", help="Use mock backend for testing")

    # Info command
    info_parser = subparsers.add_parser("info", parents=[option_parser], help="Display CAN bus information")
    info_parser.set_defaults(cmd=_cmd_can_info)

    # RTACP send command
    rtacp_send_parser = subparsers.add_parser("rtacp-send", parents=[option_parser], help="Send an RTACP message")
    rtacp_send_parser.add_argument("--target", type=lambda x: int(x, 0), required=True, help="Target address (hex, 0x00 for broadcast)")
    rtacp_send_parser.add_argument("--data", type=str, default="", help="Data to send (hex string, max 16 chars/8 bytes)")
    rtacp_send_parser.add_argument("--priority", type=str, default="MED_LOW", choices=["HIGH", "MED_HIGH", "MED_LOW", "LOW"], help="Message priority")
    rtacp_send_parser.add_argument("--wait-ack", action="store_true", help="Wait for ACK (only for targeted messages)")
    rtacp_send_parser.set_defaults(cmd=_cmd_can_send_rtacp)

    # RTACP receive command
    rtacp_recv_parser = subparsers.add_parser("rtacp-receive", parents=[option_parser], help="Receive RTACP messages")
    rtacp_recv_parser.add_argument("--timeout", type=int, default=1000, help="Timeout in milliseconds (default: 1000)")
    rtacp_recv_parser.set_defaults(cmd=_cmd_can_receive_rtacp)

    # Publish command
    publish_parser = subparsers.add_parser("publish", parents=[option_parser], help="Publish to a topic")
    publish_parser.add_argument("--topic", type=lambda x: int(x, 0), required=True, help="Topic ID (hex, 0x0B-0xF4, or 0x00 for broadcast)")
    publish_parser.add_argument("--data", type=str, default="", help="Data to publish (hex string)")
    publish_parser.add_argument("--priority", type=str, default="MED_LOW", choices=["HIGH", "MED_HIGH", "MED_LOW", "LOW"], help="Message priority")
    publish_parser.add_argument("--high-priority", action="store_true", help="Use high priority pub/sub")
    publish_parser.set_defaults(cmd=_cmd_can_publish)

    # Subscribe command
    subscribe_parser = subparsers.add_parser("subscribe", parents=[option_parser], help="Subscribe to topics")
    subscribe_parser.add_argument("--timeout", type=int, default=1000, help="Timeout in milliseconds (default: 1000)")
    subscribe_parser.set_defaults(cmd=_cmd_can_subscribe)

    # RPC call command
    rpc_call_parser = subparsers.add_parser("rpc-call", parents=[option_parser], help="Call an RPC")
    rpc_call_parser.add_argument("--target", type=lambda x: int(x, 0), required=True, help="Target node address (hex, 0x01-0x3F)")
    rpc_call_parser.add_argument("--procedure-id", type=int, required=True, help="RPC procedure ID (0-127)")
    rpc_call_parser.add_argument("--payload", type=str, default="", help="RPC payload (hex string)")
    rpc_call_parser.add_argument("--priority", type=str, default="MED_LOW", choices=["HIGH", "MED_HIGH", "MED_LOW", "LOW"], help="Message priority")
    rpc_call_parser.add_argument("--synchronous", action="store_true", default=True, help="Synchronous RPC (default)")
    rpc_call_parser.add_argument("--asynchronous", action="store_false", dest="synchronous", help="Asynchronous RPC")
    rpc_call_parser.set_defaults(cmd=_cmd_can_rpc_call)
