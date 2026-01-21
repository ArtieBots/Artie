# Artie RPC Broker Design Document

Artie RPC Broker is a microservice that facilitates communication between various Artie microservices using Remote Procedure Calls (RPC). This document outlines the design and architecture of the Artie RPC Broker.

## Overview

The Artie RPC Broker is built using the [RPyC library](https://rpyc.readthedocs.io/en/latest/),
which allows for transparent remote procedure calls between microservices. The broker acts as an intermediary
that routes requests from clients to the appropriate service handlers, decoupling service implementations from their consumers.

## Requirements

- **Service discovery**: Services can be discovered at runtime based on their capabilities.
  All Artie RPC services are derived from the `ArtieRPCService` class found in
  [artie_service_client/artie_service.py](../../libraries/artie-service-client/src/artie_service_client/artie_service.py)
  and are further inherited from mixins defined in [artie_service_client/interfaces](../../libraries/artie-service-client/src/artie_service_client/interfaces/service.py). The `ArtieRPCService` class provides methods
  to register with the Artie RPC Broker. Clients can then query the broker, either by service name
  or by interface signature (e.g., "I am looking for a service that is an 'led-driver' and an 'accelerometer-driver').

## Architecture

Please note the following IPC convention: in Artie, we use RPC to *do* something or to *get* something
if it can be retrieved *synchronously*. If you are going to be doing something in a loop, like reading
a sensor value from some driver again and again, it probably shouldn't be an RPC call. Instead,
a single RPC call might start the sensor read, and then the driver will post the values to a pub/sub topic
that you can read from.

TODO: Here are some notes to compile into a fully-fleshed out document later:

* We have an RPC Broker that runs as a service
* The RPC broker's hostname and port are mapped into every service container by the base Helm Chart as
  environment variables.
* Every RPC service is derived from ArtieRPCService, which handles registering with the broker
  by means of RPyC's registry code.
* Each RPC service has a service name, which is the human readable name, such as "MouthDriver",
  but it also has a fully-qualified name, which lists all its interfaces as well, and conforms
  to the following format: `<service-name>:<interface1-name>:<interface2-name>`
* Interface names MUST end in '-interface-vX', where X is the version number, starting with 1.
* Need to figure out how to have multiple Arties in the same Kubernetes network. Right now
  I think the idea is we just append the Artie ID to each service name.
