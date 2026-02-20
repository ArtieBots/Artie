"""
This module contains convenience functions for publishing datastreams in Artie services
and consuming from datastreams. A datastream is a stream of data published by a service on a particular topic.
The datastream is published at a certain frequency and can be subscribed to by other services.

This module also tries to provide a uniform API that encapsulates the details of how datastreams are implemented in Artie,
so that the implementation can be easily switched out if needed without affecting the rest of the codebase.
"""
from artie_util import artie_logging as alog
from artie_util import constants
import json
import kafka
import os
import ssl

def _get_bootstrap_servers() -> str:
    """
    Get the Kafka bootstrap servers from environment variables.
    Returns a string in the format "hostname:port".
    """
    hostname = os.getenv(constants.ArtieEnvVariables.ARTIE_PUBSUB_BROKER_HOSTNAME, 'localhost')
    port = os.getenv(constants.ArtieEnvVariables.ARTIE_PUBSUB_BROKER_PORT, '9092')
    return f"{hostname}:{port}"

def list_topics(timeout_s=10) -> list[str]:
    """
    List all topics currently in the pubsub broker.

    Args:
        timeout_s: Timeout in seconds for the list_topics operation (default: 10)

    Returns:
        A list of topic names
    """
    bootstrap_servers = _get_bootstrap_servers()
    alog.info(f"Connecting to Kafka broker at {bootstrap_servers} to list topics...")

    # Create SSL context that accepts self-signed certificates
    if os.getenv(constants.ArtieEnvVariables.ARTIE_PUBSUB_USE_SSL, 'false').lower() == 'true':
        ssl_context = ssl.create_default_context()
        ssl_context.check_hostname = False
        ssl_context.verify_mode = ssl.CERT_NONE
    else:
        ssl_context = None

    # Configure admin client with SSL support for self-signed certificates
    admin_client = kafka.KafkaAdminClient(bootstrap_servers=bootstrap_servers, request_timeout_ms=timeout_s*1000, security_protocol='SSL', ssl_context=ssl_context)
    topics = admin_client.list_topics()
    return topics

class ArtieStreamPublisher:
    """
    This class is a wrapper around the KafkaProducer that provides convenience functions for publishing datastreams in Artie services.
    It also provides a uniform API that encapsulates the details of how datastreams are implemented in Artie,
    so that the implementation can be easily switched out if needed without affecting the rest of the codebase.
    """
    def __init__(self, topic: str, service_name: str, compress=False, encrypt=True, certfpath=None, keyfpath=None, batch_size_bytes=16384, linger_ms=5, max_request_size_bytes=10*1024*1024):
        """
        Args
        ----

        - topic: The topic to publish the datastream on. This should typically be in the format <simple interface name>:<interface version>:<id> (e.g. "my-service:sensor-imu-v1:imu-1").
        - service_name: The name of the service publishing the datastream. This is used as the client ID for the Kafka producer and should typically be the same as the simple name of the service (e.g. "my-service").
        - compress: Whether to compress the data before publishing. This is useful for large datastreams or datastreams with high batching that would otherwise take up a lot of bandwidth. Compression is done using gzip.
        - encrypt: Whether to encrypt the data before publishing. Defaults to True. Disabling will provide a performance boost, but the datastream will not be encrypted and should not be used for sensitive data.
                 If encryption is enabled, the certfpath and keyfpath parameters must be provided and point to the appropriate certificate and key files for the service.
        - certfpath: The path to the certificate file to use for encryption. This is only used if encrypt is True.
        - keyfpath: The path to the key file to use for encryption. This is only used if encrypt is True.
        - batch_size_bytes: The batch size in bytes. The publisher will wait `linger_ms` to accumulate a batch of messages
                  that is at least this size before publishing. If the batch size is not reached after `linger_ms`,
                  the publisher will publish whatever messages it has accumulated. Tune `batch_size_bytes` and `linger_ms`
                  together to achieve the desired tradeoff between latency and throughput. If you want a snappy response,
                  disable linger_ms and set batch_size_bytes to the largest size you expect a single message to be.
                  On the other hand, if you want to maximize throughput and don't care about latency,
                  set a large batch_size_bytes and a large linger_ms - particularly if you enable encryption.
        - linger_ms: The linger time in milliseconds. See `batch_size_bytes` for more explanation.
        - max_request_size_bytes: The maximum request size in bytes. This is the maximum size of a single message that can be published.
                  If a batch exceeds this size, it will be rejected by the Kafka producer.
                  Tune this together with batch_size_bytes and linger_ms to achieve the desired performance.

        """
        # Check args
        self._topic = topic
        request_timeout_ms = 30000  # Kafka default is 30 seconds

        # Check if SSL should be used based on environment variable or explicit encrypt parameter
        use_ssl_env = os.getenv(constants.ArtieEnvVariables.ARTIE_PUBSUB_USE_SSL, 'false').lower() == 'true'
        use_ssl = encrypt or use_ssl_env

        self._producer = kafka.KafkaProducer(
            batch_size=batch_size_bytes,
            bootstrap_servers=_get_bootstrap_servers(),
            client_id=service_name,
            compression_type='gzip' if compress else None,
            delivery_timeout_ms=request_timeout_ms + linger_ms,
            metrics_enabled=True,
            linger_ms=linger_ms,
            max_request_size=max_request_size_bytes,
            request_timeout_ms=request_timeout_ms,
            security_protocol='SSL' if use_ssl else 'PLAINTEXT',
            ssl_check_hostname=False,
            ssl_cafile=None,
            ssl_certfile=certfpath if (use_ssl and certfpath) else None,  # Client cert for mTLS
            ssl_keyfile=keyfpath if (use_ssl and keyfpath) else None,  # Client key for mTLS
            value_serializer=lambda v: json.dumps(v).encode('utf-8'),
        )

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.flush()
        self._producer.close()

    @property
    def topic(self):
        return self._topic

    def flush(self, timeout=None):
        """
        Flush the producer to ensure all messages are sent.
        This is useful to call before shutting down the service to ensure all data is published.
        """
        self._producer.flush(timeout=timeout)

    def publish(self, data: dict):
        """
        Publish data. The data should be JSON-serializable. Typically, this format is
        specified by the interface that the datastream is associated with.
        """
        self._producer.send(self._topic, value=data)

    def publish_blocking(self, data: dict, timeout_s=None):
        """
        Publish data and block until the publish is successful. The data should be JSON-serializable. Typically, this format is
        specified by the interface that the datastream is associated with.
        """
        future = self._producer.send(self._topic, value=data)
        future.get(timeout=timeout_s)

