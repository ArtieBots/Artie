from artie_util import artie_logging as alog
from artie_util import constants
from artie_util import util
import blueprints
import argparse
import flask
import os

if __name__ == "__main__":
    # Set up logging
    parser = argparse.ArgumentParser()
    parser.add_argument("--ipv6", action='store_true', help="Use IPv6 if given, otherwise IPv4.")
    parser.add_argument("-p", "--port", type=int, default=int(os.environ.get("PORT", 8782)), help="The port to run the server on.")
    parser.add_argument("--host", type=str, default=os.environ.get("HOST", "0.0.0.0"), help="The host to run the server on.")
    parser.add_argument("-l", "--loglevel", type=str, default=None, choices=["debug", "info", "warning", "error"], help="The log level.")
    args, _ = parser.parse_known_args()
    alog.init("artie-api-server", args)

    # Generate our self-signed certificate (if not already present)
    # These are used for RPC encryption between the API server and the Artie services.
    certfpath = "/etc/cert.pem"
    keyfpath = "/etc/pkey.pem"
    util.generate_self_signed_cert(certfpath, keyfpath, days=None, force=True)

    # We also have a cert for the API server itself to use for HTTPS.
    server_certfpath = "/etc/artie-api-server/certs/tls.crt"
    server_keyfpath = "/etc/artie-api-server/certs/tls.key"
    if util.mode() not in (constants.ArtieRunModes.INTEGRATION_TESTING, constants.ArtieRunModes.PRODUCTION):
        util.generate_self_signed_cert(server_certfpath, server_keyfpath, days=None, force=True)

    # Initialization
    app = flask.Flask(__name__)
    app.register_blueprint(blueprints.api_server_api.api_server_api)
    app.register_blueprint(blueprints.logs_api.logs_api)
    app.register_blueprint(blueprints.metrics_api.metrics_api)
    app.register_blueprint(blueprints.api_interface_mcu.mcu_api)
    app.register_blueprint(blueprints.api_interface_driver.driver_api)
    app.register_blueprint(blueprints.api_interface_servo.servo_api)
    app.register_blueprint(blueprints.api_interface_statusled.statusled_api)
    app.register_blueprint(blueprints.api_interface_service.service_api)

    # Run the server
    if args.ipv6:
        alog.info("Starting Artie API Server on IPv6 %s:%d", args.host, args.port)
        app.run(host=args.host, port=args.port, ssl_context=(server_certfpath, server_keyfpath), debug=(args.loglevel == "debug"), threaded=True)
    else:
        alog.info("Starting Artie API Server on IPv4 %s:%d", args.host, args.port)
        app.run(host=args.host, port=args.port, ssl_context=(server_certfpath, server_keyfpath), debug=(args.loglevel == "debug"), threaded=True)
