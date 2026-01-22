"""
All the machinery for installing.
"""
from typing import Dict
from typing import List
from typing import Tuple
from .. import common
from .. import kube
from artie_tooling import hw_config
from artie_tooling import kubespec
import argparse
import base64
import datetime
import getpass
import os
import pathlib
import re
import subprocess
import time
import yaml

def _create_unique_artie_name(node_names: List[str]) -> str:
    """
    Create a name for Artie that is not already included in `artie_names`.
    """
    template_names = [n for n in node_names if re.compile("Artie-[0-9]+").match(n)]
    number_suffixes = [int(n.split('-')[1]) for n in template_names]
    if number_suffixes:
        highest_number = sorted(number_suffixes)[-1]
        return f"Artie-{highest_number+1:03d}"
    else:
        return f"Artie-{1:03d}"

def _validate_k3s_token(k3s_token: str) -> bool:
    """
    Return False if the given token is obviously invalid, otherwise return True.
    """
    if not k3s_token:
        return False
    elif "^V" in k3s_token:
        return False
    else:
        return True

def _get_token_from_file(args) -> Tuple[bool, str|None]:
    with open(args.token_file, 'r') as f:
        k3s_token = "".join([line.strip() for line in f.readlines()])

    if not _validate_k3s_token(k3s_token):
        common.error(f"The given token file {args.token_file} does not contain a valid token, or perhaps it contains more than just a token.")
        return False, None

    return True, k3s_token

def _get_token_from_user(args) -> Tuple[bool, str|None]:
    k3s_token = args.token if args.token is not None else getpass.getpass("Token: ").strip()
    valid_token = _validate_k3s_token(k3s_token)
    if not valid_token and args.token:
        common.error("Invalid token given via command line.")
        return False, None
    elif not valid_token and not args.token:
        while not _validate_k3s_token(k3s_token):
            common.error("Invalid token. If you are copying and pasting, this may be a bug in your shell or in Python's getpass. Try passing the token in via a file instead.")
            k3s_token = getpass.getpass("Token: ").strip()

    return True, k3s_token

def _get_token(args) -> Tuple[bool, str|None]:
    """
    Get the K3S token (somehow - based on args).
    """
    if args.token_file:
        return _get_token_from_file(args)
    else:
        return _get_token_from_user(args)

