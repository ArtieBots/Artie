"""
API client for communicating with the API Server's servo endpoints.

Each `*Response` object corresponds to the response from a specific API endpoint.

See [API documentation](../../../../../misc-micro-services/artie-api-server/README.md) for more details.
"""
from . import api_client
from .. import errors
from artie_service_client.interfaces import servo as servo_interface

class ServoClient(
    servo_interface.ServoInterfaceV1,
    api_client.APIClient
    ):
    def __init__(self, *args, **kwargs) -> None:
        super().__init__(*args, **kwargs)

    def servo_list(self) -> list[str] | errors.HTTPError:
        response = self.get(f"/{self.service_name}/servo/list")
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error listing servos: {response.content.decode('utf-8')}")
        elif 'servo-names' not in response.json():
            return errors.HTTPError(500, "Malformed response from server: missing 'servo-names' field.")
        return response.json()['servo-names']

    def servo_set_position(self, servo_id: str, position: float) -> errors.HTTPError | None:
        body = {
            "position": position
        }
        response = self.post(f"/{self.service_name}/servo/position", body=body, params={'which': servo_id})
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error setting servo '{servo_id}' position: {response.content.decode('utf-8')}")
        return None

    def servo_get_position(self, servo_id: str) -> float | errors.HTTPError:
        response = self.get(f"/{self.service_name}/servo/position", params={'which': servo_id})
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error getting servo '{servo_id}' position: {response.content.decode('utf-8')}")
        elif 'position' not in response.json():
            return errors.HTTPError(500, "Malformed response from server: missing 'position' field.")
        return response.json()['position']
