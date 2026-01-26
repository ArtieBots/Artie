# API for the MCU Interface

This API reference is for the general MCU interface (see [interfaces/status_led.py](../../../libraries/artie-service-client/src/artie_service_client/interfaces/mcu.py)).

In this documentation, `<service>` is the service making use of this interface,
for example, 'mouth' or 'eyebrows'.

# Version 1

## Reload Firmware

Reload MCU firmware for the given MCU.

* *POST*: `/<service>/mcu_reload_fw`
    * *Parameters*:
        * `artie-id`: The Artie ID.
    * *Payload*: None

# TODO: Do the rest of this document based on the mcu.py interface
