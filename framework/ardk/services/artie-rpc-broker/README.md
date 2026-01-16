# Artie RPC Broker Design Document

Artie RPC Broker is a microservice that facilitates communication between various Artie microservices using Remote Procedure Calls (RPC). This document outlines the design and architecture of the Artie RPC Broker.

## Overview

The Artie RPC Broker is built using the [RPyC library](https://rpyc.readthedocs.io/en/latest/),
which allows for transparent remote procedure calls between microservices. The broker acts as an intermediary
that routes requests from clients to the appropriate service handlers, decoupling service implementations from their consumers.

## Requirements

- **Service discovery**: Services can be discovered at runtime based on their capabilities. Each service
  registers a list of interface names with this broker, and each interface name corresponds to a documented
  interface in [the interface specifications folder](../../../../docs/specifications/service-interfaces/README.md)

## Architecture
