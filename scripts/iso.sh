#!/bin/sh
make grub
grub-mkrescue -o AegrOS.iso ./rootfs
