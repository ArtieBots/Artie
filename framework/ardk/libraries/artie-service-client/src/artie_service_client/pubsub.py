"""
This module contains convenience functions for publishing datastreams in Artie services
and consuming from datastream. A datastream is a stream of data published by a service on a particular topic.
The datastream is published at a certain frequency and can be subscribed to by other services.

This module also tries to provide a uniform API that encapsulates the details of how datastreams are implemented in Artie,
so that the implementation can be easily switched out if needed without affecting the rest of the codebase.
"""
from artie_util import constants
import json
import kafka
import os

class ArtieStreamPublisher(kafka.KafkaProducer):
    """
    This class is a wrapper around the KafkaProducer that provides convenience functions for publishing datastreams in Artie services.
    It also provides a uniform API that encapsulates the details of how datastreams are implemented in Artie,
    so that the implementation can be easily switched out if needed without affecting the rest of the codebase.
    """
    def __init__(self, topic: str, service_name: str, compress=False, encrypt=True, certfpath=None, keyfpath=None, batch_size_bytes=16384, linger_ms=5, max_request_size_bytes=10*1024*1024):
        """
        Args
        ----

        topic: The topic to publish the datastream on. This should typically be in the format <simple interface name>:<interface version>:<id> (e.g. "my-service:sensor-imu-v1:imu-1").
        service_name: The name of the service publishing the datastream. This is used as the client ID for the Kafka producer and should typically be the same as the simple name of the service (e.g. "my-service").
        compress: Whether to compress the data before publishing. This is useful for large datastreams or datastreams with high batching that would otherwise take up a lot of bandwidth. Compression is done using gzip.
        encrypt: Whether to encrypt the data before publishing. Defaults to True. Disabling will provide a performance boost, but the datastream will not be encrypted and should not be used for sensitive data.
                 If encryption is enabled, the certfpath and keyfpath parameters must be provided and point to the appropriate certificate and key files for the service.
        certfpath: The path to the certificate file to use for encryption. This is only used if encrypt is True.
        keyfpath: The path to the key file to use for encryption. This is only used if encrypt is True.
        batch_size_bytes: The batch size in bytes. The publisher will wait `linger_ms` to accumulate a batch of messages
                  that is at least this size before publishing. If the batch size is not reached after `linger_ms`,
                  the publisher will publish whatever messages it has accumulated. Tune `batch_size_bytes` and `linger_ms`
                  together to achieve the desired tradeoff between latency and throughput. If you want a snappy response,
                  disable linger_ms and set batch_size_bytes to the largest size you expect a single message to be.
                  On the other hand, if you want to maximize throughput and don't care about latency,
                  set a large batch_size_bytes and a large linger_ms - particularly if you enable encryption.
        linger_ms: The linger time in milliseconds. See `batch_size_bytes` for more explanation.
        max_request_size_bytes: The maximum request size in bytes. This is the maximum size of a single message that can be published.
                  If a batch exceeds this size, it will be rejected by the Kafka producer.
                  Tune this together with batch_size_bytes and linger_ms to achieve the desired performance.

        """
        # Check args
        if encrypt and (certfpath is None or keyfpath is None):
            raise ValueError("If encryption is enabled, certfpath and keyfpath must be provided.")

        request_timeout_ms = 30000  # Kafka default is 30 seconds
        super().__init__(
            batch_size=batch_size_bytes,
            bootstrap_servers=f"{os.getenv(constants.ArtieEnvVariables.ARTIE_PUBSUB_BROKER_HOSTNAME, 'localhost')}:{os.getenv(constants.ArtieEnvVariables.ARTIE_PUBSUB_BROKER_PORT, '9092')}",
            client_id=service_name,
            compression_type='gzip' if compress else None,
            delivery_timeout_ms=request_timeout_ms + linger_ms,
            metrics_enabled=True,
            linger_ms=linger_ms,
            max_request_size=max_request_size_bytes,
            request_timeout_ms=request_timeout_ms,
            security_protocol='SSL' if encrypt else 'PLAINTEXT',
            ssl_certfile=certfpath if encrypt else None,
            ssl_keyfile=keyfpath if encrypt else None,
            value_serializer=lambda v: json.dumps(v).encode('utf-8'),
        )
        self._topic = topic

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.flush()
        self.close()

    @property
    def topic(self):
        return self._topic

    def flush(self):
        """
        Flush the producer to ensure all messages are sent.
        This is useful to call before shutting down the service to ensure all data is published.
        """
        super().flush()

    def publish(self, data: dict):
        """
        Publish data. The data should be JSON-serializable. Typically, this format is
        specified by the interface that the datastream is associated with.
        """
        self.send(self._topic, value=data)

    def publish_blocking(self, data: dict, timeout_s=None):
        """
        Publish data and block until the publish is successful. The data should be JSON-serializable. Typically, this format is
        specified by the interface that the datastream is associated with.
        """
        future = self.send(self._topic, value=data)
        future.get(timeout=timeout_s)

