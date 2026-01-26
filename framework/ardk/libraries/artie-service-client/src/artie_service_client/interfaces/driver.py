"""
This module contains the code for the `DriverInterface` mixin.
"""
class DriverInterfaceV1:
    """
    `DriverInterfaceV1` is a mixin to be used by all RPC driver services in Artie.
    """
    @staticmethod
    def __interface_name__() -> str:
        """Return the name of this interface. All interfaces must implement this method."""
        return "driver-interface-v1"

    def status(self) -> dict[str, str]:
        """
        Return the status of this service's submodules.

        The returned dict is of the form:
        ```python
        {
            "SubmoduleName1": "Status1",
            "SubmoduleName2": "Status2",
            ...
        }
        ```

        where each submodule name is defined by the driver service, and each status
        is one of the enum values of `artie_util.constants.SubmoduleStatuses`.
        """
        raise NotImplementedError("Driver services must implement the `status` method.")

    def self_check(self):
        """
        Run a self diagnostics check and set our submodule statuses appropriately.
        """
        raise NotImplementedError("Driver services must implement the `self_check` method.")
