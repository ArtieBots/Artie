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
* Secure: all communications must be encrypted and authenticated.

## Design

Single point of ingress/egress: the default Helm Chart prevents all egress/ingress from/to the cluster
except for the Artie API Server.

For anything running *inside* the cluster, Artie Service Client should be used directly instead.
This library can be found here: [artie-service-client](../../libraries/artie-service-client/README.md).

An Artie API Server exists for each Artie on a cluster. Arties are namespaced in the K3S cluster
to separate them, and each one has a deployment of microservices.

Encryption/authentication is handled via TLS. Inside the cluster, communication is encrypted via
self-signed certificates. However, at the cluster edge, a proper TLS certificate must be used.
However, we don't want to use a certificate authority (CA), since that would require paying for
a CA and managing it. Instead, during installation, the controller node acts as a CA and generates
a certificate for the Artie API Server. This cert is deployed to the cluster as a Kubernetes secret,
and is used by the Artie API Server for TLS. Clients must trust the controller node's CA cert to communicate
with the Artie API Server. This provides a good balance of security and ease-of-use, since it doesn't
require paying for a CA, but still provides encryption, plus authentication using a shared secret
that other parties shouldn't be able to get to (as it comes from the physical controller node).

## Common Objects

### MCU IDs

MCU IDs must match the names found in the Artie HW file (e.g., [Artie00](../../../../artie00/artie00.yml)).

### SBC IDs

SBC IDs must match the names found in the Artie HW file (e.g., [Artie00](../../../../artie00/artie00.yml)).

### Statuses

These are possible values for the submodule status reports:

* `working`: The module or submodule is in a healthy state.
* `degraded`: The module or submodule is operating in a limited capacity.
* `not working`: The module or submodule is known to be non-functional at this time.
* `unknown`: The module or submodule's status is not currently known.

### Common Responses

* *Response 200* or *Response 201*: The request has been successfully delivered to the appropriate cluster resource
  and as far as we can tell, it worked.
    * *Payload (JSON)*:
        ```json
        {
        }
        ```
    The payload may include additional information; generally it will include all supplied parameters.
* *Response 400*: This is given if the request contains problems such as missing query parameters or an invalid argument value.
    * *Payload (JSON)*:
        ```json
        {
            "error": "A description of the error."
        }
        ```
    The payload may include additional information; generally it will include all supplied parameters.
* *Response 504*: The Artie API server has sent the request onto the appropriate cluster resource,
  but has not received a response in a reasonable amount of time. You can try again, but it probably
  means the resource is overloaded or down.
    * *Payload (JSON)*:
        ```json
        {
        }
        ```
    The payload may include additional information; generally it will include all supplied parameters.
