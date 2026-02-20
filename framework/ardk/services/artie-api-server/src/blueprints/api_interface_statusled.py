"""
API interface for services that control status LEDs.
"""
from artie_util import artie_logging as alog
from artie_service_client import client as asc
from flask import request as r
import flask

statusled_api = flask.Blueprint('statusled_api', __name__, url_prefix="/<service>")

@statusled_api.route("/led/list", methods=["GET"])
@alog.function_counter("list_leds", alog.MetricSWCodePathAPIOrder.CALLS)
def list_leds(service: str):
    """
    List all status LEDs.
    """
    # Get the service and list LEDs
    try:
        s = asc.ServiceConnection(service)
        led_names = list(s.led_list())
        return {
            "service": service,
            "led-names": led_names
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
            "error": f"Timed out trying to list LEDs: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "error": f"Error trying to list LEDs: {e}"
        }
        return errbody, 500

@statusled_api.route("/led", methods=["POST"])
@alog.function_counter("set_led_state", alog.MetricSWCodePathAPIOrder.CALLS)
def set_led_state(service: str):
    """
    Update the state of a status LED.
    """
    # Check for 'state' parameter
    if 'state' not in r.args:
        errbody = {
            "service": service,
            "error": "Missing 'state' parameter."
        }
        return errbody, 400

    state = r.args['state']
    which = r.args.get('which', None)

    # Validate state
    if state not in ['on', 'off', 'heartbeat']:
        errbody = {
            "service": service,
            "which": which,
            "error": f"Invalid state '{state}'. Must be 'on', 'off', or 'heartbeat'."
        }
        return errbody, 400

    # Get the service and set LED state
    try:
        s = asc.ServiceConnection(service)
        result = bool(s.led_set(which, state))
        return {
            "service": service,
            "which": which,
            "state": state,
            "success": result
        }
    except KeyError as e:
        errbody = {
            "service": service,
            "which": which,
            "error": f"Service or LED not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "which": which,
            "error": f"Timed out trying to set LED state: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "which": which,
            "error": f"Error trying to set LED state: {e}"
        }
        return errbody, 500

@statusled_api.route("/led", methods=["GET"])
@alog.function_counter("get_led_state", alog.MetricSWCodePathAPIOrder.CALLS)
def get_led_state(service: str):
    """
    Get the current state of a status LED.
    """
    which = r.args.get('which', None)

    # Get the service and get LED state
    try:
        s = asc.ServiceConnection(service)
        state = str(s.led_get(which))
        return {
            "service": service,
            "which": which,
            "state": state
        }
    except KeyError as e:
        errbody = {
            "service": service,
            "which": which,
            "error": f"Service or LED not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "which": which,
            "error": f"Timed out trying to get LED state: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "which": which,
            "error": f"Error trying to get LED state: {e}"
        }
        return errbody, 500
