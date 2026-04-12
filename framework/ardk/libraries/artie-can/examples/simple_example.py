"""
Simple example demonstrating Artie CAN library usage.

This example shows how to use the mock backend for testing without
actual CAN hardware.
"""

import artie_can
import time

def example_rtacp():
    """Example: Real-time messages"""
    print("\n=== RTACP Example ===")

    # Create two CAN nodes
    with artie_can.ArtieCAN(node_address=0x01, backend=artie_can.BackendType.MOCK_DEADEND) as can1, \
         artie_can.ArtieCAN(node_address=0x02, backend=artie_can.BackendType.MOCK_DEADEND) as can2:

        # Node 1 sends a message to Node 2
        print("Node 0x01 sending: Hello")
        can1.rtacp_send(target_addr=0x02, data=b"Hello", priority=artie_can.Priority.HIGH)

        # Note: In a real scenario, these would be on different processes/threads
        # For the mock backend, we can directly receive
        print("Would receive on Node 0x02 (requires separate process in real usage)")

def example_pubsub():
    """Example: Pub/Sub messaging"""
    print("\n=== Pub/Sub Example ===")

    with artie_can.ArtieCAN(node_address=0x01, backend=artie_can.BackendType.MOCK_DEADEND) as can:
        # Publish to a topic
        print("Publishing sensor data to topic 0x10")
        sensor_data = b"\x01\x02\x03\x04"  # Example sensor reading
        can.psacp_publish(topic=0x10, data=sensor_data, priority=artie_can.Priority.MED_LOW)
        print("Published successfully")

def example_rpc():
    """Example: Remote Procedure Call"""
    print("\n=== RPC Example ===")

    with artie_can.ArtieCAN(node_address=0x01, backend=artie_can.BackendType.MOCK_DEADEND) as can:
        # Call a remote procedure
        print("Calling RPC procedure 5 on node 0x02")
        try:
            can.rpcacp_call(
                target_addr=0x02,
                procedure_id=5,
                payload=b"\x01\x02\x03",
                priority=artie_can.Priority.MED_HIGH,
                synchronous=True
            )
            print("RPC call sent (would wait for response in real scenario)")
        except Exception as e:
            print(f"RPC call note: {e}")
            print("(This is expected with mock backend without a responder)")

def main():
    """Run all examples"""
    print("Artie CAN Library Examples")
    print("=" * 40)
    print("\nThese examples use the MOCK backend for demonstration.")
    print("In production, use artie_can.BackendType.SOCKETCAN for real CAN hardware.")

    try:
        example_rtacp()
        example_pubsub()
        example_rpc()

        print("\n" + "=" * 40)
        print("Examples completed!")
        print("\nFor real hardware usage:")
        print("1. Ensure CAN interface is configured (e.g., can0)")
        print("2. Use artie_can.BackendType.SOCKETCAN instead of artie_can.BackendType.MOCK_DEADEND")
        print("3. Run sender and receiver in separate processes")

    except Exception as e:
        print(f"\nError running examples: {e}")
        print("\nNote: The artie-can library may not be installed.")
        print("Install with: pip install -e .")
        return 1

    return 0

if __name__ == "__main__":
    exit(main())
