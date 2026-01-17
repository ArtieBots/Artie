# Artie Publisher/Subscriber (Pub/Sub) Broker Design Document

TODO: This document will describe the design and architecture of the Artie Pub/Sub Broker.

## Overview

TODO

## Requirements

- **Topic discovery**: Topics can be discovered at runtime based on their interfaces. Each service
  registers a list of interface names with this broker, and each interface name corresponds to a documented
  interface in [the interface specifications folder](../../../../docs/specifications/service-interfaces/README.md)

## Architecture

Please note the following IPC convention: in Artie, we use RPC to *do* something or to *get* something
if it can be retrieved *synchronously*. If you are going to be doing something in a loop, like reading
a sensor value from some driver again and again, it probably shouldn't be an RPC call. Instead,
a single RPC call might start the sensor read, and then the driver will post the values to a pub/sub topic
that you can read from.
