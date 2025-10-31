PATH := $(HOME)/opt/cross/bin:$(PATH)
export PATH

K=kernel

$(shell mkdir -p build)
$(shell mkdir -p user/build)
$(shell mkdir -p rootfs/bin)
$(shell mkdir -p rootfs/boot/grub)
$(shell mkdir -p rootfs/dev)
$(shell mkdir -p rootfs/etc)
$(shell touch rootfs/etc/devtab)

# Create the grub.cfg file if it doesn't exist.
ifeq ("$(wildcard rootfs/boot/grub/grub.cfg)","")
    $(shell echo 'set timeout=0' > rootfs/boot/grub/grub.cfg && \
            echo '' >> rootfs/boot/grub/grub.cfg && \
            echo 'menuentry "AegrOS" {' >> rootfs/boot/grub/grub.cfg && \
            echo '	multiboot /boot/kernel' >> rootfs/boot/grub/grub.cfg && \
            echo '}' >> rootfs/boot/grub/grub.cfg)
endif

SRC_DIRS := $(shell find ./kernel -type d)
BUILD_DIRS := $(patsubst ./kernel/%,./build/%,$(SRC_DIRS))
$(shell mkdir -p $(BUILD_DIRS))
ASM_FILES := $(filter-out ./kernel/x86/entryother.asm ./kernel/initcode.asm,$(wildcard $(addsuffix /*.asm, $(SRC_DIRS))))
C_FILES := $(wildcard $(addsuffix /*.c, $(SRC_DIRS)))
ASM_OBJS := $(ASM_FILES:./kernel/%.asm=./build/%.o)
C_OBJS := $(C_FILES:./kernel/%.c=./build/%.o)
OBJS := $(ASM_OBJS) $(C_OBJS)
TOOLPREFIX = i686-elf-
QEMU = qemu-system-i386
CC = $(TOOLPREFIX)gcc
AS = nasm
LD = $(TOOLPREFIX)ld
INCLUDE = -I./include
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump
CFLAGS = -nostdlib -ffreestanding -fno-pic -static -fno-builtin -fno-strict-aliasing -O0 -Wall -MD -ggdb -m32 -fno-omit-frame-pointer -std=gnu23
CFLAGS += $(INCLUDE)
ASFLAGS += $(INCLUDE)
LDFLAGS += -m elf_i386
CFLAGS += -fno-pie -no-pie
CPUS := 8
MEMORY := 512
QEMUEXTRA := -display gtk,zoom-to-fit=on,gl=off,window-close=on,grab-on-hover=off
QEMUGDB = -S -gdb tcp::1234 -d int -D qemu.log
QEMUOPTS = -drive file=disk.img,index=0,media=disk,format=raw -smp $(CPUS) -m $(MEMORY)

build/kernel: CFLAGS += -fsanitize=undefined -fstack-protector

asm_headers: FORCE
	./scripts/c_to_nasm.sh ./include syscall.asm traps.asm memlayout.asm mmu.asm asm.asm param.asm

build/x86/entryother: asm_headers FORCE
	$(AS) $(ASFLAGS) -f elf kernel/x86/entryother.asm -o build/x86/entryother.o
	$(LD) $(LDFLAGS) -Ttext 0x7000 -o build/x86/bootblockother.o build/x86/entryother.o
	$(OBJCOPY) -S -O binary -j .text build/x86/bootblockother.o build/x86/entryother

build/initcode: asm_headers FORCE
	$(AS) $(ASFLAGS) -f elf kernel/initcode.asm -o build/initcode.o
	$(LD) $(LDFLAGS) -Ttext 0 -o build/initcode.out build/initcode.o
	$(OBJCOPY) -S -O binary build/initcode.out build/initcode

build/kernel: $(OBJS) build/initcode build/x86/entryother asm_headers FORCE
	$(LD) $(LDFLAGS) -T $K/kernel.ld -o build/kernel $(OBJS) -b binary build/initcode build/x86/entryother

build/%.o: $K/%.c FORCE
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: $K/%.asm asm_headers FORCE
	$(AS) $(ASFLAGS) -f elf $< -o $@

.PHONY: apps
apps: asm_headers FORCE
	cd ./user && $(MAKE) all

grub: build/kernel apps FORCE
	cp build/kernel ./rootfs/boot/kernel
	grub-file --is-x86-multiboot ./rootfs/boot/kernel
	./scripts/create-grub-image.sh

qemu: grub FORCE
	./scripts/create_tap.sh
	$(QEMU) -serial mon:stdio $(QEMUOPTS) $(QEMUEXTRA)

qemu-nox: grub
	$(QEMU) -nographic $(QEMUOPTS)

qemu-gdb: grub
	@echo "*** Now run 'gdb'." 1>&2
	$(QEMU) -daemonize $(QEMUOPTS) $(QEMUGDB) $(QEMUEXTRA)

qemu-nox-gdb: grub
	@echo "*** Now run 'gdb'." 1>&2
	$(QEMU) -nographic $(QEMUOPTS) $(QEMUGDB)

vbox: grub FORCE
	./scripts/create_tap.sh
	./scripts/start_vbox.sh $(MEMORY)

qemu-nobuild:
	$(QEMU) -serial mon:stdio $(QEMUOPTS) $(QEMUEXTRA)

.PHONY: clean
clean:
	@echo "Cleaning up..."
	rm -rf rootfs/
	rm -rf build/
	rm -rf user/build

# Force rebuild of all files
.PHONY: FORCE
FORCE:

build/x86/entryother.o:
	@echo "ERROR: entryother.o should not be built as an object file. It must be excluded from OBJS."
	@exit 1

build/initcode.o:
	@echo "ERROR: initcode.o should not be built as an object file. It must be excluded from OBJS."
	@exit 1
