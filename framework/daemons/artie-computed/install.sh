# !/bin/bash
set -eu

Help()
{
   echo "Artie Computed Installation"
   echo
   echo "Syntax: install.sh [-h] [--help|--host-ip|--token|--no-docker]"
   echo "Arguments:"
   echo "--host-ip            Required. The IP address of the Artie Admind machine."
   echo "--token              Required. The token that you were given after installing the Artie Admin daemon. Can also be found on that machine at /var/lib/rancher/k3s/server/node-token"
   echo
   echo "Options:"
   echo "--no-docker          Do not install Docker. This is useful if you already have a Docker configuration set up."
   echo "--use-nfs            Use NFS for K3S persistent volumes. If you already have NFS installed, it will use the existing configuration, adding a new folder specified by --volume-path."
   echo "--volume-path        Specify a custom path for K3S persistent volumes. Only needed if using NFS or a custom CSI driver that requires this. (Default is /artie/nfs/k3s-storage)."
   echo "--remote-volume-path Specify a custom remote path for K3S persistent volumes when using NFS. Only needed if using NFS or a custom CSI driver that requires this. (Default is /artie/nfs/k3s-storage)."
   echo "--no-kerberos        Do not configure Kerberos for NFS. Only use this if your environment does not require Kerberos (you do not need encrypted traffic) or if you have already configured Kerberos manually."
   echo "-h | --help          Print this Help."
   echo
}

NO_DOCKER=0
URL=""
TOKEN=""

optspec=":h-:"
while getopts "$optspec" optchar; do
   case "${optchar}" in
      -)
         case "${OPTARG}" in
            host-ip)
               val="${!OPTIND}"; OPTIND=$(( $OPTIND + 1 ))
               URL="https://artie-admin:6443"
               echo "artie-admin    $val" | sudo tee -a /etc/hosts
               ;;
            host-ip=*)
               val=${OPTARG#*=}
               URL="https://artie-admin:6443"
               echo "artie-admin    $val" | sudo tee -a /etc/hosts
               ;;
            token)
               val="${!OPTIND}"; OPTIND=$(( $OPTIND + 1 ))
               TOKEN="$val"
               ;;
            token=*)
               val=${OPTARG#*=}
               TOKEN="$val"
               ;;
            no-docker)
               NO_DOCKER=1
               ;;
            use-nfs)
               USE_NFS=1
               ;;
            volume-path)
               val="${!OPTIND}"; OPTIND=$(( $OPTIND + 1 ))
               sudo mkdir -p "$val"
               VOLUME_PATH="$val"
               ;;
            remote-volume-path)
               val="${!OPTIND}"; OPTIND=$(( $OPTIND + 1 ))
               REMOTE_VOLUME_PATH="$val"
               ;;
            no-kerberos)
               NO_KERBEROS=1
               ;;
            help)
               Help
               exit 2
               ;;
            *)
               if [ "$OPTERR" = 1 ] && [ "${optspec:0:1}" != ":" ]; then
                  echo "Unknown option --${OPTARG}" >&2
               fi
               ;;
         esac;;
         h)
            Help
            exit 2
            ;;
         *)
            if [ "$OPTERR" != 1 ] || [ "${optspec:0:1}" = ":" ]; then
               echo "Non-option argument: '-${OPTARG}'" >&2
            fi
            ;;
   esac
done

if [[ -z "$TOKEN" ]]; then
   echo "Missing --token argument."
   exit 1
fi

if [[ -z "$URL" ]]; then
   echo "Missing --host-ip argument"
   exit 1
fi

# Set default volume path if not set
if [[ -z "${VOLUME_PATH:-}" ]]; then
   VOLUME_PATH="/artie/nfs/k3s-storage"
fi

# Set the default remote volume path if not set
if [[ -z "${REMOTE_VOLUME_PATH:-}" ]]; then
   REMOTE_VOLUME_PATH="/artie/nfs/k3s-storage"
fi

# Update the package list
sudo apt-get update -y

# If using NFS, set it up (or ensure it is already set up)
if [[ "$USE_NFS" == 1 ]]; then
   echo "Configuring NFS for K3S persistent volumes..."

   sudo apt-get install -y nfs-common
   sudo mkdir -p "$VOLUME_PATH"
   echo "artie-admin:$REMOTE_VOLUME_PATH $VOLUME_PATH nfs rsize=8192,wsize=8192,timeo=14,intr" | sudo tee -a /etc/fstab
   sudo mount -a

   # Enable Kerberos to be compliant with Artie's security model (no unencrypted traffic over the network)
   if [[ "$NO_KERBEROS" != 1 ]]; then
      # TODO
      echo "Configuring Kerberos for NFS..."
      echo "This is not implemented yet. Please configure Kerberos manually."
      exit 1
   fi
fi

# Install docker
if [[ "$NO_DOCKER" != 1 ]]; then
   curl -fsSL https://get.docker.com -o get-docker.sh
   sudo sh get-docker.sh
fi

# Install K3S (version is limited by Yocto compatibility. See: https://layers.openembedded.org/layerindex/branch/master/layer/meta-virtualization/)
# By passing K3S_URL, we set this node to be a K3S agent, as opposed to a server.
# INSTALL_K3S_NAME tells the installer what to call the systemd service file.
curl -sfL https://get.k3s.io | INSTALL_K3S_VERSION="v1.32.0+k3s1" K3S_URL=$URL INSTALL_K3S_NAME="k3s-agent" K3S_TOKEN=$TOKEN INSTALL_K3S_EXEC="--write-kubeconfig-mode=644" sh -s - --docker

# Pin Docker to the current version to avoid unwanted automatic upgrades
sudo apt-mark hold docker-ce docker-ce-cli containerd.io docker-compose-plugin docker-ce-rootless-extras docker-buildx-plugin docker-model-plugin

# Open the firewall for the K3S API server
sudo ufw allow 6443/tcp

# Update K3S to require Artie Computed
sudo sed -i '/After=network-online.target/a PartOf=artie-computed.service' /etc/systemd/system/k3s-agent.service

# Install the daemon
sudo cp ./artie-computed.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl restart k3s-agent.service
sudo systemctl start artie-computed.service
sudo systemctl enable artie-computed.service

# Label the node appropriately
kubectl label node "$(hostname)" artie/node-role=compute --kubeconfig /etc/rancher/k3s/k3s.yaml
if [[ "$USE_NFS" == 1 ]]; then
   kubectl label node "$(hostname)" artie/storage=nfs --kubeconfig /etc/rancher/k3s/k3s.yaml
fi
