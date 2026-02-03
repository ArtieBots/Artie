"""
API interface for services that control servos.
"""
from artie_util import artie_logging as alog
from artie_service_client import client as asc
from flask import request as r
import flask

servo_api = flask.Blueprint('servo_api', __name__, url_prefix="/<service>/servo")

@servo_api.route("/list", methods=["GET"])
@alog.function_counter("list_servos", alog.MetricSWCodePathAPIOrder.CALLS)
def list_servos(service: str):
    """
    List all servos controlled by the given service.
    """
    try:
        s = asc.ServiceConnection(service)
        servos = list(s.servo_list())  # Necessary to copy the generator to a list for return
        return {
            "service": service,
            "servo-names": servos
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
            "error": f"Timed out trying to list servos: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "error": f"Error trying to list servos: {e}"
        }
        return errbody, 500

@servo_api.route("/position", methods=["POST"])
@alog.function_counter("set_servo_position", alog.MetricSWCodePathAPIOrder.CALLS)
def set_servo_position(service: str):
    """
    Set the position of a specific servo.
    """
    # Check for 'which' parameter
    if 'which' not in r.args:
        errbody = {
            "service": service,
            "error": "Missing 'which' parameter."
        }
        return errbody, 400

    servo_id = r.args['which']

    # Get the payload
    try:
        data = r.get_json()
        if not data or 'position' not in data:
            errbody = {
                "service": service,
                "which": servo_id,
                "error": "Missing 'position' in JSON payload."
            }
            return errbody, 400

        position = float(data['position'])
    except (ValueError, TypeError) as e:
        errbody = {
            "service": service,
            "which": servo_id,
            "error": f"Invalid position value: {e}"
        }
        return errbody, 400
    except Exception as e:
        errbody = {
            "service": service,
            "which": servo_id,
            "error": f"Error parsing payload: {e}"
        }
        return errbody, 400

    # Get the service and set servo position
    try:
        s = asc.ServiceConnection(service)
        s.servo_set_position(servo_id, position)
        return {
            "service": service,
            "which": servo_id,
            "position": position,
            "status": "success"
        }
    except KeyError as e:
        errbody = {
            "service": service,
            "which": servo_id,
            "error": f"Service or servo not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "which": servo_id,
            "error": f"Timed out trying to set servo position: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "which": servo_id,
            "error": f"Error trying to set servo position: {e}"
        }
        return errbody, 500

@servo_api.route("/position", methods=["GET"])
@alog.function_counter("get_servo_position", alog.MetricSWCodePathAPIOrder.CALLS)
def get_servo_position(service: str):
    """
    Get the current position of a specific servo.
    """
    # Check for 'which' parameter
    if 'which' not in r.args:
        errbody = {
            "service": service,
            "error": "Missing 'which' parameter."
        }
        return errbody, 400

    servo_id = r.args['which']

    # Get the service and get servo position
    try:
        s = asc.ServiceConnection(service)
        position = float(s.servo_get_position(servo_id))
        return {
            "service": service,
            "which": servo_id,
            "position": position
        }
    except KeyError as e:
        errbody = {
            "service": service,
            "which": servo_id,
            "error": f"Service or servo not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "which": servo_id,
            "error": f"Timed out trying to get servo position: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "which": servo_id,
            "error": f"Error trying to get servo position: {e}"
        }
        return errbody, 500
