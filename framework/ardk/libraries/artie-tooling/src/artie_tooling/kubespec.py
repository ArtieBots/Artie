"""
This module contains K8S specification keys, utilities, and
procedurally-generated structures used throughout Artie Tooling.

Please see accompanying documentation for more details.

Note that this module should NOT depend on actual kubernetes libraries.
"""
from artie_tooling import hw_config
import base64
import dataclasses
import enum

class TaintEffects(enum.StrEnum):
    """
    Possible effects of node taints.
    """
    NO_SCHEDULE = "NoSchedule"
    """Kubernetes will not schedule the pod onto that node."""
    PREFER_NO_SCHEDULE = "PreferNoSchedule"  # Kubernetes will try to not schedule the pod onto the node
    """Kubernetes will try to not schedule the pod onto the node."""
    NO_EXECUTE = "NoExecute"
    """The pod will be evicted from the node (if it is already running on the node), and will not be scheduled onto the node (if it is not yet running on the node)."""

class ArtieK8sKeys(enum.StrEnum):
    """
    An enum of available Artie labels/annotations, etc.
    """
    ARTIE_ID = "artie/artie-id"
    """Label assigned to all Artie K8s objects to identify the Artie robot instance."""
    NODE_ROLE = "artie/node-role"
    """Label assigned to nodes to identify their role in the Artie cluster."""

    PHYSICAL_BOT_NODE_TAINT = "artie/physical-bot-node"
    """Taint assigned to physical bot nodes to prevent scheduling of non-bot workloads."""
    CONTROLLER_NODE_TAINT = "artie/controller-node"
    """Taint assigned to controller nodes to prevent scheduling of non-controller workloads."""

class ArtieK8sValues(enum.StrEnum):
    """
    An enum of known values for labels/annotations, etc.
    """
    DEFAULT_NAMESPACE = "artie"
    """The default Kubernetes namespace where Artie components are deployed. This can be overridden by the user."""

    NAME = "artie"
    """The name assigned to Artie K8s objects."""

    MANAGED_BY_ARTIE_TOOLING = "artie-tooling"
    """Value for app.kubernetes.io/managed-by indicating management by Artie Tooling."""
    MANAGED_BY_HELM = "Helm"
    """Value for app.kubernetes.io/managed-by indicating management by Helm."""

    CONTROLLER_NODE_ID = "controller-node"
    """Value for artie/node-role indicating a controller node. This is also the prefix for the controller node's name, suffixed by the Artie ID (lowercase)."""

    INSTANCE_INFRA = "artie-infrastructure"
    """Value for app.kubernetes.io/instance indicating infrastructure components."""

    PART_OF = "artie"
    """Value for app.kubernetes.io/part-of indicating part of the Artie ecosystem."""

    COMPONENT_HW_CONFIG = "hw-config"
    """Value for app.kubernetes.io/component indicating this is the HW configuration mapping."""
    COMPONENT_CERT_SECRET = "artie-api-server-cert"
    """Value for app.kubernetes.io/component indicating this is the Artie API server certificate secret."""
    COMPONENT_NAMESPACE = "namespace"
    """Value for app.kubernetes.io/component indicating this is the Artie namespace."""


class ArtieK8sDefaultLabels(enum.StrEnum):
    """
    An enum of default labels applied to all Artie K8s objects.
    """
    NAME = "app.kubernetes.io/name"
    MANAGED_BY = "app.kubernetes.io/managed-by"
    INSTANCE = "app.kubernetes.io/instance"
    VERSION = "app.kubernetes.io/version"
    COMPONENT = "app.kubernetes.io/component"
    PART_OF = "app.kubernetes.io/part-of"

