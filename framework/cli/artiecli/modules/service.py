from .. import common
from artie_service_client import pubsub
from artie_tooling import errors
from artie_util import constants
from rpyc.utils.registry import TCPRegistryClient
import argparse
import datetime
import json
import os

def _connect_registrar(args) -> TCPRegistryClient:
    registrar = TCPRegistryClient(os.environ.get(constants.ArtieEnvVariables.ARTIE_SERVICE_BROKER_HOSTNAME, "localhost"), int(os.environ.get(constants.ArtieEnvVariables.ARTIE_SERVICE_BROKER_PORT, 18864)))
    return registrar

def _cmd_list(args):
    registrar = _connect_registrar(args)
    common.format_print_result(registrar.list(filter_host=args.host), "service", "list", args.artie_id)

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

def _cmd_list_topics(args):
    """List all topics in the pubsub broker."""
    topics = pubsub.list_topics()
    common.format_print_result(f"Topics: {str(topics)}", "service", "list-topics", args.artie_id)

def _cmd_publish(args):
    """Publish a message to a topic."""
    # Parse the message data as JSON
    try:
        data = json.loads(args.data)
    except json.JSONDecodeError:
        raise ValueError(f"Invalid JSON data: {args.data}")

    # Create a publisher with optional encryption
    encrypt = bool(args.cert and args.key)

    # Publish the message
    try:
        with pubsub.ArtieStreamPublisher(topic=args.topic, service_name="artie-cli", certfpath=args.cert if encrypt else None, keyfpath=args.key if encrypt else None, encrypt=encrypt) as publisher:
            publisher.publish_blocking(data, timeout_s=10)
    except Exception as e:
        common.format_print_result(f"Error: {e}", "service", "publish", args.artie_id)
        return

    common.format_print_result(f"Success. Topic: {args.topic}", "service", "publish", args.artie_id)

def _cmd_subscribe(args):
    """Subscribe to a topic and print messages."""
    # Determine consumer group ID
    if args.consumer_group:
        consumer_group_id = args.consumer_group
    else:
        # Use a unique group ID if not specified
        consumer_group_id = f"artie-cli-{os.getpid()}-{int(datetime.datetime.now().timestamp())}"

    # Create a subscriber with optional encryption
    try:
        with pubsub.ArtieStreamSubscriber(topics=args.topic, service_name="artie-cli", consumer_group_id=consumer_group_id, certfpath=args.cert if (args.cert and args.key) else None, keyfpath=args.key if (args.cert and args.key) else None, auto_offset_reset='earliest') as subscriber:
            # Read messages
            messages_received = 0
            while messages_received < (args.count if args.count is not None else float('inf')):
                batch = subscriber.read_batch(timeout_s=args.timeout)
                if batch:
                    for msg in batch:
                        common.format_print_result({"topic": args.topic, "data": str(msg)}, "service", "subscribe", args.artie_id)
                        messages_received += 1
    except Exception as e:
        common.format_print_result(f"Error: {e}", "service", "subscribe", args.artie_id)

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

    ## List Topics
    list_topics_parser = subparsers.add_parser("list-topics", parents=[option_parser], help="List all topics in the pubsub broker")
    list_topics_parser.set_defaults(cmd=_cmd_list_topics)

    ## Publish
    publish_parser = subparsers.add_parser("publish", parents=[option_parser], help="Publish a message to a topic")
    publish_parser.add_argument("topic", type=str, help="The topic to publish to")
    publish_parser.add_argument("data", type=str, help="The message data as a JSON string")
    publish_parser.add_argument("--cert", type=str, default=None, help="Path to certificate file for encryption (optional)")
    publish_parser.add_argument("--key", type=str, default=None, help="Path to key file for encryption (optional)")
    publish_parser.set_defaults(cmd=_cmd_publish)

    ## Subscribe
    subscribe_parser = subparsers.add_parser("subscribe", parents=[option_parser], help="Subscribe to a topic and receive messages")
    subscribe_parser.add_argument("topic", type=str, help="The topic to subscribe to")
    subscribe_parser.add_argument("--consumer-group", type=str, default=None, help="The consumer group ID (optional, for load balancing)")
    subscribe_parser.add_argument("--count", type=int, default=None, help="Maximum number of messages to receive before exiting (optional)")
    subscribe_parser.add_argument("--timeout", type=int, default=30, help="Timeout in seconds to wait for messages (default: 30)")
    subscribe_parser.add_argument("--cert", type=str, default=None, help="Path to certificate file for encryption (optional)")
    subscribe_parser.add_argument("--key", type=str, default=None, help="Path to key file for encryption (optional)")
    subscribe_parser.set_defaults(cmd=_cmd_subscribe)
