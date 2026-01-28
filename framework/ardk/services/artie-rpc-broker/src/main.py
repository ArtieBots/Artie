"""
Main module for the Artie RPC Broker service.

The Artie RPC Broker is responsible for registering and
discovering services in the Artie cluster that are making use of RPC.

Note about testing: this service is tested mostly implicitly through
integration tests of other services that make use of it, as there aren't
any CLI commands that make use of this service directly (and there shouldn't be -
it's a critical infrastructural component).
"""
from artie_service_client import dns
from artie_util import artie_logging as alog
from rpyc.utils.registry import TCPRegistryServer
from . import cachemonitor
from . import service
import argparse
import functools
import os
import time

SERVICE_NAME = "rpc-broker-service"

class ArtieRPCBrokerServer(TCPRegistryServer):
    """
    This class implements the RPC Broker service, which is
    responsible for registering and discovering services in the
    Artie cluster that are making use of RPC.
    """
    def __init__(self, host: str, port: int, broker_cache_dpath: str):
        super().__init__(host, port, allow_listing=True)

        alog.info(f"{SERVICE_NAME} initialized on port {port}.")

        self._broker_cache_dpath = broker_cache_dpath
        self._broker_cache_fpath = os.path.join(broker_cache_dpath, "broker-cache.txt")

        # Create the broker cache file if it does not exist
        os.makedirs(broker_cache_dpath, exist_ok=True)
        if not os.path.exists(self._broker_cache_fpath):
            with open(self._broker_cache_fpath, "w") as f:
                f.write("")

        # Start the cache monitor
        self._cache_monitor = cachemonitor.CacheMonitor(broker_cache_dpath)
        self._cache_monitor.start()

        # self.services is a dict of the form {service_name: {service.ServiceRegistration: timestamp}}
        # where service_name is the simple name of the service (not the fully-qualified name)

    def read_cache(func):
        """
        Load the cache from the cache file before executing the decorated method.
        """
        @functools.wraps(func)
        def wrapper(self, *args, **kwargs):
            # Check if the cache is valid
            if not self._cache_monitor.cache_valid:
                alog.info("Cache is invalid, reloading from cache file.")
                self._read_cache_from_file()
                self._cache_monitor.cache_valid = True

            # Execute the decorated method
            return func(self, *args, **kwargs)
        return wrapper

    def write_cache(func):
        """
        Write the cache to the cache file after executing the decorated method.
        """
        @functools.wraps(func)
        def wrapper(self, *args, **kwargs):
            # Execute the decorated method
            result = func(self, *args, **kwargs)

            # Write the cache to the cache file
            alog.info(f"Updating cache file at {self._broker_cache_fpath}.")
            self._write_cache_to_file()

            return result
        return wrapper

    @read_cache
    @alog.function_counter("cmd_query", alog.MetricSWCodePathAPIOrder.CALLS)
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
        # We need to parse the `name` argument to determine what the client is querying for
        match dns.ServiceQuery.from_string(name).query_type:
            case dns.ServiceQueryType.FULLY_QUALIFIED_NAME:
                return self._query_by_fully_qualified_name(name.upper())
            case dns.ServiceQueryType.INTERFACE_LIST:
                interface_names = [iface.strip().upper() for iface in name.split(",")]
                return self._query_by_interface_list(interface_names)
            case dns.ServiceQueryType.SINGLE_INTERFACE:
                return self._query_by_interface(name.upper())
            case dns.ServiceQueryType.SIMPLE_NAME:
                return self._query_by_simple_name(name.upper())

    @read_cache
    @alog.function_counter("cmd_list", alog.MetricSWCodePathAPIOrder.CALLS)
    def cmd_list(self, host: str, filter_host: tuple[str]|None) -> tuple[str]|None:
        """
        This method is called when a client wants to list all
        registered services.

        The `filter_host` argument can be used to filter services
        by a specific host, and is a tuple of exactly one item, which must be
        the name of a host.

        Returns a list of fully-qualified service names (or `None`, if we do not allow listing).
        """
        alog.debug("Querying for services list:")

        if not self.allow_listing:
            alog.debug("Listing is disabled")
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

        alog.debug(f"Replying with {services}")

        return services

    @read_cache
    @write_cache
    @alog.function_counter("cmd_register", alog.MetricSWCodePathAPIOrder.CALLS)
    def cmd_register(self, host: str, names: list[str], port: int) -> str:
        """
        Registers the given host and port under the given service names,
        which should be a list of fully-qualified names.
        """
        if issubclass(type(names), str):
            names = [names]

        alog.debug(f"Registering {host}:{port} as {', '.join(names)}")

        for name in names:
            s = service.ServiceRegistration(name, host, port)
            self._add_service(s.simple_name, s)

        return "OK"

    @read_cache
    @write_cache
    @alog.function_counter("cmd_unregister", alog.MetricSWCodePathAPIOrder.CALLS)
    def cmd_unregister(self, host: str, port: int) -> str:
        """
        Unregisters the given host and port from all services.
        """
        alog.debug(f"Unregistering {host}:{port}")

        remove = []
        for name in self.services.keys():
            for s in self.services[name].keys():
                if s.host == host and s.port == port:
                    remove.append((name, s))

        # Remove after iterating to avoid modifying the dict while iterating
        for name, s in remove:
            alog.info(f"Removing service {name} at {s.host}:{s.port}")
            self._remove_service(name, s)

        return "OK"

    def _add_service(self, name: str, s: service.ServiceRegistration) -> None:
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

        if not servers:
            alog.debug("No such service")
            alog.update_counter(1, "dns_miss", alog.MetricSWCodePathAPICallFamily.FAILURE, unit=alog.MetricUnits.CALLS, description="Number of times a cmd_query failed to find a service.")
            return ()

        alog.debug(f"Replying with {servers!r}")
        alog.update_counter(1, "dns_hit", alog.MetricSWCodePathAPICallFamily.SUCCESS, unit=alog.MetricUnits.CALLS, description="Number of times a cmd_query successfully found a service.")
        return tuple(servers)

    def _query_by_interface(self, interface_name: str) -> tuple[tuple[str, int]]:
        """
        """
        return self._query_by_interface_list([interface_name])

    def _query_by_simple_name(self, name: str) -> tuple[tuple[str, int], ...]:
        """
        """
        if name not in self.services:
            alog.debug("No such service")
            alog.update_counter(1, "dns_miss", alog.MetricSWCodePathAPICallFamily.FAILURE, unit=alog.MetricUnits.CALLS, description="Number of times a cmd_query failed to find a service.")
            return ()

        oldest = time.time() - self.pruning_timeout
        all_servers = sorted(self.services[name].items(), key=lambda x: x.port)
        servers = []
        for s, t in all_servers:
            if t < oldest:
                alog.debug(f"Discarding stale {s.host}:{s.port}")
                self._remove_service(name, s)
            else:
                servers.append((s.host, s.port))

        if not servers:
            alog.debug("No such service")
            alog.update_counter(1, "dns_miss", alog.MetricSWCodePathAPICallFamily.FAILURE, unit=alog.MetricUnits.CALLS, description="Number of times a cmd_query failed to find a service.")
            return ()

        alog.debug(f"Replying with {servers!r}")
        alog.update_counter(1, "dns_hit", alog.MetricSWCodePathAPICallFamily.SUCCESS, unit=alog.MetricUnits.CALLS, description="Number of times a cmd_query successfully found a service.")
        return tuple(servers)

    def _query_by_fully_qualified_name(self, name: str) -> tuple[tuple[str, int], ...]:
        """
        """
        # Get the simple name and query using it
        simple_name = name.split(":")[0]
        return self._query_by_simple_name(simple_name)

    def _read_cache_from_file(self) -> None:
        """Reads the cache from the cache file."""
        if not os.path.exists(self._broker_cache_fpath):
            alog.error(f"Cache file {self._broker_cache_fpath} does not exist.")
            return

        self.services.clear()
        with open(self._broker_cache_fpath, "r") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue

                try:
                    s = service.ServiceRegistration.from_cache_line(line)
                    self._add_service(s.simple_name, s)
                except Exception as e:
                    alog.error(f"Error parsing cache line '{line}': {e}")

    def _remove_service(self, name: str, s: service.ServiceRegistration) -> None:
        """Removes a single server of the given service."""
        self.services[name].pop(s, None)
        if not self.services[name]:
            del self.services[name]

    def _write_cache_to_file(self) -> None:
        """Writes the cache to the cache file."""
        with open(self._broker_cache_fpath, "w") as f:
            for name in self.services.keys():
                for s in self.services[name].keys():
                    f.write(s.to_cache_line() + "\n")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-l", "--loglevel", type=str, default=None, choices=["debug", "info", "warning", "error"], help="The log level.")
    parser.add_argument("-p", "--port", type=int, default=18864, help="The port to bind for the RPC server.")
    parser.add_argument("--host", type=str, default="0.0.0.0", help="The host to bind for the RPC server.")
    parser.add_argument("--broker-cache-path", type=str, default=os.getenv("BROKER_CACHE_PATH", "/broker-cache"), help="Path to the broker cache directory.")
    args = parser.parse_args()

    # Set up logging
    alog.init(SERVICE_NAME, args)

    # Instantiate the single (multi-tenant) server instance and block forever, serving
    server = ArtieRPCBrokerServer(args.host, args.port, args.broker_cache_path)
    server.start()
