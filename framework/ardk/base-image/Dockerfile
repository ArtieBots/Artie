# Base Image for Artie Docker components
FROM python:3.11.2-bullseye

# Install dependencies
RUN apt-get update && apt-get install -y \
    i2c-tools \
    libusb-0.1-4 \
    libusb-1.0-0 \
    libftdi1 \
    openssl \
    cmake \
    build-essential
RUN --mount=type=cache,target=/root/.cache/pip pip install \
    rpyc==6.0.2 \
    kafka-python==3.0.11

# Build the ardk libraries. They all go in one layer so that a change to any
# one of them only invalidates a single (cheap, local-install) layer.
COPY ./tmp/artie-util /tmp/ardk/artie-util/
COPY ./tmp/artie-tooling /tmp/ardk/artie-tooling/
COPY ./tmp/artie-i2c /tmp/ardk/artie-i2c/
COPY ./tmp/artie-gpio /tmp/ardk/artie-gpio/
COPY ./tmp/artie-service-client /tmp/ardk/artie-service-client/
COPY ./tmp/artie-can /tmp/ardk/artie-can/
RUN --mount=type=cache,target=/root/.cache/pip pip install \
    /tmp/ardk/artie-util \
    /tmp/ardk/artie-tooling \
    /tmp/ardk/artie-i2c \
    "/tmp/ardk/artie-gpio[rpi]" \
    /tmp/ardk/artie-service-client \
    /tmp/ardk/artie-can \
    && rm -rf /tmp/ardk
WORKDIR /
