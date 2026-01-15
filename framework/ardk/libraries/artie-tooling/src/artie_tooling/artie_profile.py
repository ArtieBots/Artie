"""
This module contains the code for keeping track of an Artie Profile.
"""
from artie_tooling import artie_secrets
from artie_tooling import hw_config
import dataclasses
import pathlib
import json

# Default save path is in the user's home directory under .artie/workbench/profiles
DEFAULT_SAVE_PATH = pathlib.Path.home() / ".artie" / "workbench" / "profiles"

@dataclasses.dataclass
class APIServerInfo:
    """
    Information about the Artie API server.
    """
    host: str
    """The hostname or IP address of the API server."""

    port: int = 8782
    """The port for the API server."""

    cert_path: str = str(pathlib.Path.home() / ".artie" / "controller-node-CA" / "controller-node-ca.crt")
    """The controller node's root certificate. This is the CA bundle we use for authenticating the API server is who it says it is."""

    bearer_token: str = None
    """An optional bearer token for the API server."""

    def to_json_str(self) -> dict:
        """Convert to JSON-serializable dict. Excludes bearer_token for security."""
        d = dataclasses.asdict(self)
        d.pop("bearer_token", None)
        return d

@dataclasses.dataclass
class K3SInfo:
    """
    Information about the K3S installation on the Artie.
    """
    admin_node_ip: str = None
    """The IP address of the admin node."""

    token: str = None
    """The K3S token for the Artie."""

    def to_json_str(self) -> dict:
        """Convert to JSON-serializable dict. Excludes token for security."""
        d = dataclasses.asdict(self)
        d.pop("token", None)
        return d

@dataclasses.dataclass
class Credentials:
    """
    Credentials for accessing the Artie.
    """
    username: str = None
    """The username for the Artie."""

    password: str = None
    """The password for the Artie."""

    def to_json_str(self) -> dict:
        """Convert to JSON-serializable dict. Excludes password for security."""
        d = dataclasses.asdict(self)
        d.pop("password", None)
        return d