def _initialize_controller_node(args, sbc_config: hw_config.SBC, artie_name: str, artie_ip: str, artie_username: str, artie_password: str, k3s_token: str, admin_ip: str) -> Tuple[bool, str, str]:
    """
    Initialize the controller node by creating its K3S config and joining it to the cluster.

    Returns: (success, controller node CA bundle, API server certificate)
    """
    node_name = f"{kubespec.ArtieK8sValues.NODE_ROLE_CONTROLLER}-{artie_name}".lower()

    common.info(f"Initializing SBC: {sbc_config.name} (node: {node_name})...")

    # A PEM passphrase will be required to create the controller node's CA and sign the API server certificate.
    # If args has a 'pem_passphrase' attribute, use that.
    # If not, we try the user password, but if that passphrase is too short (must be at least 4 chars),
    # we create one by base64 encoding the user name + password + artie name and take the first 8 characters.
    if hasattr(args, 'pem_passphrase') and args.pem_passphrase is not None:
        pem_passphrase = args.pem_passphrase
    elif len(artie_password) >= 4 and len(artie_password) <= 1024:
        pem_passphrase = artie_password
    else:
        common.info(f"User password length {len(artie_password)} is not suitable for use as a PEM passphrase (must be between 4 and 1024 characters). Generating a passphrase instead...")
        raw_pass = f"{artie_username}:{artie_password}:{artie_name}".encode('utf-8')
        b64_pass = base64.b64encode(raw_pass).decode('utf-8')
        pem_passphrase = b64_pass[:8]

    # Create K3S config file
    config_file_contents = "\n".join([
        f'node-name: "{node_name}"',
        f'token: "{k3s_token}"',
        f'server: "https://{admin_ip}:6443"',
    ])

    config_file = os.path.join(common.get_scratch_location(), f"config-{sbc_config.name}.yaml")
    with open(config_file, 'w') as f:
        f.write(config_file_contents)

    # Copy config to the SBC
    common.debug(f"Copying K3S config to controller node at /etc/rancher/k3s/config.yaml...")
    common.ssh("mkdir -p /etc/rancher/k3s", artie_ip, artie_username, artie_password)
    common.ssh("rm -rf /etc/rancher/k3s/config.yaml", artie_ip, artie_username, artie_password, fail_okay=True)
    common.scp_to(artie_ip, artie_username, artie_password, target=config_file, dest="/etc/rancher/k3s/config.yaml")
    os.remove(config_file)

    # Restart k3s agent
    common.debug("Restarting k3s-agent.service...")
    common.ssh("systemctl daemon-reload", artie_ip, artie_username, artie_password)
    common.ssh("systemctl restart k3s-agent.service", artie_ip, artie_username, artie_password)

    # Generate the CA bundle, generate the API server certificate, and sign the certificate
    # By the way, a .csr file is a Certificate Signing Request file. Typically, certificate signing
    # is done by CAs, and the way that works is that the CAs present an API that requires
    # a csr file which contains the request and associated metadata. The CA then returns
    # the requested .crt file, signed by the CA.
    common.debug("Generating controller node CA and API server certificate...")
    common.ssh("rm -rf /artie/controller-node-CA", artie_ip, artie_username, artie_password, fail_okay=True)
    common.ssh(f"mkdir -p /artie/controller-node-CA", artie_ip, artie_username, artie_password)
    with open(os.path.join(common.get_scratch_location(), "extfile"), 'w') as f:
        f.write(
"""authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment
subjectAltName = @alt_names
[alt_names]
DNS.1 = artie-api-server.local
""")
    common.scp_to(artie_ip, artie_username, artie_password, target=os.path.join(common.get_scratch_location(), "extfile"), dest="/artie/controller-node-CA/api-server.v3.ext")
    ## Create the RSA key
    common.ssh(f"openssl genrsa -aes256 -passout pass:{pem_passphrase} -out /artie/controller-node-CA/controller-node.key 4096", artie_ip, artie_username, artie_password)
    ## Generate a CA root (that's what the -x509 arg does)
    common.ssh(f"openssl req -x509 -passin pass:{pem_passphrase} -new -nodes -key /artie/controller-node-CA/controller-node.key -sha256 -out /artie/controller-node-CA/controller-node.crt -subj '/CN={artie_name}-controller-node/O=Artie'", artie_ip, artie_username, artie_password)
    ## Generate the CSR (the signing request) that we will use to get a cert for our Artie API server
    common.ssh(f"openssl req -new -passin pass:{pem_passphrase} -nodes -out /artie/controller-node-CA/api-server.csr -newkey rsa:4096 -keyout /artie/controller-node-CA/api-server.key -subj '/CN={artie_name}-api-server/O=Artie'", artie_ip, artie_username, artie_password)
    ## Use the CSR to create a controller-node-signed cert for the Artie API server
    common.ssh(f"openssl x509 -req -passin pass:{pem_passphrase} -in /artie/controller-node-CA/api-server.csr -CA /artie/controller-node-CA/controller-node.crt -CAkey /artie/controller-node-CA/controller-node.key -CAcreateserial -out /artie/controller-node-CA/api-server.crt -days 3650 -sha256 -extfile /artie/controller-node-CA/api-server.v3.ext", artie_ip, artie_username, artie_password)

    # Copy the CA bundle and API server cert from the controller node to this machine
    ca_bundle = common.scp_from(artie_ip, artie_username, artie_password, target="/artie/controller-node-CA/controller-node.crt", dest=None)
    api_server_cert = common.scp_from(artie_ip, artie_username, artie_password, target="/artie/controller-node-CA/api-server.crt", dest=None)

    return True, ca_bundle, api_server_cert

