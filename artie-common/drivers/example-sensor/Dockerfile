ARG ARTIE_BASE_IMG=artie/ardk-base:latest
FROM ${ARTIE_BASE_IMG}

# Copy in the source
COPY ./src /src

# Configuration stuff
ARG SERVICE_PORT=18865
ENV PORT=${SERVICE_PORT}
ENV ARTIE_RUN_MODE=production
ARG GIT_TAG=unversioned
ENV ARTIE_GIT_TAG=${GIT_TAG}
ENV ARTIE_LOG_LEVEL=INFO

# Default command is to run the application
WORKDIR /
CMD [ "/bin/bash", "-c", "python -m src.main --port ${PORT}" ]