@dataclasses.dataclass
class ArtieProfile:
    """
    An ArtieProfile instance contains all the information pertaining
    to a particular Artie that we might need in order to access or install it.
    """
    artie_name: str = None
    """The name of this Artie."""

    controller_node_ip: str = None
    """The IP address of Artie's controller node."""

    hardware_config: hw_config.HWConfig = None
    """The hardware configuration for this Artie."""

    credentials: Credentials = dataclasses.field(default_factory=Credentials)
    """Credentials for accessing the Artie."""

    k3s_info: K3SInfo = dataclasses.field(default_factory=K3SInfo)
    """Information about the K3S installation on the Artie."""

    api_server_info: APIServerInfo = dataclasses.field(default_factory=lambda: APIServerInfo(host=""))
    """Information about the Artie API server."""

    @staticmethod
    def load(artie_name=None, path=None) -> 'ArtieProfile':
        """
        Load an Artie profile from disk.

        If both `artie_name` and `path` are given, we assume `path` is a directory
        and try to load a file named `'<artie_name>.json'` from `path` directory.

        If `artie_name` is given but `path` is not, we assume `path` is the default directory
        and attempt to load a file named `'<artie_name>.json'` from that directory.

        If `artie_name` is not given but `path` is, `path` should be a path to the *file*.

        If neither `artie_name` nor `path` are given, we attempt to load `'unnamed_artie.json'`
        from the default directory. This will likely fail, resulting in a FileNotFound exception.
        """
        if artie_name is not None and path is not None:
            path = pathlib.Path(path) / f"{artie_name}.json"
        elif artie_name is not None and path is None:
            path = DEFAULT_SAVE_PATH / f"{artie_name}.json"
        elif artie_name is None and path is not None:
            path = pathlib.Path(path)
        else:
            path = DEFAULT_SAVE_PATH / "unnamed_artie.json"

        with open(path, 'r') as f:
            data = json.load(f)

        profile = ArtieProfile(
            artie_name=data.get("artie_name"),
            controller_node_ip=data.get("controller_node_ip"),
            hardware_config=hw_config.HWConfig.from_json_str(data.get("hardware_config")) if data.get("hardware_config") else None,
            credentials=Credentials(**data.get("credentials")) if data.get("credentials") else Credentials(),
            k3s_info=K3SInfo(**data.get("k3s_info")) if data.get("k3s_info") else K3SInfo(),
            api_server_info=APIServerInfo(**data.get("api_server_info")) if data.get("api_server_info") else APIServerInfo(host=""),
        )

        # Load the secrets
        profile.credentials.password = artie_secrets.retrieve_secret(f"artie_{profile.artie_name}_password")
        profile.k3s_info.token = artie_secrets.retrieve_secret(f"artie_{profile.artie_name}_token")
        profile.api_server_info.bearer_token = artie_secrets.retrieve_secret(f"artie_{profile.artie_name}_bearer_token")
        profile.api_server_info.cert_path = artie_secrets.retrieve_api_server_cert(f"artie_{profile.artie_name}_api_server_cert")

        return profile

    def save(self, path=None):
        """
        Save the Artie profile to disk. If `path` is not given, we save to the default location.

        `path` should be a directory; the filename will be derived from the Artie name.
        """
        if self.artie_name is not None and path is not None:
            path = pathlib.Path(path) / f"{self.artie_name}.json"
        elif self.artie_name is not None and path is None:
            path = DEFAULT_SAVE_PATH / f"{self.artie_name}.json"
        elif self.artie_name is None and path is not None:
            path = pathlib.Path(path) / "unnamed_artie.json"
        else:
            path = DEFAULT_SAVE_PATH / "unnamed_artie.json"

        # Ensure the parent directory exists
        path.parent.mkdir(parents=True, exist_ok=True)

        # Serialize to JSON and write to file, but not including the password and token for security reasons
        hw_config_data = self.hardware_config.to_json_str() if self.hardware_config else None
        credentials_data = self.credentials.to_json_str() if self.credentials else None
        k3s_info_data = self.k3s_info.to_json_str() if self.k3s_info else None
        api_server_info_data = self.api_server_info.to_json_str() if self.api_server_info else None

        data = dataclasses.asdict(self)

        data.pop("hardware_config", None)
        data.pop("credentials", None)
        data.pop("k3s_info", None)
        data.pop("api_server_info", None)

        data["hardware_config"] = hw_config_data
        data["credentials"] = credentials_data
        data["k3s_info"] = k3s_info_data
        data["api_server_info"] = api_server_info_data

        with open(path, 'w') as f:
            json.dump(data, f, indent=4)

        # Save the password and token in an OS-dependent manner
        artie_secrets.store_secret(f"artie_{self.artie_name}_password", self.credentials.password if self.credentials else None)
        artie_secrets.store_secret(f"artie_{self.artie_name}_token", self.k3s_info.token if self.k3s_info else None)
        artie_secrets.store_secret(f"artie_{self.artie_name}_api_server_bearer_token", self.api_server_info.bearer_token if self.api_server_info else None)
        artie_secrets.store_api_server_cert(self.api_server_info.cert_path if self.api_server_info else None)

    def delete(self, path=None):
        """
        Delete the Artie profile from disk and remove associated secrets.

        `path` should be a directory; the filename will be derived from the Artie name.
        """
        if path is None:
            name = self.artie_name or "unnamed_artie"
            path = DEFAULT_SAVE_PATH / f"{name}.json"
        else:
            path = pathlib.Path(path) / f"{self.artie_name}.json"

        path = pathlib.Path(path)

        # Delete the JSON file
        if path.exists():
            path.unlink()

        # Delete the associated secrets
        artie_secrets.delete_secret(f"artie_{self.artie_name}_password")
        artie_secrets.delete_secret(f"artie_{self.artie_name}_token")
        artie_secrets.delete_secret(f"artie_{self.artie_name}_api_server_bearer_token")
        artie_secrets.delete_secret(self.api_server_info.cert_path)

def list_profiles(path=None) -> list[ArtieProfile]:
    """
    List all saved Artie profiles in the given path.
    If `path` is not given, we look in the default location.

    `path` should be a directory.
    """
    if path is None:
        path = DEFAULT_SAVE_PATH
    else:
        path = pathlib.Path(path)

    profiles = []
    if path.exists() and path.is_dir():
        for file in path.glob("*.json"):
            profiles.append(ArtieProfile.load(file.stem, path=path))

    return profiles
