# API for the Display Interface

This API reference is for the general display interface
(see [interfaces/display.py](../../../../libraries/artie-service-client/src/artie_service_client/interfaces/display.py)).

In this documentation, `<service>` is the service making use of this interface,
for example, 'mouth' or 'eyebrows'.

# Version 1

## List Displays

List the displays that a service is responsible for.

* *GET*: `/<service>/display/list`
    * *Query Parameters*: None
    * *Payload*: None
* *Response 200*:
    * *Payload (JSON)*:
        ```json
        {
            "displays":
                [
                    "<DISPLAY1>",
                    "<DISPLAY2>",
                ]
        }
        ```

## Set Display

Set the display to show the given state, first powering it on if necessary.

* *POST*: `/<service>/display/contents`
    * *Query Parameters*:
        * `which`: The specific display to target; determined by service.
    * *Payload (JSON)*:
        ```json
        {
            "display": "base-64-encoded binary whose contents are determined by service"
        }
        ```

## Get Display

* *GET*: `/<service>/display/contents`
    * *Query Parameters*:
        * `which`: The specific display to target; determined by service).
* *Response 200*:
    * *Payload (JSON)*:
        ```json
        {
            "which": "<determined by service>",
            "display": "current display mode; will be base64-encoded binary whose contents are determined by service, but wll include the following possible values: test, clear, error"
        }
        ```

## Test a Display's Hardware

Draw a test image on the display or do some other hardware test to determine if it is functioning properly.

* *POST*: `/<service>/display/test`
    * *Query Parameters*:
        * `which`: The specific display to target; determined by service.
    * *Payload*: None

## Clear a Display

Erase the contents on a display, potentially powering it down.

* *POST*: `/<service>/display/off`
    * *Query Parameters*:
        * `which`: The specific display to target; determined by service.
    * *Payload*: None
