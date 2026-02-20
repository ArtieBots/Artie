#!/bin/bash

# Check if SSL should be enabled
USE_SSL=${ARTIE_PUBSUB_USE_SSL:-false}

if [ "$USE_SSL" = "true" ]; then
    echo "SSL is enabled. Kafka will use SSL for encryption."

    # See: https://kafka.apache.org/42/security/encryption-and-authentication-using-ssl/
    #
    # SSL certs will need to be mounted at /etc/kafka/secrets with the following filenames:
    # - kafka.keystore.p12: The keystore containing the broker's SSL certificate and private key in PKCS12 format
    # - kafka.truststore.p12: The truststore containing the CA certificate(s) that the broker will use to verify client certificates, also in PKCS12 format
    #
    # Additionally, in each client that connects to the broker, you will neeed to
    # add the CA certificate(s) to the client's truststore.

    # Set SSL listener configuration
    # Use hostname for advertised listener if available, otherwise localhost
    HOSTNAME=${HOSTNAME:-localhost}
    BROKER_LISTENER="SSL://0.0.0.0:9092"
    ADVERTISED_LISTENER="SSL://${HOSTNAME}:9092"
    PROTOCOL_MAP="SSL:SSL,CONTROLLER:PLAINTEXT"
    INTER_BROKER_PROTOCOL="SSL"
else
    echo "SSL is disabled. Kafka will use PLAINTEXT."

    # Set PLAINTEXT listener configuration
    # Use hostname for advertised listener if available, otherwise localhost
    HOSTNAME=${HOSTNAME:-localhost}
    BROKER_LISTENER="PLAINTEXT://0.0.0.0:9092"
    ADVERTISED_LISTENER="PLAINTEXT://${HOSTNAME}:9092"
    PROTOCOL_MAP="PLAINTEXT:PLAINTEXT,CONTROLLER:PLAINTEXT"
    INTER_BROKER_PROTOCOL="PLAINTEXT"
fi

# Create directory for Kafka configuration
mkdir -p /etc/kafka/kraft

# Create Kafka server.properties directly to avoid issues with the configure script
echo "Creating Kafka server.properties..."
cat > /etc/kafka/kraft/server.properties << EOF
# Process roles
process.roles=${KAFKA_PROCESS_ROLES:-broker,controller}
node.id=${KAFKA_NODE_ID:-1}
controller.quorum.voters=${KAFKA_CONTROLLER_QUORUM_VOTERS:-1@localhost:9093}

# Listeners
listeners=${BROKER_LISTENER},CONTROLLER://localhost:9093
advertised.listeners=${ADVERTISED_LISTENER}
listener.security.protocol.map=${PROTOCOL_MAP}
controller.listener.names=${KAFKA_CONTROLLER_LISTENER_NAMES:-CONTROLLER}
inter.broker.listener.name=${INTER_BROKER_PROTOCOL}

# Log configuration
log.dirs=/tmp/kafka-logs

# Replication
offsets.topic.replication.factor=${KAFKA_OFFSETS_TOPIC_REPLICATION_FACTOR:-1}
transaction.state.log.replication.factor=1
transaction.state.log.min.isr=1

# Topic configuration
num.partitions=${KAFKA_NUM_PARTITIONS:-3}
EOF

# Add SSL configuration only if SSL is enabled
if [ "$USE_SSL" = "true" ]; then
    cat >> /etc/kafka/kraft/server.properties << EOF

# SSL Configuration
ssl.keystore.location=/etc/kafka/secrets/kafka.keystore.p12
ssl.keystore.password=changeit
ssl.key.password=changeit
ssl.keystore.type=PKCS12
ssl.truststore.location=/etc/kafka/secrets/kafka.truststore.p12
ssl.truststore.password=changeit
ssl.truststore.type=PKCS12
ssl.client.auth=requested
EOF
fi

# Format storage if this is the first run
if [ ! -d "/tmp/kafka-logs" ]; then
    echo "Formatting KRaft storage..."
    export KAFKA_CLUSTER_ID=${KAFKA_CLUSTER_ID:-$(/opt/kafka/bin/kafka-storage.sh random-uuid)}
    /opt/kafka/bin/kafka-storage.sh format -t "$KAFKA_CLUSTER_ID" -c /etc/kafka/kraft/server.properties
fi

# Start Kafka
echo "Starting Kafka server..."
exec /opt/kafka/bin/kafka-server-start.sh /etc/kafka/kraft/server.properties
