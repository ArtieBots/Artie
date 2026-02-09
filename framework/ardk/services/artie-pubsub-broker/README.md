# Artie Publisher/Subscriber (Pub/Sub) Broker Design Document

This document describes the design and architecture of the Artie Pub/Sub Broker.

The Pub/Sub Broker differs from the Artie Service Broker in that it is responsible for managing topics and message delivery between
publishers and subscribers, rather than managing service registration and RPC calls. The Pub/Sub Broker will allow services
to publish messages to topics and subscribe to topics to receive messages, enabling a decoupled communication pattern between services.
In Artie's architecture, the publisher/subscriber registration is typically handled by means of interfaces and the Artie Service Broker.
The Pub/Sub Broker is a behind-the-scenes component that programmers should not typically need to interact with directly,
as the Artie Service Client library will provide high-level abstractions for publishing and subscribing to topics (and this
will typically be done for them automatically by making sure their services inherit the appropriate interfaces).

We use Apache Kafka as the underlying technology for our Pub/Sub broker, which is an industry standard messaging system.
The broker will be responsible for managing topics, subscriptions, and message delivery between publishers and subscribers.

## Overview

Core concepts from Kafka that we will be using include:

- **Topics**: A topic is a channel to which messages are published. In our system,
              topics will be associated with specific interfaces.
- **Producers**: Producers are entities that publish messages to topics.
              In our system, producers make use of the [`artie_service_client.pubsub.ArtieStreamPublisher` class](../../libraries/artie-service-client/src/artie_service_client/pubsub.py)
              and publish messages to topics that correspond to the interfaces they implement.
- **Consumers**: Consumers are entities that subscribe to topics and consume messages.
              In our system, consumers make use of the [`artie_service_client.pubsub.ArtieStreamSubscriber` class](../../libraries/artie-service-client/src/artie_service_client/pubsub.py) to consume the messages published to the topics.


## Requirements

- **Topic discovery**: Topics can be discovered at runtime based on the interfaces that services register
  with the Artie Service Broker. When a service registers an interface, the Pub/Sub Broker will automatically create a corresponding topic for that interface. The interfaces are documented in
  [the interface specifications folder](../../../../docs/specifications/service-interfaces/README.md)
  The topic names will typically follow the format `<simple service name>:<interface name>:<sensor id>`
  with optional additional qualifiers for things like different sensor data if the sensor supports multiple data types.
  See the appropriate interface and the service that implements it for complete documentation.
- **Message format**: Messages published to topics will be in a format that corresponds to the data model
  defined by the interface associated with that topic. This will typically be a JSON object.
- **Encryption**: The Pub/Sub Broker will support encryption for messages published to topics.
  Services can choose to encrypt their messages by providing a certificate and key when publishing to a topic,
  and subscribers can decrypt those messages by providing the corresponding certificate when subscribing to the topic.
- **Scalability**: The Pub/Sub Broker should be able to handle a large number of topics, producers, and consumers
  without significant performance degradation. Kafka is designed to be horizontally scalable,
  so we can add more brokers to the cluster as needed to handle increased load.
- **Fault tolerance**: The Pub/Sub Broker should be able to recover from failures without losing messages.
  Kafka provides built-in fault tolerance through replication and partitioning, so we can configure our Kafka cluster
  to ensure that messages are not lost in the event of a broker failure.
- **Latency/Throughput**: Depending on the use case, we may have specific requirements for message latency and throughput.
  Kafka is designed to provide low latency and high throughput, and to have several knobs for tuning one way or the other,
  so we can configure our Kafka cluster to meet these requirements as needed.
- **Idempotency**: We ensure that messages are delivered to subscribers in an idempotent manner,
  meaning that if a message is published multiple times, it will only be processed once by each subscriber.
- **Monitoring and metrics**: The Pub/Sub Broker should provide monitoring and metrics to allow us to track the health and
  performance of the system.
- **Consumer groups**: The Pub/Sub Broker should support consumer groups, which allow multiple consumers to subscribe to the
  same topic and share the workload of processing messages in a load-balanced (perhaps round-robin) manner, while still ensuring that each message is only processed by one consumer in the group.

## Data Format

Topics are typically named according to the interface they are associated with,
and the messages published to those topics will typically be in a format that corresponds to the
data model defined by that interface. This will usually be a JSON object.

## Architecture

Please note the following IPC convention: in Artie, we use RPC to *do* something or to *get* something
if it can be retrieved *synchronously*. If you are going to be doing something in a loop, like reading
a sensor value from some driver again and again, it probably shouldn't be an RPC call. Instead,
a single RPC call might start the sensor read, and then the driver will post the values to a pub/sub topic
that you can read from.