class ArtieStreamSubscriber(kafka.KafkaConsumer):
    """
    This class is a wrapper around the KafkaConsumer that provides convenience functions for consuming datastreams in Artie services.
    It also provides a uniform API that encapsulates the details of how datastreams are implemented in Artie,
    so that the implementation can be easily switched out if needed without affecting the rest of the codebase.
    """
    def __init__(self, topics: str|list[str]|tuple[str], service_name: str, fetch_min_bytes=1, certfpath=None, keyfpath=None, fetch_max_bytes=10*1024*1024):
        """
        Args
        ----

        topics: Either a single topic or a list/tuple of topics to subscribe to.
                The topics should typically be in the format <simple interface name>:<interface version>:<id>
                (e.g. "my-service:sensor-imu-v1:imu-1"). Topics can also be subscribed to later using the subscribe method.
        service_name: The name of the service subscribing to the datastream. This is used as the client ID
                and the group ID for the Kafka consumer and should typically be the same as the simple name of the service (e.g. "my-service").
        fetch_min_bytes: The minimum amount of data the subscriber will fetch in a single request.
                This is useful to tune together with the publish batch size and linger time of the publisher to achieve
                the desired tradeoff between latency and throughput.
        certfpath: The path to the certificate file to use for decryption. This is only used if the stream is encrypted.
        keyfpath: The path to the key file to use for decryption. This is only used if the stream is encrypted.
        fetch_max_bytes: The maximum amount of data the subscriber will fetch in a single request.
                This should typically be set to the same value as the max_request_size_bytes parameter of the publisher to ensure that messages are not rejected by the consumer for being too large.

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

        super().__init__(
            *topics,
            group_id=service_name,
            client_id=service_name,
            allow_auto_create_topics=True,
            bootstrap_servers=f"{os.getenv(constants.ArtieEnvVariables.ARTIE_PUBSUB_BROKER_HOSTNAME, 'localhost')}:{os.getenv(constants.ArtieEnvVariables.ARTIE_PUBSUB_BROKER_PORT, '9092')}",
            fetch_min_bytes=fetch_min_bytes,
            fetch_max_bytes=fetch_max_bytes,
            ssl_certfile=certfpath,
            ssl_keyfile=keyfpath,
        )

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def subscribe(self, topics: str|list[str]|tuple[str]):
        """
        Subscribe to a new topic or topics. This can be called multiple times to subscribe to additional topics.
        The topics should typically be in the format <simple interface name>:<interface version>:<id>
        (e.g. "my-service:sensor-imu-v1:imu-1").
        """
        super().subscribe(topics)

    def unsubscribe_all(self):
        """
        Unsubscribe from all topics.
        """
        return super().unsubscribe()

    def unsubscribe(self, topics: str|list[str]|tuple[str]):
        """
        Unsubscribe from a topic or topics.
        The topics should typically be in the format <simple interface name>:<interface version>:<id>
        """
        # Save our current topic list:
        current_topics = set(self.subscription())

        # Remove the topics we want to unsubscribe from:
        if isinstance(topics, str):
            topics = [topics]

        for topic in topics:
            current_topics.discard(topic)

        # Unsubscribe from all topics and then re-subscribe to the updated topic list:
        self.unsubscribe_all()
        if current_topics:
            self.subscribe(list(current_topics))
