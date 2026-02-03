"""
API interface for services that make use of displays.
"""
import binascii
from artie_util import artie_logging as alog
from artie_service_client import client as asc
from flask import request as r
import base64
import flask

display_api = flask.Blueprint('display_api', __name__, url_prefix="/<service>/display")

@display_api.route("/list", methods=["GET"])
@alog.function_counter("list_displays", alog.MetricSWCodePathAPIOrder.CALLS)
def list_displays(service: str):
    """
    List all displays available on the given service.
    """
    # Get the service and list displays
    try:
        s = asc.ServiceConnection(service)
        displays = list(s.display_list())  # Necessary to copy the generator to a list for return
        return {
            "service": service,
            "displays": displays
        }
    except KeyError as e:
        errbody = {
            "service": service,
            "error": f"Service not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "error": f"Timed out trying to list displays: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "error": f"Error trying to list displays: {e}"
        }
        return errbody, 500

@display_api.route("/contents", methods=["POST"])
@alog.function_counter("set_display_contents", alog.MetricSWCodePathAPIOrder.CALLS)
def set_display_contents(service: str):
    """
    Set the contents of a specific display on the given service.
    """
    # Check params
    if 'which' not in r.args:
        errbody = {
            "service": service,
            "error": "Missing 'which' parameter."
        }
        return errbody, 400

    display_id = r.args['which']

    # Get the payload
    data = r.get_json()
    if not data or 'display' not in data:
        errbody = {
            "service": service,
            "which": display_id,
            "error": "Missing 'display' in JSON payload."
        }
        return errbody, 400

    # Get the service and set display contents
    try:
        s = asc.ServiceConnection(service)
        s.display_set(display_id, data['display'])
        return {
            "service": service,
            "which": display_id,
            "status": "success"
        }
    except binascii.Error as e:
        errbody = {
            "service": service,
            "which": display_id,
            "error": f"Display content is not valid base64: {e}"
        }
        return errbody, 400
    except TypeError as e:
        errbody = {
            "service": service,
            "which": display_id,
            "error": f"Display content has invalid type: {e}"
        }
        return errbody, 400
    except KeyError as e:
        errbody = {
            "service": service,
            "which": display_id,
            "error": f"Service or display not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "which": display_id,
            "error": f"Timed out trying to set display contents: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "which": display_id,
            "error": f"Error trying to set display contents: {e}"
        }
        return errbody, 500


@display_api.route("/contents", methods=["GET"])
@alog.function_counter("get_display_contents", alog.MetricSWCodePathAPIOrder.CALLS)
def get_display_contents(service: str):
    """
    Get the contents of a specific display on the given service.
    """
    # Check params
    if 'which' not in r.args:
        errbody = {
            "service": service,
            "error": "Missing 'which' parameter."
        }
        return errbody, 400

    display_id = r.args['which']

    # Get the service and get display contents
    try:
        s = asc.ServiceConnection(service)
        content = str(s.display_get(display_id))
        return {
            "service": service,
            "which": display_id,
            "content": content
        }
    except binascii.Error as e:
        errbody = {
            "service": service,
            "which": display_id,
            "error": f"Display content is not valid base64: {e}"
        }
        return errbody, 400
    except TypeError as e:
        errbody = {
            "service": service,
            "which": display_id,
            "error": f"Display content has invalid type: {e}"
        }
        return errbody, 400
    except KeyError as e:
        errbody = {
            "service": service,
            "which": display_id,
            "error": f"Service or display not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "which": display_id,
            "error": f"Timed out trying to get display contents: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "which": display_id,
            "error": f"Error trying to get display contents: {e}"
        }
        return errbody, 500

@display_api.route("/test", methods=["POST"])
@alog.function_counter("test_display", alog.MetricSWCodePathAPIOrder.CALLS)
def test_display(service: str):
    """
    Run a test pattern on the specified display.
    """
    # Check params
    if 'which' not in r.args:
        errbody = {
            "service": service,
            "error": "Missing 'which' parameter."
        }
        return errbody, 400

    display_id = r.args['which']

    # Get the service and test display
    try:
        s = asc.ServiceConnection(service)
        s.display_test(display_id)
        return {
            "service": service,
            "which": display_id,
            "status": "success"
        }
    except KeyError as e:
        errbody = {
            "service": service,
            "which": display_id,
            "error": f"Service or display not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "which": display_id,
            "error": f"Timed out trying to test display: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "which": display_id,
            "error": f"Error trying to test display: {e}"
        }
        return errbody, 500

@display_api.route("/off", methods=["POST"])
@alog.function_counter("clear_display", alog.MetricSWCodePathAPIOrder.CALLS)
def clear_display(service: str):
    """
    Clear the contents of the specified display.
    """
    # Check params
    if 'which' not in r.args:
        errbody = {
            "service": service,
            "error": "Missing 'which' parameter."
        }
        return errbody, 400

    display_id = r.args['which']

    # Get the service and clear display
    try:
        s = asc.ServiceConnection(service)
        s.display_clear(display_id)
        return {
            "service": service,
            "which": display_id,
            "status": "success"
        }
    except KeyError as e:
        errbody = {
            "service": service,
            "which": display_id,
            "error": f"Service or display not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "which": display_id,
            "error": f"Timed out trying to clear display: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "which": display_id,
            "error": f"Error trying to clear display: {e}"
        }
        return errbody, 500
