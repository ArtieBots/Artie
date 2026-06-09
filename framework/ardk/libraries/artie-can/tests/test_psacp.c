// Setup:
//
// * 4 nodes
// * node 1: Subscribes to topic 0x10
// * node 2: Subscribes to topics 0x10, 0x0C
// * node 3: Subscribes to topics 0x0C, 0x23
// * node 4: Subscribes to topics 0x0C, 0x23, 0x45
//
// Tests:
//
// * Test 1: Node 1 sends a single byte to topic 0x0C
//   - Nodes 2 and 3 receive
//   - Nodes 1 and 4 do not
// * Test 2: Node 1 sends a single byte to broadcast (topic 0x00)
//   - All nodes receive (including the sender - node 1 - note that this means
//     that a sending node must check an outgoing message to see if it should "receive"
//     it)
// * Test 3: Node 1 sends a single byte to topic 0x10
//   - Nodes 1 and 2 receive
//   - Nodes 3 and 4 do not
// * Test 4: Node 1 begins a BWACP transaction to node 4.
//   While the transaction is going on, Node 3 publishes a byte to topic 0x45 at high priority pub/sub.
//   BWACP transaction should complete correctly and the byte should have been transferred as well.
// * Test 5: Node 1 begins a BWACP transaction to node 4.
//   While the transaction is going on, Node 3 spams the bus with low priority pub/sub messages,
//   to simulate logging while a BWACP transfer is going on. The BWACP transaction should complete
//   successfully. We don't bother checking the received bytes of pub/sub in this test - we are just
//   attempting to ensure that the BWACP transfer is unaffected by the low-priority pub/sub noise
//   on the bus.
// * Test 6: Node 1 sends a single byte to topic 0x0C
//   - Nodes 2 and 3 receive
//   - Nodes 1 and 4 do not
//   Now node 2 unsubscribes from topic 0x0C, and Node 1 sends again
//   - This time, node 3 receives and all others do not