@dataclasses.dataclass
class K8sObjectMeta:
    """
    Metadata for K8s objects.
    """
    name: str
    labels: dict[str, str] | None = dataclasses.field(default_factory=lambda: {
        ArtieK8sDefaultLabels.NAME: ArtieK8sValues.NAME,
        ArtieK8sDefaultLabels.MANAGED_BY: ArtieK8sValues.MANAGED_BY_ARTIE_TOOLING,
        ArtieK8sDefaultLabels.INSTANCE: ArtieK8sValues.INSTANCE_INFRA,
        ArtieK8sDefaultLabels.PART_OF: ArtieK8sValues.PART_OF,
    })
    annotations: dict[str, str] | None = None

    def to_dict(self) -> dict:
        """Get the dictionary representation of the metadata"""
        meta_dict = {
            "name": str(self.name),
            "labels": {str(k): str(v) for k, v in self.labels.items()} if self.labels else {},
            "annotations": {str(k): str(v) for k, v in self.annotations.items()} if self.annotations else {},
        }
        return meta_dict

@dataclasses.dataclass
class K8sBaseSpec:
    """
    A base K8s spec class.

    Call `to_dict()` to get the dictionary representation for use with the K8s API.
    """
    api_version: str = "v1"
    metadata: K8sObjectMeta = dataclasses.field(default_factory=lambda: K8sObjectMeta(name=ArtieK8sValues.NAME))

    def to_dict(self) -> dict:
        """Get the dictionary representation of the base spec"""
        return {
            "apiVersion": str(self.api_version),
            "metadata": self.metadata.to_dict(),
        }

@dataclasses.dataclass
class ConfigMap:
    """
    A base ConfigMap class.

    Call `to_dict()` to get the dictionary representation for use with the K8s API.
    """
    base_spec: K8sBaseSpec = dataclasses.field(default_factory=lambda: K8sBaseSpec())
    data: dict[str, str] = dataclasses.field(default_factory=dict)
    immutable: bool = False

    def to_dict(self) -> dict:
        """Get the dictionary representation of the ConfigMap"""
        spec_dict = self.base_spec.to_dict()
        spec_dict.update({
            "kind": "ConfigMap",
            "data": {str(k): str(v) for k, v in self.data.items()},
            "immutable": self.immutable,
        })
        return spec_dict

@dataclasses.dataclass
class Secret:
    """
    A base Secret class.

    Call `to_dict()` to get the dictionary representation for use with the K8s API.
    """
    base_spec: K8sBaseSpec = dataclasses.field(default_factory=lambda: K8sBaseSpec())
    data: dict[str, bytes] = dataclasses.field(default_factory=dict)
    immutable: bool = False
    type: str = "Opaque"

    def to_dict(self) -> dict:
        """Get the dictionary representation of the Secret"""
        spec_dict = self.base_spec.to_dict()
        spec_dict.update({
            "kind": "Secret",
            "data": {str(k): str(v) for k, v in self.data.items()},
            "immutable": self.immutable,
            "type": str(self.type),
        })
        return spec_dict

class HWConfigMap:
    """
    A ConfigMap that holds the hardware configuration for Artie.

    Call `to_dict()` to get the dictionary representation for use with the K8s API.
    """
    def __init__(self, artie_name: str, image_tag: str, artie_hw_config: hw_config.HWConfig):
        self.configmap_name = HWConfigMap.get_name()

        additional_labels = {
            ArtieK8sDefaultLabels.VERSION: image_tag,
            ArtieK8sDefaultLabels.COMPONENT: ArtieK8sValues.COMPONENT_HW_CONFIG,
            ArtieK8sKeys.ARTIE_ID: artie_name.lower(),
        }
        metadata = K8sObjectMeta(name=self.configmap_name)
        metadata.labels.update(additional_labels)

        base_spec = K8sBaseSpec(metadata=metadata)

        data = {k: v for k, v in artie_hw_config.to_yaml_dict().items()}

        self.configmap = ConfigMap(base_spec=base_spec, data=data)

    @staticmethod
    def get_name() -> str:
        """Get the standard name for the HW ConfigMap"""
        return f"artie-hw-config".lower()

    def to_dict(self) -> dict:
        """Get the dictionary representation of the HW ConfigMap"""
        return self.configmap.to_dict()

