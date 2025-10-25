#!/bin/bash

set -euo pipefail

readonly IMG_PATH="./disk.img"
readonly MOUNT_POINT="${MOUNT_POINT:-/mnt/aegr}"

sudo mkdir -p "$MOUNT_POINT"

loopdev=""
cleanup() {
    set +e
    if mountpoint -q "$MOUNT_POINT"; then
        sudo umount "$MOUNT_POINT" >/dev/null 2>&1 || true;
    fi
    if [[ -n "${loopdev}" ]]; then
        sudo losetup -d "$loopdev" >/dev/null 2>&1 || true;
    fi
    set -e
}
trap cleanup EXIT

rm -f "${IMG_PATH}"
dd if=/dev/zero of="${IMG_PATH}" bs=512 count=131072 status=none

# Create MBR partition table and a single bootable partition
parted -s "${IMG_PATH}" mklabel msdos
parted -s "${IMG_PATH}" mkpart primary ext2 1MiB 100%
parted -s "${IMG_PATH}" set 1 boot on

# Set up loop device with partition scanning
loopdev=$(sudo losetup -fP --show "${IMG_PATH}")

sudo mkfs.ext2 "${loopdev}p1" -L AegrOS -b 1024
sudo mount -t ext2 "${loopdev}p1" "$MOUNT_POINT"

sudo grub-install --target=i386-pc --boot-directory="$MOUNT_POINT/boot" --modules="normal part_msdos ext2 multiboot" "$loopdev"

sudo cp -r ./rootfs/. "$MOUNT_POINT/"
sync

sudo umount "$MOUNT_POINT"

# Validate the filesystem before committing the image.
sudo e2fsck -f -n "${loopdev}p1"

sudo losetup -d "$loopdev"
loopdev=""
trap - EXIT
