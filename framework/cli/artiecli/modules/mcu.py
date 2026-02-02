"""
CLI code for MCU interfaces.
"""
from .. import common
from artie_tooling.api_clients import mcu_client
from artie_tooling import errors
import argparse

def _connect_client(args) -> common._ConnectionWrapper | mcu_client.MCUClient:
    if common.in_test_mode(args):
        connection = common.connect("localhost", args.port, ipv6=args.ipv6)
    else:
        connection = mcu_client.MCUClient(args.service_name, profile=args.artie_profile, integration_test=args.integration_test, unit_test=args.unit_test)
    return connection

def _cmd_mcu_list(args):
    client = _connect_client(args)
    common.format_print_result(client.mcu_list(), "mcu", "list", args.artie_id)

def _cmd_mcu_reload_fw(args):
    client = _connect_client(args)
    common.format_print_result(client.mcu_fw_load(args.mcu_id), "mcu", "reload-fw", args.artie_id)

def _cmd_mcu_reset(args):
    client = _connect_client(args)
    common.format_print_result(client.mcu_reset(args.mcu_id), "mcu", "reset", args.artie_id)

def _cmd_mcu_self_check(args):
    client = _connect_client(args)
    common.format_print_result(client.mcu_self_check(args.mcu_id), "mcu", "self-check", args.artie_id)

def _cmd_mcu_status(args):
    client = _connect_client(args)
    result = client.mcu_status(args.mcu_id)
    if issubclass(type(result), errors.HTTPError):
        common.format_print_result(result, "mcu", "status", args.artie_id)
    else:
        common.format_print_result(f"MCU status: {result}", "mcu", "status", args.artie_id)

def _cmd_mcu_version(args):
    client = _connect_client(args)
    result = client.mcu_version(args.mcu_id)
    if issubclass(type(result), errors.HTTPError):
        common.format_print_result(result, "mcu", "version", args.artie_id)
    else:
        common.format_print_result(f"MCU version: {result}", "mcu", "version", args.artie_id)

def fill_subparser(parser: argparse.ArgumentParser, parent: argparse.ArgumentParser):
    subparsers = parser.add_subparsers(title="Commands", description="The MCU module's commands")

    # Args that are useful for all mcu module commands
    option_parser = argparse.ArgumentParser(parents=[parent], add_help=False)
    group = option_parser.add_argument_group("MCU Module", "MCU Module Options")
    group.add_argument("-n", "--service-name", type=str, default=None, required=True, help="The name of the service to connect to.")

    # Add all the commands
    list_parser = subparsers.add_parser("list", parents=[option_parser])
    list_parser.set_defaults(cmd=_cmd_mcu_list)

    reload_fw_parser = subparsers.add_parser("reload-fw", parents=[option_parser])
    reload_fw_parser.add_argument("mcu_id", type=str, default=None, help="The MCU ID to reload firmware for. Must match the ID in the Artie HW Manifest. Use `cli mcu list` to see available MCUs.")
    reload_fw_parser.set_defaults(cmd=_cmd_mcu_reload_fw)

    reset_parser = subparsers.add_parser("reset", parents=[option_parser])
    reset_parser.add_argument("mcu_id", type=str, default=None, help="The MCU ID to reset. Must match the ID in the Artie HW Manifest. Use `cli mcu list` to see available MCUs.")
    reset_parser.set_defaults(cmd=_cmd_mcu_reset)

    self_check_parser = subparsers.add_parser("self-check", parents=[option_parser])
    self_check_parser.add_argument("mcu_id", type=str, default=None, help="The MCU ID to run self check on. Must match the ID in the Artie HW Manifest. Use `cli mcu list` to see available MCUs.")
    self_check_parser.set_defaults(cmd=_cmd_mcu_self_check)

    status_parser = subparsers.add_parser("status", parents=[option_parser])
    status_parser.add_argument("mcu_id", type=str, default=None, help="The MCU ID to get status for. Must match the ID in the Artie HW Manifest. Use `cli mcu list` to see available MCUs.")
    status_parser.set_defaults(cmd=_cmd_mcu_status)

    version_parser = subparsers.add_parser("version", parents=[option_parser])
    version_parser.add_argument("mcu_id", type=str, default=None, help="The MCU ID to get version for. Must match the ID in the Artie HW Manifest. Use `cli mcu list` to see available MCUs.")
    version_parser.set_defaults(cmd=_cmd_mcu_version)
