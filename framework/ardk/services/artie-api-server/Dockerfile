ARG ARTIE_BASE_IMG=artie/ardk-base:latest
FROM ${ARTIE_BASE_IMG}

# Pip requirements (openssl already comes from the base image)
RUN --mount=type=cache,target=/root/.cache/pip pip install flask==3.1.3 gunicorn==26.0.0

# Copy in the files (last, so source edits don't invalidate the layers above)
COPY ./src /app
RUN chmod +x /app/run.sh

# Configuration stuff
ARG SERVE_ON_PORT=8782
ENV PORT=${SERVE_ON_PORT}
ENV ARTIE_RUN_MODE=production
ARG GIT_TAG=unversioned
ENV ARTIE_GIT_TAG=${GIT_TAG}
ENV ARTIE_LOG_LEVEL=INFO

# Default command is to run the application
WORKDIR /app
CMD [ "./run.sh" ]
