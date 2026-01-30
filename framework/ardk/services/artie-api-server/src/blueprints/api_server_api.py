"""
API for the Artie API Server itself.
"""
from artie_util import artie_logging as alog
from artie_service_client import dns
from flask import request as r
import flask

api_server_api = flask.Blueprint('api_server_api', __name__, url_prefix='/api_server')

@api_server_api.route('/list_services', methods=['GET'])
def list_services():
    """
    List all registered services in the API server.
    """
    # Check query parameters
    if 'filter_host' in r.args:
        filter_host = r.args.get('filter_host')
        alog.debug(f"Filtering services by host: {filter_host}")
    else:
        filter_host = None

    if 'filter_name' in r.args:
        filter_name = r.args.get('filter_name')
        alog.debug(f"Filtering services by name: {filter_name}")
    else:
        filter_name = None

    if 'filter_interfaces' in r.args:
        filter_interfaces = r.args.getlist('filter_interfaces')
        alog.debug(f"Filtering services by interfaces: {filter_interfaces}")
    else:
        filter_interfaces = None

    services = dns.list_services(filter_host=filter_host, filter_name=filter_name, filter_interfaces=filter_interfaces)
    alog.debug(f"Found {len(services)} services matching filters: {services}.")

    return {
        "filter_host": filter_host,
        "filter_name": filter_name,
        "filter_interfaces": filter_interfaces,
        "services": services
    }
