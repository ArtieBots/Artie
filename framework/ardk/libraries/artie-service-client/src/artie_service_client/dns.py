"""
This module contains mappings from hostnames to IP addresses/Kubernetes Services, etc.
"""
from artie_util import artie_logging as alog
from artie_util import constants
from artie_util import util
from rpyc.utils.registry import TCPRegistryClient
import dataclasses
import enum
import os
import re

# Cached environment variables
if util.in_test_mode() or util.mode() == constants.ArtieRunModes.INTEGRATION_TESTING:
    ARTIE_ID = None
    RPC_BROKER_HOSTNAME = "localhost"
    RPC_BROKER_PORT = 18864
else:
    ARTIE_ID = os.getenv(constants.ArtieEnvVariables.ARTIE_ID, None)
    RPC_BROKER_HOSTNAME = os.getenv(constants.ArtieEnvVariables.RPC_BROKER_HOSTNAME, "localhost")
    RPC_BROKER_PORT = int(os.getenv(constants.ArtieEnvVariables.RPC_BROKER_PORT, 18864))

# Cached regex patterns for service query types
_fully_qualified_pattern = re.compile(r"^(?P<name>[a-zA-Z0-9\-_]+)(?P<interfaces>(:[a-zA-Z0-9\-_]+)+)$")
_interface_list_pattern = re.compile(r"^([a-zA-Z0-9\-_]+)(,[a-zA-Z0-9\-_]+)+$")
_single_interface_pattern = re.compile(r"^[a-zA-Z0-9\-_]+-v[0-9]+$")

# Cached registry client
_registry_client = TCPRegistryClient(RPC_BROKER_HOSTNAME, RPC_BROKER_PORT)

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

@dataclasses.dataclass
class ServiceQuery:
    """
    A class for parsing and representing service queries.
    """
    query_type: ServiceQueryType
    """The type of service query."""

    data: str | list[str]
    """The data associated with the service query. This is either a string or a list of strings, depending on the query type."""

    def __eq__(self, value):
        if not isinstance(value, ServiceQuery):
            return False
        return self.query_type == value.query_type and self.data == value.data

    def __hash__(self):
        return hash((self.query_type, tuple(self.data) if isinstance(self.data, list) else self.data))

    def __str__(self):
        return f"{self.data}"

    @staticmethod
    def from_string(query: str) -> 'ServiceQuery':
        """
        Determine the type of service query based on the given string
        and return a tuple of the form (ServiceQueryType, data), where data is
        either a string or a list of strings, depending on the type.
        """
        if _fully_qualified_pattern.match(query):
            return ServiceQuery(ServiceQueryType.FULLY_QUALIFIED_NAME, query.upper())
        elif _interface_list_pattern.match(query):
            interface_names = [iface.strip().upper() for iface in query.split(",")]
            return ServiceQuery(ServiceQueryType.INTERFACE_LIST, interface_names)
        elif _single_interface_pattern.match(query):
            return ServiceQuery(ServiceQueryType.SINGLE_INTERFACE, query.upper())
        else:
            return ServiceQuery(ServiceQueryType.SIMPLE_NAME, query.upper())

def lookup(item: str|ServiceQuery) -> tuple[str, int]:
    """
    Look up the given item and return a tuple of the form (host (str), port (int))
    Raise a KeyError if we can't find the given item.
    """
    if issubclass(type(item), ServiceQuery):
        query = item.data
    else:
        query = item

    alog.debug(f"Looking up service for query: {query}")
    host_and_port_list = _registry_client.discover(query)
    if not host_and_port_list:
        alog.error(f"Service not found for query: {item!r}")
        raise KeyError(f"Service not found for query: {item!r}")

    alog.debug(f"Found service for query {query}: {host_and_port_list[0]}")
    return host_and_port_list[0]  # Return the first result

def list_services(filter_host: str|None = None, filter_name: str|None = None, filter_interfaces: list[str]|None = None) -> tuple[str]|None:
    """
    List all registered services.

    The `filter_host` argument can be used to filter services
    by a specific host.

    The `filter_name` argument can be used to filter services
    by a specific service name.

    The `filter_interfaces` argument can be used to filter services
    by a list of interfaces.

    Returns a list of fully-qualified service names (or `None`, if we do not allow listing).
    """
    if filter_host:
        names = _registry_client.list((filter_host,))
    else:
        names = _registry_client.list()

    if names is None:
        return None

    filtered_names = []
    for fully_qualified_name in names:
        if filter_name and not fully_qualified_name.upper().startswith(filter_name.upper()):
            continue

        if filter_interfaces:
            match = _fully_qualified_pattern.match(fully_qualified_name)
            if not match:
                continue

            interfaces_part = match.group("interfaces")
            service_interfaces = [iface.strip().upper() for iface in interfaces_part.split(":") if iface.strip()]

            if not all(req_iface.upper() in service_interfaces for req_iface in filter_interfaces):
                continue

        filtered_names.append(fully_qualified_name)

    return tuple(filtered_names)
