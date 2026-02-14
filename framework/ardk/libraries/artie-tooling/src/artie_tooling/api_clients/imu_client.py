"""
API client for communicating with the API Server's IMU (Inertial Measurement Unit) endpoints.

Each `*Response` object corresponds to the response from a specific API endpoint.

See [API documentation](../../../../../misc-micro-services/artie-api-server/README.md) for more details.
"""
from . import api_client
from .. import errors
from artie_service_client.interfaces import sensor_imu
from artie_tooling import artie_profile

class IMUClient(
    sensor_imu.SensorIMUV1,
    api_client.APIClient
    ):
    def __init__(self, service_name: str, profile: artie_profile.ArtieProfile, integration_test=False, unit_test=False, nretries=3) -> None:
        super().__init__(service_name=service_name, profile=profile, integration_test=integration_test, unit_test=unit_test, nretries=nretries)

    def imu_list(self) -> list[str] | errors.HTTPError:
        response = self.get(f"/{self.service_name}/imu/list")
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error listing IMUs: {response.content.decode('utf-8')}")
        elif 'imu-ids' not in response.json():
            return errors.HTTPError(500, "Malformed response from server: missing 'imu-ids' field.")
        return response.json()['imu-ids']

    def imu_whoami(self, imu_id: str) -> str | errors.HTTPError:
        response = self.get(f"/{self.service_name}/imu/whoami", params={'which': imu_id})
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error getting IMU '{imu_id}' name: {response.content.decode('utf-8')}")
        elif 'name' not in response.json():
            return errors.HTTPError(500, "Malformed response from server: missing 'name' field.")
        return response.json()['name']

    def imu_self_check(self, imu_id: str) -> bool | errors.HTTPError:
        response = self.get(f"/{self.service_name}/imu/self-check", params={'which': imu_id})
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error performing self-check on IMU '{imu_id}': {response.content.decode('utf-8')}")
        elif 'ok' not in response.json():
            return errors.HTTPError(500, "Malformed response from server: missing 'ok' field.")
        return response.json()['ok']

    def imu_on(self, imu_id: str) -> bool | errors.HTTPError:
        body = {}
        response = self.post(f"/{self.service_name}/imu/on", body=body, params={'which': imu_id})
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error turning on IMU '{imu_id}': {response.content.decode('utf-8')}")
        elif 'ok' not in response.json():
            return errors.HTTPError(500, "Malformed response from server: missing 'ok' field.")
        return response.json()['ok']

    def imu_off(self, imu_id: str) -> bool | errors.HTTPError:
        body = {}
        response = self.post(f"/{self.service_name}/imu/off", body=body, params={'which': imu_id})
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error turning off IMU '{imu_id}': {response.content.decode('utf-8')}")
        elif 'ok' not in response.json():
            return errors.HTTPError(500, "Malformed response from server: missing 'ok' field.")
        return response.json()['ok']

    def imu_get_data(self, imu_id: str) -> dict | errors.HTTPError:
        response = self.get(f"/{self.service_name}/imu/data", params={'which': imu_id})
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error getting data from IMU '{imu_id}': {response.content.decode('utf-8')}")
        data = response.json()
        required_fields = ['accelerometer', 'gyroscope', 'magnetometer', 'timestamp']
        for field in required_fields:
            if field not in data:
                return errors.HTTPError(500, f"Malformed response from server: missing '{field}' field.")
        return data

    def imu_start_stream(self, imu_id: str, freq_hz=None) -> bool | errors.HTTPError:
        body = {}
        if freq_hz is not None:
            body['freq_hz'] = freq_hz
        response = self.post(f"/{self.service_name}/imu/start-stream", body=body, params={'which': imu_id})
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error starting stream for IMU '{imu_id}': {response.content.decode('utf-8')}")
        elif 'ok' not in response.json():
            return errors.HTTPError(500, "Malformed response from server: missing 'ok' field.")
        return response.json()['ok']

    def imu_stop_stream(self, imu_id: str) -> bool | errors.HTTPError:
        body = {}
        response = self.post(f"/{self.service_name}/imu/stop-stream", body=body, params={'which': imu_id})
        if response.status_code != 200:
            return errors.HTTPError(response.status_code, f"Error stopping stream for IMU '{imu_id}': {response.content.decode('utf-8')}")
        elif 'ok' not in response.json():
            return errors.HTTPError(500, "Malformed response from server: missing 'ok' field.")
        return response.json()['ok']
