#!/bin/bash

# Generate SSL certificates for Kafka broker using keytool (no openssl needed)
mkdir -p /etc/kafka/secrets

echo "Generating SSL certificates for Kafka broker using keytool..."

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
listeners=${KAFKA_LISTENERS:-SSL://0.0.0.0:9092,CONTROLLER://localhost:9093}
advertised.listeners=${KAFKA_ADVERTISED_LISTENERS:-SSL://localhost:9092}
listener.security.protocol.map=${KAFKA_LISTENER_SECURITY_PROTOCOL_MAP:-SSL:SSL,CONTROLLER:PLAINTEXT}
controller.listener.names=${KAFKA_CONTROLLER_LISTENER_NAMES:-CONTROLLER}

# Log configuration
log.dirs=/tmp/kafka-logs

# Replication
offsets.topic.replication.factor=${KAFKA_OFFSETS_TOPIC_REPLICATION_FACTOR:-1}
transaction.state.log.replication.factor=1
transaction.state.log.min.isr=1

# Topic configuration
num.partitions=${KAFKA_NUM_PARTITIONS:-3}

# SSL Configuration
ssl.keystore.location=/etc/kafka/secrets/kafka.keystore.p12
ssl.keystore.password=changeit
ssl.key.password=changeit
ssl.keystore.type=PKCS12
ssl.truststore.location=/etc/kafka/secrets/kafka.truststore.p12
ssl.truststore.password=changeit
ssl.truststore.type=PKCS12
ssl.client.auth=none
ssl.endpoint.identification.algorithm=
EOF

# Format storage if this is the first run
if [ ! -d "/tmp/kafka-logs" ]; then
    echo "Formatting KRaft storage..."
    export KAFKA_CLUSTER_ID=${KAFKA_CLUSTER_ID:-$(/opt/kafka/bin/kafka-storage.sh random-uuid)}
    /opt/kafka/bin/kafka-storage.sh format -t "$KAFKA_CLUSTER_ID" -c /etc/kafka/kraft/server.properties
fi

# Start Kafka
echo "Starting Kafka server..."
exec /opt/kafka/bin/kafka-server-start.sh /etc/kafka/kraft/server.properties
