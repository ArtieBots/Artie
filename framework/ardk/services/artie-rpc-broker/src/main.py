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
import argparse
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
        ##### TODO #######
        name = name.upper()
        self.logger.debug(f"querying for {name!r}")
        if name not in self.services:
            self.logger.debug("no such service")
            return ()

        oldest = time.time() - self.pruning_timeout
        all_servers = sorted(self.services[name].items(), key=lambda x: x[1])
        servers = []
        for addrinfo, t in all_servers:
            if t < oldest:
                self.logger.debug(f"discarding stale {addrinfo[0]}:{addrinfo[1]}")
                self._remove_service(name, addrinfo)
            else:
                servers.append(addrinfo)

        self.logger.debug(f"replying with {servers!r}")
        return tuple(servers)
        #################################

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
