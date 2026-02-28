from . import dependency
from . import result
from . import test_job
from .. import common
from .. import docker
import time


class PytestTest:
    """Represents a single pytest test suite execution."""

    def __init__(self, test_name: str, expected_outputs: list[test_job.ExpectedOutput]=None, unexpected_outputs: list[test_job.UnexpectedOutput]=None) -> None:
        self.test_name = test_name
        self.expected_outputs = expected_outputs or []
        self.unexpected_outputs = unexpected_outputs or []
        self.stop_event = False
        self.producing_task_name = None  # Filled in by Job

        if not expected_outputs and not unexpected_outputs:
            raise ValueError(f"Test {test_name} has no expected outputs or unexpected outputs. At least one of them is required.")

    def __call__(self, args) -> result.TestResult:
        """The test setup has already run the Pytest suite. Now we just collect this test's results."""
        try:
            # Check the DUT(s) output(s)
            common.debug(f"Checking DUT(s) for expected outputs for test {self.test_name}...")
            results = self._check_duts(args)
            results = [r for r in results if r is not None and r.status != result.TestStatuses.SUCCESS]

            # If we got more than one result, let's log the various problems and just return the first failing one
            if len(results) > 1:
                common.error(f"Multiple failures detected in {self.test_name}. Returning the first detected failure and logging all of them.")

            for r in results:
                common.error(f"Error in test {self.test_name}: {r.to_verbose_str()}")

            if results:
                return results[0]

            return result.TestResult(self.test_name, producing_task_name=self.producing_task_name, status=test_job.TestStatuses.SUCCESS)

        except Exception as e:
            common.error(f"Exception while running test {self.test_name}: {e}")
            return result.TestResult(self.test_name, producing_task_name=self.producing_task_name, status=test_job.TestStatuses.FAIL, exception=e)

    def link_pids_to_expected_outs(self, args, pids: dict[str, str]):
        """
        Link each of this test's ExpectedOutput objects to its actual pid.
        """
        for e in self.expected_outputs:
            where = e.evaluated_where(args)
            if where in pids:
                e.pid = pids[e.evaluated_where(args)]
            else:
                raise KeyError(f"Cannot find a Docker ID corresponding to a Docker container that is expected to be running in this test. Offending container: {where}; available PIDs: {pids}")

        for u in self.expected_outputs:
            where = u.evaluated_where(args)
            if where in pids:
                u.pid = pids[u.evaluated_where(args)]
            else:
                raise KeyError(f"Cannot find a Docker ID corresponding to a Docker container that is expected to be running in this test. Offending container: {where}; available PIDs: {pids}")

    def _check_duts(self, args) -> list[result.TestResult]:
        """
        Runs _check_dut() on each DUT pid, managing the total test timeout appropriately.
        """
        timeout_s = args.test_timeout_s
        results = []
        for expected_out in self.expected_outputs:
            r = expected_out.check(args, self.test_name, self.producing_task_name, timeout_s)
            if r is not None and r.exception is not None and type(r.exception) == TimeoutError:
                timeout_s = 1  # Give us a chance to collect the rest of the results
            results.append(r)

        for unexpected_out in self.unexpected_outputs:
            r = unexpected_out.check(args, self.test_name, self.producing_task_name, timeout_s)
            if r is not None and r.exception is not None and type(r.exception) == TimeoutError:
                timeout_s = 1  # Give us a chance to collect the rest of the results
            results.append(r)

        return results

class SingleContainerPytestSuiteJob(test_job.TestJob):
    """
    Job that runs pytest test suites inside a single container.

    This job type is designed for Python projects with pytest-based unit tests.
    It runs the entire test suite in one container and reports the results.
    """

    def __init__(self, steps: list[PytestTest], docker_image_under_test: str | dependency.Dependency, cmd_to_run_in_dut: str|None) -> None:
        super().__init__(artifacts=[], steps=steps)
        self.dut = docker_image_under_test
        self.cmd_to_run_in_dut = cmd_to_run_in_dut

    def setup(self, args):
        """
        Set up the DUT by using this object's `docker_cmd`.
        """
        super().setup(args)
        if issubclass(type(self.dut), dependency.Dependency):
            docker_image_name = self.dut.evaluate(args).item
        else:
            docker_image_name = str(docker.construct_docker_image_name(args, self.dut, common.host_platform()))

        kwargs = {'environment': {'ARTIE_RUN_MODE': 'unit'}}
        self._dut_container = docker.start_docker_container(docker_image_name, self.cmd_to_run_in_dut, **kwargs)
        for step in self.steps:
            step.link_pids_to_expected_outs(args, {docker_image_name: self._dut_container.id})

        # Give some time for the container to initialize before we start testing it
        common.info("Waiting for DUT to come online...")
        time.sleep(min(args.test_timeout_s / 3, 10))

    def teardown(self, args, results: list[result.TestResult]):
        """
        Shutdown any Docker containers still at large.
        """
        if args.skip_teardown:
            common.info(f"--skip-teardown detected. You will need to manually clean up the Docker containers.")
            return

        super().teardown(args, results)
        common.info(f"Tearing down. Stopping docker container...")
        try:
            self._dut_container.stop()
        except docker.docker_errors.NotFound:
            pass  # Container already stopped
