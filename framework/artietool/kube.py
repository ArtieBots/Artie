"""
Semi-permeable wrapper around the Kubernetes API for convenience stuff.

This module is separated from the `kubespec` module, which provides the programatic specification
that we use for Artie Kubernetes deployments.
"""
from typing import Dict
from typing import List
from typing import Tuple
from . import common
from artie_tooling import hw_config
from artie_tooling import kubespec
import argparse
import io
import dataclasses
import enum
import io
import json
import kubernetes as k8s
import pathlib
import subprocess
import urllib3
import yaml

class HelmChartStatuses(enum.StrEnum):
    """
    Possible status values for a Helm Chart.
    """
    UNKNOWN = "unknown"
    DEPLOYED = "deployed"
    UNINSTALLED = "uninstalled"
    SUPERSEDED = "superseded"
    FAILED = "failed"
    UNINSTALLING = "uninstalling"
    PENDING_INSTALL = "pending-install"
    PENDING_UPGRADE = "pending-upgrade"
    PENDING_ROLLBACK = "pending-rollback"

class JobStatuses(enum.StrEnum):
    """
    Possible statuses for a Kubernetes Job.
    """
    INCOMPLETE = "Incomplete"  # At least one pod is not done running
    FAILED = "Failed"  # At least one pod has 'failed' status
    SUCCEEDED = "Succeeded"  # All pods have completed and succeeded
    UNKNOWN = "Unknown"  # Can't determine the state of this job

@dataclasses.dataclass
class HelmChart:
    """
    A Helm chart class to keep track of its various bits.
    """
    # The name of the Helm chart, as used in deployments.
    name: str
    # The version to use for this Helm chart or None, in which case we do not override the chart's value for this.
    version: str | None
    # The chart reference, typically a path to one on the file system, but could be a URL.
    chart: str

def _configure(args: argparse.Namespace, need_artie_name: bool = False):
    """
    Load the Kube config from the environment. If `need_artie_name` is `True`, we
    also configure `args` with 'artie_name` based on the only Artie we find in the cluster.
    If no 'artie_name' is given, `need_artie_name` is `True`, and there is more than one or less
    than one Artie on the cluster, we raise a ValueError (for zero Arties) or a KeyError (for more than one Artie).
    """
    config_file = args.kube_config
    k8s.config.load_kube_config(config_file=config_file)

    if need_artie_name:
        _determine_artie_name(args)

def _determine_artie_name(args: argparse.Namespace) -> argparse.Namespace:
    """
    Determine what Artie name we want to use. If the user has not specified one and we can't
    determine it from the cluster, we throw an exception.

    This function adds the appropriate argument ('artie_name') to `args`
    if `args` does not already have it, and it fills in the value if the value
    is not already filled in by the user.
    """
    if 'artie_name' in args and args.artie_name is not None:
        return args

    args.artie_name = None
    names = get_artie_names(args)
    if len(names) == 0:
        raise ValueError(f"Cannot find any deployed Arties.")
    elif len(names) > 1:
        raise KeyError(f"Cannot determine a unique Artie ID from the cluster. More than one found. Please specify a single one with --artie-name. Names found: {names}")
    else:
        args.artie_name = names[0]
        return args

def _get_node_from_name(v1: k8s.client.CoreV1Api, name: str) -> k8s.client.V1Node:
    """
    Retrieve the API node object that has the given name in the cluster.
    Raises ValueError if we can't find the node.
    """
    node_list = v1.list_node().items
    for node in node_list:
        if node.metadata.name == name:
                return node

    raise ValueError(f"Cannot find node {name} in cluster.")

def _handle_transient_network_errors(cmd: List[str], n=5):
    """
    Run the given `cmd` up to `n` times, returning (whether we succeeded or not, retcode, stderr, stdout).
    """
    n_retries = 3
    i = 1
    p = subprocess.run(cmd, capture_output=True, encoding='utf-8')
    while i < n_retries and p.returncode !=0 and ('tcp' in p.stdout.lower() or 'tcp' in p.stderr.lower()):
        common.warning(f"Networking error. Retrying.")
        p = subprocess.run(cmd, capture_output=True, encoding='utf-8')
        i += 1
    return p.returncode == 0, p.returncode, p.stderr, p.stdout

