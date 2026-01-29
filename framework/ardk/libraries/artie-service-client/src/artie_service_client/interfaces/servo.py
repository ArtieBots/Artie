"""
This module contains the code for the `ServoInterface` mixin.
"""
class ServoInterfaceV1:
    """
    `ServoInterfaceV1` is a mixin to be used by services that provide one or more
    servos.
    """
    @staticmethod
    def __interface_name__() -> str:
        """Return the name of this interface. All interfaces must implement this method."""
        return "servo-interface-v1"

    def servo_list(self) -> list[str]:
        """
        List the servos that this service is responsible for.

        Returns:
            A list of servo IDs.
        """
        raise NotImplementedError("servo_list method must be implemented by the service.")

    def servo_set_position(self, servo_id: str, position: float) -> None:
        """
        Set the position of a specific servo.

        Args:
            servo_id: The ID of the servo to set.
            position: The position to set the servo to, as a float.
        """
        raise NotImplementedError("servo_set_position method must be implemented by the service.")

    def servo_get_position(self, servo_id: str) -> float:
        """
        Get the current position of a specific servo.

        Note: If the servo does not support position feedback, this method should
        return a best guess.

        Args:
            servo_id: The ID of the servo to get.

        Returns:
            The current position of the servo, as a float.
        """
        raise NotImplementedError("servo_get_position method must be implemented by the service.")
