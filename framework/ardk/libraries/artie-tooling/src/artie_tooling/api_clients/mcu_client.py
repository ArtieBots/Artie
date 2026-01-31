"""
API client for communicating with the API Server's MCU endpoints.

Each `*Response` object corresponds to the response from a specific API endpoint.

See [API documentation](../../../../../misc-micro-services/artie-api-server/README.md) for more details.
"""
from . import api_client
from .. import errors
from artie_service_client.interfaces import mcu as mcu_interface

class MCUClient(
    mcu_interface.MCUInterfaceV1,
    api_client.APIClient
    ):
    def __init__(self, *args, **kwargs) -> None:
        super().__init__(*args, **kwargs)

    def mcu_list(self) -> list[str] | errors.HTTPError:
        response = self.get(f"/{self.service_name}/mcu_list")
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error listing MCUs: {response.content.decode('utf-8')}")
        elif 'mcu-names' not in response.json():
            return errors.HTTPError(500, "Malformed response from server: missing 'mcu-names' field.")
        return response.json()['mcu-names']

    def mcu_fw_load(self, mcu_id: str) -> bool | errors.HTTPError:
        params = {}
        if mcu_id:
            params['mcu-id'] = mcu_id
        response = self.post(f"/{self.service_name}/mcu_reload_fw", params=params)
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error reloading firmware for MCU '{mcu_id}': {response.content.decode('utf-8')}")
        return response.json().get('success', True)

    def mcu_reset(self, mcu_id: str) -> errors.HTTPError | None:
        params = {}
        if mcu_id:
            params['mcu-id'] = mcu_id
        response = self.post(f"/{self.service_name}/mcu_reset", params=params)
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error resetting MCU '{mcu_id}': {response.content.decode('utf-8')}")
        return None

    def mcu_self_check(self, mcu_id: str) -> errors.HTTPError | None:
        params = {}
        if mcu_id:
            params['mcu-id'] = mcu_id
        response = self.post(f"/{self.service_name}/mcu_self_check", params=params)
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error running self check on MCU '{mcu_id}': {response.content.decode('utf-8')}")
        return None

    def mcu_status(self, mcu_id: str) -> str | errors.HTTPError:
        params = {}
        if mcu_id:
            params['mcu-id'] = mcu_id
        response = self.get(f"/{self.service_name}/mcu_status", params=params)
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error getting status for MCU '{mcu_id}': {response.content.decode('utf-8')}")
        elif 'status' not in response.json():
            return errors.HTTPError(500, "Malformed response from server: missing 'status' field.")
        return response.json()['status']

    def mcu_version(self, mcu_id: str) -> str | errors.HTTPError:
        params = {}
        if mcu_id:
            params['mcu-id'] = mcu_id
        response = self.get(f"/{self.service_name}/mcu_version", params=params)
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error getting version for MCU '{mcu_id}': {response.content.decode('utf-8')}")
        elif 'version' not in response.json():
            return errors.HTTPError(500, "Malformed response from server: missing 'version' field.")
        return response.json()['version']
