from typing import Dict
from typing import List
from typing import Tuple
from . import dependency
from . import test_job
from .. import common
from .. import docker
import os
import yaml

class DockerComposeTestSuiteJob(test_job.TestJob):
    def __init__(self, steps: List[test_job.CLITest], compose_fname: str, compose_docker_image_variables: List[Tuple[str, str|dependency.Dependency]], docker_network_name: str) -> None:
        super().__init__(artifacts=[], steps=steps)
        self.compose_fname = compose_fname
        self.compose_variables = compose_docker_image_variables  # List of (key, value) pairs; gets transformed into dict[str: str] when setup() is called
        self.compose_dpath = os.path.join(common.repo_root(), "framework", "artietool", "compose-files")
        self.docker_network_name = docker_network_name
        self.project_name = os.path.splitext(self.compose_fname)[0].replace('.', '-')
        self._dut_pids = {}

    def _set_compose_variables(self, args):
        compose_variables = {}
        for k, v in self.compose_variables:
            if issubclass(type(v), dependency.Dependency):
                compose_variables[k] = v.evaluate(args).item
            else:
                compose_variables[k] = v
        self.compose_variables = compose_variables

    def _extract_cli_environment_from_compose(self) -> Dict[str, str]:
        """
        Extract environment variables from the compose file that should be passed to CLI containers.
        Looks for common Artie environment variables in any service and extracts them.
        """
        compose_fpath = os.path.join(self.compose_dpath, self.compose_fname)
        if not os.path.exists(compose_fpath):
            common.warning(f"Compose file {compose_fpath} not found, cannot extract environment variables")
            return {}

        try:
            with open(compose_fpath, 'r') as f:
                compose_config = yaml.safe_load(f)
        except Exception as e:
            common.warning(f"Failed to parse compose file {compose_fpath}: {e}")
            return {}

        # Environment variables we want to extract for CLI
        target_env_vars = [
            'ARTIE_PUBSUB_BROKER_HOSTNAME',
            'ARTIE_PUBSUB_BROKER_PORT',
            'ARTIE_SERVICE_BROKER_HOSTNAME',
            'ARTIE_SERVICE_BROKER_PORT',
        ]

        extracted_env = {}

        # Look through all services to find the target environment variables
        services = compose_config.get('services', {})
        for service_name, service_config in services.items():
            env_list = service_config.get('environment', [])

            # Environment can be either a list or a dict
            if isinstance(env_list, list):
                for env_item in env_list:
                    if isinstance(env_item, str):
                        # Format: "KEY=VALUE"
                        if '=' in env_item:
                            key, value = env_item.split('=', 1)
                            if key in target_env_vars:
                                extracted_env[key] = value
                    elif isinstance(env_item, dict):
                        # Format: {KEY: VALUE}
                        for key, value in env_item.items():
                            if key in target_env_vars:
                                extracted_env[key] = str(value)
            elif isinstance(env_list, dict):
                for key, value in env_list.items():
                    if key in target_env_vars:
                        extracted_env[key] = str(value)

        if extracted_env:
            common.info(f"Extracted environment variables for CLI from compose file: {', '.join(extracted_env.keys())}")

        return extracted_env

    def clean(self, args):
        """
        Clean up any Docker containers created by this job. This runs even if the job failed during setup or execution.
        Honors args.skip_teardown.
        """
        super().clean(args)

        if hasattr(args, "skip_teardown") and args.skip_teardown:
            common.info(f"--skip-teardown detected. You will need to manually clean up the Docker containers.")
            return

        common.info(f"Cleaning up Docker compose containers for project {self.project_name}...")

        try:
            self._set_compose_variables(args)
            docker.compose_down(self.project_name, self.compose_dpath, self.compose_fname, envs=self.compose_variables)
        except Exception as e:
            common.error(f"Error during docker compose down. There may be leftover containers or networks: {e}")

        try:
            docker.remove_network(self.docker_network_name)
        except Exception as e:
            common.error(f"Error removing docker network {self.docker_network_name}; there may be leftover networks: {e}")

    def setup(self, args):
        """
        Set up the DUTs by using Docker compose.
        """
        super().setup(args)
        if not os.path.isdir(self.compose_dpath):
            raise FileNotFoundError(f"Cannot find compose-files directory at {self.compose_dpath}")

        docker.add_network(self.docker_network_name)
        self._set_compose_variables(args)
        self._dut_pids = docker.compose(self.project_name, self.compose_dpath, self.compose_fname, args.test_timeout_s, envs=self.compose_variables)

        # Extract environment variables from compose file and set them on all CLI tests
        cli_environment = self._extract_cli_environment_from_compose()
        for step in self.steps:
            step.link_pids_to_expected_outs(args, self._dut_pids)
            # Merge extracted environment with any existing environment in the test
            # Test-specific environment takes precedence
            step.environment = {**cli_environment, **step.environment}

    def teardown(self, args):
        """
        Shutdown any Docker containers still at large.
        """
        if args.skip_teardown:
            common.info(f"--skip-teardown detected. You will need to manually clean up the Docker containers.")
            return

        super().teardown(args)
        common.info(f"Tearing down. Stopping docker containers...")
        docker.compose_down(self.project_name, self.compose_dpath, self.compose_fname, envs=self.compose_variables)
        docker.remove_network(self.docker_network_name)
