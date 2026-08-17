FROM debian:bullseye

ARG ARCHITECTURE=amd64

# Prereqs
RUN apt-get update && apt-get install -y \
    openssl \
    python3 \
    python3-pip \
    wget

# Pip
RUN pip3 install requests==2.34.2

# Install Prometheus (checksums come from the release's sha256sums.txt)
WORKDIR /app
ARG PROMETHEUS_VERSION=2.37.8
ARG PROMETHEUS_SHA256_AMD64=d23a5b8d8ae864df941c75b381832f707127fdd2d8622fb80040cf7b5a668ebf
ARG PROMETHEUS_SHA256_ARM64=2172088577619231c6b7e0733affaa98ff65e1c5cbb4e741ee0c6aa02cbdd1d5
RUN wget --tries=3 https://github.com/prometheus/prometheus/releases/download/v${PROMETHEUS_VERSION}/prometheus-${PROMETHEUS_VERSION}.linux-${ARCHITECTURE}.tar.gz -O prometheus.tar.gz && \
    if [ "${ARCHITECTURE}" = "arm64" ]; then expected="${PROMETHEUS_SHA256_ARM64}"; else expected="${PROMETHEUS_SHA256_AMD64}"; fi && \
    echo "${expected}  prometheus.tar.gz" | sha256sum -c - && \
    tar -xzf prometheus.tar.gz -C . --strip-components=1 && rm prometheus.tar.gz

# Args for prometheus's shell script
ENV PORT=9090
ENV DATA_STORAGE_PATH=/data
ENV DATA_RETENTION_TIME=1y

# Prep some stuff for the launch script
RUN mkdir -p ${DATA_STORAGE_PATH}

# Copy in the config file
RUN mkdir -p /etc/prometheus
COPY ./prometheus.yaml /etc/prometheus/prometheus.yml

# Copy in the test config files
COPY ./test-configs/* /etc/prometheus/

# Copy in the launch script (LF line endings are guaranteed by .gitattributes)
COPY ./run.sh /run.sh
RUN chmod +x /run.sh

# Copy the test script as well
COPY ./test.py /app/test.py

CMD [ "/run.sh" ]
