"""
CLI code for servo interfaces.
"""
from .. import common
from artie_tooling.api_clients import servo_client
from artie_tooling import errors
import argparse

def _connect_client(args) -> common._ConnectionWrapper | servo_client.ServoClient:
    if common.in_test_mode(args):
        connection = common.connect("localhost", args.port, ipv6=args.ipv6)
    else:
        connection = servo_client.ServoClient(profile=args.artie_profile, integration_test=args.integration_test, unit_test=args.unit_test)
    return connection

def _cmd_servo_list(args):
    client = _connect_client(args)
    common.format_print_result(client.servo_list(), "servo", "list", args.artie_id)

def _cmd_servo_set_position(args):
    client = _connect_client(args)
    common.format_print_result(client.servo_set_position(args.which, args.position), "servo", "set-position", args.artie_id)

def _cmd_servo_get_position(args):
    client = _connect_client(args)
    result = client.servo_get_position(args.which)
    common.format_print_result(f"{args.which} position: {result}", "servo", "get-position", args.artie_id)

def fill_subparser(parser: argparse.ArgumentParser, parent: argparse.ArgumentParser):
    subparsers = parser.add_subparsers(title="Commands", description="The servo module's commands")

    # Args that are useful for all servo module commands
    option_parser = argparse.ArgumentParser(parents=[parent], add_help=False)
    group = option_parser.add_argument_group("Servo Module", "Servo Module Options")
    group.add_argument("-n", "--service-name", type=str, default=None, required=True, help="The name of the service to connect to.")

    # Add all the commands
    list_parser = subparsers.add_parser("list", parents=[option_parser])
    list_parser.set_defaults(cmd=_cmd_servo_list)

    set_position_parser = subparsers.add_parser("set-position", parents=[option_parser])
    set_position_parser.add_argument("which", type=str, help="Which servo to set. Must match the name in the Artie HW Manifest. Use `cli servo list` to see available servos.")
    set_position_parser.add_argument("position", type=float, help="The position value to set (as a float).")
    set_position_parser.set_defaults(cmd=_cmd_servo_set_position)

    get_position_parser = subparsers.add_parser("get-position", parents=[option_parser])
    get_position_parser.add_argument("which", type=str, help="Which servo to get position for. Must match the name in the Artie HW Manifest. Use `cli servo list` to see available servos.")
    get_position_parser.set_defaults(cmd=_cmd_servo_get_position)
