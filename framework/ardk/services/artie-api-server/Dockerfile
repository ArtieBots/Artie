ARG ARTIE_BASE_IMG=artie/ardk-base:latest
FROM ${ARTIE_BASE_IMG}

# Pip requirements
RUN pip install flask gunicorn

# Copy in the files
COPY ./src /app

# Install apt dependencies
RUN apt-get update && apt-get install -y openssl dos2unix
RUN apt-get clean && rm -rf /var/lib/apt/lists/*

RUN dos2unix /app/run.sh

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
