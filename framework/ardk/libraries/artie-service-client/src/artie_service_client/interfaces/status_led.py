"""
This module contains the `StatusLEDInterface` mixin definition.
"""

class StatusLEDInterfaceV1:
    """
    `StatusLEDInterfaceV1` is a mixin to be used by all RPC services that
    control at least one status LED.
    """
    @staticmethod
    def __interface_name__() -> str:
        """Return the name of this interface. All interfaces must implement this method."""
        return "status-led-interface-v1"

    def led_list(self) -> list[str]:
        """
        RPC method to list all available status LEDs.

        Returns
        -------
        list[str]: A list of all available status LED names.
        """
        raise NotImplementedError("Services with status LEDs must implement the `led_list` method.")

    def led_set(self, which: str, state: str) -> bool:
        """
        RPC method to turn the led to heartbeat mode.

        Args
        ----
        - which: Which LED to set to the given state.
        - state: The state to set the LED to. Must be one of 'on', 'off', or 'heartbeat'.

        Returns
        -------
        bool: True if we do not detect an error. False otherwise.
        """
        raise NotImplementedError("Services with status LEDs must implement the `led_set` method.")

    def led_get(self, which: str) -> str:
        """
        RPC method to get the state of the given LED.

        Args
        ----
        - which: Which LED to get the state of.

        Returns
        -------
        str: The current state of the LED. Should be one of the enum values of
                `artie_util.constants.StatusLEDStates`.
        """
        raise NotImplementedError("Services with status LEDs must implement the `led_get` method.")
