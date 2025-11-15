PATH := $(HOME)/opt/cross/bin:$(PATH)
export PATH

K=kernel

$(shell mkdir -p build)
$(shell mkdir -p user/build)
$(shell mkdir -p rootfs/bin)
$(shell mkdir -p rootfs/boot/grub)
$(shell mkdir -p rootfs/dev)
$(shell mkdir -p rootfs/etc)
$(shell mkdir -p rootfs/test)
$(shell touch rootfs/etc/devtab)

# Create a big text file inside the rootfs/test directory for testing purposes.
ifeq ("$(wildcard rootfs/test/bigfile.txt)","")
    $(shell base64 /dev/urandom | head -c 10485760 > rootfs/test/bigfile.txt)
endif

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
CFLAGS = -nostdlib -ffreestanding -fno-pic -static -fno-builtin -fno-strict-aliasing -Wall -MD -m32 -fno-omit-frame-pointer -std=gnu23
CFLAGS += $(INCLUDE)
ASFLAGS += $(INCLUDE)
LDFLAGS += -m elf_i386
CFLAGS += -fno-pie -no-pie
CPUS := 8
MEMORY := 512
QEMUEXTRA := -display gtk,zoom-to-fit=on,gl=off,window-close=on,grab-on-hover=off
QEMUGDB = -S -gdb tcp::1234 -d int -D qemu.log
QEMUOPTS = -drive file=disk.img,index=0,media=disk,format=raw -smp $(CPUS) -m $(MEMORY)
QEMU_NETWORK=-netdev tap,id=net0,ifname=tap0,script=no,downscript=no -device e1000,netdev=net0

qemu-nox-gdb qemu-nox qemu qemu-gdb: CFLAGS += -fsanitize=undefined -fstack-protector -ggdb -O0 -DDEBUG -DGRAPHICS
qemu-nox-gdb qemu-nox qemu qemu-gdb: ASFLAGS += -DDEBUG -DGRAPHICS
vbox qemu-nox-perf qemu-perf qemu-perf-no-net: CFLAGS += -O3 -DGRAPHICS -DDEBUG
vbox qemu-nox-perf qemu-perf qemu-perf-no-net: ASFLAGS += -DGRAPHICS
vbox-textmode qemu-nox-perf-textmode qemu-perf-textmode qemu-perf-no-net-textmode: CFLAGS += -O3 -DDEBUG

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

.PHONY: doom
doom:
	$(MAKE) -C user/doom clean
	$(MAKE) -C user/doom

grub: build/kernel apps doom FORCE
	cp build/kernel ./rootfs/boot/kernel
	cp assets/wpaper.bmp ./rootfs/wpaper.bmp
	cp assets/doom.wad ./rootfs/bin/doom.wad
	cp assets/doom.wad ./rootfs/doom.wad
	cp assets/fbdoom ./rootfs/bin/doom
	grub-file --is-x86-multiboot ./rootfs/boot/kernel
	./scripts/create-grub-image.sh
	./scripts/create_tap.sh

qemu: grub FORCE
	$(QEMU) -serial mon:stdio $(QEMUOPTS) $(QEMUEXTRA) $(QEMU_NETWORK)

qemu-nox: grub FORCE
	$(QEMU) -nographic $(QEMUOPTS) $(QEMU_NETWORK)

qemu-gdb: grub FORCE
	@echo "*** Now run 'gdb'." 1>&2
	$(QEMU) -daemonize $(QEMUOPTS) $(QEMUGDB) $(QEMUEXTRA) $(QEMU_NETWORK)

qemu-nox-gdb: grub FORCE
	@echo "*** Now run 'gdb'." 1>&2
	$(QEMU) -nographic $(QEMUOPTS) $(QEMUGDB) $(QEMU_NETWORK)

vbox: grub FORCE
	./scripts/start_vbox.sh $(MEMORY)

vbox-textmode: grub FORCE
	./scripts/start_vbox.sh $(MEMORY)

bochs: grub FORCE
	SKIP_GRUB=1 ./scripts/start_bochs.sh $(MEMORY)

qemu-nobuild:
	$(QEMU) -serial mon:stdio $(QEMUOPTS) $(QEMUEXTRA) $(QEMU_NETWORK)

qemu-perf: grub FORCE
	$(QEMU) -serial mon:stdio $(QEMUOPTS) $(QEMUEXTRA) $(QEMU_NETWORK) -accel kvm -cpu host

qemu-perf-textmode: grub FORCE
	$(QEMU) -serial mon:stdio $(QEMUOPTS) $(QEMUEXTRA) $(QEMU_NETWORK) -accel kvm -cpu host

qemu-perf-net-default: grub FORCE
	$(QEMU) -serial mon:stdio $(QEMUOPTS) $(QEMUEXTRA) -accel kvm -cpu host

qemu-perf-no-net: grub FORCE
	$(QEMU) -serial mon:stdio $(QEMUOPTS) $(QEMUEXTRA) -accel kvm -cpu host -nic none

qemu-perf-no-net-textmode: grub FORCE
	$(QEMU) -serial mon:stdio $(QEMUOPTS) $(QEMUEXTRA) -accel kvm -cpu host -nic none

qemu-nox-perf: grub FORCE
	$(QEMU) -nographic $(QEMUOPTS) $(QEMU_NETWORK) -accel kvm -cpu host

qemu-nox-perf-textmode: grub FORCE
	$(QEMU) -nographic $(QEMUOPTS) $(QEMU_NETWORK) -accel kvm -cpu host

.PHONY: clean
clean:
	@echo "Cleaning up..."
	rm -rf rootfs/
	rm -rf build/
	rm -rf user/build
	rm assets/fbdoom

# Force rebuild of all files
.PHONY: FORCE
FORCE:

build/x86/entryother.o:
	@echo "ERROR: entryother.o should not be built as an object file. It must be excluded from OBJS."
	@exit 1

build/initcode.o:
	@echo "ERROR: initcode.o should not be built as an object file. It must be excluded from OBJS."
	@exit 1
