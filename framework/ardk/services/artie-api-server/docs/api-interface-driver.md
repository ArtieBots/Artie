# API for Driver Interface

This API reference is for the general driver interface (see [interfaces/status_led.py](../../../libraries/artie-service-client/src/artie_service_client/interfaces/driver.py)).

In this documentation, `<service>` is the service making use of this interface,
for example, 'mouth' or 'eyebrows'.

# Version 1

## Get Status

Get the service's submodules' statuses.

* *GET*: `/<service>/status`
    * *Parameters*:
        * `artie-id`: The Artie ID.
* *Response 200*:
    * *Payload (JSON)*:
        ```json
        {
            "artie-id": "The Artie ID.",
            "submodule-statuses":
                {
                    "<SUBMODULE NAME>": "<Status>"
                }
        }
        ```
    where `<Status>` is one of the available
    status values as [found in the top-level API README](../README.md#statuses)

## Self Test

Initiate a self-test.

* *POST*: `/eyebrows/self-test`
    * *Parameters*:
        * `artie-id`: The Artie ID.
    * *Payload*: None
