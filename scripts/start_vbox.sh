#!/usr/bin/env bash

MEMORY=${1:-512} # Default to 512 MB if not provided as an argument
VM_NAME="AegrOS"

if VBoxManage showvminfo "$VM_NAME" >/dev/null 2>&1; then
  if VBoxManage showvminfo "$VM_NAME" | grep -q "running"; then
    VBoxManage controlvm "$VM_NAME" poweroff;
    sleep 2;
  fi;
  VBoxManage unregistervm "$VM_NAME" --delete-all;
fi

rm -f disk.vdi || true
VBoxManage convertfromraw disk.img disk.vdi --format VDI
VBoxManage createvm --name "$VM_NAME" --register --basefolder .
VBoxManage storagectl "$VM_NAME" --name "IDE" --add ide --controller PIIX4
VBoxManage storageattach "$VM_NAME" --storagectl "IDE" --port 0 --device 0 --type hdd --medium disk.vdi
VBoxManage modifyvm "$VM_NAME" --memory "$MEMORY" --vram 16 --graphicscontroller vboxvga
VBoxManage modifyvm "$VM_NAME" --nic1 bridged --bridgeadapter1 tap0
VBoxManage modifyvm "$VM_NAME" --ioapic on --cpus 8 --chipset piix3
VBoxManage setextradata "$VM_NAME" GUI/ScaleFactor "1.75"
VBoxManage setextradata "$VM_NAME" GUI/DefaultCloseAction "poweroff"
VBoxManage startvm "$VM_NAME"