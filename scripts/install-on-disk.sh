#!/usr/bin/env bash
set -euo pipefail

DISK="${DISK:-/dev/sdb}"
MOUNT_POINT="${MOUNT_POINT:-/mnt/aegr}"

if [ ! -b "$DISK" ]; then
	echo "Error: $DISK is not a block device" >&2
	exit 1
fi

echo "Installing to $DISK (mount point $MOUNT_POINT)"

sudo umount "${DISK}"?* 2>/dev/null || true
sudo mkdir -p "$MOUNT_POINT"

sudo wipefs -a "$DISK"
sudo dd if=/dev/zero of="$DISK" bs=1M count=10 conv=fsync

sudo parted -s "$DISK" mklabel msdos
sudo parted -s "$DISK" mkpart primary fat16 1MiB 33MiB
sudo parted -s "$DISK" set 1 boot on
sudo partprobe "$DISK"

sudo mkfs.vfat -F 16 "${DISK}1"
sudo mount -t vfat "${DISK}1" "$MOUNT_POINT"

sudo grub-install \
	--target=i386-pc \
	--boot-directory="$MOUNT_POINT/boot" \
	--modules="normal part_msdos multiboot" \
	"$DISK"

sudo cp -r ./rootfs/. "$MOUNT_POINT/"

sync
sudo umount "$MOUNT_POINT"
