"""
This module contains the code for the datastream published by the example sensor driver service.
"""
from artie_util import artie_logging as alog
from artie_util import artie_time
from artie_util import qutil
from artie_service_client import pubsub
import queue
import time

def stream( q: queue.Queue, certfpath: str, keyfpath: str, service_simple_name: str, imu_id: str, freq_hz=1.0):
    topic_name = f"{service_simple_name}:sensor-imu-v1:{imu_id}"
    alog.info(f"Starting datastream for topic {topic_name} with frequency {freq_hz} Hz")
    with pubsub.ArtieStreamPublisher(topic_name, service_simple_name, encrypt=True, certfpath=certfpath, keyfpath=keyfpath) as publisher:
        while True:
            # Publish dummy data
            data = {
                "accelerometer": (0.0, 0.0, 9.8),
                "gyroscope": (0.0, 0.0, 0.0),
                "magnetometer": None,
                "timestamp": artie_time.now_str(),
            }
            publisher.publish(data)

            stop = qutil.get(q, timeout=1.0 / freq_hz)
            if stop:
                alog.info(f"Stopping datastream for topic {topic_name}")
                break
