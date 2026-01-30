"""
API interface for driver services.
"""
from artie_util import artie_logging as alog
from artie_service_client import client as asc
import flask

driver_api = flask.Blueprint('driver_api', __name__, url_prefix="/<service>")

@driver_api.route("/status", methods=["GET"])
@alog.function_counter("get_driver_status", alog.MetricSWCodePathAPIOrder.CALLS)
def get_driver_status(service: str):
    """
    Get the service's submodules' statuses.
    """
    try:
        s = asc.ServiceConnection(service)
        statuses = s.status()
        return {
            "service": service,
            "submodule-statuses": statuses
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
            "error": f"Timed out trying to get status: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "error": f"Error trying to get status: {e}"
        }
        return errbody, 500

@driver_api.route("/self-test", methods=["POST"])
@alog.function_counter("driver_self_test", alog.MetricSWCodePathAPIOrder.CALLS)
def driver_self_test(service: str):
    """
    Initiate a self-test. Issue a status request to get the results.
    """
    try:
        s = asc.ServiceConnection(service)
        s.self_check()
        return {
            "service": service,
            "status": "success"
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
            "error": f"Timed out trying to run self-test: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "error": f"Error trying to run self-test: {e}"
        }
        return errbody, 500
