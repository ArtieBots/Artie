# API for Status LED Interface

This API reference is for the general status LED interface
(see [interfaces/status_led.py](../../../libraries/artie-service-client/src/artie_service_client/interfaces/status_led.py)).

In this documentation, `<service>` is the service making use of this interface,
for example, 'mouth' or 'eyebrows'.

# Version 1

## Update LED State

* *POST*: `/<service>/led`
    * *Parameters*:
        * `state`: One of `on`, `off`, or `heartbeat`
        * `which`: Optional. If given, should be the name of the status LED to update.
    * *Payload*: None

## Get LED State

* *GET*: `/<service>/led`
    * *Parameters*:
        * `which`: Optional. If given, should be the name of the status LED whose status we should get.
* *Response 200*:
    * *Payload (JSON)*:
        ```json
        {
            "state": "on, off, or heartbeat",
            "which": "[Optional] Which status LED. Only present if specified in parameters."
        }
        ```
