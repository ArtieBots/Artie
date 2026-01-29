# API for the MCU Interface

This API reference is for the general MCU interface
(see [interfaces/mcu.py](../../../../libraries/artie-service-client/src/artie_service_client/interfaces/mcu.py)).

In this documentation, `<service>` is the service making use of this interface,
for example, 'mouth' or 'eyebrows'.

# Version 1

## Reload Firmware

Reload MCU firmware for the given MCU.

* *POST*: `/<service>/mcu_reload_fw`
    * *Query Parameters*:
        * `mcu-id`: The name of the MCU to target (determined by service).
    * *Payload*: None

## List MCUs

List the MCUs that a service is responsible for.

* *GET*: `/<service>/mcu_list`
    * *Query Parameters*: None
    * *Payload*: None
* *Response 200*:
    * *Payload (JSON)*:
        ```json
        {
            "mcu-names":
                [
                    "<NAME1>",
                    "<NAME2>",
                ]
        }
        ```

## Reset MCU

Reset the given MCU.

* *POST*: `/<service>/mcu_reset`
    * *Query Parameters*:
        * `mcu-id`: The name of the MCU to reset (determined by service).
    * *Payload*: None

## MCU Self Check

Run a self diagnostics check on the given MCU and set submodule statuses appropriately.

* *POST*: `/<service>/mcu_self_check`
    * *Query Parameters*:
        * `mcu-id`: The name of the MCU to run diagnostics on (determined by service).
    * *Payload*: None

## MCU Status

Get the current status of the given MCU.

* *GET*: `/<service>/mcu_status`
    * *Query Parameters*:
        * `mcu-id`: The name of the MCU to get status for (determined by service).
    * *Payload*: None
* *Response 200*:
    * *Payload (JSON)*:
        ```json
        {
            "status": "<STATUS>",
            "mcu-id": "<MCU-ID>"
        }
        ```
    * Note: `<STATUS>` should be one of the enum values from [artie_util.constants.SubmoduleStatuses](../../../../libraries/artie-util/src/artie_util/constants.py).

## MCU Version

Get the firmware version information for the given MCU.

* *GET*: `/<service>/mcu_version`
    * *Query Parameters*:
        * `mcu-id`: The name of the MCU to get version information for (determined by service).
    * *Payload*: None
* *Response 200*:
    * *Payload (JSON)*:
        ```json
        {
            "version": "<VERSION>",
            "mcu-id": "<MCU-ID>"
        }
        ```
