PATH := $(HOME)/opt/cross/bin:$(PATH)
TOOLPREFIX = i686-elf-

CC = $(TOOLPREFIX)gcc
AS = nasm
LD = $(TOOLPREFIX)ld
INCLUDE = -I../include -I./include
CFLAGS = -nostdlib -ffreestanding -fno-pic -static -fno-builtin -fno-strict-aliasing -O0 -Wall -MD -ggdb -m32 -fno-omit-frame-pointer -std=gnu23
CFLAGS += $(INCLUDE)
ASFLAGS += $(INCLUDE)
LDFLAGS += -m elf_i386
CFLAGS += -fno-pie -no-pie

LIBC = build/ulib.o build/usys.o build/printf.o build/umalloc.o build/dirwalk.o build/crt0.o build/crt1.o build/math.o build/time.o build/errno.o

$(LIBC):
	$(AS) $(ASFLAGS) -f elf libc/usys.asm -o build/usys.o
	$(AS) $(ASFLAGS) -f elf libc/crt0.asm -o build/crt0.o
	$(CC) $(CFLAGS) -c -o build/crt1.o libc/crt1.c
	$(CC) $(CFLAGS) -c -o build/ulib.o libc/ulib.c
	$(CC) $(CFLAGS) -c -o build/printf.o libc/printf.c
	$(CC) $(CFLAGS) -c -o build/umalloc.o libc/umalloc.c
	$(CC) $(CFLAGS) -c -o build/dirwalk.o libc/dirwalk.c
	$(CC) $(CFLAGS) -c -o build/math.o libc/math.c
	$(CC) $(CFLAGS) -c -o build/time.o libc/time.c
	$(CC) $(CFLAGS) -c -o build/errno.o libc/errno.c

build/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/wm/%.o: wm/%.c
	@mkdir -p build/wm
	$(CC) $(CFLAGS) -c -o $@ $<

WM_OBJS = build/wm/main.o build/wm/desktop.o build/wm/bmp.o build/wm/rect.o build/wm/video_context.o build/wm/window.o

build/wm: $(WM_OBJS) $(LIBC)
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
	build/wm\

all: $(UPROGS)
	cp $(UPROGS) ../rootfs/bin/
