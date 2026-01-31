# Library Contribution Guide

[Back to Driver Contributions](./driver-contributions.md) | [Forward to Artie CLI Contributions](./artie-cli-contributions.md)

This document provides guidelines and best practices for contributing
library code to the Artie Project.

## Overview of the Libraries

Libraries are software libraries that might be used by more than one Artie
component. They come in a few flavors:

* Firmware Libraries: These live in `framework/ardk/firmware/libraries/` and are libraries for use in the firmware.
* Artie Component Libraries: These live in `framework/ardk/libraries/` and are libraries for use in
  Artie microservices (Docker images).
* Artie Infrastructure Libraries: These also live in `framework/ardk/libraries/` and are used
  by more than one Artie infrastructure component, such
  as Artie Tool and Artie Workbench.

## Building the Libraries

Building application libraries (as opposed to firmware libraries)
is typically done inside the Artie Base Image, which is
a Docker image [found here](../../framework/ardk/base-image/Dockerfile).
The base image is pulled into the other build tasks by means
of their Dockerfiles as needed.

So if you add a new library, you will likely add it to the
Artie Base Image and then rebuild your target with Artie Tool,
which will detect the change and rebuild the dependency chain.

## Interfaces

Please note that all the Artie Services use well-defined interfaces
to communicate with each other. If you are adding a new interface,
you must add the following:

* An interface definition in `framework/ardk/libraries/artie-service-client/src/artie_service_client/interfaces/`
* An API server definition in `framework/ardk/services/artie-api-server/docs/api-interface-<your-interface>.md`
* A client implementation in `framework/ardk/libraries/artie-tooling/src/artie_tooling/api_clients/<your_interface>_client.py`
* A CLI module in `framework/cli/artiecli/modules/<your_interface>.py`
* A server implementation in the service that provides the interface.
* Tests for both client and server implementations.

[Back to Driver Contributions](./driver-contributions.md) | [Forward to Artie CLI Contributions](./artie-cli-contributions.md)
