# Artie API Server Design Document

TODO: This document will describe the design and architecture of the Artie API Server.

## Requirements

Artie API Server's purpose is to provide a single point of interface to the Artie cluster.
This makes it easy to create applications that can communicate with and control an Artie,
by having a single well-documented API for everything.

As such, here are the requirements:

* Single point of ingress/egress to/from the cluster: this is the only point of ingress/egress.
* Well-documented: the API should be kept up-to-date and be well-documented.
* Expose all public RPC methods via HTTPS.
* Expose all public pub/sub topics by means of an encrypted streaming interface.
* Expose all public audio/video streams by means of an encrypted streaming interface.
* Stateless: the server must be ephemeral; it should not matter how many instances of the
  server are running or which one the client connects to.

## Design

Single point of ingress/egress: TODO: figure out how to enforce Kubernetes networking rules
to make this happen. Also include autoscaling and load balancing.

Documentation: Provide an interface translation for each service in the framework by default,
but what about ecosystem drivers/services? Probably the API should be kept with the service/driver's
code.

The build system will then need to combine all the pieces into the image. Anything not running
on the actual Artie system we are deployed to will simply return a 404 when accessed.
