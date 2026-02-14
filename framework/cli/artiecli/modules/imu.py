"""
CLI code for IMU (Inertial Measurement Unit) interfaces.
"""
from .. import common
from artie_tooling.api_clients import imu_client
from artie_tooling import errors
import argparse
import json

def _connect_client(args) -> common._ConnectionWrapper | imu_client.IMUClient:
    if common.in_test_mode(args):
        connection = common.connect("localhost", args.port, ipv6=args.ipv6)
    else:
        connection = imu_client.IMUClient(args.service_name, profile=args.artie_profile, integration_test=args.integration_test, unit_test=args.unit_test)
    return connection

def _cmd_imu_list(args):
    client = _connect_client(args)
    common.format_print_result(client.imu_list(), "imu", "list", args.artie_id)

def _cmd_imu_whoami(args):
    client = _connect_client(args)
    result = client.imu_whoami(args.which)
    common.format_print_result(f"{args.which}: {result}", "imu", "whoami", args.artie_id)

def _cmd_imu_self_check(args):
    client = _connect_client(args)
    result = client.imu_self_check(args.which)
    if isinstance(result, errors.HTTPError):
        common.format_print_result(result, "imu", "self-check", args.artie_id)
    else:
        status = "working" if result else "not_working"
        common.format_print_result(f"{args.which}: {status}", "imu", "self-check", args.artie_id)

def _cmd_imu_on(args):
    client = _connect_client(args)
    result = client.imu_on(args.which)
    if isinstance(result, errors.HTTPError):
        common.format_print_result(result, "imu", "on", args.artie_id)
    else:
        status = "success" if result else "failed"
        common.format_print_result(f"{args.which}: {status}", "imu", "on", args.artie_id)

def _cmd_imu_off(args):
    client = _connect_client(args)
    result = client.imu_off(args.which)
    if isinstance(result, errors.HTTPError):
        common.format_print_result(result, "imu", "off", args.artie_id)
    else:
        status = "success" if result else "failed"
        common.format_print_result(f"{args.which}: {status}", "imu", "off", args.artie_id)

def _cmd_imu_get_data(args):
    client = _connect_client(args)
    result = client.imu_get_data(args.which)
    if isinstance(result, errors.HTTPError):
        common.format_print_result(result, "imu", "get-data", args.artie_id)
    else:
        # Format the data nicely
        output = {
            "imu_id": args.which,
            "timestamp": result['timestamp'],
            "accelerometer": result['accelerometer'],
            "gyroscope": result['gyroscope'],
            "magnetometer": result['magnetometer']
        }
        common.format_print_result(json.dumps(output, indent=2), "imu", "get-data", args.artie_id)

def _cmd_imu_start_stream(args):
    client = _connect_client(args)
    result = client.imu_start_stream(args.which, freq_hz=args.freq_hz)
    if isinstance(result, errors.HTTPError):
        common.format_print_result(result, "imu", "start-stream", args.artie_id)
    else:
        status = "success" if result else "failed"
        freq_msg = f" at {args.freq_hz} Hz" if args.freq_hz else ""
        common.format_print_result(f"{args.which}: stream started{freq_msg} - {status}", "imu", "start-stream", args.artie_id)

def _cmd_imu_stop_stream(args):
    client = _connect_client(args)
    result = client.imu_stop_stream(args.which)
    if isinstance(result, errors.HTTPError):
        common.format_print_result(result, "imu", "stop-stream", args.artie_id)
    else:
        status = "success" if result else "failed"
        common.format_print_result(f"{args.which}: stream stopped - {status}", "imu", "stop-stream", args.artie_id)

def fill_subparser(parser: argparse.ArgumentParser, parent: argparse.ArgumentParser):
    subparsers = parser.add_subparsers(title="Commands", description="The IMU module's commands")

    # Args that are useful for all IMU module commands
    option_parser = argparse.ArgumentParser(parents=[parent], add_help=False)
    group = option_parser.add_argument_group("IMU Module", "IMU Module Options")
    group.add_argument("-n", "--service-name", type=str, default=None, required=True, help="The name of the service to connect to.")

    # Add all the commands
    list_parser = subparsers.add_parser("list", parents=[option_parser], help="List all available IMU sensor IDs")
    list_parser.set_defaults(cmd=_cmd_imu_list)

    whoami_parser = subparsers.add_parser("whoami", parents=[option_parser], help="Get the name of an IMU sensor")
    whoami_parser.add_argument("which", type=str, help="Which IMU sensor to query. Use 'artie-cli imu list' to see available IMUs.")
    whoami_parser.set_defaults(cmd=_cmd_imu_whoami)

    self_check_parser = subparsers.add_parser("self-check", parents=[option_parser], help="Perform a self-check on an IMU sensor")
    self_check_parser.add_argument("which", type=str, help="Which IMU sensor to check. Use 'artie-cli imu list' to see available IMUs.")
    self_check_parser.set_defaults(cmd=_cmd_imu_self_check)

    on_parser = subparsers.add_parser("on", parents=[option_parser], help="Turn on an IMU sensor")
    on_parser.add_argument("which", type=str, help="Which IMU sensor to turn on. Use 'artie-cli imu list' to see available IMUs.")
    on_parser.set_defaults(cmd=_cmd_imu_on)

    off_parser = subparsers.add_parser("off", parents=[option_parser], help="Turn off an IMU sensor")
    off_parser.add_argument("which", type=str, help="Which IMU sensor to turn off. Use 'artie-cli imu list' to see available IMUs.")
    off_parser.set_defaults(cmd=_cmd_imu_off)

    get_data_parser = subparsers.add_parser("get-data", parents=[option_parser], help="Get the latest data from an IMU sensor")
    get_data_parser.add_argument("which", type=str, help="Which IMU sensor to get data from. Use 'artie-cli imu list' to see available IMUs.")
    get_data_parser.set_defaults(cmd=_cmd_imu_get_data)

    start_stream_parser = subparsers.add_parser("start-stream", parents=[option_parser], help="Start streaming data from an IMU sensor")
    start_stream_parser.add_argument("which", type=str, help="Which IMU sensor to start streaming. Use 'artie-cli imu list' to see available IMUs.")
    start_stream_parser.add_argument("--freq-hz", type=float, default=None, help="Optional: Desired streaming frequency in Hz")
    start_stream_parser.set_defaults(cmd=_cmd_imu_start_stream)

    stop_stream_parser = subparsers.add_parser("stop-stream", parents=[option_parser], help="Stop streaming data from an IMU sensor")
    stop_stream_parser.add_argument("which", type=str, help="Which IMU sensor to stop streaming. Use 'artie-cli imu list' to see available IMUs.")
    stop_stream_parser.set_defaults(cmd=_cmd_imu_stop_stream)
