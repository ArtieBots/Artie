"""
This module contains the code for a `Service` datastructure.
"""
from artie_util import artie_logging as alog

class ServiceRegistration:
    """
    This class represents a service that can be registered
    with the Artie RPC Broker.

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
