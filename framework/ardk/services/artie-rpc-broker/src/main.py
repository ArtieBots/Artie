"""
Main module for the Artie RPC Broker service.

The Artie RPC Broker is responsible for registering and
discovering services in the Artie cluster that are making use of RPC.
"""
from artie_service_client import dns
from artie_util import artie_logging as alog
from artie_util import constants
from artie_util import util
from rpyc.utils.registry import TCPRegistryServer
from . import cachemonitor
from . import service
from . import test_server
import argparse
import datetime
import functools
import multiprocessing
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
        # where service_name is the fully-qualified name of the service.

        # This dict keeps a mapping of simple name to fully-qualified names for quick lookup
        # on the typical case of querying by simple name.
        self.name_mapping = {}

    def read_cache(func):
        """
        Load the cache from the cache file before executing the decorated method.
        """
        @functools.wraps(func)
        def wrapper(self, *args, **kwargs):
            # Check if the cache is valid
            if not self._cache_monitor.cache_valid:
                alog.debug("Cache is invalid, reloading from cache file.")
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
            alog.debug(f"Updating cache file at {self._broker_cache_fpath}.")
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
                return self._query_by_fully_qualified_name()
            case dns.ServiceQueryType.INTERFACE_LIST:
                interface_names = [iface.strip() for iface in name.split(",")]
                return self._query_by_interface_list(interface_names)
            case dns.ServiceQueryType.SINGLE_INTERFACE:
                return self._query_by_interface(name)
            case dns.ServiceQueryType.SIMPLE_NAME:
                return self._query_by_simple_name(name)

    @read_cache
    @alog.function_counter("cmd_list", alog.MetricSWCodePathAPIOrder.CALLS)
    def cmd_list(self, host: str, filter_host: tuple[str|None]) -> tuple[str]|None:
        """
        This method is called when a client wants to list all
        registered services.

        The `filter_host` argument can be used to filter services
        by a specific host, and is a tuple of exactly one item, which must be
        the name of a host.

        Returns a tuple of fully-qualified service names (or `None`, if we do not allow listing).
        """
        alog.debug("Querying for services list")

        if not self.allow_listing:
            alog.info("Listing is disabled")
            return None

        services = []
        if filter_host[0]:
            for fully_qualified_name in self.services.keys():
                host_to_fqname_map = {s.host: s.fully_qualified_name for s in self.services[fully_qualified_name].keys()}
                if filter_host[0] in host_to_fqname_map:
                    services.append(host_to_fqname_map[filter_host[0]])
            services = tuple(services)
        else:
            services = [fully_qualified_name for fully_qualified_name in self.services.keys()]
            services = tuple(services)

        alog.test(f"Found {services}", tests=["rpc-broker-unit-tests:list-services"])
        return services

    @read_cache
    @write_cache
    @alog.function_counter("cmd_register", alog.MetricSWCodePathAPIOrder.CALLS)
    def cmd_register(self, host: str, names: list[str], port: int) -> str:
        """
        Registers the given host and port under the given service names,
        which should be a list of a single fully-qualified name.

        This API is overridden from the base RPyC registry, which is why
        `names` is a list, even though it should only have one item.
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
        for fully_qualified_name in self.services.keys():
            for s in self.services[fully_qualified_name].keys():
                if s.host == host and s.port == port:
                    remove.append((fully_qualified_name, s))

        # Remove after iterating to avoid modifying the dict while iterating
        for name, s in remove:
            alog.test(f"Removing service {name} at {s.host}:{s.port}", tests=["rpc-broker-unit-tests:unregister"])
            self._remove_service(name, s)

        return "OK"

    def _add_service(self, fully_qualified_name: str, s: service.ServiceRegistration) -> None:
        """Registers the service and updates the service's keep-alive time stamp"""
        if fully_qualified_name not in self.services:
            self.services[fully_qualified_name] = {}  # Dict of {service.ServiceRegistration: timestamp}

        self.services[fully_qualified_name][s] = datetime.datetime.now()
        if s.simple_name in self.name_mapping:
            self.name_mapping[s.simple_name].append(fully_qualified_name)
            self.name_mapping[s.simple_name] = list(set(self.name_mapping[s.simple_name]))
        else:
            self.name_mapping[s.simple_name] = [fully_qualified_name]

    def _query_by_interface_list(self, interface_names: list[str]) -> tuple[tuple[str, int]]:
        """
        Query for services that implement all of the given interfaces.
        """
        servers = []
        for fully_qualified_name in self.services.keys():
            for s in self.services[fully_qualified_name].keys():
                if all(iface in s.interface_names for iface in interface_names):
                    servers.append((s.host, s.port))

        if not servers:
            alog.test("No such service", tests=["rpc-broker-unit-tests:query-by-interface-list-expected-no", "rpc-broker-unit-tests:query-by-interface-list-partial-expected-no"])
            alog.update_counter(1, "dns_miss", alog.MetricSWCodePathAPICallFamily.FAILURE, unit=alog.MetricUnits.CALLS, description="Number of times a cmd_query failed to find a service.")
            return ()

        alog.test(f"Found {servers}", tests=["rpc-broker-unit-tests:query-by-interface-list-expected-yes", "rpc-broker-unit-tests:query-by-interface-list-partial-expected-yes"])
        alog.update_counter(1, "dns_hit", alog.MetricSWCodePathAPICallFamily.SUCCESS, unit=alog.MetricUnits.CALLS, description="Number of times a cmd_query successfully found a service.")
        return tuple(servers)

    def _query_by_interface(self, interface_name: str) -> tuple[tuple[str, int]]:
        """
        Query for services that implement the given interface.
        """
        return self._query_by_interface_list([interface_name])

    def _query_by_simple_name(self, simple_name: str) -> tuple[tuple[str, int], ...]:
        """
        Query for services by simple name.
        """
        alog.debug(f"Querying for service by simple name: {simple_name}")
        if simple_name not in self.name_mapping:
            alog.test("No such service", tests=["rpc-broker-unit-tests:query-by-simple-name-expected-no"])
            alog.update_counter(1, "dns_miss", alog.MetricSWCodePathAPICallFamily.FAILURE, unit=alog.MetricUnits.CALLS, description="Number of times a cmd_query failed to find a service.")
            return ()

        # Get all fully-qualified names for this simple name
        fully_qualified_names = self.name_mapping[simple_name]

        # Get all servers for all the fully-qualified names
        servers = []
        for fq_name in fully_qualified_names:
            servers.extend([(s.host, s.port) for s in self.services[fq_name].keys()])

        if len(servers) == 0:
            alog.error("No such service: inconsistent state detected.")
            alog.update_counter(1, "dns_miss", alog.MetricSWCodePathAPICallFamily.FAILURE, unit=alog.MetricUnits.CALLS, description="Number of times a cmd_query failed to find a service.")
            return ()
        else:
            alog.test(f"Found {servers}", tests=["rpc-broker-unit-tests:query-by-simple-name-expected-yes"])
            alog.update_counter(1, "dns_hit", alog.MetricSWCodePathAPICallFamily.SUCCESS, unit=alog.MetricUnits.CALLS, description="Number of times a cmd_query successfully found a service.")
            return tuple(servers)

    def _query_by_fully_qualified_name(self, fq_name: str) -> tuple[tuple[str, int], ...]:
        """
        Query for services by fully-qualified name.
        """
        alog.debug(f"Querying for service by fully-qualified name: {fq_name}")
        if fq_name not in self.services:
            alog.test("No such service", tests=["rpc-broker-unit-tests:query-by-fully-qualified-name-expected-no"])
            alog.update_counter(1, "dns_miss", alog.MetricSWCodePathAPICallFamily.FAILURE, unit=alog.MetricUnits.CALLS, description="Number of times a cmd_query failed to find a service.")
            return ()

        servers = [(s.host, s.port) for s in self.services[fq_name].keys()]
        alog.test(f"Found {servers}", tests=["rpc-broker-unit-tests:query-by-fully-qualified-name-expected-yes"])
        alog.update_counter(1, "dns_hit", alog.MetricSWCodePathAPICallFamily.SUCCESS, unit=alog.MetricUnits.CALLS, description="Number of times a cmd_query successfully found a service.")
        return tuple(servers)

    def _read_cache_from_file(self) -> None:
        """Reads the cache from the cache file."""
        if not os.path.exists(self._broker_cache_fpath):
            alog.error(f"Cache file {self._broker_cache_fpath} does not exist.")
            return

        self.services.clear()
        self.name_mapping.clear()
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

    def _remove_service(self, fq_name: str, s: service.ServiceRegistration) -> None:
        """Removes a single server of the given service."""
        self.services[fq_name].pop(s, None)
        if not self.services[fq_name]:
            del self.services[fq_name]

        if s.simple_name in self.name_mapping:
            self.name_mapping[s.simple_name] = [name for name in self.name_mapping[s.simple_name] if name != fq_name]
            if not self.name_mapping[s.simple_name]:
                del self.name_mapping[s.simple_name]

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

    # If we are in unit-testing mode, we need to start up a mock RPCService in addition
    # to the RPC Broker.
    if util.mode() == constants.ArtieRunModes.UNIT_TESTING:
        alog.info("Starting test RPC service for unit-testing mode.")
        test_process = multiprocessing.Process(target=test_server.start_test_rpc_service, args=(args.port,))
        test_process.start()

    # Instantiate the single server instance and block forever, serving
    server = ArtieRPCBrokerServer(args.host, args.port, args.broker_cache_path)
    server.start()
