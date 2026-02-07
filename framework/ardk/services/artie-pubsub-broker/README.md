# Artie Publisher/Subscriber (Pub/Sub) Broker Design Document

This document describes the design and architecture of the Artie Pub/Sub Broker.

We use Apache Kafka as the underlying technology for our Pub/Sub broker, which is an industry standard messaging system.
The broker will be responsible for managing topics, subscriptions, and message delivery between publishers and subscribers.

## Overview

Core concepts from Kafka that we will be using include:

- **Topics**: A topic is a category or feed name to which messages are published. In our system, topics will be associated with specific interfaces that services can publish to or subscribe from.
- **Producers**: Producers are entities that publish messages to topics. In our system, services that generate data or events will act as producers.
- **Consumers**: Consumers are entities that subscribe to topics and consume messages. In our system, services that need to react to data or events will act as consumers.

## Requirements

- **Topic discovery**: Topics can be discovered at runtime based on the interfaces that services register
  with the Artie Service Broker. When a service registers an interface, the Pub/Sub Broker will automatically create a corresponding topic for that interface. The interfaces are documented in
  [the interface specifications folder](../../../../docs/specifications/service-interfaces/README.md)
  The topic names will typically follow the format `<simple service name>:<interface name>:<sensor id>`
  with optional additional qualifiers for things like different sensor data if the sensor supports multiple data types.
  See the appropriate interface and the service that implements it for complete documentation.

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
