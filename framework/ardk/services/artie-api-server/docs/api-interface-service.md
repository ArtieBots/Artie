# API for the Service Interface

This API reference is for the general service interface
(see [interfaces/service.py](../../../libraries/artie-service-client/src/artie_service_client/interfaces/service.py)).

In this documentation, `<service>` is the service making use of this interface,
for example, 'mouth' or 'eyebrows'.

# Version 1

## Whoami

Gets the service's human-friendly name and its git hash.

* *GET*: `/<service>/whoami`
    * *Parameters*: None
* *Response 200*:
    * *Payload (JSON)*:
        ```json
        {
            "name": "Human-friendly name of the service.",
            "git-hash": "The git hash of the service."
        }
        ```
