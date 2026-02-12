from typing import Dict
from typing import List
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
import os
import threading
import time
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
    expected_outputs: List['ExpectedOutput']
    unexpected_outputs: List['UnexpectedOutput'] = None

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

    def check_in_logs(self, args, logs: str, test_name: str, task_name: str) -> result.TestResult:
        """
        Check that the unexpected text is NOT in the logs. If it is found, the test fails.
        """
        common.info(f"Checking {test_name}'s logs to ensure '{self.what}' is NOT present...")
        if self.what in logs:
            return result.TestResult(test_name, task_name, result.TestStatuses.FAIL, msg=f"Found unexpected output '{self.what}' in logs")
        else:
            return result.TestResult(test_name, task_name, result.TestStatuses.SUCCESS)

class HWTest:
    """
    A HWTest is a list of CLI commands to run inside the one Kubernetes hw test job and the output we expect.

    Unlike other tests, this does not run as a sub-job. Instead, a single HWTestSuiteJob is created from
    the information in each of these instances.
    """
    def __init__(self, test_name: str, cmds_to_run_in_cli: str, expected_results: Dict[str, str]) -> None:
        self.test_name = test_name
        self.cmds_to_run_in_cli = cmds_to_run_in_cli
        self.expected_results = expected_results

class CLITest:
    def __init__(self, test_name: str, cli_image: str, cmd_to_run_in_cli: str=None, expected_outputs: List[ExpectedOutput]=None, need_to_access_cluster=False, network=None, setup_cmds: List[str]=None, teardown_cmds: List[str]=None, unexpected_outputs: List[UnexpectedOutput]=None, parallel_cmds: List[ParallelCommand]=None, environment: Dict[str, str]=None) -> None:
        self.test_name = test_name
        self.cli_image = cli_image
        self.cmd_to_run_in_cli = cmd_to_run_in_cli
        self.parallel_cmds = parallel_cmds or []
        self.expected_outputs = expected_outputs or []
        self.unexpected_outputs = unexpected_outputs or []
        self.producing_task_name = None  # Filled in by Job
        self.need_to_access_cluster = need_to_access_cluster
        self.network = network
        self.setup_cmds = setup_cmds or []
        self.teardown_cmds = teardown_cmds or []
        self.environment = environment or {}
        # Validate that we have either cmd_to_run_in_cli OR parallel_cmds, but not both
        if cmd_to_run_in_cli and parallel_cmds:
            raise ValueError(f"Test {test_name} cannot have both 'cmd-to-run-in-cli' and 'parallel-cmds'")
        if not cmd_to_run_in_cli and not parallel_cmds:
            raise ValueError(f"Test {test_name} must have either 'cmd-to-run-in-cli' or 'parallel-cmds'")

    def __call__(self, args) -> result.TestResult:
        # Run setup commands
        try:
            self._run_setup_cmds(args)
        except Exception as e:
            common.error(f"Setup command failed for test {self.test_name}: {e}")
            return result.TestResult(self.test_name, producing_task_name=self.producing_task_name, status=TestStatuses.FAIL, exception=e)

        try:
            # Launch the CLI command
            common.debug(f"Running CLI command for test {self.test_name}...")
            res = self._run_cli(args)
            if res.status != result.TestStatuses.SUCCESS:
                common.error(f"CLI command failed for test {self.test_name}: {res.msg if res.msg else res.exception}")
                return res

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

            return result.TestResult(self.test_name, producing_task_name=self.producing_task_name, status=TestStatuses.SUCCESS)
        finally:
            # Always run teardown commands, even if test failed
            try:
                self._run_teardown_cmds(args)
            except Exception as e:
                common.error(f"Teardown command failed for test {self.test_name}: {e}")

    def link_pids_to_expected_outs(self, args, pids: Dict[str, str]):
        """
        Link each of this test's ExpectedOutput objects to its actual pid.
        """
        for e in self.expected_outputs:
            where = e.evaluated_where(args)
            if where in pids:
                e.pid = pids[e.evaluated_where(args)]
            elif where == "artie-cli":
                # We don't have a PID for the CLI container yet. We'll handle that later when we run the CLI command.
                e.cli = True
            else:
                raise KeyError(f"Cannot find a Docker ID corresponding to a Docker container that is expected to be running in this test. Offending container: {where}; available PIDs: {pids}")

    def _evaluated_cli_image(self, args) -> str:
        """
        Returns self.cli_image after evaluating it if it is a Dependency.
        """
        if issubclass(type(self.cli_image), dependency.Dependency):
            cli_img = self.cli_image.evaluate(args).item
        else:
            platform = common.host_platform()
            cli_img = str(docker.construct_docker_image_name(args, self.cli_image, platform))
        return cli_img

    def _find_expected_cli_out(self, args) -> ExpectedOutput|None:
        """
        Attempt to find the CLI output from the ExpectedOutputs and return it.
        """
        for ex in self.expected_outputs:
            if ex.cli:
                return ex
        return None

    def _run_cli(self, args) -> result.TestResult:
        """
        Run CLI command(s) and check for expected/unexpected outputs.
        Handles both single command and parallel commands.

        Return None if success or a failing TestResult otherwise.
        """
        if self.parallel_cmds:
            common.debug(f"Running parallel CLI commands for test {self.test_name}...")
            return self._run_parallel_cmds(args)
        else:
            common.debug(f"Running single CLI command for test {self.test_name}...")
            return self._run_single_cmd(args)

    def _run_single_cmd(self, args) -> result.TestResult:
        """
        Run a single CLI command and check it for expected_cli_out and unexpected_cli_out.

        Return None if success or a failing TestResult otherwise.
        """
        logs = self._try_ntimes(args, 5, self.cmd_to_run_in_cli)

        # Check expected outputs
        expected_cli_out = self._find_expected_cli_out(args)
        if expected_cli_out is not None:
            common.info(f"Checking CLI output for expected output '{expected_cli_out.what}'...")
            res = expected_cli_out.check_in_logs(args, logs, self.test_name, self.producing_task_name)
            if res.status != result.TestStatuses.SUCCESS:
                return res

        # Check unexpected outputs
        for unexpected_out in self.unexpected_outputs:
            if unexpected_out.cli:
                common.info(f"Checking CLI output for unexpected output '{unexpected_out.what}'...")
                res = unexpected_out.check_in_logs(args, logs, self.test_name, self.producing_task_name)
                if res.status != result.TestStatuses.SUCCESS:
                    return res

        return result.TestResult(self.test_name, self.producing_task_name, result.TestStatuses.SUCCESS)

    def _run_parallel_cmds(self, args) -> result.TestResult:
        """
        Run multiple CLI commands in parallel and check their outputs.
        """
        common.info(f"Running {len(self.parallel_cmds)} parallel commands for test {self.test_name}...")

        # Storage for results from each thread
        results = {}
        exceptions = {}

        def run_cmd_thread(idx: int, parallel_cmd: ParallelCommand):
            """Run a single command in a thread and store its output."""
            try:
                logs = self._try_ntimes(args, 5, parallel_cmd.cmd)
                results[idx] = logs
            except Exception as e:
                exceptions[idx] = e

        # Start all commands in parallel
        threads = []
        for idx, parallel_cmd in enumerate(self.parallel_cmds):
            thread = threading.Thread(target=run_cmd_thread, args=(idx, parallel_cmd))
            thread.start()
            threads.append(thread)

        # Wait for all threads to complete
        for thread in threads:
            thread.join()

        # Check if any thread had an exception
        if exceptions:
            first_exception = exceptions[min(exceptions.keys())]
            return result.TestResult(self.test_name, self.producing_task_name, result.TestStatuses.FAIL, exception=first_exception)

        # Check expected outputs for each parallel command
        for idx, parallel_cmd in enumerate(self.parallel_cmds):
            logs = results[idx]

            # Check expected outputs
            for expected_out in parallel_cmd.expected_outputs:
                if expected_out.what not in logs:
                    return result.TestResult(
                        self.test_name,
                        self.producing_task_name,
                        result.TestStatuses.FAIL,
                        msg=f"Expected output '{expected_out.what}' not found in parallel command {idx+1}"
                    )

            # Check unexpected outputs
            for unexpected_out in parallel_cmd.unexpected_outputs:
                if unexpected_out.what in logs:
                    return result.TestResult(
                        self.test_name,
                        self.producing_task_name,
                        result.TestStatuses.FAIL,
                        msg=f"Unexpected output '{unexpected_out.what}' found in parallel command {idx+1}"
                    )

        return result.TestResult(self.test_name, self.producing_task_name, result.TestStatuses.SUCCESS)

    def _try_ntimes(self, args, n: int, cmd: str):
        """
        Try running the CLI command up to `n` times to guard against transient timing errors. Yuck.
        """
        cli_img = self._evaluated_cli_image(args)
        kwargs = {'network_mode': 'host'} if self.network is None else {'network': self.network}

        # Add environment variables
        if self.environment:
            kwargs['environment'] = self.environment

        # Add kubeconfig if we need it
        if self.need_to_access_cluster:
            bind = {'bind': '/mnt/kube_config', 'mode': 'ro'}
            kube_config_dpath = os.path.dirname(args.kube_config)
            kwargs['volumes'] = {kube_config_dpath: bind}

        for i in range(n):
            try:
                logs = docker.run_docker_container(cli_img, cmd, timeout_s=args.test_timeout_s, log_to_stdout=args.docker_logs, **kwargs)
                return logs
            except Exception:
                if i != n - 1:
                    common.warning(f"Got an exception while trying to run CLI. Will try {n - (i+1)} more times.")
                    time.sleep(1)
                else:
                    raise

    def _check_duts(self, args) -> List[result.TestResult]:
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
        return results

    def _run_setup_cmds(self, args):
        """
        Run all setup commands before the main test command.
        """
        if not self.setup_cmds:
            return

        common.info(f"Running {len(self.setup_cmds)} setup command(s) for test {self.test_name}...")
        cli_img = self._evaluated_cli_image(args)
        kwargs = {'network_mode': 'host'} if self.network is None else {'network': self.network}

        # Add environment variables
        if self.environment:
            kwargs['environment'] = self.environment

        # Add kubeconfig if we need it
        if self.need_to_access_cluster:
            bind = {'bind': '/mnt/kube_config', 'mode': 'ro'}
            kube_config_dpath = os.path.dirname(args.kube_config)
            kwargs['volumes'] = {kube_config_dpath: bind}

        for i, cmd in enumerate(self.setup_cmds):
            common.info(f"Running setup command {i+1}/{len(self.setup_cmds)}: {cmd}")
            docker.run_docker_container(cli_img, cmd, timeout_s=args.test_timeout_s, log_to_stdout=args.docker_logs, **kwargs)

    def _run_teardown_cmds(self, args):
        """
        Run all teardown commands after the main test command.
        """
        if not self.teardown_cmds:
            return

        common.info(f"Running {len(self.teardown_cmds)} teardown command(s) for test {self.test_name}...")
        cli_img = self._evaluated_cli_image(args)
        kwargs = {'network_mode': 'host'} if self.network is None else {'network': self.network}

        # Add environment variables
        if self.environment:
            kwargs['environment'] = self.environment

        # Add kubeconfig if we need it
        if self.need_to_access_cluster:
            bind = {'bind': '/mnt/kube_config', 'mode': 'ro'}
            kube_config_dpath = os.path.dirname(args.kube_config)
            kwargs['volumes'] = {kube_config_dpath: bind}

        for i, cmd in enumerate(self.teardown_cmds):
            common.info(f"Running teardown command {i+1}/{len(self.teardown_cmds)}: {cmd}")
            try:
                docker.run_docker_container(cli_img, cmd, timeout_s=args.test_timeout_s, log_to_stdout=args.docker_logs, **kwargs)
            except Exception as e:
                common.warning(f"Teardown command {i+1} failed (continuing anyway): {e}")

