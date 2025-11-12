PATH := $(HOME)/opt/cross/bin:$(PATH)
TOOLPREFIX = i686-elf-

CC = $(TOOLPREFIX)gcc
AS = nasm
LD = $(TOOLPREFIX)ld
INCLUDE = -I../include -I./include
CFLAGS = -nostdlib -ffreestanding -fno-pic -static -fno-builtin -fno-strict-aliasing -Wno-unused-variable -O0 -Wall -MD -ggdb -m32 -fno-omit-frame-pointer -std=gnu23
CFLAGS += $(INCLUDE)
ASFLAGS += $(INCLUDE)
LDFLAGS += -m elf_i386
CFLAGS += -fno-pie -no-pie

LIBC_C_SRCS = $(wildcard libc/*.c)
LIBC_ASM_SRCS = $(wildcard libc/*.asm)
LIBC_C_OBJS = $(patsubst libc/%.c,build/%.o,$(LIBC_C_SRCS))
LIBC_ASM_OBJS = $(patsubst libc/%.asm,build/%.o,$(LIBC_ASM_SRCS))
LIBC = $(LIBC_C_OBJS) $(LIBC_ASM_OBJS)

build/%.o: libc/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/%.o: libc/%.asm
	$(AS) $(ASFLAGS) -f elf $< -o $@

build/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/wm/%.o: wm/%.c
	@mkdir -p build/wm
	$(CC) $(CFLAGS) -c -o $@ $<

# Auto-discover window manager sources
WM_SRCS = $(wildcard wm/*.c)
WM_OBJS = $(patsubst wm/%.c,build/wm/%.o,$(WM_SRCS))

build/startwm: $(WM_OBJS) $(LIBC)
	$(LD) $(LDFLAGS) -Ttext 0 -o $@ $^

build/%: build/%.o $(LIBC)
	$(LD) $(LDFLAGS) -Ttext 0 -o $@ $^

UPROGS=\
	build/cat\
	build/echo\
	build/forktest\
	build/grep\
	build/init\
	build/kill\
	build/ln\
	build/ls\
	build/mkdir\
	build/rm\
	build/sh\
	build/stressfs\
	build/usertests\
	build/wc\
	build/zombie\
	build/fib\
	build/touch\
	build/fbline\
	build/edit\
	build/getwinsz\
	build/startwm\

all: $(UPROGS)
	cp $(UPROGS) ../rootfs/bin/
