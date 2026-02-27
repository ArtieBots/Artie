from . import artifact
from . import dependency
from . import job
from . import result
from .. import common
from .. import docker
from dataclasses import dataclass
from enum import Enum
from enum import unique
import datetime
import traceback

@unique
class TestStatuses(Enum):
    """
    These are the allowable status values for a test result.
    """
    SUCCESS = 0
    FAIL = 1
    DID_NOT_RUN = 2

@dataclass
class ParallelCommand:
    """
    A command to run in parallel with other commands, with its own expected outputs.
    """
    cmd: str
    expected_outputs: list['ExpectedOutput']
    unexpected_outputs: list['UnexpectedOutput'] = None

    def __post_init__(self):
        if self.unexpected_outputs is None:
            self.unexpected_outputs = []

class ExpectedOutput:
    """
    An `ExpectedOutput` is a string that we expect to find inside a Docker container.
    """
    def __init__(self, what: str, where: str | dependency.Dependency, cli=False) -> None:
        self.what = what
        self.where = where
        self.cli = cli  # is 'where' the CLI container? It gets treated differently than all the others
        self.pid = None  # Needs to be filled in by whoever launches the DUT(s)

    def evaluated_where(self, args) -> str:
        """
        Returns our `where`, after evaluating it if it is a dependency.
        """
        if issubclass(type(self.where), dependency.Dependency):
            where = self.where.evaluate(args).item
        else:
            where = self.where
        return where

    def check(self, args, test_name: str, task_name: str, timeout_s: float) -> result.TestResult|None:
        """
        Return whether the what can be found in the where. Ignores requests to check for CLI containers.
        """
        if self.cli:
            return None

        common.info(f"Checking {test_name}'s DUT {self.pid} for output...")
        container = docker.get_container(self.pid)
        if container is None:
            return result.TestResult(test_name, producing_task_name=task_name, status=result.TestStatuses.FAIL, msg=f"Could not find container corresponding to {self.evaluated_where(args)}")

        timestamp = datetime.datetime.now().timestamp()
        try:
            common.info(f"Reading logs from {container.name} to find '{self.what}'...")
            for line in container.logs(stream=True, follow=True):
                if args.docker_logs:
                    common.info(line.decode())

                if self.what in line.decode():
                    return result.TestResult(test_name, producing_task_name=task_name, status=result.TestStatuses.SUCCESS)

                if datetime.datetime.now().timestamp() - timestamp > timeout_s:
                    return result.TestResult(test_name, producing_task_name=task_name, status=TestStatuses.FAIL, exception=TimeoutError(f"Timeout waiting for '{self.what}' in {self.evaluated_where(args)}"))
        except docker.docker_errors.NotFound:
            return result.TestResult(test_name, producing_task_name=task_name, status=TestStatuses.FAIL, msg=f"Container closed unexpectedly while reading its logs.")

        return result.TestResult(test_name, producing_task_name=task_name, status=TestStatuses.FAIL, msg=f"Container exited while we were waiting for '{self.what}' in {self.evaluated_where(args)}")

    def check_in_logs(self, args, logs: str, test_name: str, task_name: str) -> result.TestResult:
        """
        Same as check, but uses logs to do the checking, instead of the where and is typically used for CLI containers.
        """
        common.info(f"Checking {test_name}'s DUT(s) for '{self.what}' in logs...")
        if self.what in logs:
            return result.TestResult(test_name, task_name, result.TestStatuses.SUCCESS)
        else:
            return result.TestResult(test_name, task_name, result.TestStatuses.FAIL)

class UnexpectedOutput:
    """
    An `UnexpectedOutput` is a string that we expect NOT to find inside a Docker container or logs.
    If found, the test should fail.
    """
    def __init__(self, what: str, where: str | dependency.Dependency, cli=False) -> None:
        self.what = what
        self.where = where
        self.cli = cli  # is 'where' the CLI container? It gets treated differently than all the others
        self.pid = None  # Needs to be filled in by whoever launches the DUT(s)

    def evaluated_where(self, args) -> str:
        """
        Returns our `where`, after evaluating it if it is a dependency.
        """
        if issubclass(type(self.where), dependency.Dependency):
            where = self.where.evaluate(args).item
        else:
            where = self.where
        return where

    def check(self, args, test_name: str, task_name: str, timeout_s: float) -> result.TestResult|None:
        """
        Return whether the what can be found in the where. Ignores requests to check for CLI containers.

        We will wait up to timeout amount of time for the what to appear, and if it does we will return a failure.
        If it does not appear by that time, we will return a success.
        """
        if self.cli:
            return None

        common.info(f"Checking {test_name}'s DUT {self.pid} for output...")
        container = docker.get_container(self.pid)
        if container is None:
            return result.TestResult(test_name, producing_task_name=task_name, status=result.TestStatuses.FAIL, msg=f"Could not find container corresponding to {self.evaluated_where(args)}")

        timestamp = datetime.datetime.now().timestamp()
        try:
            common.info(f"Reading logs from {container.name} to see if '{self.what}' is in them...")
            for line in container.logs(stream=True, follow=True):
                if args.docker_logs:
                    common.info(line.decode())

                if self.what in line.decode():
                    return result.TestResult(test_name, producing_task_name=task_name, status=result.TestStatuses.FAIL, msg=f"Found unexpected output '{self.what}' in logs")

                if datetime.datetime.now().timestamp() - timestamp > timeout_s:
                    return result.TestResult(test_name, producing_task_name=task_name, status=result.TestStatuses.SUCCESS)
        except docker.docker_errors.NotFound:
            return result.TestResult(test_name, producing_task_name=task_name, status=TestStatuses.FAIL, msg=f"Container closed unexpectedly while reading its logs.")

        return result.TestResult(test_name, producing_task_name=task_name, status=TestStatuses.SUCCESS)

    def check_in_logs(self, args, logs: str, test_name: str, task_name: str) -> result.TestResult:
        """
        Check that the unexpected text is NOT in the logs. If it is found, the test fails.
        """
        common.info(f"Checking {test_name}'s logs to ensure '{self.what}' is NOT present...")
        if self.what in logs:
            return result.TestResult(test_name, task_name, result.TestStatuses.FAIL, msg=f"Found unexpected output '{self.what}' in logs")
        else:
            return result.TestResult(test_name, task_name, result.TestStatuses.SUCCESS)

