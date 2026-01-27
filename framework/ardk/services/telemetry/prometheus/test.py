"""
This script queries the Prometheus metrics endpoint to verify that
a specific metric is being collected. It serves as a test to ensure that
the metrics are being scraped from at least one other service.

The important piece is that it prints "Test Passed" to the console,
which we check for in our test suite to determine if the test was successful.
"""
import requests

if __name__ == "__main__":
    r = requests.get("http://localhost:9090/api/v1/query?query=eyebrows_service_eyebrows_service_sw_code_paths_api_led_on_calls_total")
    try:
        for metric in r.json()['data']['result']:
            if metric['value']:
                print("Test Passed", flush=True)
                exit(0)
    except KeyError:
        print(f"Invalid response: {r}", flush=True)
