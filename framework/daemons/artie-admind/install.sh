# !/bin/bash
set -eu

Help()
{
   echo "Artie Admind Installation"
   echo
   echo "Syntax: install.sh [-h] [--help|--no-docker]"
   echo
   echo "Options:"
   echo "--no-docker     Do not install Docker. This is useful if you already have a Docker configuration set up."
   echo "--volume-path   Specify a custom path for K3S persistent volumes. (Default is /var/lib/rancher/k3s/storage when not using NFS and /artie/nfs/k3s-storage when using NFS)."
   echo "--use-nfs       Use NFS for K3S persistent volumes. If you already have NFS installed, it will use the existing configuration, adding a new folder specified by --volume-path."
   echo "--csi-path      Use a custom CSI driver for K3S persistent volumes. This argument is a path to an appropriate Kubernetes Persistent Volume YAML file for that CSI driver."
   echo "--no-kerberos   Do not configure Kerberos for NFS. Only use this if your environment does not require Kerberos (you do not need encrypted traffic) or if you have already configured Kerberos manually."
   echo "--storage-size  Specify the size of the persistent volume to create (Default is 10Gi; the more you can give it, the better)."
   echo "-h | --help     Print this Help."
   echo
}

NO_DOCKER=0

optspec=":h-:"
while getopts "$optspec" optchar; do
   case "${optchar}" in
      -)
         case "${OPTARG}" in
            no-docker)
               NO_DOCKER=1
               ;;
            volume-path)
               val="${!OPTIND}"; OPTIND=$(( $OPTIND + 1 ))
               sudo mkdir -p "$val"
               sudo sed -i "s|/var/lib/rancher/k3s/storage|$val|g" ./k3s-persistent-volume-local.yaml
               ;;
            use-nfs)
               USE_NFS=1
               ;;
            csi-path)
               USE_CSI=1
               CSI_PATH="${!OPTIND}"; OPTIND=$(( $OPTIND + 1 ))
               # Check if the file exists
               if [ ! -f "$CSI_PATH" ]; then
                  echo "The specified CSI path '$CSI_PATH' does not exist. Please provide a valid file path."
                  exit 1
               fi
               ;;
            no-kerberos)
               NO_KERBEROS=1
               # Remove the '- sec=krb5p' mount option from the NFS persistent volume YAML
               sudo sed -i '/- sec=krb5p/d' ./k3s-persistent-volume-nfs.yaml
               ;;
            storage-size)
               val="${!OPTIND}"; OPTIND=$(( $OPTIND + 1 ))
               sudo sed -i "s/storage: .*/storage: $val/g" ./k3s-persistent-volume-local.yaml
               sudo sed -i "s/storage: .*/storage: $val/g" ./k3s-persistent-volume-nfs.yaml
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

# First check that the arguments make sense. Can't use both NFS and CSI
if [[ "$USE_NFS" == 1 && "$USE_CSI" == 1 ]]; then
   echo "Cannot use both NFS and CSI for K3S persistent volumes. Please choose one or the other."
   exit 1
fi

# Set defaults if not set
if [[ -z "${USE_NFS+x}" ]]; then
   USE_NFS=0
fi

if [[ -z "${USE_CSI+x}" ]]; then
   USE_CSI=0
fi

# Set default volume path if not set
if [[ -z "${VOLUME_PATH:-}" ]]; then
   if [[ "$USE_NFS" == 1 ]]; then
      VOLUME_PATH="/artie/nfs/k3s-storage"
   else
      VOLUME_PATH="/var/lib/rancher/k3s/storage"
   fi
fi

# Update the package list
sudo apt-get update -y

# If using NFS, set it up (or ensure it is already set up)
if [[ "$USE_NFS" == 1 ]]; then
   echo "Configuring NFS for K3S persistent volumes..."

   # First, check if this system is already an NFS client. If so, fail out. The user will need
   # to add the /artie/nfs/k3s-storage export to their existing configuration manually.
   if grep -q "nfs" /proc/mounts; then
      echo "This system is already configured as an NFS client. Please add /artie/nfs/k3s-storage to your existing NFS server manually or do not use the --use-nfs option and re-run this script."
      exit 1
   fi

   # Now make sure that we have nfs installed and configured
   sudo apt-get install -y nfs-kernel-server

   # Check for a configuration file at /etc/exports
   if ! grep -q "$VOLUME_PATH" /etc/exports; then
      sudo mkdir -p "$VOLUME_PATH"
      sudo chown nobody:nogroup "$VOLUME_PATH"
      echo "$VOLUME_PATH *(rw,sync,no_subtree_check,no_root_squash)" | sudo tee -a /etc/exports
      sudo exportfs -a
   fi

   # Enable Kerberos to be compliant with Artie's security model (no unencrypted traffic over the network)
   if [[ "$NO_KERBEROS" != 1 ]]; then
      # TODO
      echo "Configuring Kerberos for NFS..."
      echo "This is not implemented yet. Please configure Kerberos manually."
      exit 1
   fi

   # Update k3s-persistent-volume-nfs.yaml with the correct server IP and path
   SERVER_IP=$(hostname -I | awk '{print $1}')
   sudo sed -i "s/server: .*/server: $SERVER_IP/" ./k3s-persistent-volume-nfs.yaml
   sudo sed -i "s|path: .*|path: $VOLUME_PATH|" ./k3s-persistent-volume-nfs.yaml

   # Start the NFS server
   sudo systemctl enable nfs-kernel-server.service
   sudo systemctl start nfs-kernel-server.service
