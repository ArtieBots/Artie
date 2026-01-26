"""
This module contains the `MCUInterface` mixin definition.
"""

class MCUInterfaceV1:
    """
    `MCUInterfaceV1` is a mixin to be used by all RPC services in Artie that
    take control of one or more microcontrollers (MCUs) on the robot.
    """
    @staticmethod
    def __interface_name__() -> str:
        """Return the name of this interface. All interfaces must implement this method."""
        return "mcu-interface-v1"

    def mcu_fw_load(self, mcu_id: str) -> bool:
        """
        Load firmware onto the given MCU ID.

        * *Parameters*:
            * `mcu_id`: The ID of the MCU to load firmware onto.

        *Returns*: `True` if the firmware load was successful, `False` otherwise.
        """
        raise NotImplementedError("MCUInterface services must implement the `mcu_fw_load` method.")

    def mcu_list(self) -> list[str]:
        """
        Return a list of MCU IDs that this service is responsible for.
        """
        raise NotImplementedError("MCUInterface services must implement the `mcu_list` method.")

    def mcu_reset(self, mcu_id: str):
        """
        Reset the given MCU ID.

        * *Parameters*:
            * `mcu_id`: The ID of the MCU to reset.
        """
        raise NotImplementedError("MCUInterface services must implement the `mcu_reset` method.")

    def mcu_self_check(self, mcu_id: str):
        """
        Run a self diagnostics check and set our submodule statuses appropriately.
        """
        raise NotImplementedError("MCUInterface services must implement the `mcu_self_check` method.")

    def mcu_status(self, mcu_id: str) -> str:
        """
        Return the status of the given MCU ID.

        * *Parameters*:
            * `mcu_id`: The ID of the MCU to get status for.

        *Returns*: A string representing the status of the MCU. This string
        should be one of the enum values of `artie_util.constants.SubmoduleStatuses`.
        """
        raise NotImplementedError("MCUInterface services must implement the `mcu_status` method.")

    def mcu_version(self, mcu_id: str) -> str:
        """
        Return the firmware version information for the given MCU ID.

        * *Parameters*:
            * `mcu_id`: The ID of the MCU to get version information for.

        *Returns*: A string representing the firmware version of the MCU.
        """
        raise NotImplementedError("MCUInterface services must implement the `mcu_version` method.")
