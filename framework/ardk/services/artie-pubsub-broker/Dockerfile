FROM apache/kafka:4.1.1

# Artie configuration stuff
ENV ARTIE_RUN_MODE=production
ARG GIT_TAG=unversioned
ENV ARTIE_GIT_TAG=${GIT_TAG}
ENV ARTIE_LOG_LEVEL=INFO

# Kafka Defaults. Can be overridden in docker-compose or in Helm Chart
ARG PUBSUB_BROKER_PORT=9092
ARG CONTROLLER_PORT=9093

# Use KRaft mode (so you don't need ZooKeeper)
ENV KAFKA_KRAFT_MODE=true

# Cluster ID. Can just be hard-coded here because
# different Arties are K3S-namespaced and so won't interfere with each other.
ENV KAFKA_CLUSTER_ID=artie-cluster

# Node ID for this Kafka broker (must be unique within the cluster)
# The Helm Chart will typically override this with a unique value for each
# replica, e.g. using the StatefulSet ordinal
ENV KAFKA_NODE_ID=1

# The Kafka controller is responsible for managing the cluster metadata
# and coordinating partition leadership. There is always exactly one
# controller in the cluster, and it is elected from the set of brokers.
# In Helm, when there are more than one brokers, this environment
# variable will be overridden for all the brokers that are not
# designated as the controller.
ENV KAFKA_PROCESS_ROLES=broker,controller

# This is the set of nodes (format: node_id@host:port,...)
# that are eligible to be elected and to vote for controller election.
# Note that the port specified here is the controller port, not the client port.
# The controller port is used for inter-broker communication and
# controller election, while the client port is used for communication
# between producers/consumers and the Kafka broker.
ENV KAFKA_CONTROLLER_QUORUM_VOTERS=1@localhost:${CONTROLLER_PORT}

# Kafka topics have a replication factor that determines how many
# copies of each partition are maintained across the cluster for
# fault tolerance.
# A partition is a unit of parallelism in Kafka, where each topic is
# divided into one or more partitions.
# The Helm Chart will typically override this with a value equal to
# the number of replicas in the StatefulSet.
ENV KAFKA_OFFSETS_TOPIC_REPLICATION_FACTOR=1

# The listeners that Kafka will bind to. We use SSL for client connections
# (producers/consumers) on PUBSUB_BROKER_PORT, and CONTROLLER for inter-broker
# communication and controller election on CONTROLLER_PORT.
# Bind to 0.0.0.0 to accept connections from any network interface (Docker network, localhost, etc.)
ENV KAFKA_LISTENERS=SSL://0.0.0.0:${PUBSUB_BROKER_PORT},CONTROLLER://localhost:${CONTROLLER_PORT}

# The advertised listeners that Kafka will tell clients to connect to.
# This is important for when the Kafka broker is running in a container,
# because the internal hostname and port may be different from what clients
# need to use to connect from outside the container.
# For Docker Compose, this will be overridden with the container name.
# Default to localhost for local development.
ENV KAFKA_ADVERTISED_LISTENERS=SSL://localhost:${PUBSUB_BROKER_PORT}

# Same thing for this, but for some reason you only have to specify "CONTROLLER"
ENV KAFKA_CONTROLLER_LISTENER_NAMES=CONTROLLER

# Default number of partitions for auto-created topics.
# Multiple partitions allow for parallel consumption by multiple consumers in the same consumer group.
# This should be set to at least the expected number of concurrent consumers for load balancing.
ENV KAFKA_NUM_PARTITIONS=3

# Security protocol mapping - SSL listener uses SSL protocol
ENV KAFKA_LISTENER_SECURITY_PROTOCOL_MAP=SSL:SSL,CONTROLLER:PLAINTEXT

# Copy and set up the entrypoint script that generates SSL certificates
# Run as root to allow certificate generation and let the original entrypoint handle user switching
USER root
COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
