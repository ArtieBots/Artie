"""
This module contains the `SensorIMUV1` mixin definition.
"""

class SensorIMUV1:
    """
    `SensorIMUV1` is a mixin to be used by all services in Artie that
    publish IMU data. In Artie, an IMU (Inertial Measurement Unit) is a sensor
    that can encompasses an accelerometer and/or gyroscope and/or magnetometer.
    When not all three of these sub-sensors are present, the datastream published
    by the service should still be referred to as an "IMU datastream" and the
    format will simply have empty fields for the missing sub-sensors.
    """
    @staticmethod
    def __interface_name__() -> str:
        """Return the name of this interface. All interfaces must implement this method."""
        return "sensor-imu-v1"

    def imu_list(self) -> list[str]:
        """
        Return a list of all IMU sensor IDs that this service provides data for.
        """
        raise NotImplementedError("This method should be implemented by the service that implements this interface.")

    def imu_whoami(self, imu_id: str) -> str:
        """
        Return the name of the IMU sensor with the given ID.
        """
        raise NotImplementedError("This method should be implemented by the service that implements this interface.")

    def imu_self_check(self, imu_id: str) -> bool:
        """
        Perform a self-check on the IMU sensor with the given ID and return True if it is functioning properly, and False otherwise.
        """
        raise NotImplementedError("This method should be implemented by the service that implements this interface.")

    def imu_on(self, imu_id: str) -> bool:
        """
        Turn on the IMU sensor with the given ID.
        """
        raise NotImplementedError("This method should be implemented by the service that implements this interface.")

    def imu_off(self, imu_id: str) -> bool:
        """
        Turn off the IMU sensor with the given ID.
        """
        raise NotImplementedError("This method should be implemented by the service that implements this interface.")

    def imu_get_data(self, imu_id: str) -> dict:
        """
        Get the latest data from the IMU sensor with the given ID. The returned
        dictionary should have the following format:
        {
            "accelerometer": (x, y, z) (all floating point numbers) or None,
            "gyroscope": (x, y, z) (all floating point numbers) or None,
            "magnetometer": (x, y, z) (all floating point numbers) or None,
            "timestamp": standard (str) Artie time format as a string,
        }
        If a sub-sensor is not present in the IMU, its value should be None.

        It is up to the service implementing this interface to determine whether
        this is a synchronous method that returns the latest data on demand, or an
        asynchronous method that returns the data from the most recent datastream
        publication for the given IMU ID.
        """
        raise NotImplementedError("This method should be implemented by the service that implements this interface.")

    def imu_start_stream(self, imu_id: str, freq_hz=None) -> bool:
        """
        Start streaming data from the IMU sensor with the given ID. The service
        should publish new data to the appropriate datastream at the given
        frequency (in Hz) if possible. It is implementation-dependent whether the service will honor
        this parameter. If called again before imu_stop_stream, the service should update the frequency of the stream if possible
        to the new frequency.

        Returns True if the stream was successfully started (or the frequency was successfully updated), and False otherwise.

        The published data can be found under the name <simple interface name>:sensor-imu-v1:<imu_id>
        (e.g. "my-service:sensor-imu-v1:imu-1") and should be in the same format as described in imu_get_data.
        """
        raise NotImplementedError("This method should be implemented by the service that implements this interface.")

    def imu_stop_stream(self, imu_id: str) -> bool:
        """
        Stop streaming data from the IMU sensor with the given ID.

        Returns True if the stream was successfully stopped, and False otherwise.
        Returns False if the stream was not active in the first place.
        """
        raise NotImplementedError("This method should be implemented by the service that implements this interface.")
