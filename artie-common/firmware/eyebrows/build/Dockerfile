ARG ARTIE_BASE_IMG=thisarg/isrequired:latest
FROM ${ARTIE_BASE_IMG} AS BASE_IMG

RUN mkdir -p /pico/src/graphics/lcd

# Build context is the repo root
COPY ./artie-common/firmware/eyebrows/src /pico/src
COPY ./framework/ardk/firmware/libraries/leds /pico/src/leds
COPY ./framework/ardk/firmware/libraries/errors /pico/src/errors
COPY ./framework/ardk/firmware/libraries/cmds /pico/src/cmds
WORKDIR /pico/src/build

ARG BUILD_TYPE=Debug
ARG LOG_LEVEL=INFO
RUN cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DLOG_LEVEL=${LOG_LEVEL} .. && make -j4

CMD [ "/bin/bash", "-c", "echo 'build artifacts are located in /pico/src/build/' && sleep infinity" ]
