#!/bin/bash

DEVICE="/dev/mmcblk0"

#COLORS
RST="\e[0m"
Y="\e[33m"
G="\e[32m"
R="\e[31m"

# hypriotOS version
VERSION="1.8.0"
IMAGE="https://github.com/hypriot/image-builder-rpi/releases/download/v${VERSION}/hypriotos-rpi-v${VERSION}.img.zip"

install_flash() {
  curl -O https://raw.githubusercontent.com/hypriot/flash/master/flash
  chmod +x flash
  mv flash /tmp/flash
  export PATH="/tmp:$PATH"
}

command -v flash >/dev/null 2>&1 || install_flash

echo -e "${Y}Insert SD card and provide hostname${RST}"
read -r HOSTNAME

flash --bootconf config.txt --userdata config.yml --device "${DEVICE}" --hostname "${HOSTNAME}" "${IMAGE}"

sync
echo -e "${G}Flashing completed.${RST}"
echo -e "${Y}Resizing root partition...${RST}"
sudo parted --script -a optimal "${DEVICE}" resizepart 2 2560MiB || exit 1
sudo e2fsck -f /dev/mmcblk0p2
sudo resize2fs "${DEVICE}"p2 || exit 1
echo -e "${Y}Creating docker partition...${RST}"
sudo parted --script -a optimal "${DEVICE}" mkpart primary 2561MiB 100% || exit 1
sudo mkfs.ext4 "${DEVICE}"p3 || exit 1
sudo e2label "${DEVICE}"p3 docker || exit 1

echo -e "${G}Finished.${RST}"
sync
