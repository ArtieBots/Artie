"""
API client for communicating with the API Server's driver endpoints.

Each `*Response` object corresponds to the response from a specific API endpoint.

See [API documentation](../../../../../misc-micro-services/artie-api-server/README.md) for more details.
"""
from . import api_client
from .. import errors
from artie_service_client.interfaces import driver as driver_interface

class DriverClient(
    driver_interface.DriverInterfaceV1,
    api_client.APIClient
    ):
    def __init__(self, *args, **kwargs) -> None:
        super().__init__(*args, **kwargs)

    def status(self) -> dict[str, str] | errors.HTTPError:
        response = self.get(f"/{self.service_name}/status")
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error getting driver status: {response.content.decode('utf-8')}")
        elif 'submodule-statuses' not in response.json():
            return errors.HTTPError(500, "Malformed response from server: missing 'submodule-statuses' field.")
        return response.json()['submodule-statuses']

    def self_check(self) -> errors.HTTPError | None:
        response = self.post(f"/{self.service_name}/self-test")
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error running self-test: {response.content.decode('utf-8')}")
        return None
