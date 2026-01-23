"""
This module exposes an RPyC Server subclass which should act as the
base class for all services that make use of any service interfaces
from the interfaces folder.
"""
import inspect
import os
import rpyc
from artie_util import artie_logging as alog
from artie_util import constants
from artie_util import util
from rpyc.utils.registry import TCPRegistryClient

class ArtieRPCService(rpyc.Service):
    ALIASES = []
    """List of alternative names for this service. We use this to tell RPyC the name of our service."""

    def __init__(self, service_name: str, port: int, register_service=True):
        """
        Initialize the ArtieRPCService, registering it with the RPC broker
        service by default. Pass `register_service=False` to disable automatic
        registration on startup.
        """
        self.ALIASES = [service_name] + self.ALIASES
        super().__init__()

        # Args
        self.port = port
        self.registrar = TCPRegistryClient(os.environ.get(constants.ArtieEnvVariables.RPC_BROKER_HOSTNAME, "localhost"), int(os.environ.get(constants.ArtieEnvVariables.RPC_BROKER_PORT, 18864)))

        # Fully-qualified service name, including interface names
        self.fully_qualified_name = self.get_fully_qualified_name()

        if register_service:
            self.register_service()

        # Now that we are up and running, log something
        alog.info(f"{service_name} initialized on port {port}.")

    def get_fully_qualified_name(self) -> str:
        """
        Get the fully-qualified name of this service, including
        all of the interfaces that it implements, this should return a string of
        the form: "<service-name>:<interface1>:<interface2>:..."
        where <service-name> is the first entry in the ALIASES list (the human-readable name of the service).
        """
        cls = self if inspect.isclass(self) else self.__class__
        mro = inspect.getmro(cls)  # Get Method Resolution Order
        interface_classes = [base for base in mro[1:-1]] # Exclude the first (main class) and last ('object')
        interface_names = [c.__interface_name__() for c in interface_classes if hasattr(c, '__interface_name__')]
        return ":".join([self.ALIASES[0]] + interface_names)

    def register_service(self):
        """
        Register this service with the RPC Broker service.
        """
        self.registrar.register(self.fully_qualified_name, self.port)

    def unregister_service(self):
        """
        Unregister this service from the RPC Broker service.
        """
        self.registrar.unregister(self.port)

    def _rpyc_getattr(self, name):
        # See https://rpyc.readthedocs.io/en/latest/docs/security.html#attribute-access
        if name.startswith("__"):
            # disallow special and private attributes
            raise AttributeError("cannot access private/special names")
        # allow all other attributes
        return getattr(self, name)
