# API for Logging Module

## Common Parameters

Most logging API methods require the following parameters:

* `level`: Must be one of:
  - `"DEBUG"`: All logs are returned.
  - `"INFO"`: All logs except for DEBUG logs are returned.
  - `"WARNING"`: All logs except DEBUG and INFO are returned.
  - `"ERROR"`: All logs except DEBUG, INFO, and WARNING are returned.
  - `"UNKNOWN"`: Logs are given this tag when their log level name is unable to be processed for some reason.
* `process`: The name of the generating process. Can be '*' for all. Might be 'Unknown' when returned.
* `thread`: The name of the generating thread. Can be '*' for all. Might be 'Unknown' when returned.
* `service`: The name of the generating Artie service. Can be '*' for all.

## Get Recent Logs

Get logs from the last N seconds.

Note that when N is small enough, this can give the feeling of 'live'
logs, but this is not the preferred way of streaming logs.

TODO: We have a REST server for ingress/egress to/from the cluster,
      which is useful for almost everything we could want to do with Artie.
      However, live streaming of telemetry and sensor data (including
      video and audio, which must be encrypted) is not feasible over a REST
      server. Instead, we need to determine a way to egress realtime data
      appropriately.

* *GET*: `/logs/recent`
    * *Query Parameters*:
        * `seconds`: Integer value. We get logs from the last this many seconds.
        * `level`: (Optional) Only return logs of this level or higher in importance. See [Common Parameters](#common-parameters).
        * `process`: (Optional) Only return logs coming from the given process. See [Common Parameters](#common-parameters).
        * `thread`: (Optional) Only return logs coming from the given thread. See [Common Parameters](#common-parameters).
        * `service`: (Optional) Only return logs coming from the given Artie service. See [Common Parameters](#common-parameters).
* *Response 200*:
    * *Payload (JSON)*:
        ```json
        {
            "seconds": "Integer. The number of seconds queried",
            "level": "[Optional] Log level. See Common Parameters",
            "process": "[Optional] The process name. See Common Parameters",
            "thread": "[Optional] The thread name. See Common Parameters",
            "service": "[Optional] The Artie service. See Common Parameters",
            "logs": [
                {
                    "level": "Log level. See Common Parameters",
                    "message": "The actual log message.",
                    "processname": "The name of the process.",
                    "threadname": "The name of the thread.",
                    "timestamp": "Timestamp in artie logging's date format",
                    "servicename": "The Artie service.",
                }
            ]
        }
        ```

## Query Logs

Get a list of logs queried by means of a set of parameters.

* *GET*: `/logs/query`
    * *Query Parameters*:
        * `limit`: (Optional) An integer maximum number of logs to receive.
        * `starttime`: (Optional) Receive logs generated after this time. Format is artie logging's date format.
        * `endtime`: (Optional) Receive logs generated before this time. Format is artie logging's date format.
        * `messagecontains`: (Optional) A Python regular expression that must match any message returned.
        * `level`: (Optional) Only return logs of this level or higher in importance. See [Common Parameters](#common-parameters).
        * `process`: (Optional) Only return logs coming from the given process. See [Common Parameters](#common-parameters).
        * `thread`: (Optional) Only return logs coming from the given thread. See [Common Parameters](#common-parameters).
        * `service`: (Optional) Only return logs coming from the given Artie service. See [Common Parameters](#common-parameters).
* *Response 200*:
    * *Payload (JSON)*:
        ```json
        {
            "limit": "[Optional] Integer. The maximum number of logs requested.",
            "starttime": "[Optional] Start time. See parameters.",
            "endtime": "[Optional] End time. See parameters.",
            "messagecontains": "[Optional] The message regex. See parameters.",
            "level": "[Optional] Log level. See Common Parameters",
            "process": "[Optional] The process name. See Common Parameters",
            "thread": "[Optional] The thread name. See Common Parameters",
            "service": "[Optional] The Artie service. See Common Parameters",
            "logs": [
                {
                    "level": "Log level. See Common Parameters",
                    "message": "The actual log message.",
                    "processname": "The name of the process.",
                    "threadname": "The name of the thread.",
                    "timestamp": "Timestamp in artie logging's date format",
                    "servicename": "The Artie service.",
                }
            ]
        }
        ```

## List Services

List the logging services that are available. Values returned
from this request are valid for inputs as `service` into the parameters
of other requests in this API.

* *GET*: `/logs/services`
    * *Query Parameters*: None
* *Response 200*:
    * *Payload (JSON)*:
        ```json
        {
            "services": [
                "service1",
                "service2",
                "etc."
            ]
        }
        ```
