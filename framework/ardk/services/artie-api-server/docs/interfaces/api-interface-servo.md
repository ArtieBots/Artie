# API for the Servo Interface

This API reference is for the general Servo interface
(see [interfaces/servo.py](../../../../libraries/artie-service-client/src/artie_service_client/interfaces/servo.py)).

In this documentation, `<service>` is the service making use of this interface,
for example, 'mouth' or 'eyebrows'.

# Version 1

## List Servos

List the servos that a service is responsible for.

* *GET*: `/<service>/servo/list`
    * *Query Parameters*: None
    * *Payload*: None
* *Response 200*:
    * *Payload (JSON)*:
        ```json
        {
            "servo-names":
                [
                    "<NAME1>",
                    "<NAME2>",
                ]
        }
        ```

## Set Servo Position

* *POST*: `/<service>/servo/position`
    * *Query Parameters*:
        * `which`: The servo to set the position of; this is service-dependent.
    * *Payload*:
        ```json
        {
            "position": "<allowed values specified by service>"
        }
        ```

## Get Servo Position

Note: for servos that do not have encoders, this value is only an approximation
of the current position based on the last command sent to the servo and/or other
algorithms used by the driver.

* *GET*: `/<service>/servo/position`
    * *Query Parameters*: None
* *Response 200*:
    * *Payload (JSON)*:
        ```json
        {
            "which": "<servo name>",
            "position": "<allowed values specified by service>"
        }
        ```
