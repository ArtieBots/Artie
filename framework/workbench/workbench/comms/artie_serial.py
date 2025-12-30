"""
This module contains the code for communicating with an Artie over serial to its
Controller Node.
"""
from artie_tooling import hw_config
from workbench.comms import base
from workbench.util import error
from workbench.util import log
import dataclasses
import datetime
import re
import serial
import serial.tools.list_ports
import time

@dataclasses.dataclass
class WifiNetwork:
    """Represents a WiFi network with its details."""
    bssid: str
    frequency: int
    signal_level: int
    flags: str
    ssid: str

@dataclasses.dataclass
class StaticIPConfig:
    """Configuration for static IP settings"""
    ip_address: str
    subnet_mask: str = None
    gateway: str = None
    dns: str = None

class ArtieSerialConnection(base.ArtieCommsBase):
    def __init__(self, port: str = None, baudrate: int = 115200, timeout: float = 1.0, logging_handler=None):
        super().__init__(logging_handler)
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout

        # The underlying connection
        self._serial_connection = None

    @staticmethod
    def list_ports() -> list[str]:
        """List all available ports."""
        # TODO: Filter out all ports that can't possibly be an Artie Controller Node
        #       based on VID/PID, etc. See: https://www.pyserial.com/docs/api-reference#listportinfo-properties
        ports = serial.tools.list_ports.comports()
        return [port.device for port in ports]

    def close(self):
        """Close the connection."""
        super().close()
        if self._serial_connection and self._serial_connection.is_open:
            self._serial_connection.close()

    def open(self):
        """Open the underlying connection."""
        super().open()
        if self.port:
            self._serial_connection = serial.Serial(port=self.port, baudrate=self.baudrate, timeout=self.timeout)

    def get_hardware_config(self) -> tuple[Exception|None, hw_config.HWConfig|None]:
        """Get the hardware configuration from the connected Artie."""
        if not self._serial_connection or not self._serial_connection.is_open:
            return error.SerialConnectionError("Connection not open."), None

        # TODO: Implement the actual logic to retrieve the hardware configuration
        return None, hw_config.HWConfig(
            artie_type_name="artieTestType",
            sbcs=[hw_config.SBC(name="controller-node", architecture="linux/arm64", buses=["i2c0"])],
            mcus=[hw_config.MCU(name="mcu0", buses=["i2c0"])],
            sensors=[],
            actuators=[]
        )

    def scan_for_wifi_networks(self) -> tuple[Exception, list[WifiNetwork]]:
        """Scan for wifi networks and return a list of them."""
        if not self._serial_connection or not self._serial_connection.is_open:
            return error.SerialConnectionError("Connection not open."), []

        # First, stop any existing wpa_cli instances
        err, _ = self._run_cmd("wpa_cli terminate".encode())
        if err:
            return err, []

        # Remove any existing wpa_supplicant PID files
        err, _ = self._run_cmd("rm -f /var/run/wpa_supplicant/wlan0".encode(), check_return_code=True)
        if err:
            return err, []

        # Stop any existing wpa_supplicant systemd services
        err, _ = self._run_cmd("systemctl stop wpa_supplicant@wlan0".encode())
        if err:
            return err, []

        err, _ = self._run_cmd("systemctl stop wpa_supplicant".encode())
        if err:
            return err, []

        # Now start a new wpa_supplicant instance in the background
        err, _ = self._run_cmd("wpa_supplicant -B -i wlan0 -c /etc/wpa_supplicant/wpa_supplicant-wlan0.conf".encode(), check_return_code=True)
        if err:
            return err, []

        # Initiate a scan
        err, _ = self._run_cmd("wpa_cli scan".encode(), check_return_code=True)
        if err:
            return err, []

        # Wait a bit for the scan to complete
        time.sleep(0.1)

        # Retrieve the scan results
        err, lines = self._run_cmd("wpa_cli scan_results".encode())
        if err:
            return err, []

        # Parse the scan results into WifiNetwork objects
        # A typical line looks like this:
        # bssid              frequency signal_level flags                   ssid
        # But there are plenty of lines that do not conform to this format,
        # so we use a regex to extract the fields.
        lines = lines.splitlines()
        pattern = re.compile(r'^(?P<bssid>([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2})\s+(?P<frequency>\d+)\s+(?P<signal_level>(-)?\d+)\s+(?P<flags>.*)\s+(?P<ssid>.*)$')  # hex:hex:hex:hex:hex:hex<whitespace>frequency<whitespace>signal_level<whitespace>flags<whitespace>ssid
        networks = []
        for line in lines:
            match = pattern.match(line)
            if match:
                log.info(f"Found a network: SSID={match.group('ssid')}, BSSID={match.group('bssid')}, Signal Level={match.group('signal_level')}, Frequency={match.group('frequency')}, Flags={match.group('flags')}")
                network = WifiNetwork(
                    bssid=match.group('bssid'),
                    frequency=int(match.group('frequency')),
                    signal_level=int(match.group('signal_level')),
                    flags=match.group('flags'),
                    ssid=match.group('ssid')
                )
                networks.append(network)

        return None, networks

    def select_wifi(self, bssid: str, ssid: str, password: str, static_ip: StaticIPConfig = None) -> Exception|None:
        """Select the wifi network and enter its password."""
        if not self._serial_connection or not self._serial_connection.is_open:
            return error.SerialConnectionError("Connection not open.")

        # Check if there is already a network
        err, response_lines = self._run_cmd('wpa_cli list_networks'.encode())
        if err:
            return err

        if len(response_lines.splitlines()) > 1:
            # How many networks are there?
            pattern = re.compile(r'^(?P<id>\d+)\s+$')
            network_ids = []
            for line in response_lines[1:]:
                match = pattern.match(line)
                if match:
                    network_ids.append(match.group('id'))

            # Remove existing networks
            for network_id in network_ids:
                err, _ = self._run_cmd(f'wpa_cli remove_network {network_id}'.encode(), check_return_code=True)
                if err:
                    return err

        # Add a new network
        err, response_lines = self._run_cmd(f'wpa_cli add_network'.encode(), check_return_code=True)
        if err:
            return err

        # The network ID is the first line that just has a single integer
        network_id = None
        for line in response_lines:
            line = line.strip()
            if line.isdigit():
                network_id = line
                break

        if network_id is None:
            return error.SerialConnectionError("Could not determine network ID from wpa_cli add_network response.")

        # Set the SSID and password
        err, _ = self._run_cmd(f'wpa_cli set_network {network_id} bssid "{bssid}"'.encode(), check_return_code=True)
        if err:
            return err

        err, _ = self._run_cmd(f'wpa_cli set_network {network_id} ssid \'"{ssid}"\''.encode(), check_return_code=True)
        if err:
            return err

        err = self._write_line(f"wpa_passphrase '{ssid}' '{password}'".encode(), log_mask=password)
        if err:
            return err

        # Parse out the PSK from the output
        psk = None
        err, data = self._read_until("}".encode(), log_mask=password)
        if err:
            return err
        for line in data.decode().splitlines():
            psk_match = re.match(r'\s+psk=(.+)', line)
            if psk_match:
                psk = psk_match.group(1)
                break

        # Clear the bash history to avoid leaving the password in there
        err = self._write_line("history -c".encode())
        if err:
            return err

        err, _ = self._run_cmd(f'wpa_cli set_network {network_id} psk "{psk}"'.encode(), check_return_code=True)
        if err:
            return err

        err, _ = self._run_cmd(f'wpa_cli enable_network {network_id}'.encode(), check_return_code=True)
        if err:
            return err

        err, _ = self._run_cmd('wpa_cli save_config'.encode(), check_return_code=True)
        if err:
            return err

        if static_ip is not None:
            err = self._set_static_ip(static_ip)
            if err:
                return err

        # Stop the wpa_supplicant instance we started
        err, _ = self._run_cmd("wpa_cli terminate".encode())
        if err:
            return err, []

        # Now set wpa_supplicant in systemd
        err, _ = self._run_cmd("systemctl enable wpa_supplicant@wlan0".encode(), check_return_code=True)
        if err:
            return err

        err, _ = self._run_cmd("systemctl start wpa_supplicant@wlan0".encode(), check_return_code=True)
        if err:
            return err

        # Wait a bit for wpa_supplicant to start
        deadline = datetime.datetime.now() + datetime.timedelta(seconds=10)
        while (result := self._run_cmd("systemctl status wpa_supplicant@wlan0 -l".encode(), check_return_code=True)):
            err, status_output = result
            if err:
                return err

            # There are color codes in the output, so we have to ignore them
            pattern = re.compile(r'\x1b\[[0-9;]*m')
            status_output = pattern.sub('', status_output)
            if "Active: active (running)" in status_output and "CTRL-EVENT-CONNECTED" in status_output:
                break

            if datetime.datetime.now() > deadline:
                # We did our best. Maybe it is working anyway. Just return and let
                # the caller verify the connection later.
                log.warning("Timeout waiting for wpa_supplicant to start. Proceeding anyway.")
                break
            else:
                time.sleep(1)

        # Ensure we wait at least a few seconds. Pinging seems to be unavailable for at least a few seconds.
        log.info("Waiting a few seconds to ensure WiFi connection is fully established...")
        time.sleep(5)

        return None

    def set_credentials(self, username: str, password: str) -> Exception|None:
        """Set the credentials on the connected Artie."""
        if not self._serial_connection or not self._serial_connection.is_open:
            return error.SerialConnectionError("Connection not open.")

        # TODO: On artie-image-dev, we only have a root user, so ignore this for now
        #       Otherwise, we would implement the logic to change the default user's name and password here
        err = self._sign_in(username, password)
        if err:
            return err

    def verify_wifi_connection(self) -> tuple[Exception|None, str|None]:
        """Verify that Artie is connected to the wifi network and return the IP address."""
        if not self._serial_connection or not self._serial_connection.is_open:
            return error.SerialConnectionError("Connection not open."), None

        # Verify connection by checking that we can ping this machine or 8.8.8.8
        err = self._write_line("ping -c 3 8.8.8.8".encode())
        if err:
            return err, None

        err, data = self._read_until("3 packets transmitted".encode(), timeout_s=10.0)
        if err:
            return err, None

        if '0 received' in data.decode():
            return Exception("Test packets not received. WiFi connection may have failed."), None

        # Return the IP address of Artie
        err = self._write_line("ip addr show wlan0 | grep 'inet '".encode())
        if err:
            return err, None

        err, inet_lines = self._read_all_lines()
        if err:
            return err, None

        # Search for the IP address in the output
        ip_match = re.search(r'inet (?P<ip>(\d+\.\d+\.\d+\.\d+))', "\n".join(inet_lines))
        if not ip_match:
            return Exception("Could not find IP address."), None

        ip_address = ip_match.group('ip')
        return None, ip_address

    def _read_all_lines(self) -> tuple[Exception, list[str]|None]:
        """Read all available lines from the serial connection."""
        lines = []
        while True:
            try:
                line = self._serial_connection.readline()
                log.debug("<-- " + line.decode(errors='ignore').strip())
            except serial.SerialException as e:
                return error.SerialConnectionError(str(e)), None

            if not line:
                break

            lines.append(line.decode().strip())

        return None, lines

    def _read_until(self, terminator_or_regex: bytes|re.Pattern, timeout_s=None, log_mask=None) -> tuple[Exception, bytes|None]:
        """
        Read from the serial connection until the terminator is found or
        until the regular expression pattern is matched.
        """
        # Determine if we have a terminator or a regex pattern
        pattern = None
        is_regex = False
        if issubclass(type(terminator_or_regex), re.Pattern):
            pattern = terminator_or_regex
            is_regex = True
        else:
            terminator = terminator_or_regex

        # Update timeout if provided
        old_timeout = self._serial_connection.timeout
        if timeout_s is not None:
            self._serial_connection.timeout = timeout_s

        # Read out bytes until we find the terminator or match the regex
        # (or we timeout)
        buffer = b''
        while True:
            try:
                byte = self._serial_connection.read(1)
            except serial.SerialException as e:
                self._serial_connection.timeout = old_timeout
                return error.SerialConnectionError(str(e)), None

            if not byte:
                # Timeout reached
                log.warning(f"Read {buffer.replace(log_mask.encode(), b'***') if log_mask else buffer} from serial but did not find terminator or match regex ({terminator_or_regex}) before timeout.")
                self._serial_connection.timeout = old_timeout
                return error.SerialConnectionError(f"Read timeout while looking for {terminator_or_regex}."), None

            buffer += byte

            if is_regex:
                if pattern.search(buffer.decode(errors='ignore')):
                    log.debug(f"<-- {buffer.replace(log_mask.encode(), b'***').decode(errors='ignore') if log_mask else buffer.decode(errors='ignore')}")
                    self._serial_connection.timeout = old_timeout
                    return None, buffer
            else:
                if buffer.endswith(terminator):
                    log.debug(f"<-- {buffer.replace(log_mask.encode(), b'***').decode(errors='ignore') if log_mask else buffer.decode(errors='ignore')}")
                    self._serial_connection.timeout = old_timeout
                    return None, buffer

    def _run_cmd(self, command: bytes, check_return_code=False) -> tuple[Exception, str|None]:
        """Run a command on the serial connection and return its output."""
        err = self._write_line(command)
        if err:
            return err, None

        # username@host:path#
        pattern = re.compile(r"^(?P<user>[\w\-]+)@(?P<host>[\w\-]+):(?P<path>.+)#\s*$", re.MULTILINE)
        err, data = self._read_until(pattern)
        if err:
            return err, None

        # Extract just the command output (everything except the last line)
        lines = data.decode().splitlines()

        # Check return code
        if check_return_code:
            self._serial_connection.write(b'echo $?\n')
            err, ret_code_lines = self._read_all_lines()
            if err:
                return err, None

            if not any([line.strip() == '0' for line in ret_code_lines]):
                ret_code_str = ", ".join(ret_code_lines)
                log.error(f"Command returned non-zero exit code: {ret_code_str}")
                return error.SerialConnectionError(f"Command returned non-zero exit code: {ret_code_str}"), None

        return None, "\n".join(lines[:-1])

    def _sign_in(self, username: str, password: str) -> Exception|None:
        """Sign in to Artie with the provided credentials."""
        if not self._serial_connection or not self._serial_connection.is_open:
            return serial.SerialException("Connection not open.")

        try:
            self._read_until("login: ".encode())
        except error.SerialConnectionError:
            pass  # Possibly already logged in

        # TODO: Implement the actual sign-in and username/password change logic for artie-image-release
        self._write_line("root".encode())

    def _write_line(self, data: bytes, log_mask: str = None) -> Exception|None:
        """Write a line to the serial connection."""
        try:
            log.debug(f"--> {data.replace(log_mask.encode(), b'***').decode(errors='ignore').strip() if log_mask else data.decode(errors='ignore').strip()}")
            self._serial_connection.write(data + b'\r\n')
            _ = self._serial_connection.read(len(data + b'\r\n'))  # Echo back
            return None
        except serial.SerialException as e:
            return error.SerialConnectionError(str(e))

    def _set_static_ip(self, static_ip: StaticIPConfig) -> Exception|None:
        """Configure static IP settings on the Artie."""
        # Get the DNS IP
        if not static_ip.dns:
            err, ip_lines = self._run_cmd("cat /etc/resolv.conf | grep nameserver".encode(), check_return_code=True)
            if err:
                return err

            dns_ip = None
            for line in ip_lines.splitlines():
                match = re.match(r'nameserver (?P<ip>(\d+\.\d+\.\d+\.\d+))', line)
                if match:
                    dns_ip = match.group('ip')
                    break

            if not dns_ip:
                return error.SerialConnectionError("Could not determine DNS IP from /etc/resolv.conf.")
        else:
            dns_ip = static_ip.dns

        # Get the Gateway IP
        if not static_ip.gateway:
            err, route_lines = self._run_cmd("ip route | grep default".encode(), check_return_code=True)
            if err:
                return err

            gateway_ip = None
            for line in route_lines.splitlines():
                match = re.match(r'default via (?P<ip>(\d+\.\d+\.\d+\.\d+))', line)
                if match:
                    gateway_ip = match.group('ip')
                    break

            if not gateway_ip:
                return error.SerialConnectionError("Could not determine Gateway IP from ip route.")
        else:
            gateway_ip = static_ip.gateway

        # Determine the subnet mask (CIDR notation or full address)
        if static_ip.subnet_mask:
            # If subnet mask is provided, convert to CIDR if it's in dotted decimal notation
            if '.' in static_ip.subnet_mask:
                # Convert subnet mask like "255.255.255.0" to CIDR like "/24"
                cidr = sum([bin(int(x)).count('1') for x in static_ip.subnet_mask.split('.')])
                address_with_mask = f"{static_ip.ip_address}/{cidr}"
            else:
                # Assume it's already in CIDR format
                address_with_mask = f"{static_ip.ip_address}/{static_ip.subnet_mask}"
        else:
            # Default to /24 if no subnet mask provided
            address_with_mask = f"{static_ip.ip_address}/24"

        # Update the network configuration
        err = self._write_line(f'echo -e "[Match]\\nName=wlan0\\n\\n[Network]\\nAddress={address_with_mask}\\nGateway={gateway_ip}\\nDNS={dns_ip}" > /etc/systemd/network/80-wifi-station.network'.encode())
        if err:
            return err

        return None
