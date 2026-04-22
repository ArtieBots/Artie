# Artie CAN Library

The Artie CAN Library is a C/C++ and Python library for sending and receiving Artie CAN protocol frames.
For gritty details on the various forms of the Artie CAN protocol,
see [the Artie CAN protocol document](../../../../docs/specifications/CANProtocol.md). That document
describes the protocol in detail. This document on the other hand, describes the software library
that implements the protocol.

## General Architecture

This library is meant to run both on an OS and in a bare-metal embedded context, so it is
entirely heapless (except for the TCP backend, which is used in testing). This means that
structs used by the library are managed by the caller and their lifetime must last the entire
lifetime of the library (until deinitialization, if ever). This is called out in any API documentation
as appropriate.

The general architecture of the library is this:

* Lowest level: backend (transport layer) - the library is meant to work over CAN bus,
  but for testing, using sockets with TCP is awfully convenient. Additionally, every device
  has a different way to interact with a CAN bus. Some microcontrollers have a CAN peripheral,
  while others make use of an external SPI to CAN translator (such as the ubiquitous MCP 2515).
  Therefore, we provide a couple of backends that we use in Artie reference implementations,
  and custom backends are easy enough to supply to the library.
* Next layer up: protocol state machines - the Artie CAN protocol is really several different
  protocols that are all meant to be run on the same CAN bus without interfering with one another.
  This means that a single CAN node could be listening for messages that make use of different
  protocols while also sending messages over yet another protocol. Each sub protocol needs its
  own state machine.
* Highest layer: API - the Artie CAN library is quite complicated under the hood, but it is meant
  to be very easy to use. The API layer provides a small set of useful functions that should enable
  pretty much any use case the Artie CAN protocol allows.

## API

The C API documentation is generated as part of a release and can be found (TODO - where?).

The gist of it is that you configure a context struct with the various options for the various
versions of the protocol you are interested in - for example, if you want to make use of the RTACP
(real-time Artie CAN protocol) and the RPCACP (remote procedure call Artie CAN protocol),
you would configure the context struct with the various options required by those protocols,
such as the functions that are exposed over RPC.

Next, you configure a handle struct, which consumes the context struct and is where you choose
the backend implementation (the transport layer) - how do we get the frames out onto the CAN bus?

You then call the artie_can_init function with the configured handle to finalize initialization
of the library. After that, frames will come in asynchronously when received, which will trigger
a callback that you supplied during the initialization steps. Sending frames on the other hand,
is synchronous.

## Integrating

To integrate the Artie CAN Library into an application, there are several ways to do it depending
on the programming language and the hardware.

TODO
