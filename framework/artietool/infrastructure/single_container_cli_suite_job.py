from typing import Dict
from typing import List
from . import dependency
from . import result
from . import test_job
from .. import common
from .. import docker
import os
import threading
import time

class CLITest:
    def __init__(self, test_name: str, cli_image: str, cmd_to_run_in_cli: str=None, expected_outputs: list[test_job.ExpectedOutput]=None, need_to_access_cluster=False, network=None, setup_cmds: list[str]=None, teardown_cmds: list[str]=None, unexpected_outputs: list[test_job.UnexpectedOutput]=None, parallel_cmds: list[test_job.ParallelCommand]=None, environment: dict[str, str]=None) -> None:
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
        self._stop_event = False

        # Validate args
        if cmd_to_run_in_cli and parallel_cmds:
            raise ValueError(f"Test {test_name} cannot have both 'cmd-to-run-in-cli' and 'parallel-cmds'")

        if not cmd_to_run_in_cli and not parallel_cmds:
            raise ValueError(f"Test {test_name} must have either 'cmd-to-run-in-cli' or 'parallel-cmds'")

        if not expected_outputs and not unexpected_outputs:
            raise ValueError(f"Test {test_name} has no expected outputs or unexpected outputs. At least one of them is required.")

    @property
    def stop_event(self):
        return self._stop_event

    @stop_event.setter
    def stop_event(self, value: bool):
        common.info(f"Setting stop_event for CLI test {self.test_name} to {value}")
        self._stop_event = value

    def __call__(self, args) -> result.TestResult:
        # Run setup commands
        try:
            self._run_setup_cmds(args)
        except Exception as e:
            common.error(f"Setup command failed for test {self.test_name}: {e}")
            return result.TestResult(self.test_name, producing_task_name=self.producing_task_name, status=test_job.TestStatuses.FAIL, exception=e)

        try:
            # Launch the CLI command
            common.debug(f"Running CLI command for test {self.test_name}...")
            res = self._run_cli(args)
            if res.status != result.TestStatuses.SUCCESS:
                common.error(f"CLI command failed for test {self.test_name}: {res.msg if res.msg else res.exception}")
                try:
                    self._run_teardown_cmds(args)
                except Exception as e:
                    common.error(f"Teardown command failed for test {self.test_name} after CLI failure: {e}")
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
                try:
                    self._run_teardown_cmds(args)
                except Exception as e:
                    common.error(f"Teardown command failed for test {self.test_name} after DUT check failure: {e}")
                return results[0]

            try:
                self._run_teardown_cmds(args)
            except Exception as e:
                common.error(f"Teardown command failed for test {self.test_name} after successful DUT check: {e}")
                return result.TestResult(self.test_name, producing_task_name=self.producing_task_name, status=test_job.TestStatuses.FAIL, exception=e)

            return result.TestResult(self.test_name, producing_task_name=self.producing_task_name, status=test_job.TestStatuses.SUCCESS)
        except Exception as e:
            common.error(f"Exception while running test {self.test_name}: {e}")
            try:
                self._run_teardown_cmds(args)
            except Exception as e:
                common.error(f"Teardown command failed for test {self.test_name} after exception: {e}")
            return result.TestResult(self.test_name, producing_task_name=self.producing_task_name, status=test_job.TestStatuses.FAIL, exception=e)

    def _kill_child_threads(self):
        """
        Kill any child threads that this test has spawned.
        """
        common.kill_managed_threads()

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

        for u in self.expected_outputs:
            where = u.evaluated_where(args)
            if where in pids:
                u.pid = pids[u.evaluated_where(args)]
            elif where == "artie-cli":
                # We don't have a PID for the CLI container yet. We'll handle that later when we run the CLI command.
                u.cli = True
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

    def _find_expected_cli_out(self, args) -> test_job.ExpectedOutput|None:
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

        def run_cmd_thread(idx: int, parallel_cmd: test_job.ParallelCommand):
            """Run a single command in a thread and store its output."""
            try:
                common.info(f"Running parallel command {idx+1}/{len(self.parallel_cmds)}: {parallel_cmd.cmd}")
                logs = self._try_ntimes(args, 5, parallel_cmd.cmd)
                results[idx] = logs
                common.debug(f"Finished parallel command {idx+1}/{len(self.parallel_cmds)}. Results collected: {logs[:1000]}...")
            except Exception as e:
                exceptions[idx] = e

        # Start all commands in parallel
        threads = []
        for idx, parallel_cmd in enumerate(self.parallel_cmds):
            common.debug(f"Starting thread for parallel command {idx+1}/{len(self.parallel_cmds)}...")
            thread = threading.Thread(target=run_cmd_thread, args=(idx, parallel_cmd))
            thread.start()
            threads.append(thread)

        # Wait for all threads to complete
        for thread in threads:
            common.debug(f"Waiting for thread {thread.name} to finish...")
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
                    common.debug(f"Expected output '{expected_out.what}' not found in parallel command {idx+1}")
                    return result.TestResult(self.test_name, self.producing_task_name, result.TestStatuses.FAIL, msg=f"Expected output '{expected_out.what}' not found in parallel command {idx+1}")

            # Check unexpected outputs
            for unexpected_out in parallel_cmd.unexpected_outputs:
                if unexpected_out.what in logs:
                    common.debug(f"Unexpected output '{unexpected_out.what}' found in parallel command {idx+1}")
                    return result.TestResult(self.test_name, self.producing_task_name, result.TestStatuses.FAIL, msg=f"Unexpected output '{unexpected_out.what}' found in parallel command {idx+1}")

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
            except Exception as e:
                if i != n - 1 and not self.stop_event:
                    common.warning(f"Got an exception while trying to run CLI. Will try {n - (i+1)} more times. Exception: {e}")
                    time.sleep(1)
                elif self.stop_event:
                    common.warning(f"Got an exception while trying to run CLI. Would normally try {n - (i+1)} more times, but this test has been told to stop. Exception: {e}")
                    raise e
                else:
                    raise e

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

        for unexpected_out in self.unexpected_outputs:
            if unexpected_out.cli:
                continue  # We check for unexpected CLI output in _run_cli

            r = unexpected_out.check(args, self.test_name, self.producing_task_name, timeout_s)
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

class SingleContainerCLISuiteJob(test_job.TestJob):
    def __init__(self, steps: List[CLITest], docker_image_under_test: str | dependency.Dependency, cmd_to_run_in_dut: str|None, dut_port_mappings: Dict[int, int]) -> None:
        super().__init__(artifacts=[], steps=steps)
        self.dut = docker_image_under_test
        self.cmd_to_run_in_dut = cmd_to_run_in_dut
        self.dut_port_mappings = dut_port_mappings
        self._dut_container = None

    def setup(self, args):
        """
        Set up the DUT by using this object's `docker_cmd`.
        """
        super().setup(args)
        if issubclass(type(self.dut), dependency.Dependency):
            docker_image_name = self.dut.evaluate(args).item
        else:
            docker_image_name = str(docker.construct_docker_image_name(args, self.dut, common.host_platform()))

        kwargs = {'environment': {'ARTIE_RUN_MODE': 'unit'}, 'ports': self.dut_port_mappings}
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