def _update_helm_dependencies(args, chart: str):
    """
    Update Helm chart dependencies if the chart has any.
    Silently succeeds if there are no dependencies or if the chart doesn't exist.
    """
    # Check if chart path is a directory
    chart_path = pathlib.Path(chart)
    if not chart_path.is_dir():
        return

    # Check if Chart.yaml exists and has dependencies
    chart_yaml = chart_path / "Chart.yaml"
    if not chart_yaml.exists():
        return

    # Read Chart.yaml to check for dependencies
    try:
        with open(chart_yaml, 'r') as f:
            chart_data = yaml.safe_load(f)

        if not chart_data or 'dependencies' not in chart_data:
            return

        # Chart has dependencies, update them
        common.info(f"Updating Helm chart dependencies for {chart_path.name}...")
        cmd = ["helm", "dependency", "update", str(chart_path)]

        success, retcode, stderr, stdout = _handle_transient_network_errors(cmd)
        if not success:
            common.warning(f"Failed to update chart dependencies: {stderr}")
        else:
            common.info(f"Successfully updated dependencies for {chart_path.name}")

    except Exception as e:
        common.warning(f"Could not check/update chart dependencies: {e}")

def add_helm_repo(name: str, url: str):
    """
    Add a Helm repo.
    """
    subprocess.run(["helm", "repo", "add", name, url]).check_returncode()

def assign_node_labels(args, node_name: str, labels: dict[str, str]):
    """
    Assigns the given labels to the the given node.
    Labels should be a dict of labels to values.
    """
    _configure(args)
    v1 = k8s.client.CoreV1Api()

    body = {
        "metadata": {
            "labels": labels
        }
    }
    node = _get_node_from_name(v1, node_name)
    v1.patch_node(node.metadata.name, body)

def assign_node_taints(args, node_name: str, node_taints: dict[str, tuple[str, kubespec.TaintEffects]]):
    """
    Assign the given node taints to the given node. `node_taints` should be a
    dict of the form:
    {
        taint_key: (taint_value, taint_effect)
    }
    """
    _configure(args)
    v1 = k8s.client.CoreV1Api()

    body = {
        "spec": {
            "taints": [
                {
                    "effect": effect,
                    "key": key,
                    "value": val,
                } for key, (val, effect) in node_taints.items()
            ]
        }
    }
    node = _get_node_from_name(v1, node_name)
    v1.patch_node(node.metadata.name, body)

def check_if_helm_chart_is_deployed(args, chart_name: str, namespace=kubespec.ArtieK8sValues.DEFAULT_NAMESPACE) -> bool:
    """
    Checks if the given chart name is present in the cluster.

    Convenience function for check_helm_chart_status() == HelmChartStatuses.deployed
    """
    return check_helm_chart_status(args, chart_name, str(namespace).lower()) == HelmChartStatuses.DEPLOYED

def check_helm_chart_status(args, chart_name: str, namespace=kubespec.ArtieK8sValues.DEFAULT_NAMESPACE) -> HelmChartStatuses|None:
    """
    Returns the status for the given chart, or None if it isn't found in the namespace.
    """
    cmd = ["helm", "list", "--kubeconfig", args.kube_config, "--namespace", str(namespace).lower(), "--filter", chart_name, "--output", "json"]
    success, retcode, stderr, stdout = _handle_transient_network_errors(cmd)
    if not success:
        raise OSError(f"Helm failed: {stderr}; {stdout}")

    json_object = json.loads(stdout)
    if len(json_object) == 0:
        return None
    elif len(json_object) > 1:
        # Regex matched more than one chart name for some reason. This probably shouldn't
        # have happened. Just look for the first exact match and return that one.
        for o in json_object:
            if o['name'] == chart_name:
                return HelmChartStatuses(o['status'])
        raise AssertionError("This shouldn't be possible: regex matched more than one chart name, but then couldn't find the chart with the right name.")
    else:
        return HelmChartStatuses(json_object[0]['status'])

def check_job_status(args, job_name: str, namespace=kubespec.ArtieK8sValues.DEFAULT_NAMESPACE) -> JobStatuses:
    """
    Check and return the status of the given job.
    """
    _configure(args)
    v1 = k8s.client.BatchV1Api()

    try:
        job = v1.read_namespaced_job_status(job_name, str(namespace).lower())
        status = job.status

        if status.active is not None and status.active > 0:
            # At least one pod is still pending or running
            return JobStatuses.INCOMPLETE
        elif status.failed is not None and status.failed > 0:
            # At least one pod failed
            return JobStatuses.FAILED
        elif status.succeeded is not None and status.succeeded > 0:
            # No active or failed pods AND at least one pod is successful. Looks good.
            return JobStatuses.SUCCEEDED
        else:
            # Can't understand this state. No active, failed, or succeeded pods.
            common.error(f"Kubernetes job {str(namespace).lower()}:{job_name} has unknown status - no active, failed, or succeeded pods.")
            return JobStatuses.UNKNOWN
    except urllib3.exceptions.MaxRetryError:
        common.warning(f"Could not get the status for k8s job {str(namespace).lower()}:{job_name}; transient networking error")
        return JobStatuses.UNKNOWN

