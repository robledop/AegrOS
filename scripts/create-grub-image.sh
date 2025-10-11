#!/bin/bash

#set -e
#rm -rf ./disk.img
#dd if=/dev/zero of=./disk.img bs=512 count=65536
#mkfs.vfat -c -F 16 ./disk.img
#sudo mount -t vfat ./disk.img /mnt/d
#ld=$(losetup -j ./disk.img | grep -oP '/dev/loop[0-9]+') # Find a free loop device
#sudo grub-install --root-directory=/mnt/d --force --no-floppy --modules="normal part_msdos multiboot" "$ld"
##./scripts/generate-files.sh
#sudo cp -r ./rootfs/. /mnt/d/
#sudo umount -q /mnt/d

#!/bin/bash
set -e

rm -rf ./disk.img

dd if=/dev/zero of=./disk.img bs=512 count=65536

# Create MBR partition table and a single bootable partition
parted -s ./disk.img mklabel msdos
parted -s ./disk.img mkpart primary fat16 1MiB 100%
parted -s ./disk.img set 1 boot on

# Set up loop device with partition scanning
ld=$(sudo losetup -fP --show ./disk.img)

sudo mkfs.vfat -F 16 "${ld}p1"
sudo mount -t vfat "${ld}p1" /mnt/d

sudo grub-install --root-directory=/mnt/d --force --no-floppy --modules="normal part_msdos multiboot" "$ld"

sudo cp -r ./rootfs/. /mnt/d/

sudo umount /mnt/d
sudo losetup -d "$ld"