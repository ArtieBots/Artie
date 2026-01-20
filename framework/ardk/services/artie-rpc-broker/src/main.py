"""
Main module for the Artie RPC Broker service.

The Artie RPC Broker is responsible for registering and
discovering services in the Artie cluster that are making use of RPC.
"""
from artie_service_client import artie_service
from artie_service_client import interfaces
from artie_util import artie_logging as alog
from artie_util import util
from rpyc.utils.registry import TCPRegistryServer
from . import service
import argparse
import re
import rpyc
import time

SERVICE_NAME = "artie-rpc-broker-service"

class ArtieRPCBrokerServer(TCPRegistryServer):
    """
    This class implements the RPC Broker service, which is
    responsible for registering and discovering services in the
    Artie cluster that are making use of RPC.
    """
    def __init__(self, host: str, port: int):
        super().__init__(host, port, allow_listing=True)
        alog.info(f"{SERVICE_NAME} initialized on port {port}.")

    def cmd_query(self, host: str, name: str):
        """
        Overridden method for querying the registry.
        This method is called when a client wants to discover a service.
        The typical RPyC registry only allows querying by service name,
        but we want to allow querying by interface as well.
        Therefore, we override this method to allow for that functionality.

        The `name` argument can be any of the following:

        * A service name (e.g., "mouth-driver")
        * A fully-qualified service name with interfaces (e.g., "mouth-driver:driver-interface-v1:status-led-interface-v1")
        * An interface name (e.g., "driver-interface-v1")
        * A list of interface names separated by commas (e.g., "driver-interface-v1,status-led-interface-v1")

        This method returns a list of (host, port) tuples for services
        that match the query.
        """
        # self.services is a dict of the form {service_name: {service.Service: timestamp}}
        # where service_name is the simple name of the service (not the fully-qualified name)
        # We need to parse the `name` argument to determine what the client is querying for
        fully_qualified_pattern = re.compile(r"^(?P<name>[a-zA-Z0-9\-_]+)(?P<interfaces>(:[a-zA-Z0-9\-_]+)+)$")
        interface_list_pattern = re.compile(r"^([a-zA-Z0-9\-_]+)(,[a-zA-Z0-9\-_]+)+$")
        single_interface_pattern = re.compile(r"^[a-zA-Z0-9\-_]+-v[0-9]+$")
        match_fully_qualified = fully_qualified_pattern.match(name)
        match_interface_list = interface_list_pattern.match(name)
        match_single_interface = single_interface_pattern.match(name)
        if match_fully_qualified:
            # Querying by fully-qualified name
            alog.debug(f"Querying by fully-qualified name: {name!r}")
            return self._query_by_fully_qualified_name(name.upper())
        elif match_interface_list:
            # Querying by list of interfaces
            alog.debug(f"Querying by list of interfaces: {name!r}")
            interface_names = [iface.strip().upper() for iface in name.split(",")]
            return self._query_by_interface_list(interface_names)
        elif match_single_interface:
            # Querying by single interface
            alog.debug(f"Querying by single interface: {name!r}")
            return self._query_by_interface(name.upper())
        else:
            # Querying by simple service name
            alog.debug(f"Querying by service name: {name!r}")
            return self._query_by_simple_name(name.upper())

    def cmd_list(self, host: str, filter_host: tuple[str]|None) -> tuple[str]|None:
        """
        This method is called when a client wants to list all
        registered services.

        The `filter_host` argument can be used to filter services
        by a specific host, and is a tuple of exactly one item, which must be
        the name of a host.

        Returns a list of fully-qualified service names (or `None`, if we do not allow listing).
        """
        self.logger.debug("Querying for services list:")

        if not self.allow_listing:
            self.logger.debug("Listing is disabled")
            return None

        services = []
        if filter_host[0]:
            for name in self.services.keys():
                known_hosts = [s.host for s in self.services[name].keys()]
                if filter_host[0] in known_hosts:
                    services.append(self.services[name][filter_host[0]].fully_qualified_name)
            services = tuple(services)
        else:
            services = []
            for name in self.services.keys():
                for s in self.services[name].keys():
                    services.append(s.fully_qualified_name)
            services = tuple(services)

        self.logger.debug(f"Replying with {services}")

        return services

    def cmd_register(self, host: str, names: list[str], port: int) -> str:
        """
        Registers the given host and port under the given service names,
        which should be a list of fully-qualified names.
        """
        self.logger.debug(f"Registering {host}:{port} as {', '.join(names)}")

        for name in names:
            s = service.Service(name, host, port)
            self._add_service(s.simple_name, s)

        return "OK"

    def cmd_unregister(self, host: str, port: int) -> str:
        """
        Unregisters the given host and port from all services.
        """
        self.logger.debug(f"Unregistering {host}:{port}")

        remove = []
        for name in self.services.keys():
            for s in self.services[name].keys():
                if s.host == host and s.port == port:
                    remove.append((name, s))

        # Remove after iterating to avoid modifying the dict while iterating
        for name, s in remove:
            self._remove_service(name, s)

        return "OK"

    def _add_service(self, name: str, s: service.Service) -> None:
        """Updates the service's keep-alive time stamp"""
        if name not in self.services:
            self.services[name] = {}

        self.services[name][s] = time.time()

    def _query_by_interface_list(self, interface_names: list[str]) -> tuple[tuple[str, int]]:
        """
        """
        servers = []
        for name in self.services.keys():
            for s in self.services[name].keys():
                if all(iface in s.interface_names for iface in interface_names):
                    servers.append((s.host, s.port))

        self.logger.debug(f"Replying with {servers!r}")
        return tuple(servers)

    def _query_by_interface(self, interface_name: str) -> tuple[tuple[str, int]]:
        """
        """
        return self._query_by_interface_list([interface_name])

    def _query_by_simple_name(self, name: str) -> tuple[tuple[str, int], ...]:
        """
        """
        if name not in self.services:
            self.logger.debug("No such service")
            return ()

        oldest = time.time() - self.pruning_timeout
        all_servers = sorted(self.services[name].items(), key=lambda x: x.port)
        servers = []
        for s, t in all_servers:
            if t < oldest:
                self.logger.debug(f"Discarding stale {s.host}:{s.port}")
                self._remove_service(name, s)
            else:
                servers.append((s.host, s.port))

        self.logger.debug(f"Replying with {servers!r}")
        return tuple(servers)

    def _query_by_fully_qualified_name(self, name: str) -> tuple[tuple[str, int], ...]:
        """
        """
        # Get the simple name and query using it
        simple_name = name.split(":")[0]
        return self._query_by_simple_name(simple_name)

    def _remove_service(self, name: str, s: service.Service) -> None:
        """Removes a single server of the given service."""
        self.services[name].pop(s, None)
        if not self.services[name]:
            del self.services[name]

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-l", "--loglevel", type=str, default="info", choices=["debug", "info", "warning", "error"], help="The log level.")
    parser.add_argument("-p", "--port", type=int, default=18862, help="The port to bind for the RPC server.")
    parser.add_argument("--host", type=str, default="0.0.0.0", help="The host to bind for the RPC server.")
    args = parser.parse_args()

    # Set up logging
    alog.init(SERVICE_NAME, args)

    # Instantiate the single (multi-tenant) server instance and block forever, serving
    server = ArtieRPCBrokerServer(args.host, args.port)
    server.start()
