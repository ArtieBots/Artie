"""
A unittesting server for the Artie RPC Broker service.
"""
from artie_service_client import artie_service
from artie_service_client import interfaces
from artie_util import artie_logging as alog
from artie_util import constants
from artie_util import util
import os
import socket
import time

SERVICE_NAME = "test-rpc-service"

class TestRPCService(
    interfaces.ServiceInterfaceV1,
    interfaces.DriverInterfaceV1,
    interfaces.StatusLEDInterfaceV1,
    artie_service.ArtieRPCService
    ):
    def __init__(self, port: int):
        super().__init__(SERVICE_NAME, port, register_service=True)

        # Tests will try to query this service and then access this data on the service object
        # if they successfully find it.
        self.test_data = "success"

        alog.info(f"Test RPC service started on port {port}.")

def _port_in_use(port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            s.bind(('localhost', port))
            return False
        except socket.error:
            return True

def start_test_rpc_service(broker_port: int):
    """
    Start the test RPC service on a random port,
    then wait for the broker to come online before registering the service.
    """
    port = broker_port + 1  # Use a port different from the broker's port

    # Tell the service client library to use the broker on the given port
    os.environ[constants.ArtieEnvVariables.RPC_BROKER_PORT] = str(broker_port)

    # Generate our self-signed certificate (if not already present)
    certfpath = "/etc/cert.pem"
    keyfpath = "/etc/pkey.pem"
    util.generate_self_signed_cert(certfpath, keyfpath, days=None, force=True)

    # Wait for the RPC Broker to come online
    alog.info(f"Waiting for RPC Broker to come online on port {broker_port}...")
    while not _port_in_use(broker_port):
        time.sleep(1)

    # Instantiate the single server instance and block forever, serving
    test_service = TestRPCService(port)
    t = util.create_rpc_server(test_service, keyfpath, certfpath, port)
    t.start()