fi

# Install docker
if [[ "$NO_DOCKER" != 1 ]]; then
   echo "Installing Docker..."
   curl -fsSL https://get.docker.com -o get-docker.sh
   sudo sh get-docker.sh
fi

# Install K3S (version is limited by Yocto compatibility. See: https://layers.openembedded.org/layerindex/branch/master/layer/meta-virtualization/)
echo "Installing K3S..."
curl -sfL https://get.k3s.io | INSTALL_K3S_VERSION="v1.32.0+k3s1" INSTALL_K3S_EXEC="--write-kubeconfig-mode=644" sh -s - --docker

# Pin Docker to the current version to avoid unwanted automatic upgrades
sudo apt-mark hold docker-ce docker-ce-cli containerd.io docker-compose-plugin docker-ce-rootless-extras docker-buildx-plugin docker-model-plugin

# Open the firewall for the K3S API server
sudo ufw allow 6443/tcp

# Update K3S to require Artie Admind
sudo sed -i '/After=network-online.target/a PartOf=artie-admind.service' /etc/systemd/system/k3s.service

# Update the K3S configuration files for security best practices
sudo cp ./config.yaml /etc/rancher/k3s/
sudo cp ./psa.yaml /var/lib/rancher/k3s/server/
sudo cp ./audit.yaml /var/lib/rancher/k3s/server/
sudo cp ./k3s-network-policy.yaml /var/lib/rancher/k3s/server/manifests/
sudo mkdir -p -m 700 /var/lib/rancher/k3s/server/logs

if [[ "$USE_NFS" == 1 ]]; then
   echo "Using NFS for K3S persistent volumes..."
   sudo cp ./k3s-persistent-volume-nfs.yaml /var/lib/rancher/k3s/server/manifests/
elif [[ "$USE_CSI" == 1 ]]; then
   echo "Using custom CSI driver for K3S persistent volumes..."
   mkdir -p /var/lib/rancher/k3s/server/manifests/
   sudo cp "$CSI_PATH" /var/lib/rancher/k3s/server/manifests/
else
   echo "Using local storage for K3S persistent volumes..."
   sudo cp ./k3s-persistent-volume-local.yaml /var/lib/rancher/k3s/server/manifests/
fi

# Update kernel parameters to ensure security compliance
sudo cp ./90-kubelet.conf /etc/sysctl.d/90-kubelet.conf
sudo sysctl -p /etc/sysctl.d/90-kubelet.conf

# Install the daemon
echo "Installing Artie Admind Service..."
sudo cp ./artie-admind.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl restart k3s.service
sudo systemctl start artie-admind.service
sudo systemctl enable artie-admind.service

# Update the labels on the K3S node to indicate that it is the admin node
kubectl label node $(hostname) artie/node-role=admin --overwrite

if [[ "$USE_NFS" == 1 ]]; then
   # Also label it as NFS storage
   kubectl label node $(hostname) artie/storage=nfs --overwrite
else
   # Label it as local storage
   kubectl label node $(hostname) artie/storage=local --overwrite
fi

# Print the Token for the user, as they will need it for the next few steps in installing Artie
TOKEN=$(sudo cat /var/lib/rancher/k3s/server/node-token)
MSG="The following token is important for the next few steps of Artie installation. If you lose this token, you can always find it at /var/lib/rancher/k3s/server/node-token on this machine."
echo $MSG
echo $TOKEN

if [[ "$USE_NFS" == 1 ]]; then
   echo "NFS has been configured for K3S persistent volumes. Ensure that any compute nodes you install can access the NFS server at $(hostname -I | awk '{print $1}'):$VOLUME_PATH"
   echo "This can be done by installing compute nodes by means of the appropriate install script with the --use-nfs option, or by manually configuring NFS on those nodes."
   echo "If Kerberos was configured and you need to reconfigure it for some reason, you can do so with 'sudo dpkg-reconfigure krb5-kdc'"
fi