class ArtieStreamSubscriber:
    """
    This class is a wrapper around the KafkaConsumer that provides convenience functions for consuming datastreams in Artie services.
    It also provides a uniform API that encapsulates the details of how datastreams are implemented in Artie,
    so that the implementation can be easily switched out if needed without affecting the rest of the codebase.
    """
    def __init__(self, topics: str|list[str]|tuple[str], service_name: str, fetch_min_bytes=1, certfpath=None, keyfpath=None, fetch_max_bytes=10*1024*1024, consumer_group_id=None, auto_offset_reset='latest'):
        """
        Args
        ----

        - topics: Either a single topic or a list/tuple of topics to subscribe to.
                The topics should typically be in the format <simple interface name>:<interface version>:<id>
                (e.g. "my-service:sensor-imu-v1:imu-1"). Topics can also be subscribed to later using the subscribe method.
        - service_name: The name of the service subscribing to the datastream. This is used as the client ID
                for the Kafka consumer and should typically be the same as the simple name of the service (e.g. "my-service").
        - fetch_min_bytes: The minimum amount of data the subscriber will fetch in a single request.
                This is useful to tune together with the publish batch size and linger time of the publisher to achieve
                the desired tradeoff between latency and throughput.
        - certfpath: The path to the certificate file to use for decryption. This is only used if the stream is encrypted.
        - keyfpath: The path to the key file to use for decryption. This is only used if the stream is encrypted.
        - fetch_max_bytes: The maximum amount of data the subscriber will fetch in a single request.
                This should typically be set to the same value as the max_request_size_bytes parameter of the publisher to ensure that messages are not rejected by the consumer for being too large.
        - consumer_group_id: The name of the consumer group for this subscriber to join.
                If None (the default), consumer groups will be disabled for this subscriber. Consumer groups allow multiple subscribers
                to share the workload of processing messages from the same topic, while still ensuring that each message
                is only processed by one subscriber in the group.
        - auto_offset_reset: What to do when there is no initial offset in Kafka or if the current offset does not exist.
                'earliest' will move to the oldest available message, 'latest' (the default) will move to the most recent.

        Usage
        -----
        ```python
        with ArtieStreamSubscriber("some-service:sensor-imu-v1:imu-1", service_name="this-service") as subscriber:
            for message in subscriber:
                data = message.value  # This will be a dictionary since we use a JSON deserializer
                # Do something with the data
        ```
        """
        if isinstance(topics, str):
            topics = [topics]

        # Check if SSL should be used based on environment variable or if certificates are provided
        use_ssl_env = os.getenv(constants.ArtieEnvVariables.ARTIE_PUBSUB_USE_SSL, 'false').lower() == 'true'
        use_ssl = use_ssl_env or (certfpath is not None and keyfpath is not None)

        self._consumer = kafka.KafkaConsumer(
            *topics,
            client_id=service_name,
            group_id=consumer_group_id,
            allow_auto_create_topics=True,
            auto_offset_reset=auto_offset_reset,
            bootstrap_servers=_get_bootstrap_servers(),
            fetch_min_bytes=fetch_min_bytes,
            fetch_max_bytes=fetch_max_bytes,
            security_protocol='SSL' if use_ssl else 'PLAINTEXT',
            ssl_check_hostname=False,
            ssl_cafile=None,
            ssl_certfile=certfpath if (use_ssl and certfpath) else None,  # Client cert for mTLS
            ssl_keyfile=keyfpath if (use_ssl and keyfpath) else None,  # Client key for mTLS
            value_deserializer=lambda m: json.loads(m.decode('utf-8')),
        )

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._consumer.close()

    def __iter__(self):
        """Allow iteration over the subscriber to receive messages."""
        return iter(self._consumer)

    def read(self, timeout_s=None):
        """
        Read a single message from the subscribed topics. This will block until a message is received or the timeout is reached.
        Returns the message value, which will be a dictionary since we use a JSON deserializer.
        """
        msg = next(self._consumer.poll(timeout_ms=timeout_s*1000).values())[0]
        return msg.value

    def read_batch(self, timeout_s=None):
        """
        Read a batch of messages from the subscribed topics. This will block until at least one message is received or the timeout is reached.
        Returns a list of message values, which will be dictionaries since we use a JSON deserializer.
        Returns whatever was received when the timeout is reached, even if it's an empty list.
        """
        msgs = []
        for msg in self._consumer.poll(timeout_ms=timeout_s*1000).values():
            msgs.extend(msg)
        return [m.value for m in msgs]

    def subscribe(self, topics: str|list[str]|tuple[str], pattern=None, listener=None):
        """
        Subscribe to a new topic or topics. This can be called multiple times to subscribe to additional topics.
        The topics should typically be in the format <simple interface name>:<interface version>:<id>
        (e.g. "my-service:sensor-imu-v1:imu-1").

        Args:
            - topics: The topic or topics to subscribe to.
            - pattern: An optional regex pattern to subscribe to topics.
              This is mutually exclusive with the `topics` parameter - you must specify one or the other, but not both.
              If you want to subscribe to specific topics and also use a pattern, you can call this method twice -
              once with the specific topics and once with the pattern.
            - listener: An optional listener to receive notifications about partition assignment and revocation.
              This is only used if you are using consumer groups and want to be notified about partition assignments.
              The listener should be an instance of kafka.coordinator.assignors.AbstractPartitionAssignor.AssignmentListener.
              This is provided for compatibility with the underlying KafkaConsumer API, but is not commonly used in typical Artie services.

        """
        self._consumer.subscribe(topics, pattern=pattern, listener=listener)

    def unsubscribe_all(self):
        """
        Unsubscribe from all topics.
        """
        return self._consumer.unsubscribe()

    def unsubscribe(self, topics: str|list[str]|tuple[str]=None):
        """
        Unsubscribe from a topic or topics.
        The topics should typically be in the format <simple interface name>:<interface version>:<id>

        If `topics` is None (the default), this will unsubscribe from all topics, which is the same as calling `unsubscribe_all()`.
        """
        if topics is None:
            return self.unsubscribe_all()

        # Save our current topic list:
        current_topics = set(self._consumer.subscription())

        # Remove the topics we want to unsubscribe from:
        if isinstance(topics, str):
            topics = [topics]

        for topic in topics:
            current_topics.discard(topic)

        # Unsubscribe from all topics and then re-subscribe to the updated topic list:
        self.unsubscribe_all()
        if current_topics:
            self.subscribe(list(current_topics))
