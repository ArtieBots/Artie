"""
This module contains the code for a `Service` datastructure.
"""
from artie_util import artie_logging as alog

class ServiceRegistration:
    """
    This class represents a service that can be registered
    with the Artie Service Broker.

    This is meant to be an immutable, hashable object.
    """
    def __init__(self, fully_qualified_name: str, host: str, port: int):
        self.fully_qualified_name = fully_qualified_name
        self.simple_name = fully_qualified_name.split(":")[0]
        self.interface_names = fully_qualified_name.split(":")[1:]
        self.host = host
        self.port = port

    def __eq__(self, value):
        return (
            isinstance(value, ServiceRegistration) and
            self.fully_qualified_name == value.fully_qualified_name and
            self.host == value.host and
            self.port == value.port
        )

    def __hash__(self):
        return hash((self.fully_qualified_name, self.host, self.port))

    @staticmethod
    def from_cache_line(line: str) -> 'ServiceRegistration':
        """Creates a ServiceRegistration from a line in the cache file."""
        parts = line.strip().split(",")
        if len(parts) != 3:
            alog.error(f"Invalid cache line: {line}")
            raise ValueError(f"Invalid cache line: {line}")
        fully_qualified_name, host, port_str = parts
        return ServiceRegistration(fully_qualified_name, host, int(port_str))

    def to_cache_line(self) -> str:
        """Converts the service registration to a line for the cache file."""
        return f"{self.fully_qualified_name},{self.host},{self.port}"
