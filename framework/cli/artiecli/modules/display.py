"""
CLI code for display interfaces, such as LCD or e-ink displays.
"""
from .. import common
from artie_tooling.api_clients import display_client
from artie_tooling import errors
import argparse

def _connect_client(args) -> common._ConnectionWrapper | display_client.DisplayClient:
    if common.in_test_mode(args):
        connection = common.connect("localhost", args.port, ipv6=args.ipv6)
    else:
        connection = display_client.DisplayClient(args.service_name, profile=args.artie_profile, integration_test=args.integration_test, unit_test=args.unit_test)
    return connection

def _cmd_display_list(args):
    client = _connect_client(args)
    common.format_print_result(client.display_list(), "display", "list", args.artie_id)

def _cmd_display_set(args):
    client = _connect_client(args)
    common.format_print_result(client.display_set(args.which, args.content), "display", "set", args.artie_id)

def _cmd_display_get(args):
    client = _connect_client(args)
    result = client.display_get(args.which)
    if issubclass(type(result), errors.HTTPError):
        common.format_print_result(result, "display", "get", args.artie_id)
    else:
        common.format_print_result(f"{args.which} Display value: {str(result)}", "display", "get", args.artie_id)

def _cmd_display_test(args):
    client = _connect_client(args)
    common.format_print_result(client.display_test(args.which), "display", "test", args.artie_id)

def _cmd_display_clear(args):
    client = _connect_client(args)
    common.format_print_result(client.display_clear(args.which), "display", "clear", args.artie_id)

def fill_subparser(parser: argparse.ArgumentParser, parent: argparse.ArgumentParser):
    subparsers = parser.add_subparsers(title="Commands", description="The display module's commands")

    # Args that are useful for all display module commands
    option_parser = argparse.ArgumentParser(parents=[parent], add_help=False)
    group = option_parser.add_argument_group("Display Module", "Display Module Options")
    group.add_argument("-n", "--service-name", type=str, default=None, required=True, help="The name of the service to connect to.")

    # Add all the commands
    list_parser = subparsers.add_parser("list", parents=[option_parser])
    list_parser.set_defaults(cmd=_cmd_display_list)

    set_parser = subparsers.add_parser("set", parents=[option_parser])
    set_parser.add_argument("which", type=str, help="Which display to set. Must match the name in the Artie HW Manifest. Use `cli display list` to see available displays.")
    set_parser.add_argument("content", type=str, help="The display content to set, as a base64-encoded string.")
    set_parser.set_defaults(cmd=_cmd_display_set)

    get_parser = subparsers.add_parser("get", parents=[option_parser])
    get_parser.add_argument("which", type=str, help="Which display to get. Must match the name in the Artie HW Manifest. Use `cli display list` to see available displays.")
    get_parser.set_defaults(cmd=_cmd_display_get)

    test_parser = subparsers.add_parser("test", parents=[option_parser])
    test_parser.add_argument("which", type=str, help="Which display to test. Must match the name in the Artie HW Manifest. Use `cli display list` to see available displays.")
    test_parser.set_defaults(cmd=_cmd_display_test)

    clear_parser = subparsers.add_parser("clear", parents=[option_parser])
    clear_parser.add_argument("which", type=str, help="Which display to clear. Must match the name in the Artie HW Manifest. Use `cli display list` to see available displays.")
    clear_parser.set_defaults(cmd=_cmd_display_clear)
