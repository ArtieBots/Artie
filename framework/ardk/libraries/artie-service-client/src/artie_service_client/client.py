"""
The public interface for the various services is exposed through this module.
"""
from artie_util import artie_logging as alog
from artie_util import constants
from artie_util import util
from rpyc.utils import factory
from . import dns
import datetime
import rpyc

# A cache to store services that we have determined to be online
online_cache = set()

class ServiceConnection:
    """
    A ServiceConnection object exposes an easy-to-use API for calling
    a particular micro service. It handles all the connection attempts,
    logging, metrics collecting, and communication mechanism details
    so that clients that make use of this object do not need to worry
    about any of that.
    """
    def __init__(self, service_lookup: str|list[str]|dns.ServiceQuery, n_retries=3, timeout_s=None, ipv6=False) -> None:
        """
        Initialize the ServiceConnection object.

        Args:
            service_lookup: The fully-qualified name of the service to connect to,
                            a simple name of the service to connect to, a single
                            interface name, or a list of interface names.
                            If anything other than a fully-qualified name is given,
                            the first matching service will be used. If there is more than
                            one service that matches, the exact service that is chosen
                            is undefined.
            n_retries: The number of times to retry a failed RPC call.
            timeout_s: The maximum number of seconds to wait for the service to come online. If None, wait indefinitely.
            ipv6: Whether to use IPv6 when connecting to the service

        """
        self.n_retries = n_retries
        self.timeout_s = timeout_s
        self.ipv6 = ipv6
        self.service = dns.ServiceQuery.from_string(service_lookup) if isinstance(service_lookup, str) else service_lookup
        self.connection = self._initialize_connection(self.service)

    def __getattr__(self, attr):
        orig_attr = self.connection.root.__getattribute__(attr)
        if callable(orig_attr):
            def hooked(*args, **kwargs):
                result = self._retry_n_times(orig_attr, args, kwargs)
                if result == self.connection.root:
                    return self
                else:
                    return  result
            return hooked
        else:
            return orig_attr

    def __del__(self):
        try:
            self.connection.close()
        except Exception as e:
            alog.exception(f"Exception when trying to close service connection (service: {self.service}): ", e, stack_trace=True)

    def _retry_n_times(self, f, args, kwargs):
        for _ in range(self.n_retries):
            try:
                result = f(*args, **kwargs)
                return result
            except Exception as e:
                alog.exception(f"Exception when trying to run a function on a service connection (service: {self.service}): ", e, stack_trace=True)
                alog.update_counter(1, "connection", alog.MetricSWCodePathAPICallFamily.FAILURE, unit=alog.MetricUnits.CALLS, description="Number of times we encounter an error when trying to connect to an Artie service.")

    def _initialize_connection(self, service: dns.ServiceQuery) -> rpyc.Connection:
        block_until_online(service, timeout_s=self.timeout_s, ipv6=self.ipv6)
        host, port = dns.lookup(service)

        for _ in range(self.n_retries):
            try:
                return factory.ssl_connect(host, port, ipv6=self.ipv6)
            except Exception as e:
                alog.exception(f"Exception when trying to connect to {host}:{port}: ", e, stack_trace=True)
                alog.update_counter(1, "connection", alog.MetricSWCodePathAPICallFamily.FAILURE, unit=alog.MetricUnits.CALLS, description="Number of times we encounter an error when trying to connect to an Artie service.")

def _try_connect(host: str, port: int, ipv6=False) -> bool:
    """
    Attempts to connect to the given rpyc server and execute the whoami() method.
    Returns True if it succeeds, False otherwise.
    """
    connection = None
    try:
        connection = factory.ssl_connect(host, port, ipv6=ipv6)
        connection.root.whoami()
        return True
    except AttributeError as e:
        alog.error(f"Service running at {host}:{str(port)} does not have a 'whoami' method.")
        return True
    except Exception as e:
        alog.debug(f"Couldn't connect to {host}:{str(port)}")
    finally:
        if connection:
            connection.close()
    return False

def block_until_online(service: dns.ServiceQuery, timeout_s=30, ipv6=False):
    """
    Blocks until the given service is online.
    """
    # Check cache and return if already done
    global online_cache
    if service in online_cache:
        return

    alog.info(f"Waiting for {service} to come online...")

    # Lookup the service in the DNS
    host, port = dns.lookup(service)

    # Keep trying to connect forever if no timeout, or until timeout if we have one
    ts = datetime.datetime.now().timestamp()
    success = False
    if timeout_s is None:
        while not success:
            success = _try_connect(host, port, ipv6=ipv6)
    else:
        while not success and datetime.datetime.now().timestamp() - ts < timeout_s:
            success = _try_connect(host, port, ipv6=ipv6)

    # Add to cache or raise an error
    if success:
        online_cache.add(service)
    else:
        raise TimeoutError(f"Timeout while waiting for {service} to come online.")