class TestJob(job.Job):
    """
    All TestJobs run a setup(), then a bunch of steps, then a teardown().
    Each step should be a single test that returns a test result.

    Each TestJob also runs clean() after all tasks have run.
    """

    def __init__(self, artifacts: List[artifact.Artifact], steps: List[callable]) -> None:
        super().__init__(artifacts)
        self.steps = steps

    def __call__(self, args) -> result.JobResult:
        self.setup(args)
        results = self._run_steps(args)
        self.teardown(args)

        # Check success
        success = True
        for r in results:
            if r.status.value == TestStatuses.FAIL.value:
                success = False
                break

        self.mark_all_artifacts_as_built()
        return result.JobResult(self.name, success=success, artifacts=results)

    def _run_steps(self, args) -> List[result.TestResult]:
        """
        Run each test in this job and return the list of results.
        """
        results = []
        for i, t in enumerate(self.steps):
            try:
                test_result = common.manage_timeout(t, args.test_timeout_s, args)
                results.append(test_result)
            except Exception as e:
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
                elif not args.force_completion and i + 1 < len(self.steps):
                    common.info("Marking rest of this task's tests as DID_NOT_RUN. Use --force-completion flag to change this behavior.")
                    for remaining_test in self.steps[i+1:]:
                        results.append(result.TestResult(remaining_test.test_name, producing_task_name=self.parent_task.name, status=TestStatuses.DID_NOT_RUN))
                    return results
                else:
                    common.info(f"Finished test: {t.test_name}")
                    return results
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

    def teardown(self, args):
        """
        Clean up after ourselves. Should be overridden by the subclass.
        Note that subclasses should honor the --skip-teardown arg.
        """
        pass