def create_namespace_if_not_exists(args, namespace: str):
    """
    Create the given namespace if it does not already exist.
    """
    _configure(args)
    v1 = k8s.client.CoreV1Api()

    # Check if namespace exists
    try:
        v1.read_namespace(str(namespace).lower())
        common.debug(f"Namespace {str(namespace).lower()} already exists.")
        namespace_does_not_exist = False
    except k8s.client.exceptions.ApiException as e:
        if e.status == 404:
            namespace_does_not_exist = True
        else:
            raise

    if namespace_does_not_exist:
        common.info(f"Creating namespace {str(namespace).lower()}...")
        namespace_object = kubespec.ArtieNamespace(str(namespace).lower(), args.docker_tag or "unspecified")
        create_from_yaml(args, yaml.dump(namespace_object.to_dict()), namespace=str(namespace).lower())
        common.info(f"Namespace {str(namespace).lower()} created.")

def create_from_yaml(args, yaml_contents: str, namespace=kubespec.ArtieK8sValues.DEFAULT_NAMESPACE):
    """
    Create a K8s resource from the given `yaml_contents`, which should be a YAML definition
    like you would put in the K8s YAMl file.

    Returns the list of items that were created from the YAML file, or the single object
    that was created in the case that the list would only contain one object.
    """
    _configure(args)
    client = k8s.client.ApiClient()

    # Convert from raw YAML into Python
    print("YAML CONTENTS:", yaml_contents)
    yaml_object = yaml.safe_load(io.StringIO(yaml_contents))
    print("YAML Object:", yaml_object)
    result = k8s.utils.create_from_yaml(client, yaml_objects=[yaml_object], namespace=str(namespace).lower())

    # For whatever reason, the create_from_yaml function creates randomly nested lists.
    while hasattr(result, '__len__') and len(result) == 1 and not issubclass(type(result), dict):
        result = result[0]

    return result

def delete_configmap(args, name: str, namespace=kubespec.ArtieK8sValues.DEFAULT_NAMESPACE, ignore_errors=False):
    """
    Delete a configmap.
    """
    _configure(args)
    v1 = k8s.client.CoreV1Api()
    try:
        v1.delete_namespaced_config_map(name, str(namespace).lower(), grace_period_seconds=0, propagation_policy='Foreground')
    except Exception as e:
        if not ignore_errors:
            raise e
        else:
            common.warning(f"Error deleting config map {str(namespace).lower()}:{name}: {e}")

def delete_helm_release(args, chart_name: str, namespace=kubespec.ArtieK8sValues.DEFAULT_NAMESPACE):
    """
    Delete the given Helm chart.
    """
    status = check_helm_chart_status(args, chart_name, str(namespace).lower())
    if not status:
        common.info(f"Helm release {chart_name} not present. Cannot delete it.")
        return

    cmd = ["helm", "delete", "--kubeconfig", args.kube_config, "--namespace", str(namespace).lower(), "--wait", chart_name, "--timeout", str(args.kube_timeout_s) + 's']
    success, retcode, stderr, stdout = _handle_transient_network_errors(cmd)
    if not success:
        raise OSError(f"Helm failed: {stderr}; {stdout}")

def delete_job(args, job_name: str, namespace=kubespec.ArtieK8sValues.DEFAULT_NAMESPACE, ignore_errors=False):
    """
    Delete a K8s job.
    """
    _configure(args)
    v1 = k8s.client.BatchV1Api()
    try:
        v1.delete_namespaced_job(job_name, str(namespace).lower(), grace_period_seconds=0, propagation_policy='Foreground')
    except Exception as e:
        if not ignore_errors:
            raise e
        else:
            common.warning(f"Error deleting job {str(namespace).lower()}:{job_name}: {e}")

