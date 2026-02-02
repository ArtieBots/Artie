"""
CLI code for driver interfaces.
"""
from .. import common
from artie_tooling.api_clients import driver_client
from artie_tooling import errors
import argparse
import json

def _connect_client(args) -> common._ConnectionWrapper | driver_client.DriverClient:
    if common.in_test_mode(args):
        connection = common.connect("localhost", args.port, ipv6=args.ipv6)
    else:
        connection = driver_client.DriverClient(args.service_name, profile=args.artie_profile, integration_test=args.integration_test, unit_test=args.unit_test)
    return connection

def _cmd_driver_status(args):
    client = _connect_client(args)
    result = client.status()
    if issubclass(type(result), errors.HTTPError):
        common.format_print_result(result, "driver", "status", args.artie_id)
    else:
        status_str = json.dumps(result, indent=2)
        common.format_print_status_result(status_str, "driver", args.artie_id)

def _cmd_driver_self_check(args):
    client = _connect_client(args)
    common.format_print_result(client.self_check(), "driver", "self-check", args.artie_id)

def fill_subparser(parser: argparse.ArgumentParser, parent: argparse.ArgumentParser):
    subparsers = parser.add_subparsers(title="Commands", description="The driver module's commands")

    # Args that are useful for all driver module commands
    option_parser = argparse.ArgumentParser(parents=[parent], add_help=False)
    group = option_parser.add_argument_group("Driver Module", "Driver Module Options")
    group.add_argument("-n", "--service-name", type=str, default=None, required=True, help="The name of the service to connect to.")

    # Add all the commands
    status_parser = subparsers.add_parser("status", parents=[option_parser])
    status_parser.set_defaults(cmd=_cmd_driver_status)

    self_check_parser = subparsers.add_parser("self-check", parents=[option_parser])
    self_check_parser.set_defaults(cmd=_cmd_driver_self_check)
