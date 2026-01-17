"""
This module contains the `StatusLEDInterface` mixin definition.
"""

class StatusLEDInterfaceV1:
    """
    `StatusLEDInterfaceV1` is a mixin to be used by all RPC services that
    control at least one status LED.
    """
    def __interface_name__(self) -> str:
        """Return the name of this interface. All interfaces must implement this method."""
        return "status-led-interface-v1"

    def led_on(self, which: str = None) -> bool:
        """
        RPC method to turn led on.

        Args
        ----
        - which: If provided, should specify which LED to turn on.
          Only applicable for services with multiple status LEDs.

        Returns
        -------
        bool: True if we do not detect an error. False otherwise.
        """
        raise NotImplementedError("Services with status LEDs must implement the `led_on` method.")

    def led_off(self, which: str = None) -> bool:
        """
        RPC method to turn led off.

        Args
        ----
        - which: If provided, should specify which LED to turn off.
          Only applicable for services with multiple status LEDs.

        Returns
        -------
        bool: True if we do not detect an error. False otherwise.
        """
        raise NotImplementedError("Services with status LEDs must implement the `led_off` method.")

    def led_heartbeat(self, which: str = None) -> bool:
        """
        RPC method to turn the led to heartbeat mode.

        Args
        ----
        - which: If provided, should specify which LED to set to heartbeat mode.
          Only applicable for services with multiple status LEDs.

        Returns
        -------
        bool: True if we do not detect an error. False otherwise.
        """
        raise NotImplementedError("Services with status LEDs must implement the `led_heartbeat` method.")

    def led_get(self, which: str = None) -> str:
        """
        RPC method to get the state of the given LED.

        Args
        ----
        - which: If provided, should specify which LED to get the state of.
          Only applicable for services with multiple status LEDs.

        Returns
        -------
        str: The current state of the LED. Should be one of the enum values of
                `artie_util.constants.StatusLEDStates`.
        """
        raise NotImplementedError("Services with status LEDs must implement the `led_get` method.")
