PATH := $(HOME)/opt/cross/bin:$(PATH)
export PATH

$(shell mkdir -p ./bin)
$(shell mkdir -p ./rootfs/bin)
QEMU=qemu-system-i386
MEMORY=1024 # Be careful not to allocate too much memory as the page table may overlap with the heap
QEMU_DISPLAY=-display gtk,zoom-to-fit=on,gl=off,window-close=on,grab-on-hover=off -device VGA,vgamem_mb=16
QEMU_NETWORK=-netdev tap,id=net0,ifname=tap0,script=no,downscript=no -device e1000,netdev=net0
CC=i686-elf-gcc
AS=nasm
LD=i686-elf-ld
OBJCOPY=$(HOME)/opt/cross/bin/i686-elf-objcopy
SRC_DIRS := $(shell find ./kernel)
BUILD_DIRS := $(patsubst ./kernel/%,./build/%,$(SRC_DIRS))
$(shell mkdir -p $(BUILD_DIRS))
ASM_FILES := $(wildcard $(addsuffix /*.asm, $(SRC_DIRS)))
C_FILES := $(wildcard $(addsuffix /*.c, $(SRC_DIRS)))
ASM_OBJS := $(ASM_FILES:./kernel/%.asm=./build/%.asm.o)
C_OBJS := $(C_FILES:./kernel/%.c=./build/%.o)
FILES := $(ASM_OBJS) $(C_OBJS)
INCLUDES = -I ./include
AS_INCLUDES = -I ./include
DEBUG_FLAGS = -g
OPTIMIZATION_FLAGS = -O0
AS_HEADERS = memlayout.asm

FLAGS = -ffreestanding \
	 $(OPTIMIZATION_FLAGS) \
	-nostdlib \
	-falign-jumps \
	-falign-functions \
	-falign-labels \
	-falign-loops \
	-fstrength-reduce \
	-fno-omit-frame-pointer \
	-mstackrealign \
	-Wno-unused-function \
	-Wno-unused-variable \
	-fno-builtin \
	-Wno-unused-label \
	-Wno-cpp \
	-Wno-unused-parameter \
	-nostartfiles \
	-nodefaultlibs \
	-std=gnu23 \
	-pedantic \
	-Wextra \
	-Wall

FLAGS += -D__KERNEL__
FLAGS += -fsanitize=undefined
FLAGS += -fstack-protector
#FLAGS += -msse2 -mfpmath=sse

.PHONY: .asm_headers
.asm_headers: FORCE
	./scripts/c_to_nasm.sh ./include $(AS_HEADERS)

./build/%.asm.o: ./kernel/%.asm .asm_headers FORCE
	$(AS) $(AS_FLAGS) $(AS_INCLUDES) -f elf $(DEBUG_FLAGS) $< -o $@

./build/%.o: ./kernel/%.c FORCE
	$(CC) $(INCLUDES) $(FLAGS) $(DEBUG_FLAGS) -c $< -o $@

./bin/kernel-grub.bin: $(FILES) FORCE
	$(LD) $(DEBUG_FLAGS) -relocatable $(FILES) -o ./build/kernelfull.o
	$(CC) $(FLAGS) $(DEBUG_FLAGS) -T ./kernel/boot/linker.ld -o ./rootfs/boot/AegrOS.bin ./build/kernelfull.o

.PHONY: grub
grub: ./bin/kernel-grub.bin FORCE
	grub-file --is-x86-multiboot ./rootfs/boot/AegrOS.bin
	./scripts/create-grub-image.sh

.PHONY: tap FORCE
tap:
	./scripts/create_tap.sh

.PHONY: qemu_grub_debug
qemu_grub_debug: grub tap FORCE
	$(QEMU) -S -gdb tcp::1234 -boot d -drive file=disk.img,format=raw -m $(MEMORY) -daemonize $(QEMU_DISPLAY) $(QEMU_NETWORK) $(QEMU_DEBUG)


.PHONY: qemu_grub
qemu_grub: grub tap FORCE
	$(QEMU) -boot d -drive file=disk.img,format=raw -m $(MEMORY) -daemonize $(QEMU_DISPLAY) $(QEMU_NETWORK)


.PHONY: clean
clean:
	rm -rf ./build ./bin ./rootfs/boot/AegrOS.bin ./rootfs/bin/* ./disk.img


# Force rebuild of all files
.PHONY: FORCE
FORCE:
