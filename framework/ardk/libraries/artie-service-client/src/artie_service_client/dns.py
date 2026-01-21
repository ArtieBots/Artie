"""
This module contains mappings from hostnames to IP addresses/Kubernetes Services, etc.
"""
from artie_util import constants
from artie_util import util
from rpyc.utils.registry import TCPRegistryClient
import enum
import os
import re

# Cached environment variables
if util.in_test_mode() or util.mode() == constants.ArtieRunModes.INTEGRATION_TESTING:
    ARTIE_ID = None
else:
    ARTIE_ID = os.getenv(constants.ArtieEnvVariables.ARTIE_ID, None)

# Cached regex patterns for service query types
_fully_qualified_pattern = re.compile(r"^(?P<name>[a-zA-Z0-9\-_]+)(?P<interfaces>(:[a-zA-Z0-9\-_]+)+)$")
_interface_list_pattern = re.compile(r"^([a-zA-Z0-9\-_]+)(,[a-zA-Z0-9\-_]+)+$")
_single_interface_pattern = re.compile(r"^[a-zA-Z0-9\-_]+-v[0-9]+$")

# Cached registry client
_registry_client = TCPRegistryClient(
    os.environ.get(constants.ArtieEnvVariables.RPC_REGISTRY_HOST),
    int(os.environ.get(constants.ArtieEnvVariables.RPC_REGISTRY_PORT)),
)

@enum.unique
class ServiceQueryType(enum.Enum):
    """
    Types of service queries.
    """
    FULLY_QUALIFIED_NAME = enum.auto()
    """The fully-qualified service name, including interfaces. Looks like "mouth-driver:driver-interface-v1:status-led-interface-v1"."""

    INTERFACE_LIST = enum.auto()
    """A list of interface names. If the data type is a string, it is separated by commas.
    Looks like "driver-interface-v1,status-led-interface-v1"."""

    SIMPLE_NAME = enum.auto()
    """The simple name of the service, without any interfaces. Looks like "mouth-driver"."""

    SINGLE_INTERFACE = enum.auto()
    """A single interface name. Looks like "driver-interface-v1"."""

    @staticmethod
    def parse_string(query: str) -> tuple['ServiceQueryType', str|list[str]]:
        """
        Determine the type of service query based on the given string
        and return a tuple of the form (ServiceQueryType, data), where data is
        either a string or a list of strings, depending on the type.
        """
        if _fully_qualified_pattern.match(query):
            return (ServiceQueryType.FULLY_QUALIFIED_NAME, query)
        elif _interface_list_pattern.match(query):
            interface_names = [iface.strip().upper() for iface in query.split(",")]
            return (ServiceQueryType.INTERFACE_LIST, interface_names)
        elif _single_interface_pattern.match(query):
            return (ServiceQueryType.SINGLE_INTERFACE, query.upper())
        else:
            return (ServiceQueryType.SIMPLE_NAME, query.upper())

def lookup(item: str) -> tuple[str, int]:
    """
    Look up the given item and return a tuple of the form (host (str), port (int))
    Raise a KeyError if we can't find the given item.
    """
    _registry_client.discover(item)

    # TODO: is this how we will handle the artie_id?
    if ARTIE_ID is not None:
        return f"{_services[item]}-{ARTIE_ID}", _ports[item]
    else:
        return _services[item], _ports[item]
