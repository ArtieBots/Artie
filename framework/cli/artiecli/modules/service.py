from .. import common
from artie_service_client import pubsub
from artie_tooling import errors
from artie_util import constants
from rpyc.utils.registry import TCPRegistryClient
import argparse
import json
import os
import rpyc
import sys
import time

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
    try:
        topics = pubsub.list_topics()
        common.format_print_result({"topics": topics}, "service", "list-topics", args.artie_id)
    except Exception as e:
        common.format_print_result({"error": str(e)}, "service", "list-topics", args.artie_id)
        sys.exit(1)

def _cmd_publish(args):
    """Publish a message to a topic."""
    try:
        # Parse the message data as JSON
        try:
            data = json.loads(args.data)
        except json.JSONDecodeError:
            raise ValueError(f"Invalid JSON data: {args.data}")

        # Create a publisher with optional encryption
        encrypt = bool(args.cert and args.key)
        publisher = pubsub.ArtieStreamPublisher(
            topic=args.topic,
            service_name="artie-cli",
            certfpath=args.cert if encrypt else None,
            keyfpath=args.key if encrypt else None,
            encrypt=encrypt,
        )

        # Publish the message
        publisher.publish_blocking(data, timeout_s=10)
        publisher.close()

        common.format_print_result({
            "success": True,
            "topic": args.topic,
        }, "service", "publish", args.artie_id)
    except Exception as e:
        common.format_print_result({"error": str(e)}, "service", "publish", args.artie_id)
        sys.exit(1)

def _cmd_subscribe(args):
    """Subscribe to a topic and print messages."""
    try:
        # Determine consumer group ID
        if args.consumer_group:
            consumer_group_id = args.consumer_group
        else:
            # Use a unique group ID if not specified
            consumer_group_id = f"artie-cli-{os.getpid()}-{int(time.time())}"

        # Create a subscriber with optional encryption
        subscriber = pubsub.ArtieStreamSubscriber(
            topics=args.topic,
            service_name="artie-cli",
            consumer_group_id=consumer_group_id,
            certfpath=args.cert if (args.cert and args.key) else None,
            keyfpath=args.key if (args.cert and args.key) else None,
            auto_offset_reset='earliest',
        )

        # Print subscription confirmation
        print(f"Subscribed to topic: {args.topic}")
        if args.consumer_group:
            print(f"Consumer group: {args.consumer_group}")
        print(f"Waiting for messages (timeout: {args.timeout}s)...")
        print("---")

        # Read messages
        messages_received = 0
        start_time = time.time()

        for message in subscriber:
            messages_received += 1
            output = {
                "topic": message.topic,
                "partition": message.partition,
                "offset": message.offset,
                "timestamp": message.timestamp,
                "value": message.value
            }
            print(json.dumps(output, indent=2))

            # Check if we should stop based on count or timeout
            if args.count and messages_received >= args.count:
                break
            if time.time() - start_time >= args.timeout:
                break

        subscriber.close()
        print(f"\n--- Received {messages_received} message(s) ---")

    except Exception as e:
        common.format_print_result({"error": str(e)}, "service", "subscribe", args.artie_id)
        sys.exit(1)

def _cmd_call(args):
    """Call an RPC method on a service."""
    try:
        # Query the service to get its address
        registrar = _connect_registrar(args)
        results = registrar.discover(args.service_name)

        if not results:
            common.format_print_result({"error": f"Service '{args.service_name}' not found"}, "service", "call", args.artie_id)
            sys.exit(1)

        # Connect to the first result
        host, port = results[0]
        connection = common.connect(host, port, ipv6=args.ipv6)

        # Parse arguments as JSON if provided
        method_args = []
        method_kwargs = {}
        if args.args:
            try:
                parsed = json.loads(args.args)
                if isinstance(parsed, list):
                    method_args = parsed
                elif isinstance(parsed, dict):
                    method_kwargs = parsed
                else:
                    method_args = [parsed]
            except json.JSONDecodeError:
                # Treat as a single string argument
                method_args = [args.args]

        # Call the method
        method = getattr(connection, args.method)
        result = method(*method_args, **method_kwargs)

        common.format_print_result({"result": result}, "service", "call", args.artie_id)

    except Exception as e:
        common.format_print_result({"error": str(e)}, "service", "call", args.artie_id)
        sys.exit(1)

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

    ## Call (RPC)
    call_parser = subparsers.add_parser("call", parents=[option_parser], help="Call an RPC method on a service")
    call_parser.add_argument("service_name", type=str, help="The name of the service to call")
    call_parser.add_argument("method", type=str, help="The name of the method to call")
    call_parser.add_argument("--args", type=str, default=None, help="Arguments for the method as JSON (optional)")
    call_parser.set_defaults(cmd=_cmd_call)
