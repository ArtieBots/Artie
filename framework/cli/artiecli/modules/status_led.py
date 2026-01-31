"""
CLI code for status LED interfaces.
"""
from .. import common
from artie_tooling.api_clients import status_led_client
from artie_tooling import errors
import argparse

def _connect_client(args) -> common._ConnectionWrapper | status_led_client.StatusLEDClient:
    if common.in_test_mode(args):
        connection = common.connect("localhost", args.port, ipv6=args.ipv6)
    else:
        connection = status_led_client.StatusLEDClient(profile=args.artie_profile, integration_test=args.integration_test, unit_test=args.unit_test)
    return connection

def _cmd_led_list(args):
    client = _connect_client(args)
    common.format_print_result(client.led_list(), "status-led", "list", args.artie_id)

def _cmd_led_set(args):
    client = _connect_client(args)
    common.format_print_result(client.led_set(args.which, args.state), "status-led", "set", args.artie_id)

def _cmd_led_get(args):
    client = _connect_client(args)
    common.format_print_result(client.led_get(args.which), "status-led", "get", args.artie_id)

def fill_subparser(parser: argparse.ArgumentParser, parent: argparse.ArgumentParser):
    subparsers = parser.add_subparsers(title="Commands", description="The status LED module's commands")

    # Args that are useful for all status LED module commands
    option_parser = argparse.ArgumentParser(parents=[parent], add_help=False)
    group = option_parser.add_argument_group("Status LED Module", "Status LED Module Options")
    group.add_argument("-n", "--service-name", type=str, default=None, required=True, help="The name of the service to connect to.")

    # Add all the commands
    list_parser = subparsers.add_parser("list", parents=[option_parser])
    list_parser.set_defaults(cmd=_cmd_led_list)

    set_parser = subparsers.add_parser("set", parents=[option_parser])
    set_parser.add_argument("which", type=str, help="Which LED to set. Must match the name in the Artie HW Manifest. Use `cli status-led list` to see available LEDs.")
    set_parser.add_argument("state", type=str, choices=["on", "off", "heartbeat"], help="The state to set the LED to.")
    set_parser.set_defaults(cmd=_cmd_led_set)

    get_parser = subparsers.add_parser("get", parents=[option_parser])
    get_parser.add_argument("which", type=str, help="Which LED to get. Must match the name in the Artie HW Manifest. Use `cli status-led list` to see available LEDs.")
    get_parser.set_defaults(cmd=_cmd_led_get)
