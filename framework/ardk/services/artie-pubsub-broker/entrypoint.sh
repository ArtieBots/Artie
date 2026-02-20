#!/bin/bash

# Check if SSL should be enabled
USE_SSL=${ARTIE_PUBSUB_USE_SSL:-false}

if [ "$USE_SSL" = "true" ]; then
    echo "SSL is enabled. Generating SSL certificates for Kafka broker..."

    # Generate SSL certificates for Kafka broker using keytool (no openssl needed)
    mkdir -p /etc/kafka/secrets

    # Generate keystore with self-signed certificate using PKCS12 format (modern standard)
    keytool -genkeypair \
        -alias kafka \
        -keyalg RSA \
        -keysize 2048 \
        -keystore /etc/kafka/secrets/kafka.keystore.p12 \
        -storepass changeit \
        -keypass changeit \
        -dname "CN=kafka-broker, OU=Artie, O=Artie, L=Unknown, ST=Unknown, C=US" \
        -validity 365 \
        -storetype PKCS12 \
        -noprompt

    # Export the certificate from keystore
    keytool -exportcert \
        -alias kafka \
        -keystore /etc/kafka/secrets/kafka.keystore.p12 \
        -storepass changeit \
        -file /etc/kafka/secrets/kafka.crt \
        -noprompt

    # Create truststore and import the certificate (also PKCS12)
    keytool -importcert \
        -alias CARoot \
        -keystore /etc/kafka/secrets/kafka.truststore.p12 \
        -storepass changeit \
        -file /etc/kafka/secrets/kafka.crt \
        -storetype PKCS12 \
        -noprompt

    echo "SSL certificates generated and configured for Kafka broker."

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
ssl.endpoint.identification.algorithm=
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