def delete_namespace(args, namespace: str, ignore_errors=False):
    """
    Delete the given namespace.
    """
    _configure(args)
    v1 = k8s.client.CoreV1Api()
    try:
        v1.delete_namespace(str(namespace).lower(), grace_period_seconds=0, propagation_policy='Foreground')
    except Exception as e:
        if not ignore_errors:
            raise e
        else:
            common.warning(f"Error deleting namespace {str(namespace).lower()}: {e}")

def delete_node(args, node_name: str, ignore_errors=False):
    """
    Remove the given node from the cluster as gracefully as we can.
    """
    _configure(args)
    v1 = k8s.client.CoreV1Api()
    try:
        v1.delete_node(node_name, propagation_policy='Foreground')  # delete dependant children, then parents
    except Exception as e:
        if not ignore_errors:
            raise e
        else:
            common.warning(f"Error deleting node {node_name}: {e}")

def delete_pod(args, pod_name: str, namespace=kubespec.ArtieK8sValues.DEFAULT_NAMESPACE, ignore_errors=False):
    """
    Delete a K8s Pod.
    """
    _configure(args)
    v1 = k8s.client.CoreV1Api()
    try:
        v1.delete_namespaced_pod(pod_name, str(namespace).lower(), grace_period_seconds=0, propagation_policy='Foreground')
    except Exception as e:
        if not ignore_errors:
            raise e
        else:
            common.warning(f"Error deleting pod {str(namespace).lower()}:{pod_name}: {e}")

def delete_secret(args, name: str, namespace=kubespec.ArtieK8sValues.DEFAULT_NAMESPACE, ignore_errors=False):
    """
    Delete a secret.
    """
    _configure(args)
    v1 = k8s.client.CoreV1Api()
    try:
        v1.delete_namespaced_secret(name, str(namespace).lower(), grace_period_seconds=0, propagation_policy='Foreground')
    except Exception as e:
        if not ignore_errors:
            raise e
        else:
            common.warning(f"Error deleting secret {str(namespace).lower()}:{name}: {e}")

def get_all_pods(args, namespace=kubespec.ArtieK8sValues.DEFAULT_NAMESPACE) -> list[k8s.client.V1Pod]:
    """
    Get all pods for the given namespace.
    """
    _configure(args)
    v1 = k8s.client.CoreV1Api()
    podlist = v1.list_namespaced_pod(str(namespace).lower())
    return podlist.items

def get_artie_names(args) -> List[str]:
    """
    Returns a list of names of Arties found on the cluster.

    Note that this function CANNOT call _configure() with need_artie_name=True,
    because that would cause infinite recursion.
    """
    _configure(args, need_artie_name=False)
    v1 = k8s.client.CoreV1Api()
    node_list = v1.list_node().items
    name_list = []
    for node in node_list:
        if node.metadata.labels is not None and kubespec.ArtieK8sKeys.ARTIE_ID in node.metadata.labels:
            artie_name = node.metadata.labels[kubespec.ArtieK8sKeys.ARTIE_ID]
            name_list.append(artie_name)

    return list(set(name_list))

def get_artie_hw_config(args, namespace=kubespec.ArtieK8sValues.DEFAULT_NAMESPACE) -> hw_config.HWConfig:
    """
    Access the Artie cluster to retrieve the hardware configuration for an Artie.

    After this call, we guarantee `args` has `artie_name` in it.
    """
    _configure(args, need_artie_name=True)
    v1 = k8s.client.CoreV1Api()

    # Retrieve the ConfigMap
    configmap_name = kubespec.HWConfigMap.get_name()
    try:
        configmap = v1.read_namespaced_config_map(configmap_name, str(namespace).lower())
    except k8s.client.exceptions.ApiException as e:
        if e.status == 404:
            raise ValueError(f"Hardware configuration ConfigMap '{configmap_name}' not found for Artie '{args.artie_name}'")
        else:
            raise

    buf = io.BytesIO(configmap.data.encode())
    conf = hw_config.HWConfig.from_config(buf)
    return conf

def get_node_names(args) -> list[str]:
    """
    Returns a list of node names - one for each one found in the cluster.
    """
    _configure(args)
    v1 = k8s.client.CoreV1Api()

    node_list = v1.list_node()
    names = [node.metadata.name for node in node_list.items]
    return names

def get_node_labels(args, node_name: str) -> dict[str, str]:
    """
    Gets the dict of node label key:value pairs for the given node.
    Raises a ValueError if the node is not found in the cluster.
    """
    _configure(args)
    v1 = k8s.client.CoreV1Api()

    node = _get_node_from_name(v1, node_name)
    return node.metadata.labels

