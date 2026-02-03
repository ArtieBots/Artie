"""
API interface for the base service interface (all services).
"""
from artie_util import artie_logging as alog
from artie_service_client import client as asc
import flask

service_api = flask.Blueprint('service_api', __name__, url_prefix="/<service>")

@service_api.route("/whoami", methods=["GET"])
@alog.function_counter("whoami", alog.MetricSWCodePathAPIOrder.CALLS)
def whoami(service: str):
    """
    Get the service's human-friendly name and its git hash.
    """
    try:
        s = asc.ServiceConnection(service)
        result = str(s.whoami())

        # Parse the result which is in format "name:git-hash"
        if ':' in result:
            name, git_hash = result.split(':', 1)
        else:
            name = result
            git_hash = "unknown"

        return {
            "service": service,
            "name": name,
            "git-hash": git_hash
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
            "error": f"Timed out trying to get service info: {e}"
        }
        return errbody, 504
    except Exception as e:
        errbody = {
            "service": service,
            "error": f"Error trying to get service info: {e}"
        }
        return errbody, 500
