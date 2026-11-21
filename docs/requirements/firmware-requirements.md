# Firmware Requirements

[Back to Firmware Contributions](../contributing/firmware-contributions.md) | [All requirements](./README.md)

Terms such as *Artie instance* are defined in the [glossary](./README.md#glossary).

## Firmware Bootloader

ArDK provides a Raspberry Pi Pico bootloader that can interface with CAN bus to accept firmware images without needing to remove any MCUs from the physical Artie.

| ID | Name | Description | Milestone |
| :---: | :---: | ----- | :---: |
| FW1 | MCU Support | The FW Bootloader must be compatible with Raspberry Pi Pico 1 | Aardvark |
| FW2 | Additional MCU Support | The FW Bootloader must be compatible with Raspberry Pi Pico 2 and *may be* compatible with the wireless versions of Pico 1 and 2\. | Future |
| FW3 | CAN Bus | The FW Bootloader must be compatible with CAN over SPI by means of the [ArDK CAN library](https://github.com/ArtieBots/ArDK/blob/main/docs/requirements/library-requirements.md#ardk-can-library). In particular, it must support accepting FW images over CAN by means of the ArDK CAN library. Additionally, the ArDK CAN library should be used for logging. | Aardvark |
| FW4 | Logging | The FW Bootloader must log its activity and status by means of the CAN bus. | Aardvark |

The firmware *libraries* that the bootloader and the MCU applications build on are specified
with the rest of ArDK's libraries, in
[Library Requirements](https://github.com/ArtieBots/ArDK/blob/main/docs/requirements/library-requirements.md#firmware-libraries).
