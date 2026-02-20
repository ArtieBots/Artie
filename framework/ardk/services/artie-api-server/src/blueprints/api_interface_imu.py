"""
API interface for services that provide IMU (Inertial Measurement Unit) sensors.
"""
from artie_util import artie_logging as alog
from artie_service_client import client as asc
from flask import request as r
import flask

imu_api = flask.Blueprint('imu_api', __name__, url_prefix="/<service>/imu")

@imu_api.route("/list", methods=["GET"])
@alog.function_counter("list_imus", alog.MetricSWCodePathAPIOrder.CALLS)
def list_imus(service: str):
    """
    List all IMU sensors provided by the given service.
    """
    try:
        s = asc.ServiceConnection(service)
        imus = list(s.imu_list())
        return {
            "service": service,
            "imu-ids": imus
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
            "error": f"Timed out trying to list IMUs: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "error": f"Error trying to list IMUs: {e}"
        }
        return errbody, 500

@imu_api.route("/whoami", methods=["GET"])
@alog.function_counter("imu_whoami", alog.MetricSWCodePathAPIOrder.CALLS)
def imu_whoami(service: str):
    """
    Get the name of a specific IMU sensor.
    """
    if 'which' not in r.args:
        errbody = {
            "service": service,
            "error": "Missing 'which' parameter."
        }
        return errbody, 400

    imu_id = r.args['which']

    try:
        s = asc.ServiceConnection(service)
        name = str(s.imu_whoami(imu_id))
        return {
            "service": service,
            "which": imu_id,
            "name": name
        }
    except KeyError as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Service or IMU not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Timed out trying to get IMU name: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Error trying to get IMU name: {e}"
        }
        return errbody, 500

@imu_api.route("/self-check", methods=["GET"])
@alog.function_counter("imu_self_check", alog.MetricSWCodePathAPIOrder.CALLS)
def imu_self_check(service: str):
    """
    Perform a self-check on a specific IMU sensor.
    """
    if 'which' not in r.args:
        errbody = {
            "service": service,
            "error": "Missing 'which' parameter."
        }
        return errbody, 400

    imu_id = r.args['which']

    try:
        s = asc.ServiceConnection(service)
        ok = bool(s.imu_self_check(imu_id))
        return {
            "service": service,
            "which": imu_id,
            "ok": ok
        }
    except KeyError as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Service or IMU not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Timed out trying to perform self-check: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Error trying to perform self-check: {e}"
        }
        return errbody, 500

@imu_api.route("/on", methods=["POST"])
@alog.function_counter("imu_on", alog.MetricSWCodePathAPIOrder.CALLS)
def imu_on(service: str):
    """
    Turn on a specific IMU sensor.
    """
    if 'which' not in r.args:
        errbody = {
            "service": service,
            "error": "Missing 'which' parameter."
        }
        return errbody, 400

    imu_id = r.args['which']

    try:
        s = asc.ServiceConnection(service)
        ok = bool(s.imu_on(imu_id))
        return {
            "service": service,
            "which": imu_id,
            "ok": ok
        }
    except KeyError as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Service or IMU not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Timed out trying to turn on IMU: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Error trying to turn on IMU: {e}"
        }
        return errbody, 500

@imu_api.route("/off", methods=["POST"])
@alog.function_counter("imu_off", alog.MetricSWCodePathAPIOrder.CALLS)
def imu_off(service: str):
    """
    Turn off a specific IMU sensor.
    """
    if 'which' not in r.args:
        errbody = {
            "service": service,
            "error": "Missing 'which' parameter."
        }
        return errbody, 400

    imu_id = r.args['which']

    try:
        s = asc.ServiceConnection(service)
        ok = bool(s.imu_off(imu_id))
        return {
            "service": service,
            "which": imu_id,
            "ok": ok
        }
    except KeyError as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Service or IMU not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Timed out trying to turn off IMU: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Error trying to turn off IMU: {e}"
        }
        return errbody, 500

@imu_api.route("/data", methods=["GET"])
@alog.function_counter("imu_get_data", alog.MetricSWCodePathAPIOrder.CALLS)
def imu_get_data(service: str):
    """
    Get the latest data from a specific IMU sensor.
    """
    if 'which' not in r.args:
        errbody = {
            "service": service,
            "error": "Missing 'which' parameter."
        }
        return errbody, 400

    imu_id = r.args['which']

    try:
        s = asc.ServiceConnection(service)
        data = dict(s.imu_get_data(imu_id))
        return {
            "service": service,
            "which": imu_id,
            "accelerometer": data.get('accelerometer'),
            "gyroscope": data.get('gyroscope'),
            "magnetometer": data.get('magnetometer'),
            "timestamp": data.get('timestamp')
        }
    except KeyError as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Service or IMU not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Timed out trying to get IMU data: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Error trying to get IMU data: {e}"
        }
        return errbody, 500

@imu_api.route("/start-stream", methods=["POST"])
@alog.function_counter("imu_start_stream", alog.MetricSWCodePathAPIOrder.CALLS)
def imu_start_stream(service: str):
    """
    Start streaming data from a specific IMU sensor.
    """
    if 'which' not in r.args:
        errbody = {
            "service": service,
            "error": "Missing 'which' parameter."
        }
        return errbody, 400

    imu_id = r.args['which']

    # Get optional frequency parameter from JSON body
    freq_hz = None
    try:
        data = r.get_json()
        if data and 'freq_hz' in data:
            freq_hz = float(data['freq_hz'])
    except (ValueError, TypeError) as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Invalid freq_hz value: {e}"
        }
        return errbody, 400
    except Exception as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Error parsing payload: {e}"
        }
        return errbody, 400

    try:
        s = asc.ServiceConnection(service)
        ok = bool(s.imu_start_stream(imu_id, freq_hz=freq_hz))
        response = {
            "service": service,
            "which": imu_id,
            "ok": ok
        }
        if freq_hz is not None:
            response['freq_hz'] = freq_hz
        return response
    except KeyError as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Service or IMU not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Timed out trying to start IMU stream: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Error trying to start IMU stream: {e}"
        }
        return errbody, 500

@imu_api.route("/stop-stream", methods=["POST"])
@alog.function_counter("imu_stop_stream", alog.MetricSWCodePathAPIOrder.CALLS)
def imu_stop_stream(service: str):
    """
    Stop streaming data from a specific IMU sensor.
    """
    if 'which' not in r.args:
        errbody = {
            "service": service,
            "error": "Missing 'which' parameter."
        }
        return errbody, 400

    imu_id = r.args['which']

    try:
        s = asc.ServiceConnection(service)
        ok = bool(s.imu_stop_stream(imu_id))
        return {
            "service": service,
            "which": imu_id,
            "ok": ok
        }
    except KeyError as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Service or IMU not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Timed out trying to stop IMU stream: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "which": imu_id,
            "error": f"Error trying to stop IMU stream: {e}"
        }
        return errbody, 500
