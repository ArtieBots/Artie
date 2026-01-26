"""
User-space driver for Artie's eyebrows MCUs.

This driver is responsible for:

* Loading eyebrow MCU firmware
* Animating eyebrows
* Moving eyes

This driver accepts ZeroRPC requests and controls the
MCUs over the Controller Node's CAN bus. It is
therefore meant to be run on the Controller Node,
and it needs to be run inside a container that
has access to CAN on the Controller Node.
"""
from artie_i2c import i2c
from artie_util import artie_logging as alog
from artie_util import constants
from artie_util import util
from artie_service_client import artie_service
from artie_service_client import interfaces
from rpyc.utils.registry import TCPRegistryClient
from typing import Dict, List
from . import ebcommon
from . import fw
from . import lcd
from . import led
from . import metrics
from . import servo
import argparse
import os
import rpyc

SERVICE_NAME = "eyebrows-service"

@rpyc.service
class DriverServer(
    interfaces.ServiceInterfaceV1,
    interfaces.DriverInterfaceV1,
    interfaces.MCUInterfaceV1,
    interfaces.StatusLEDInterfaceV1,
    artie_service.ArtieRPCService
    ):
    def __init__(self, port: int, fw_fpath: str, ipv6=False):
        super().__init__(SERVICE_NAME, port)
        self._servo_submodule = servo.ServoSubmodule()
        self._led_submodule = led.LedSubmodule()
        self._lcd_submodule = lcd.LcdSubmodule()
        self._fw_submodule = fw.FirmwareSubmodule(fw_fpath, ipv6=ipv6)

        # Load FW
        self._fw_submodule.initialize_mcus()

        # Initialize
        self._led_submodule.initialize()
        self._lcd_submodule.initialize()

    @rpyc.exposed
    @alog.function_counter("status", alog.MetricSWCodePathAPIOrder.CALLS)
    @interfaces.interface_method(interfaces.DriverInterfaceV1)
    def status(self) -> Dict[str, str]:
        """
        Return the status of this service's submodules.
        """
        status = self._fw_submodule.status() | self._led_submodule.status() | self._lcd_submodule.status() | self._servo_submodule.status()
        alog.info(f"Received request for status. Status: {status}")
        return {k: str(v) for k, v in status.items()}

    @rpyc.exposed
    @alog.function_counter("self_check", alog.MetricSWCodePathAPIOrder.CALLS)
    @interfaces.interface_method(interfaces.DriverInterfaceV1)
    def self_check(self):
        """
        Run a self diagnostics check and set our submodule statuses appropriately.
        """
        alog.info("Running self check...")
        self._fw_submodule.self_check_all()
        self._led_submodule.self_check()
        self._lcd_submodule.self_check()
        self._servo_submodule.self_check()

    @rpyc.exposed
    @alog.function_counter("led_on", alog.MetricSWCodePathAPIOrder.CALLS, attributes={alog.KnownMetricAttributes.SUBMODULE: metrics.SubmoduleNames.LED})
    @interfaces.interface_method(interfaces.StatusLEDInterfaceV1)
    def led_on(self, side: str) -> bool:
        """
        RPC method to turn led on.

        Args
        ----
        - side: One of 'left' or 'right'

        Returns
        -------
        bool: True if we do not detect an error. False otherwise.
        """
        return self._led_submodule.on(side)

    @rpyc.exposed
    @alog.function_counter("led_off", alog.MetricSWCodePathAPIOrder.CALLS, attributes={alog.KnownMetricAttributes.SUBMODULE: metrics.SubmoduleNames.LED})
    @interfaces.interface_method(interfaces.StatusLEDInterfaceV1)
    def led_off(self, side: str) -> bool:
        """
        RPC method to turn led off.

        Args
        ----
        - side: One of 'left' or 'right'

        Returns
        -------
        bool: True if we do not detect an error. False otherwise.
        """
        return self._led_submodule.off(side)

    @rpyc.exposed
    @alog.function_counter("led_heartbeat", alog.MetricSWCodePathAPIOrder.CALLS, attributes={alog.KnownMetricAttributes.SUBMODULE: metrics.SubmoduleNames.LED})
    @interfaces.interface_method(interfaces.StatusLEDInterfaceV1)
    def led_heartbeat(self, side: str) -> bool:
        """
        RPC method to turn the led to heartbeat mode.

        Args
        ----
        - side: One of 'left' or 'right'

        Returns
        -------
        bool: True if we do not detect an error. False otherwise.
        """
        return self._led_submodule.heartbeat(side)

    @rpyc.exposed
    @alog.function_counter("led_get", alog.MetricSWCodePathAPIOrder.CALLS, attributes={alog.KnownMetricAttributes.SUBMODULE: metrics.SubmoduleNames.LED})
    @interfaces.interface_method(interfaces.StatusLEDInterfaceV1)
    def led_get(self, side: str) -> str:
        """
        RPC method to get the state of the given LED.
        """
        return self._led_submodule.get(side)

    @rpyc.exposed
    @alog.function_counter("lcd_test", alog.MetricSWCodePathAPIOrder.CALLS, attributes={alog.KnownMetricAttributes.SUBMODULE: metrics.SubmoduleNames.LCD})
    def lcd_test(self, side: str) -> bool:
        """
        RPC method to test the LCD.

        Args
        ----
        - side: One of 'left' or 'right'

        Returns
        -------
        bool: True if we do not detect an error. False otherwise.
        """
        return self._lcd_submodule.test(side)

    @rpyc.exposed
    @alog.function_counter("lcd_off", alog.MetricSWCodePathAPIOrder.CALLS, attributes={alog.KnownMetricAttributes.SUBMODULE: metrics.SubmoduleNames.LCD})
    def lcd_off(self, side: str) -> bool:
        """
        RPC method to turn the LCD off.

        Args
        ----
        - side: One of 'left' or 'right'

        Returns
        -------
        bool: True if we do not detect an error. False otherwise.
        """
        return self._lcd_submodule.off(side)

    @rpyc.exposed
    @alog.function_counter("lcd_draw", alog.MetricSWCodePathAPIOrder.CALLS, attributes={alog.KnownMetricAttributes.SUBMODULE: metrics.SubmoduleNames.LCD})
    def lcd_draw(self, side: str, eyebrow_state: List[str]) -> bool:
        """
        RPC method to draw a specified eyebrow state to the LCD.

        Args
        ----
        - side: One of 'left' or 'right'
        - eyebrow_state: A list of 'H' or 'L' or 'M'

        Returns
        -------
        bool: True if we do not detect an error. False otherwise.
        """
        return self._lcd_submodule.draw(side, eyebrow_state)

    @rpyc.exposed
    @alog.function_counter("lcd_get", alog.MetricSWCodePathAPIOrder.CALLS, attributes={alog.KnownMetricAttributes.SUBMODULE: metrics.SubmoduleNames.LCD})
    def lcd_get(self, side: str) -> List[str]|str:
        """
        RPC method to get the LCD value that we think
        we are displaying on the given side. Will return either
        a list of vertices, 'test', 'clear', or 'error'.
        """
        return self._lcd_submodule.get(side)

    @rpyc.exposed
    @alog.function_counter("mcu_fw_load", alog.MetricSWCodePathAPIOrder.CALLS, attributes={alog.KnownMetricAttributes.SUBMODULE: metrics.SubmoduleNames.FIRMWARE})
    @interfaces.interface_method(interfaces.MCUInterfaceV1)
    def mcu_fw_load(self, mcu_id: str) -> bool:
        """
        Load firmware onto the given MCU ID.

        * *Parameters*:
            * `mcu_id`: The ID of the MCU to load firmware onto.
        *Returns*: `True` if the firmware load was successful, `False` otherwise.
        """
        alog.info("Reloading FW...")
        worked = self._fw_submodule.initialize_mcus()

        # Initialize
        worked &= self._led_submodule.initialize()
        worked &= self._lcd_submodule.initialize()

        return worked

    @rpyc.exposed
    @alog.function_counter("mcu_list", alog.MetricSWCodePathAPIOrder.CALLS, attributes={alog.KnownMetricAttributes.SUBMODULE: metrics.SubmoduleNames.FIRMWARE})
    @interfaces.interface_method(interfaces.MCUInterfaceV1)
    def mcu_list(self) -> List[str]:
        """
        Return a list of MCU IDs that this service is responsible for.
        """
        return list(ebcommon.MCU_ADDRESS_MAP.keys())

    @rpyc.exposed
    @alog.function_counter("mcu_reset", alog.MetricSWCodePathAPIOrder.CALLS, attributes={alog.KnownMetricAttributes.SUBMODULE: metrics.SubmoduleNames.FIRMWARE})
    @interfaces.interface_method(interfaces.MCUInterfaceV1)
    def mcu_reset(self, mcu_id):
        """
        Reset the given MCU ID.

        * *Parameters*:
            * `mcu_id`: The ID of the MCU to reset.
        """
        return self._fw_submodule.reset(mcu_id)

    @rpyc.exposed
    @alog.function_counter("mcu_self_check", alog.MetricSWCodePathAPIOrder.CALLS, attributes={alog.KnownMetricAttributes.SUBMODULE: metrics.SubmoduleNames.FIRMWARE})
    @interfaces.interface_method(interfaces.MCUInterfaceV1)
    def mcu_self_check(self, mcu_id: str):
        """
        Run a self diagnostics check on the given MCU and set our submodule statuses appropriately.
        """
        return self._fw_submodule.self_check(mcu_id)

    @rpyc.exposed
    @alog.function_counter("mcu_status", alog.MetricSWCodePathAPIOrder.CALLS, attributes={alog.KnownMetricAttributes.SUBMODULE: metrics.SubmoduleNames.FIRMWARE})
    @interfaces.interface_method(interfaces.MCUInterfaceV1)
    def mcu_status(self, mcu_id: str) -> str:
        """
        Return the status of the given MCU ID.

        * *Parameters*:
            * `mcu_id`: The ID of the MCU to get status for.
        *Returns*: A string representing the status of the MCU. This string
        should be one of the enum values of `artie_util.constants.SubmoduleStatuses`.
        """
        if mcu_id == "left":
            return self._fw_submodule.left_status
        elif mcu_id == "right":
            return self._fw_submodule.right_status
        else:
            alog.error(f"Requested status for invalid MCU ID {mcu_id}.")
            return constants.SubmoduleStatuses.UNKNOWN

    @rpyc.exposed
    @alog.function_counter("mcu_version", alog.MetricSWCodePathAPIOrder.CALLS, attributes={alog.KnownMetricAttributes.SUBMODULE: metrics.SubmoduleNames.FIRMWARE})
    @interfaces.interface_method(interfaces.MCUInterfaceV1)
    def mcu_version(self, mcu_id: str) -> str:
        """
        Return the firmware version information for the given MCU ID.

        * *Parameters*:
            * `mcu_id`: The ID of the MCU to get version information for.
        *Returns*: A string representing the firmware version of the MCU.
        """
        return self._fw_submodule.version(mcu_id)

    @rpyc.exposed
    @alog.function_counter("servo_get", alog.MetricSWCodePathAPIOrder.CALLS, attributes={alog.KnownMetricAttributes.SUBMODULE: metrics.SubmoduleNames.SERVO})
    def servo_get(self, side: str) -> float:
        """
        RPC method to get the servo's degrees. This could be off
        due to inaccuracies of the servo, but also due to limiting on the left
        and right extreme ends as found during servo calibration.

        Returns
        -------
        Degrees (float).
        """
        return self._servo_submodule.get(side)

    @rpyc.exposed
    @alog.function_counter("servo_go", alog.MetricSWCodePathAPIOrder.CALLS, attributes={alog.KnownMetricAttributes.SUBMODULE: metrics.SubmoduleNames.SERVO})
    def servo_go(self, side: str, servo_degrees: float) -> bool:
        """
        RPC method to move the servo to the given location.

        Args
        ----
        - side: One of 'left' or 'right'
        - servo_degrees: Any value in the interval [0, 180]

        Returns
        -------
        bool: True if we do not detect an error. False otherwise.
        """
        return self._servo_submodule.go(side, servo_degrees)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fw_fpath", metavar="fw-fpath", type=str, help="The path to the FW file. It must be an .elf file.")
    parser.add_argument("--ipv6", action='store_true', help="Use IPv6 if given, otherwise IPv4.")
    parser.add_argument("-l", "--loglevel", type=str, default="info", choices=["debug", "info", "warning", "error"], help="The log level.")
    parser.add_argument("-p", "--port", type=int, default=18863, help="The port to bind for the RPC server.")
    args = parser.parse_args()

    # Set up logging
    alog.init(SERVICE_NAME, args)

    # Generate our self-signed certificate (if not already present)
    certfpath = "/etc/cert.pem"
    keyfpath = "/etc/pkey.pem"
    util.generate_self_signed_cert(certfpath, keyfpath, days=None, force=True)

    # If we are in testing mode, we need to manually initialize some stuff
    if util.in_test_mode():
        i2c.manually_initialize(i2c_instances=[0], instance_to_address_map={0: [ebcommon.MCU_ADDRESS_MAP['left'], ebcommon.MCU_ADDRESS_MAP['right']]})

    # Instantiate the single (multi-tenant) server instance and block forever, serving
    server = DriverServer(args.port, args.fw_fpath, ipv6=args.ipv6)
    t = util.create_rpc_server(server, keyfpath, certfpath, args.port, ipv6=args.ipv6)
    t.start()
