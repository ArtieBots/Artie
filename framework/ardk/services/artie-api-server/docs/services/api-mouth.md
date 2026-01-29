# API for Mouth Module

Available mouth displays:

* `smile`
* `frown`
* `line`
* `smirk`
* `open`
* `open-smile`
* `zig-zag`
* `talking`: Sets the mouth to talking mode, where it opens and closes until the display is told to do something else.
* `clear`
* `error`
* `test`

## General Interfaces Used

The mouth module makes use of the following general interfaces:

* [Service Interface](../interfaces/api-interface-service.md):
    * `<service>` is `mouth`
* [Driver Interface](../interfaces/api-interface-driver.md):
    * `<service>` is `mouth`
    * Submodule list includes:
        * `FW`
        * `LED`
        * `LCD`
* [MCU Interface](../interfaces/api-interface-mcu.md):
    * `<service>` is `mouth`
    * `<mcu-id>` is `mouth`
* [Status LED Interface](../interfaces/api-interface-statusled.md):
    * `<service>` is `mouth`
    * `<led-id>` is `mouth`
* [Display Interface](../interfaces/api-interface-display.md):
    * `<service>` is `mouth`
    * `<which>` is `mouth`
    * `<display>` is one of the available display values listed above.
