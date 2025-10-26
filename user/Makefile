PATH := $(HOME)/opt/cross/bin:$(PATH)
TOOLPREFIX = i686-elf-

CC = $(TOOLPREFIX)gcc
AS = nasm
LD = $(TOOLPREFIX)ld
INCLUDE = -I../include
CFLAGS = -nostdlib -ffreestanding -fno-pic -static -fno-builtin -fno-strict-aliasing -O0 -Wall -MD -ggdb -m32 -fno-omit-frame-pointer -std=gnu23
CFLAGS += $(INCLUDE)
ASFLAGS += $(INCLUDE)
LDFLAGS += -m elf_i386
CFLAGS += -fno-pie -no-pie

ULIB = build/ulib.o build/usys.o build/printf.o build/umalloc.o build/dirwalk.o

$(ULIB):
	$(AS) $(ASFLAGS) -f elf usys.asm -o build/usys.o
	$(CC) $(CFLAGS) -c -o build/ulib.o ulib.c
	$(CC) $(CFLAGS) -c -o build/printf.o printf.c
	$(CC) $(CFLAGS) -c -o build/umalloc.o umalloc.c
	$(CC) $(CFLAGS) -c -o build/dirwalk.o dirwalk.c

build/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/%: build/%.o $(ULIB)
	$(LD) $(LDFLAGS) -e main -Ttext 0 -o $@ $^

build/forktest: build/forktest.o $(ULIB)
	# forktest has less library code linked in - needs to be small
	# in order to be able to max out the proc table.
	$(LD) $(LDFLAGS) -e main -Ttext 0 -o build/forktest build/forktest.o build/ulib.o build/usys.o

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

all: $(UPROGS)
	cp $(UPROGS) ../rootfs/bin/
