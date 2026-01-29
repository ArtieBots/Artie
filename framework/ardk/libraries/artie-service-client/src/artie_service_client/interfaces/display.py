"""
This module contains the code for the `DisplayInterface` mixin.
"""
class DisplayInterfaceV1:
    """
    `DisplayInterfaceV1` is a mixin to be used by services that provide one or more
    displays, such as LCDs or e-ink screens.
    """
    @staticmethod
    def __interface_name__() -> str:
        """Return the name of this interface. All interfaces must implement this method."""
        return "display-interface-v1"

    def display_list(self) -> list[str]:
        """
        List the displays that this service is responsible for.

        Returns:
            A list of display IDs.
        """
        raise NotImplementedError("display_list method must be implemented by the service.")

    def display_set(self, display_id: str, content: bytes) -> None:
        """
        Set the content of a specific display.

        Args:
            display_id: The ID of the display to set.
            content: The content to set on the display, as bytes.
        """
        raise NotImplementedError("display_set method must be implemented by the service.")

    def display_get(self, display_id: str) -> bytes:
        """
        Get the current content of a specific display.

        Args:
            display_id: The ID of the display to get.

        Returns:
            The current content of the display, as bytes.
        """
        raise NotImplementedError("display_get method must be implemented by the service.")

    def display_test(self, display_id: str) -> None:
        """
        Run a test pattern on the specified display and/or
        perform a self-test to verify functionality.

        Args:
            display_id: The ID of the display to test.
        """
        raise NotImplementedError("display_test method must be implemented by the service.")

    def display_clear(self, display_id: str) -> None:
        """
        Clear the content of the specified display.

        Args:
            display_id: The ID of the display to clear.
        """
        raise NotImplementedError("display_clear method must be implemented by the service.")
