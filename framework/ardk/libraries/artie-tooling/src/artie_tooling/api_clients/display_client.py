"""
API client for communicating with the API Server's display endpoints.

Each `*Response` object corresponds to the response from a specific API endpoint.

See [API documentation](../../../../../misc-micro-services/artie-api-server/README.md) for more details.
"""
from . import api_client
from .. import errors
from artie_service_client.interfaces import display as display_interface
from artie_tooling import artie_profile

class DisplayClient(
    display_interface.DisplayInterfaceV1,
    api_client.APIClient
    ):
    def __init__(self, service_name: str, profile: artie_profile.ArtieProfile, integration_test=False, unit_test=False, nretries=3) -> None:
        super().__init__(service_name=service_name, profile=profile, integration_test=integration_test, unit_test=unit_test, nretries=nretries)

    def display_list(self) -> list[str]:
        response = self.get(f"/{self.service_name}/display/list")
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error listing displays: {response.content.decode('utf-8')}")
        elif 'displays' not in response.json():
            return errors.HTTPError(500, "Malformed response from server: missing 'displays' field.")
        return response.json()['displays']

    def display_set(self, display_id: str, content: str) -> errors.HTTPError|None:
        body = {
            "display": content
        }
        response = self.post(f"/{self.service_name}/display/contents", body=body, params={'which': display_id})
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error setting display '{display_id}': {response.content.decode('utf-8')}")
        return None

    def display_get(self, display_id: str) -> errors.HTTPError|str:
        response = self.get(f"/{self.service_name}/display/contents", params={'which': display_id})
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error getting display '{display_id}': {response.content.decode('utf-8')}")
        elif 'content' not in response.json():
            return errors.HTTPError(500, "Malformed response from server: missing 'content' field.")
        return response.json()['content']

    def display_test(self, display_id: str) -> errors.HTTPError|None:
        response = self.post(f"/{self.service_name}/display/test", params={'which': display_id})
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error testing display '{display_id}': {response.content.decode('utf-8')}")
        return None

    def display_clear(self, display_id: str) -> errors.HTTPError|None:
        response = self.post(f"/{self.service_name}/display/clear", params={'which': display_id})
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error clearing display '{display_id}': {response.content.decode('utf-8')}")
        return None
