# API for Driver Interface

This API reference is for the general driver interface
(see [interfaces/driver.py](../../../../libraries/artie-service-client/src/artie_service_client/interfaces/driver.py)).

In this documentation, `<service>` is the service making use of this interface,
for example, 'mouth' or 'eyebrows'.

# Version 1

## Get Status

Get the service's submodules' statuses.

* *GET*: `/<service>/status`
    * *Query Parameters*: None
* *Response 200*:
    * *Payload (JSON)*:
        ```json
        {
            "submodule-statuses":
                {
                    "<SUBMODULE NAME>": "<Status>"
                }
        }
        ```
    where `<Status>` is one of the available
    status values as [found in the top-level API README](../../README.md#statuses)
    and `<SUBMODULE NAME>` varies by service. See the service's API documentation
    for details.

## Self Test

Initiate a self-test. Issue a `status` request to get the results.

* *POST*: `/<service>/self-test`
    * *Query Parameters*: None
    * *Payload*: None
