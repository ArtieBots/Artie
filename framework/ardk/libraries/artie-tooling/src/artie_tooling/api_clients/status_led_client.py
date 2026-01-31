"""
API client for communicating with the API Server's status LED endpoints.

Each `*Response` object corresponds to the response from a specific API endpoint.

See [API documentation](../../../../../misc-micro-services/artie-api-server/README.md) for more details.
"""
from . import api_client
from .. import errors
from artie_service_client.interfaces import status_led as status_led_interface

class StatusLEDClient(
    status_led_interface.StatusLEDInterfaceV1,
    api_client.APIClient
    ):
    def __init__(self, *args, **kwargs) -> None:
        super().__init__(*args, **kwargs)

    def led_list(self) -> list[str] | errors.HTTPError:
        response = self.get(f"/{self.service_name}/led/list")
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error listing LEDs: {response.content.decode('utf-8')}")
        elif 'led-names' not in response.json():
            return errors.HTTPError(500, "Malformed response from server: missing 'led-names' field.")
        return response.json()['led-names']

    def led_set(self, which: str, state: str) -> bool | errors.HTTPError:
        params = {'state': state, 'which': which}
        response = self.post(f"/{self.service_name}/led", params=params)
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error setting LED '{which}' to heartbeat: {response.content.decode('utf-8')}")
        return response.json().get('success', True)

    def led_get(self, which: str) -> str | errors.HTTPError:
        params = {'which': which}
        response = self.get(f"/{self.service_name}/led", params=params)
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error getting LED '{which}' state: {response.content.decode('utf-8')}")
        elif 'state' not in response.json():
            return errors.HTTPError(500, "Malformed response from server: missing 'state' field.")
        return response.json()['state']
