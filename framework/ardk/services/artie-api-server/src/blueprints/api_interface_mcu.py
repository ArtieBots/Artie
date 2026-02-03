"""
API interface for services that control MCUs.
"""
from artie_util import artie_logging as alog
from artie_service_client import client as asc
from flask import request as r
import flask

mcu_api = flask.Blueprint('mcu_api', __name__, url_prefix="/<service>")

@mcu_api.route("/mcu_reload_fw", methods=["POST"])
@alog.function_counter("mcu_reload_fw", alog.MetricSWCodePathAPIOrder.CALLS)
def mcu_reload_fw(service: str):
    """
    Reload MCU firmware for the given MCU.
    """
    # Get optional mcu-id parameter
    mcu_id = r.args.get('mcu-id', None)

    # Get the service and reload firmware
    try:
        s = asc.ServiceConnection(service)
        result = bool(s.mcu_fw_load(mcu_id))
        return {
            "service": service,
            "mcu-id": mcu_id,
            "success": result
        }
    except KeyError as e:
        errbody = {
            "service": service,
            "mcu-id": mcu_id,
            "error": f"Service or MCU not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "mcu-id": mcu_id,
            "error": f"Timed out trying to reload firmware: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "mcu-id": mcu_id,
            "error": f"Error trying to reload firmware: {e}"
        }
        return errbody, 500

@mcu_api.route("/mcu_list", methods=["GET"])
@alog.function_counter("mcu_list", alog.MetricSWCodePathAPIOrder.CALLS)
def mcu_list(service: str):
    """
    List all MCUs controlled by the given service.
    """
    try:
        s = asc.ServiceConnection(service)
        mcus = list(s.mcu_list())  # Necessary to copy the generator to a list for return
        return {
            "service": service,
            "mcu-names": mcus
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
            "error": f"Timed out trying to list MCUs: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "error": f"Error trying to list MCUs: {e}"
        }
        return errbody, 500

@mcu_api.route("/mcu_reset", methods=["POST"])
@alog.function_counter("mcu_reset", alog.MetricSWCodePathAPIOrder.CALLS)
def mcu_reset(service: str):
    """
    Reset the given MCU.
    """
    # Get optional mcu-id parameter
    mcu_id = r.args.get('mcu-id', None)

    # Get the service and reset MCU
    try:
        s = asc.ServiceConnection(service)
        s.mcu_reset(mcu_id)
        return {
            "service": service,
            "mcu-id": mcu_id,
            "status": "success"
        }
    except KeyError as e:
        errbody = {
            "service": service,
            "mcu-id": mcu_id,
            "error": f"Service or MCU not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "mcu-id": mcu_id,
            "error": f"Timed out trying to reset MCU: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "mcu-id": mcu_id,
            "error": f"Error trying to reset MCU: {e}"
        }
        return errbody, 500

@mcu_api.route("/mcu_self_check", methods=["POST"])
@alog.function_counter("mcu_self_check", alog.MetricSWCodePathAPIOrder.CALLS)
def mcu_self_check(service: str):
    """
    Run a self diagnostics check on the given MCU.
    """
    # Get optional mcu-id parameter
    mcu_id = r.args.get('mcu-id', None)

    # Get the service and run self check
    try:
        s = asc.ServiceConnection(service)
        s.mcu_self_check(mcu_id)
        return {
            "service": service,
            "mcu-id": mcu_id,
            "status": "success"
        }
    except KeyError as e:
        errbody = {
            "service": service,
            "mcu-id": mcu_id,
            "error": f"Service or MCU not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "mcu-id": mcu_id,
            "error": f"Timed out trying to run self check: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "mcu-id": mcu_id,
            "error": f"Error trying to run self check: {e}"
        }
        return errbody, 500

@mcu_api.route("/mcu_status", methods=["GET"])
@alog.function_counter("mcu_status", alog.MetricSWCodePathAPIOrder.CALLS)
def mcu_status(service: str):
    """
    Get the current status of the given MCU.
    """
    # Get optional mcu-id parameter
    mcu_id = r.args.get('mcu-id', None)

    # Get the service and get MCU status
    try:
        s = asc.ServiceConnection(service)
        status = str(s.mcu_status(mcu_id))
        return {
            "service": service,
            "mcu-id": mcu_id,
            "status": status
        }
    except KeyError as e:
        errbody = {
            "service": service,
            "mcu-id": mcu_id,
            "error": f"Service or MCU not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "mcu-id": mcu_id,
            "error": f"Timed out trying to get MCU status: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "mcu-id": mcu_id,
            "error": f"Error trying to get MCU status: {e}"
        }
        return errbody, 500

@mcu_api.route("/mcu_version", methods=["GET"])
@alog.function_counter("mcu_version", alog.MetricSWCodePathAPIOrder.CALLS)
def mcu_version(service: str):
    """
    Get the firmware version information for the given MCU.
    """
    # Get optional mcu-id parameter
    mcu_id = r.args.get('mcu-id', None)

    # Get the service and get MCU version
    try:
        s = asc.ServiceConnection(service)
        version = str(s.mcu_version(mcu_id))
        return {
            "service": service,
            "mcu-id": mcu_id,
            "version": version
        }
    except KeyError as e:
        errbody = {
            "service": service,
            "mcu-id": mcu_id,
            "error": f"Service or MCU not found: {e}"
        }
        return errbody, 404
    except TimeoutError as e:
        errbody = {
            "service": service,
            "mcu-id": mcu_id,
            "error": f"Timed out trying to get MCU version: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "mcu-id": mcu_id,
            "error": f"Error trying to get MCU version: {e}"
        }
        return errbody, 500