def _create_artie_metadata_configmap(args, artie_name: str, artie_config: hw_config.HWConfig):
    """
    Create a ConfigMap in Kubernetes containing metadata about this Artie's hardware configuration.
    """
    common.info("Creating Artie hardware metadata ConfigMap...")
    configmap = kubespec.HWConfigMap(artie_name=artie_name, image_tag=args.docker_tag, artie_hw_config=artie_config)

    # Delete existing ConfigMap if it exists
    try:
        kube.delete_configmap(args, configmap.configmap_name, ignore_errors=True, namespace=artie_name)
    except:
        pass

    # Create the ConfigMap
    common.debug(f"Creating ConfigMap with the following YAML:\n{yaml.dump(configmap.to_dict())}")
    kube.create_from_yaml(args, yaml.dump(configmap.to_dict()), namespace=artie_name)
    common.info(f"Created hardware metadata ConfigMap: {configmap.configmap_name}")

def _create_artie_api_server_secret(args, artie_name: str, api_server_cert: str):
    """
    Create a Kubernetes Secret containing the API server certificate for this Artie.
    """
    common.info("Creating Artie API server certificate Secret...")
    secret = kubespec.ArtieAPIServerCertSecret(artie_name=artie_name, image_tag=args.docker_tag, api_server_cert=api_server_cert)

    # Delete existing secret if it exists
    try:
        kube.delete_secret(args, secret.secret_name, ignore_errors=True, namespace=artie_name)
    except:
        pass

    # Create the secret
    kube.create_from_yaml(args, yaml.dump(secret.to_dict()), namespace=artie_name)
    common.info(f"Created API server certificate Secret: {secret.secret_name}")

