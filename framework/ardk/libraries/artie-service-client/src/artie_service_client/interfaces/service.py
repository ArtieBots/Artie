"""
This module contains the `ServiceInterface` mixin, which all services should
include in their inheritance.
"""
from artie_util import artie_logging as alog
from artie_util import util
import rpyc

class ServiceInterfaceV1:
    """
    `ServiceInterfaceV1` is a mixin to be used by all services in Artie.
    """
    @staticmethod
    def __interface_name__() -> str:
        """Return the name of this interface. All interfaces must implement this method."""
        return "service-interface-v1"

    @rpyc.exposed
    @alog.function_counter("whoami", alog.MetricSWCodePathAPIOrder.CALLS)
    def whoami(self) -> str:
        """
        Return the name of this service and the version.

        Note that this method is decorated appropriately for exposure
        and metrics collection. No need for derived classes to redecorate
        or implement this method.
        """
        if not hasattr(self, 'get_service_name'):
            raise NotImplementedError("Services must derive from `ArtieService`.")

        return f"{self.get_service_name()}:{util.get_git_tag()}"