class ArtieAPIServerCertSecret:
    """
    A Secret that holds the Artie API server certificate (NOT the certificate
    for the Kubernetes API server).

    Call `to_dict()` to get the dictionary representation for use with the K8s API.
    """
    def __init__(self, artie_name: str, image_tag: str, api_server_cert: str):
        self.secret_name = ArtieAPIServerCertSecret.get_name()

        additional_labels = {
            ArtieK8sDefaultLabels.VERSION: image_tag,
            ArtieK8sDefaultLabels.COMPONENT: ArtieK8sValues.COMPONENT_CERT_SECRET,
            ArtieK8sKeys.ARTIE_ID: artie_name.lower(),
        }
        metadata = K8sObjectMeta(name=self.secret_name)
        metadata.labels.update(additional_labels)

        base_spec = K8sBaseSpec(metadata=metadata)

        # Base 64-encode the certificate
        data = {
            "tls.crt": base64.b64encode(api_server_cert.encode()).decode(),
            "tls.key": base64.b64encode(b"").decode(),  # No private key
        }

        # We use the "kubernetes.io/tls" type since this is a TLS certificate (see https://kubernetes.io/docs/concepts/configuration/secret/#secret-types)
        self.secret = Secret(base_spec=base_spec, data=data, type="kubernetes.io/tls")

    @staticmethod
    def get_name() -> str:
        """Get the standard name for the Artie API server cert Secret."""
        return f"artie-api-server-cert".lower()

    def to_dict(self) -> dict:
        """Get the dictionary representation of the API Server Cert Secret"""
        return self.secret.to_dict()

class ArtieNamespace:
    """
    A Kubernetes Namespace for an Artie instance.

    Call `to_dict()` to get the dictionary representation for use with the K8s API.
    """
    def __init__(self, artie_name: str, image_tag: str):
        self.namespace_name = artie_name.lower()

        additional_labels = {
            ArtieK8sDefaultLabels.MANAGED_BY: ArtieK8sValues.MANAGED_BY_ARTIE_TOOLING,
            ArtieK8sDefaultLabels.VERSION: image_tag,
            ArtieK8sDefaultLabels.COMPONENT: ArtieK8sValues.COMPONENT_NAMESPACE,
            ArtieK8sKeys.ARTIE_ID: artie_name.lower(),
        }
        metadata = K8sObjectMeta(name=self.namespace_name)
        metadata.labels.update(additional_labels)

        self.base_spec = K8sBaseSpec(metadata=metadata)

    def to_dict(self) -> dict:
        """Get the dictionary representation of the Namespace"""
        spec_dict = self.base_spec.to_dict()
        spec_dict.update({
            "kind": "Namespace",
        })
        return spec_dict

def generate_artie_node_labels(artie_name: str, node_name: str) -> dict[str, str]:
    """
    Generate the standard labels for an Artie node.
    """
    labels = {
        ArtieK8sKeys.ARTIE_ID: artie_name,
        ArtieK8sKeys.NODE_ROLE: node_name,
        ArtieK8sDefaultLabels.NAME: ArtieK8sValues.NAME,
        ArtieK8sDefaultLabels.MANAGED_BY: ArtieK8sValues.MANAGED_BY_ARTIE_TOOLING,
        ArtieK8sDefaultLabels.INSTANCE: ArtieK8sValues.INSTANCE_INFRA,
        ArtieK8sDefaultLabels.PART_OF: ArtieK8sValues.PART_OF,
    }
    return labels

def generate_node_taints(node_name: str) -> dict[str, str]:
    """
    Generate the standard taints for an Artie node. Determines the appropriate taints
    from the node name.
    """
    # Assign node taints - all physical bot nodes get the physical bot taint
    node_taints = {
        ArtieK8sKeys.PHYSICAL_BOT_NODE_TAINT: ("true", TaintEffects.NO_SCHEDULE),
    }

    # Controller node gets an additional taint
    if node_name == ArtieK8sValues.CONTROLLER_NODE_ID:
        node_taints[ArtieK8sKeys.CONTROLLER_NODE_TAINT] = ("true", TaintEffects.NO_SCHEDULE)

    return node_taints
