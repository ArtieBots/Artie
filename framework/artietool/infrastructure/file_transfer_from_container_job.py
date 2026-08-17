from artietool.infrastructure import artifact
from . import artifact
from . import dependency
from . import job
from . import result
from .. import common
from .. import docker
from typing import List
import os
import shutil

class FileTransferFromContainerJob(job.Job):
    def __init__(self, artifacts: List[artifact.Artifact], image: dependency.Dependency|str, fw_files_in_container: List[str]) -> None:
        super().__init__(artifacts)
        self.image = image
        self.fw_fpaths_in_container = fw_files_in_container

    def _evaluate_docker_image(self, args) -> str:
        # If the Docker image is a dependency, we have to evaluate it
        if issubclass(type(self.image), dependency.Dependency):
            docker_image_name = self.image.evaluate(args).item
        else:
            docker_image_name = self.image
        return docker_image_name

    def _expected_target_fpaths(self, args, docker_image_name: str) -> List[str]:
        """
        The final (tag-stamped) paths that _copy_out_files() would produce for this image.
        """
        tag = docker.get_tag_from_name(docker_image_name)
        targets = []
        for fpath in self.fw_fpaths_in_container:
            fname_no_ext, suf = os.path.splitext(os.path.basename(fpath))
            targets.append(os.path.join(args.artifact_folder, fname_no_ext) + "-" + tag + suf)
        return targets

    def _can_run_container(self, docker_image_name: str) -> tuple:
        """
        Returns (can_run, image_arch, host_arch). We can only run a container to copy files
        out if the image is present in this machine's image store AND was built for this
        machine's architecture. Neither is guaranteed: the image's build step may have been
        skipped by a --platforms filter (e.g. the amd64-only firmware builders during an
        arm64-only CI job).
        """
        image_arch = docker.get_local_image_architecture(docker_image_name)
        host_arch = docker.get_host_architecture()
        return (image_arch != "" and (host_arch == "" or image_arch == host_arch), image_arch, host_arch)

    def _copy_out_files(self, args, docker_image_name: str):
        common.info(f"Running a Docker container from image {docker_image_name} to retrieve FW files...")
        docker.docker_copy(docker_image_name, self.fw_fpaths_in_container, args.artifact_folder)

        # Rename the files to include the tag of the Docker file that built them
        tag = docker.get_tag_from_name(docker_image_name)
        fnames = [os.path.basename(fpath) for fpath in self.fw_fpaths_in_container]
        for fname in fnames:
            fpath = os.path.join(args.artifact_folder, fname)
            fname_no_ext, suf = os.path.splitext(fpath)
            target = os.path.join(args.artifact_folder, fname_no_ext) + "-" + tag + suf
            if os.path.isfile(target):
                os.remove(target)
            shutil.move(fpath, target)

    def __call__(self, args) -> result.JobResult:
        if getattr(args, 'manifests_only', False):
            common.info("--manifests-only given: skipping the container file transfer.")
            self.mark_all_artifacts_as_built()
            return result.JobResult(self.name, success=True, artifacts=self.artifacts)

        docker_image_name = self._evaluate_docker_image(args)

        can_run_container, image_arch, host_arch = self._can_run_container(docker_image_name)
        if can_run_container:
            self._copy_out_files(args, docker_image_name)
        else:
            targets = self._expected_target_fpaths(args, docker_image_name)
            if all(os.path.isfile(fpath) for fpath in targets):
                common.info(f"Cannot run a container from {docker_image_name} on this machine (image arch: {image_arch or 'not present'}, host arch: {host_arch}), but the files it would produce are already in {args.artifact_folder} (e.g. from an attached CI workspace). Skipping the container file transfer.")
            else:
                msg = (f"Cannot transfer files out of {docker_image_name}: the image is not runnable on this machine "
                       f"(image arch: {image_arch or 'not present locally'}, host arch: {host_arch}), and the expected files were not found: {targets}. "
                       f"If this image's build step was skipped by a --platforms filter, make sure the files are available in {args.artifact_folder} (e.g. via the CI workspace).")
                common.error(msg)
                raise RuntimeError(msg)

        # Now mark all artifacts
        self.mark_all_artifacts_as_built()

        return result.JobResult(self.name, success=True, artifacts=self.artifacts)

    def clean(self, args):
        super().clean(args)

    def mark_if_cached(self, args):
        """
        Mark each artifact as built if it can be found on disk.

        Override parent's version: we need to check if the files can be found on
        disk. If they aren't found, we need to check if the container that produces
        them is cached. If it is, we can pull from the container, and then mark ourselvees
        as cached as well.
        """
        for art in self.artifacts:
            art.mark_if_cached(args)

        found_on_disk = all([art.built for art in self.artifacts])
        if found_on_disk:
            # All files found on disk. Nothing left to do. We are definitely cached and the artifacts have marked themselves.
            return

        # If we couldn't find the files on disk, we need to see if the files
        # are produced by a container that can be found.
        try:
            docker_image_name = self._evaluate_docker_image(args)
        except KeyError:
            # The docker image name couldn't be determined. Default to not cached.
            return
        if not docker.check_and_pull_if_docker_image_exists(args, docker_image_name):
            # The image does not exist/is not cached. We can't get the files out without building an image.
            # So we aren't cached and the artifacts have marked themselves.
            return

        # If we pulled the image or it exists locally, let's copy out the files - but only if we
        # can actually run it on this machine (see _can_run_container).
        can_run_container, image_arch, host_arch = self._can_run_container(docker_image_name)
        if not can_run_container:
            common.info(f"Cannot run a container from {docker_image_name} on this machine (image arch: {image_arch or 'not present'}, host arch: {host_arch}), so we cannot transfer files out of it. Defaulting to not cached.")
            return
        self._copy_out_files(args, docker_image_name)

        # Now remark the artifacts
        for art in self.artifacts:
            art.mark_if_cached(args)

        # Check that they are all accounted for, otherwise we've hit an odd error (possibly a misconfiguration of the job)
        found_on_disk = all([art.built for art in self.artifacts])
        if not found_on_disk:
            errmsg = f"Not all artifacts were accounted for after transferring them from the container. Expected artifacts: {self.artifacts}; Docker image we used to transfer files: {docker_image_name}; Job: {self}"
            common.error(errmsg)
            raise AssertionError(errmsg)
