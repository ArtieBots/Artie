# API for Eyebrow Module

Eyebrows states are given as a tuple of three items per eyebrow: L/M/H x3.

For example:

* HHH: A line that is raised
* MHM: A line that looks like a ^
* LHL: Same as MHM, but sharper (edges are lower)
* LLL: A line that is low
* MMM: A line across the middle

## General Interfaces Used

The eyebrows module makes use of the following general interfaces:

* [Service Interface](../interfaces/api-interface-service.md):
    * `<service>` is `eyebrows`
* [Driver Interface](../interfaces/api-interface-driver.md):
    * `<service>` is `eyebrows`
    * Submodule list includes:
        * `FW`
        * `LED-LEFT`
        * `LED-RIGHT`
        * `LCD-LEFT`
        * `LCD-RIGHT`
        * `SERVO-LEFT`
        * `SERVO-RIGHT`
* [MCU Interface](../interfaces/api-interface-mcu.md):
    * `<service>` is `eyebrows`
    * `<mcu-id>` is either `left` or `right`
* [Status LED Interface](../interfaces/api-interface-statusled.md):
    * `<service>` is `eyebrows`
    * `<led-id>` is either `left` or `right`
* [Display Interface](../interfaces/api-interface-display.md):
    * `<service>` is `eyebrows`
    * `<which>` is either `left` or `right`
    * `<display>` is a list of three values, each of which is one of `H`, `M`, or `L` (for high, medium, or low position on the eyebrow LCD).
* [Servo Interface](../interfaces/api-interface-servo.md):
    * `<service>` is `eyebrows`
    * `<which>` is either `left` or `right`
    * `<position>` is in degrees, where 0 degrees is leftmost, 180 degrees is rightmost, and 90 degrees is center.
