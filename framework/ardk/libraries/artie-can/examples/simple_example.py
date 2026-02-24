"""
Simple example demonstrating Artie CAN library usage.

This example shows how to use the mock backend for testing without
actual CAN hardware.
"""

from artie_can import ArtieCAN, BackendType, Priority
import time

def example_rtacp():
    """Example: Real-time messages"""
    print("\n=== RTACP Example ===")

    # Create two CAN nodes
    with ArtieCAN(node_address=0x01, backend=BackendType.MOCK) as can1, \
         ArtieCAN(node_address=0x02, backend=BackendType.MOCK) as can2:

        # Node 1 sends a message to Node 2
        print("Node 0x01 sending: Hello")
        can1.rtacp_send(target_addr=0x02, data=b"Hello", priority=Priority.HIGH)

        # Note: In a real scenario, these would be on different processes/threads
        # For the mock backend, we can directly receive
        print("Would receive on Node 0x02 (requires separate process in real usage)")

def example_pubsub():
    """Example: Pub/Sub messaging"""
    print("\n=== Pub/Sub Example ===")

    with ArtieCAN(node_address=0x01, backend=BackendType.MOCK) as can:
        # Publish to a topic
        print("Publishing sensor data to topic 0x10")
        sensor_data = b"\x01\x02\x03\x04"  # Example sensor reading
        can.psacp_publish(topic=0x10, data=sensor_data, priority=Priority.MED_LOW)
        print("Published successfully")

def example_rpc():
    """Example: Remote Procedure Call"""
    print("\n=== RPC Example ===")

    with ArtieCAN(node_address=0x01, backend=BackendType.MOCK) as can:
        # Call a remote procedure
        print("Calling RPC procedure 5 on node 0x02")
        try:
            can.rpcacp_call(
                target_addr=0x02,
                procedure_id=5,
                payload=b"\x01\x02\x03",
                priority=Priority.MED_HIGH,
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
    print("In production, use BackendType.SOCKETCAN for real CAN hardware.")

    try:
        example_rtacp()
        example_pubsub()
        example_rpc()

        print("\n" + "=" * 40)
        print("Examples completed!")
        print("\nFor real hardware usage:")
        print("1. Ensure CAN interface is configured (e.g., can0)")
        print("2. Use BackendType.SOCKETCAN instead of BackendType.MOCK")
        print("3. Run sender and receiver in separate processes")

    except Exception as e:
        print(f"\nError running examples: {e}")
        print("\nNote: The artie-can library may not be installed.")
        print("Install with: pip install -e .")
        return 1

    return 0

if __name__ == "__main__":
    exit(main())