def get_pods_from_job(args, job_name: str, namespace=kubespec.ArtieK8sValues.DEFAULT_NAMESPACE) -> list[k8s.client.V1Pod]:
    """
    Get all the pods for a given job and return them as a List of K8s Job objects.
    """
    _configure(args)
    v1 = k8s.client.BatchV1Api()

    v1 = k8s.client.CoreV1Api()
    podlist = v1.list_namespaced_pod(str(namespace).lower(), label_selector=f"job-name={job_name}")
    return podlist.items

def install_helm_chart(args, name: str, chart: str, sets=None, namespace=kubespec.ArtieK8sValues.DEFAULT_NAMESPACE):
    """
    Install the given Helm chart, overriding any keys given in the `sets` dict with the corresponding values.
    """
    if sets is None:
        sets = {}

    # Update chart dependencies if the chart has any
    _update_helm_dependencies(args, chart)

    # Base command
    cmd = ["helm", "install", "--kubeconfig", args.kube_config, "--namespace", str(namespace).lower(), "--create-namespace", "--wait", "--timeout", str(args.kube_timeout_s) + 's']

    # Add value overrides
    for k, v in sets.items():
        cmd += ["--set", f"{k}={v}"]

    # Command suffix
    cmd += [name, chart]

    # Run the command
    success, retcode, stderr, stdout = _handle_transient_network_errors(cmd)
    if not success:
        common.error(f"Helm chart installation failed.")
        try:
            if not check_if_helm_chart_is_deployed(args, name, str(namespace).lower()):
                common.error(f"Attempting to clean up after ourselves...")
                delete_helm_release(args, name, str(namespace).lower())
        except Exception as e:
            cleancmd = f"helm delete --kubeconfig {args.kube_config} --namespace {str(namespace).lower()} --wait {name}"
            common.warning(f"Could not clean up the failed Helm deployment. You may need to do so manually with {cleancmd}. Error deploying in the first place: {stderr}; {stdout}")
            raise e
        raise OSError(f"Helm failed: {stderr}; {stdout}")

def log_job_results(args, job_name: str, namespace=kubespec.ArtieK8sValues.DEFAULT_NAMESPACE):
    """
    Log all lines from all pods in the given job.
    """
    _configure(args)
    v1 = k8s.client.CoreV1Api()

    pods = get_pods_from_job(args, job_name, str(namespace).lower())
    for pod in pods:
        logs = v1.read_namespaced_pod_log(pod.metadata.name, str(namespace).lower())
        common.info(f"Reading logs from pod {str(namespace).lower()}:{pod.metadata.name}:")
        for line in logs.splitlines():
            common.info(line.rstrip())

def node_is_online(args, node_name: str) -> bool:
    """
    Returns True if the we can see the given node is online. False otherwise.
    """
    _configure(args)
    v1 = k8s.client.CoreV1Api()

    node_list = v1.list_node().items
    for node in node_list:
        if node.metadata.name == node_name:
            conditions = node.status.conditions
            for c in conditions:
                if c.type == "Ready" and c.status != "True":
                    return False
                elif c.type == "Ready" and c.status == "True":
                    return True

    # Couldn't find the node
    return False

def uninstall_helm_chart(args, name: str, namespace=kubespec.ArtieK8sValues.DEFAULT_NAMESPACE):
    """
    Uninstall the given chart.
    """
    cmd = ["helm", "uninstall", "--namespace", str(namespace).lower(), "--wait", name, "--timeout", str(args.kube_timeout_s) + 's']
    success, retcode, stderr, stdout = _handle_transient_network_errors(cmd)
    if not success:
        raise OSError(f"Error uninstalling chart: {stderr}; {stdout}")

def verify_access(args) -> Tuple[bool, Exception|None]:
    """
    Check that we have access to the cluster. Returns True if we do, False if we don't.
    If we fail to connect, we also return the exception.
    """
    common.info("Verifying access to Kubernetes cluster...")
    _configure(args)
    v1 = k8s.client.CoreV1Api()

    try:
        common.info("Listing nodes in cluster to verify access...")
        v1.list_node()
        common.info("Access to Kubernetes cluster verified.")
        return True, None
    except Exception as e:
        common.error(f"Failed to verify access to Kubernetes cluster: {e}")
        return False, e
