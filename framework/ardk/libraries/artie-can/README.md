# Artie CAN Library

This document describes the Artie CAN library, which provides an interface for communication over the
Controller Area Network (CAN) bus used in Artie robots.

It also describes the mechanical and electrical design of the CAN bus system.

The protocol that we use for communication over CAN is described in the
[Artie CAN Protocol Specification](../../../../docs/specifications/CANProtocol.md).

## Library Overview

The Artie CAN library provides an API for serializing and deserializing messages
using the [Artie CAN Protocol](../../../../docs/specifications/CANProtocol.md).

This library is written in C but also provides a Python wrapper for ARM64 platforms.

In addition to the serialization and deserialization functions, the library also provides functions for
initializing the CAN bus, sending messages, and receiving messages. It provides an ARM64 backend,
a bare-metal backend, and a mock backend for testing. In all cases, no dynamic memory allocation is used.
In the ARM64 backend, the library uses the SocketCAN interface to communicate with the CAN bus. In the bare-metal
backend, the library assumes an MCP2515 CAN controller is used and provides functions for initializing the controller
and sending/receiving messages. We also provide a call-back interface for registering
custom backends, which can be used to support other CAN controllers or platforms.

Raspberry Pi devices will need something like the following in their config.txt files:

`dtoverlay=mcp2515-can0,oscillator=16000000,interrupt=25`

which says to enable the MCP2515 CAN controller on the first CAN interface (can0) with a 16 MHz oscillator
and an interrupt on GPIO 25.

## Mechanical and Electrical Design

TODO
