ARG ARTIE_BASE_IMG=artie/ardk-base:latest
FROM ${ARTIE_BASE_IMG}

# Copy in the source
COPY ./src /src

# Configuration stuff
ARG RPC_PORT=4242
ENV PORT=${RPC_PORT}
ENV LOGLEVEL=info
ENV ARTIE_RUN_MODE=production
ARG GIT_TAG=unversioned
ENV ARTIE_GIT_TAG=${GIT_TAG}

# Default command is to run the application
WORKDIR /src
CMD [ "/bin/bash", "-c", "python main.py --port ${PORT} --loglevel ${LOGLEVEL}" ]
