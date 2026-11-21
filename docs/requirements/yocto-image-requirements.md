# Yocto Image Requirements

[Back to Yocto Image Contributions](../contributing/yocto-image-contributions.md) | [All requirements](./README.md)

Terms such as *SBC* are defined in the [glossary](./README.md#glossary).

## Yocto Images

ArDK Yocto Images provide the operating system for known supported single board computers (SBCs).

| ID | Name | Description | Milestone |
| :---: | :---: | ----- | :---: |
| Controller Node |  |  |  |
| YC1 | Kubernetes Support | The Artie Controller Node Image must support Kubernetes. | Aardvark |
| YC2 | Docker Support | The Artie Controller Node Image must support Docker. | Aardvark |
| YC3 | LED Heartbeat | The Artie Controller Node Image must provide indication that it is running by means of an LED, if the LED hardware is present.  | Aardvark |
| YC4 | Over the Air Update (OTA) | The Artie Controller Node Image must provide an OTA mechanism such that the SD card does not need to be removed from the SBC in order to update the image. | Aardvark |
| YC5 | SBC Support | The Artie Controller Node Image must support the Raspberry Pi 4B single board computer. Additional support for Raspberry Pi 5 (or other SBCs) is reserved for future releases. | Aardvark |
| YC6 | CAN Bus | The Artie Controller Node Image must support user-space access to the CAN bus by means of the SPI interface. | Aardvark |
| YC7 | UART | The Artie Controller Node Image must support UART. | Aardvark |
| YC8 | Wifi | The Artie Controller Node Image must support Wifi. | Aardvark |
| YC9 | Artie CLI | The Artie Controller Node Image must come equipped with Artie CLI. | Aardvark |
| YC10 | SSH | The Artie Controller Node Image must support SSH. Further, the Artie Controller Node Image must support disabling SSH after installation is complete (future). | Aardvark |
| YC11 | Non-root User | The Artie Controller Node Image must support a non-root user and must require a non-root user be set up during installation. | Future |
| Jetson Images |  |  |  |
| YJ1 | Kubernetes Support | The Artie Jetson Images must support Kubernetes. | Aardvark |
| YJ2 | Docker Support | The Artie Jetson Images must support Docker. Further, the Artie Jetson Images must provide support for Docker containers to access their GPUs. | Aardvark |
| YJ3 | LED Heartbeat | The Artie Jetson Images must provide indication that they are running by means of an LED, if the LED hardware is present.  | Aardvark |
| YJ4 | Over the Air Update (OTA) | The Artie Jetson Images must provide an OTA mechanism such that the SD cards do not need to be removed from the SBCs in order to update the images. | Aardvark |
| YJ5 | SBC Support | The Artie Jetson Images must support the TODO single board computer. | Aardvark |
| YJ6 | CAN Bus | The Artie Jetson Images must support user-space access to the CAN bus \<by means of the SPI interface?\>. | Aardvark |
| YJ7 | Wifi | The Artie Jetson Images must support Wifi. | Aardvark |
| YJ8 | Artie CLI | The Artie Jetson Images must come equipped with Artie CLI. | Aardvark |
| YJ10 | Non-root User | The Artie Jetson Images must support a non-root user and must require a non-root user be set up during installation. | Future |