def install(args):
    """
    Top-level install function.
    """
    retcode = 0

    # Check that we have kubectl access
    common.info("Checking for access to the cluster...")
    access, err = kube.verify_access(args)
    if not access:
        common.error(f"Could not access the Artie cluster: {err}")
        retcode = 1
        return retcode

    # Check that we have helm installed
    common.info("Checking for Helm...")
    try:
        p = subprocess.run("helm version".split(), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        retcode = p.returncode
    except FileNotFoundError:
        retcode = 1

    if retcode != 0:
        common.error(f"Could not run helm command. Is Helm installed?")
        return retcode

    # Load Artie type configuration
    common.info(f"Loading Artie type configuration from {args.artie_type_file}...")
    try:
        artie_config = hw_config.HWConfig.from_config(args.artie_type_file)
        common.info(f"Found {len(artie_config.sbcs)} SBC(s), {len(artie_config.mcus)} MCU(s), {len(artie_config.sensors)} sensor(s), and {len(artie_config.actuators)} actuator(s).")
    except Exception as e:
        common.error(f"Failed to load Artie type configuration: {e}")
        retcode = 1
        return retcode

    # Assert that there is at least a controller node
    if artie_config.controller_node is None:
        common.error(f"No {kubespec.ArtieK8sValues.NODE_ROLE_CONTROLLER} found in this Artie configuration. Cannot proceed with installation.")
        retcode = 1
        return retcode

    # Check for any already installed arties and get their names
    common.info("Checking existing Arties...")
    node_names = kube.get_node_names(args)
    artie_name = args.artie_name if args.artie_name is not None else _create_unique_artie_name(node_names)
    common.info(f"Will use {artie_name} for this Artie's name.")

    # Create the requested namespace if it doesn't already exist
    common.info(f"Ensuring namespace {artie_name.lower()} exists...")
    kube.create_namespace_if_not_exists(args, artie_name)
    common.info(f"Namespace {artie_name.lower()} is ready.")

    # Ask for password if we don't have it
    artie_ip = args.artie_ip
    artie_username = args.username
    artie_password = args.password if args.password is not None else getpass.getpass("Artie's Password: ")

    # Check that we can access Artie
    common.info("Verifying that we can connect to Artie...")
    access, err = common.verify_ssh_connection(artie_ip, artie_username, artie_password)
    if not access:
        common.error(f"Cannot access Artie at IP address {artie_ip} with username {artie_username} and the given password: {err}")
        retcode = 1
        return retcode

    # Ask for token if we don't have it
    got_token, k3s_token = _get_token(args)
    if not got_token:
        common.error("Problem getting Artie token. Cannot proceed.")
        retcode = 1
        return retcode

    # Initialize the controller node
    initialized_nodes = []
    controller_node = next((sbc for sbc in artie_config.sbcs if sbc.name == kubespec.ArtieK8sValues.NODE_ROLE_CONTROLLER), None)
    controller_node_name = f"{kubespec.ArtieK8sValues.NODE_ROLE_CONTROLLER}-{artie_name}".lower()
    if controller_node is None:
        common.error(f"No {controller_node_name} found in this Artie configuration. Cannot proceed with installation.")
        retcode = 1
        return retcode

    success, ca_bundle, api_server_cert = _initialize_controller_node(args, controller_node, artie_name, artie_ip, artie_username, artie_password, k3s_token, args.admin_ip)
    if not success:
        common.error(f"Failed to initialize SBC: {controller_node_name}")
        retcode = 1
        return retcode

    # Save the CA bundle to disk
    os.makedirs(args.ca_savedir, exist_ok=True)
    with open(pathlib.Path(args.ca_savedir) / "controller-node-ca.crt", "w") as f:
        f.write(ca_bundle)

    # Initialize all other SBCs from the configuration file
    common.info(f"Initializing {len(artie_config.sbcs)} single board computer(s)...")
    initialized_nodes.append((controller_node_name, controller_node))
    for sbc_config in artie_config.sbcs:
        # Skip if this is the controller node (we already initialized it)
        node_name = f"{sbc_config.name}-{artie_name}".lower()
        if node_name == controller_node_name:
            continue

        # Wait for node to come online
        timeout_s = 120
        common.info(f"Waiting up to {timeout_s} seconds for {node_name} to come online...")
        start_time = datetime.datetime.now().timestamp()
        while not kube.node_is_online(args, node_name):
            time.sleep(1)
            if datetime.datetime.now().timestamp() - start_time > timeout_s:
                common.error(f"Timed out waiting for {node_name} to come online.")
                retcode = 1
                return retcode

        common.info(f"Node {node_name} is online.")
        initialized_nodes.append((node_name, sbc_config))

    # Assign labels and taints to all nodes
    common.info("Configuring node labels and taints...")
    for node_name, sbc_config in initialized_nodes:
        # Assign node labels
        node_labels = kubespec.generate_artie_node_labels(artie_name, node_name)
        kube.assign_node_labels(args, node_name, node_labels)

        # Assign node taints
        node_taints = kubespec.generate_node_taints(node_name)
        kube.assign_node_taints(args, node_name, node_taints)
        common.info(f"Configured {node_name}")

    # Create ConfigMap with hardware metadata
    _create_artie_metadata_configmap(args, artie_name, artie_config)

    # Create Secret for API server certificate
    _create_artie_api_server_secret(args, artie_name, api_server_cert)

    return retcode

def fill_subparser(parser_install: argparse.ArgumentParser, parent: argparse.ArgumentParser):
    parser_install.add_argument("-u", "--username", required=True, type=str, help="Username for the Artie we are installing.")
    parser_install.add_argument("--artie-ip", required=True, type=common.validate_input_ip, help="IP address for the Artie we are installing.")
    parser_install.add_argument("--admin-ip", required=True, type=common.validate_input_ip, help="IP address for the admin server.")
    parser_install.add_argument("--artie-type-file", required=True, type=common.argparse_file_path_type, help="Path to the YAML file defining this Artie's hardware configuration (e.g., artie00/artie00.yml).")
    parser_install.add_argument("--ca-savedir", type=str, default=str(pathlib.Path.home() / ".artie" / "controller-node-CA"), help="Directory to save the CA certificate of the controller node.")
    parser_install.add_argument("--pem-passphrase", type=str, default=None, help="Passphrase to use for the PEM files created for the controller node's CA and API server certificate. If not given, the user's password will be used if it is between 4 and 1024 characters; otherwise, a passphrase will be generated automatically.")
    parser_install.add_argument("-p", "--password", type=str, default=None, help="The password for the Artie we are adding. It is more secure to pass this in over stdin when prompted, if possible.")
    parser_install.add_argument("-t", "--token", type=str, default=None, help="Token that you were given after installing Artie Admind. If you have lost it, you can find it on the admin server at /var/lib/rancher/k3s/server/node-token. It is more secure to pass this in over stdin when prompted, if possible.")
    parser_install.add_argument("--token-file", type=common.argparse_file_path_type, default=None, help="A file that contains the Artie Admind token as its only contents.")
    parser_install.set_defaults(cmd=install, module="install-artie")