class TestJob(job.Job):
    """
    All TestJobs run a setup(), then a bunch of steps, then a teardown().
    Each step should be a single test that returns a test result.

    Each TestJob also runs clean() after all tasks have run.
    """

    def __init__(self, artifacts: list[artifact.Artifact], steps: list[callable]) -> None:
        super().__init__(artifacts)
        self.steps = steps

    def __call__(self, args) -> result.JobResult:
        failed = False
        results = []
        common.info(f"******* Starting test job {self.name} *******")

        # Try to run the setups
        try:
            self.setup(args)
        except Exception as e:
            common.error(f"Exception during setup of test job {self.name}: {e}")
            results += [result.TestResult("Exception during setup", self.parent_task.name, TestStatuses.FAIL, exception=e)]
            failed = True

        # Try to run the steps, but only if setup didn't fail.
        if not failed:
            try:
                results += self._run_steps(args)
            except Exception as e:
                common.error(f"Exception while running test job {self.name}: {e}")
                results += [result.TestResult("Exception in test job", self.parent_task.name, TestStatuses.FAIL, exception=e)]

        # Try to run teardowns, regardless of what has happened so far.
        try:
            self.teardown(args, results)
        except Exception as e:
            common.error(f"Exception during teardown of test job {self.name}: {e}")
            results += [result.TestResult("Exception during teardown", self.parent_task.name, TestStatuses.FAIL, exception=e)]

        # Check success
        success = True
        for r in results:
            if r.status.value == TestStatuses.FAIL.value:
                success = False
                break

        self.mark_all_artifacts_as_built()
        return result.JobResult(self.name, success=success, artifacts=results)

    def _run_steps(self, args) -> list[result.TestResult]:
        """
        Run each test in this job and return the list of results.
        """
        results = []
        for i, t in enumerate(self.steps):
            common.info(f"::::::::::: Running test {i+1}/{len(self.steps)}: {t.test_name} :::::::::::")
            try:
                test_result = common.manage_timeout(t, args.test_timeout_s, args)
                results.append(test_result)
                common.info(f"Finished test: {t.test_name} with result: {test_result.status.name}")
                if test_result.status != TestStatuses.SUCCESS and args.fail_fast:
                    common.info(f"--fail-fast enabled and test {t.test_name} failed. Marking rest of this task's tests as DID_NOT_RUN.")
                    results = self._mark_remaining_tests_as_did_not_run(results, i)
                    break
            except Exception as e:
                common.error(f"Exception while running test {t.test_name}: {e}")

                # At this point, we may have child threads from test steps that are managing
                # timeouts themselves. We need to kill those threads to prevent them from continuing to run and potentially
                # interfering with future tests.
                if hasattr(t, "kill_child_threads"):
                    common.info(f"Killing child threads of test {t.test_name} to prevent interference with future tests...")
                    t._kill_child_threads()

                # Log exception if --enable-error-tracing
                if args.enable_error_tracing:
                    common.error(f"Error running test {t.test_name}: {''.join(traceback.format_exception(e))}")
                else:
                    common.error(f"Test {t.test_name} failed due to an exception ({e})")

                # Add this result
                results.append(result.TestResult(t.test_name, producing_task_name=self.parent_task.name, status=TestStatuses.FAIL, exception=e))

                # Mark all remaining tests as DID_NOT_RUN if user is not using --force-completion
                if args.force_completion:
                    common.info("--force-completion argument detected. Running rest of tests in this task.")
                elif i + 1 < len(self.steps):
                    common.info("Marking rest of this task's tests as DID_NOT_RUN. Use --force-completion flag to change this behavior.")
                    results = self._mark_remaining_tests_as_did_not_run(results, i)
                    return results
                else:
                    common.debug(f"Finished test {t.test_name} with an exception, but there are no more tests to run in this task, so we're not marking any additional tests as DID_NOT_RUN.")
        return results

    def link(self, parent, index: int):
        super().link(parent, index)
        for s in self.steps:
            s.producing_task_name = parent.name

    def setup(self, args):
        """
        Set up for all the tests we will run in this task.
        """
        pass

    def teardown(self, args, results: list[result.TestResult]):
        """
        Clean up after ourselves. Should be overridden by the subclass.
        Note that subclasses should honor the --skip-teardown arg.
        """
        pass

    def _mark_remaining_tests_as_did_not_run(self, results: list[result.TestResult], current_index: int) -> list[result.TestResult]:
        """
        Mark all tests after the current_index in results as DID_NOT_RUN.
        """
        for remaining_test in self.steps[current_index+1:]:
            results.append(result.TestResult(remaining_test.test_name, producing_task_name=self.parent_task.name, status=TestStatuses.DID_NOT_RUN))
        return results
