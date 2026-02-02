#!/bin/bash

# If --test, check and exit
for arg in "$@"; do
    if [[ "$arg" == "--test" ]]; then
        gunicorn --check-config main:app
        exit 0
    fi
done

# If PORT is not set, default to 8782
PORT=${PORT:-8782}

mkdir -p /etc/artie-api-server/certs

# Invoke a quick script to create the certs if they don't exist.
python -c "from artie_util import util, constants
server_certfpath = '/etc/artie-api-server/certs/tls.crt'
server_keyfpath = '/etc/artie-api-server/certs/tls.key'
if util.mode() != constants.ArtieRunModes.PRODUCTION:
    util.generate_self_signed_cert(server_certfpath, server_keyfpath, days=None, force=True)
"

gunicorn -w 4 -b 0.0.0.0:${PORT} --certfile=/etc/artie-api-server/certs/tls.crt --keyfile=/etc/artie-api-server/certs/tls.key main:app
