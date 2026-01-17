# Error Handling Specification

This document describes how errors are handled at each layer in the Artie
system.

## Firmware Error Handling

Errors detected in firmware fall into a few categories:

- **System Critical Errors**: These errors are unrecoverable system-level
  errors, such as a bus fault or a watchdog timer expiration. These cannot
  be handled other than by a reset and they typically cannot be logged
  before the reset. If possible, information pertaining to the reset
  should be inspected on power up and logged to CAN bus using
  the [CAN Bus Error Specification](#can-bus-error-specification)
  at the critical level.
  See [this excellent article on debugging hard faults in ARM Cortex chips](https://interrupt.memfault.com/blog/cortex-m-hardfault-debug).
- **Error**: These are errors that are recoverable, but probably signify
  that something is very wrong, for example, the flash chip we are
  expecting to be on the other end of the SPI bus is not responding.
  We can continue, but performance is likely to be significantly degraded.
  In this case, the firmware should log the error using the
  [CAN Bus Error Specification](#can-bus-error-specification),
  at the error level,
  making note of the submodule that has stopped working.
  If the firmware's status is queried by a driver application,
  it should report this information there as well.
- **Warning**: These are errors that seem suspicious but do not
  necessarily mean something has gone wrong. Perhaps something
  took longer to respond than is typical, or we only got half the
  data we were expecting from the last sensor read. These should
  be logged using the [CAN Bus Error Specification](#can-bus-error-specification)
  at the warning level. This does not have to be reported
  in the next status query, as we expect it is probably transient.

In general, firmware should **always** have a watchdog timer enabled when
in release mode.

## Yocto System Error Handling

Single board computers (SBCs) running Yocto should come equipped with
Prometheus hardware scrapers to alert Artie Workbench when something
has gone wrong with them. This is handled by the telemetry stack
and is therfore documented [in the telemetry SDD](../../framework/ardk/services/telemetry/README.md).

## Service Error Handling

Microservices run in Docker containers deployed through Kubernetes
and therefore have some error handling built-in, at least by means of restarting
a crashed application. Nevertheless, they should conform to the following
error handling principles:

* Exceptions should be caught as soon as feasible, then
  information about the exception should be collected and logged using
  Artie logging service.
* Exceptions should never crash the application.
* Instead, if an exception is so fundamental to a service that the service
  cannot do any part of its job, it should at the very least be able to print
  something to the console.

## CAN Bus Error Specification

TODO: Logging errors to CAN bus (what does the error look like and who
is listening for them?)

## Top-level Error Handling

Throughout the Artie stack, errors are handled as appropriately as possible,
then information about them is gathered and migrated upwards, eventually
finding its way to the Artie telemetry services. The telemetry services
log the errors and store them for retention in the same way that they log
and store any other telemetry. See [the telemetry SDD for details](../../framework/ardk/services/telemetry/README.md).

The errors can be viewed by means of Artie Workbench, which should make it
abundantly clear when exceptional behavior is taking place in the system.
From there, the user can determine how best to proceed.
