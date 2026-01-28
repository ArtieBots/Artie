from .. import common
from artie_tooling import errors
from artie_util import constants
from rpyc.utils.registry import TCPRegistryClient
import argparse
import os

def _connect_registrar(args) -> TCPRegistryClient:
    registrar = TCPRegistryClient(os.environ.get(constants.ArtieEnvVariables.RPC_BROKER_HOSTNAME, "localhost"), int(os.environ.get(constants.ArtieEnvVariables.RPC_BROKER_PORT, 18864)))
    return registrar

#########################################################################################
################################# List Subsystem ########################################
#########################################################################################

def _cmd_list(args):
    registrar = _connect_registrar(args)
    common.format_print_result(registrar.list(filter_host=args.host), "service", "list", args.artie_id)

#########################################################################################
################################# Query Subsystem #######################################
#########################################################################################

def _cmd_query(args):
    if args.name and args.interfaces:
        query = f"{args.name}:{','.join([i.strip() for i in args.interfaces.split(',')])}"
    elif args.name:
        query = args.name
    elif args.interfaces:
        query = ','.join([i.strip() for i in args.interfaces.split(',')])
    else:
        raise ValueError("You must specify at least one of --name or --interfaces to query for a service.")

    registrar = _connect_registrar(args)
    common.format_print_result(registrar.discover(query), "service", "query", args.artie_id)

#########################################################################################
################################## PARSERS ##############################################
#########################################################################################
def fill_subparser(parser: argparse.ArgumentParser, parent: argparse.ArgumentParser):
    subparsers = parser.add_subparsers(title="service", description="The service module's subcommands")

    # Args that are useful for all service module commands
    option_parser = argparse.ArgumentParser(parents=[parent], add_help=False)
    #group = option_parser.add_argument_group("Service Module", "Service Module Options")

    # Add all the commands for each subcommand
    ## List
    list_parser = subparsers.add_parser("list", parents=[option_parser])
    list_parser.add_argument("--host", type=str, default=None, help="Hostname to filter on. Only services on this host will be listed.")
    list_parser.set_defaults(cmd=_cmd_list)

    ## Query
    query_parser = subparsers.add_parser("query", parents=[option_parser])
    query_parser.add_argument("--name", type=str, default=None, help="The fully-qualified or simple name of the service to query.")
    query_parser.add_argument("--interfaces", type=str, default=None, help="Comma-separated list of interface names to query by.")
    query_parser.set_defaults(cmd=_cmd_query)
