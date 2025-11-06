#pragma once

// config.asm is generated from config.h using the script c_to_nasm.sh
// This file must only contain #ifndef, #define and #endif

#define TOTAL_INTERRUPTS 512

// #define HEAP_SIZE_BYTES 510 * 1024 * 1024
#define HEAP_BLOCK_SIZE 4096

// https://wiki.osdev.org/Memory_Map_(x86)
#define HEAP_ADDRESS 0x01'000'000
#define HEAP_TABLE_ADDRESS 0x00007E00

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

#define MAX_PATH_LENGTH 108
#define MAX_FILE_SYSTEMS 10
#define MAX_FILE_DESCRIPTORS 512

#define MAX_FMT_STR 10'240

#define PROGRAM_VIRTUAL_ADDRESS 0x400'000

// Must be aligned to 4096 bytes page size
#define USER_STACK_SIZE (1024 * 256)
#define KERNEL_STACK_SIZE (1024 * 256)

#define KERNEL_CODE_SELECTOR 0x08
#define KERNEL_DATA_SELECTOR 0x10
#define USER_CODE_SELECTOR 0x1B
#define USER_DATA_SELECTOR 0x23
#define TSS_SELECTOR 0x28

#define USER_STACK_TOP 0x3FF'000
#define USER_STACK_BOTTOM (USER_STACK_TOP - USER_STACK_SIZE)

#define MAX_PROGRAM_ALLOCATIONS 1024
#define MAX_PROCESSES 64

#define MAX_SYSCALLS 1024
#define KEYBOARD_BUFFER_SIZE 1024

#define KERNEL_LOAD_ADDRESS 0x200'000

#define DESKTOP_BACKGROUND_COLOR 0x113399

#define VESA_CHAR_WIDTH 8
#define VESA_CHAR_HEIGHT 12
#define VESA_LINE_HEIGHT 14

////////////////////////////////////////////////////////////////////////////////
#define NPROC 64                  // maximum number of processes
#define KSTACKSIZE 4096           // size of per-process kernel stack
#define NCPU 8                    // maximum number of CPUs
#define NOFILE 16                 // open files per process
#define NFILE 100                 // open files per system
#define NINODE 50                 // maximum number of active i-nodes
#define NDEV 10                   // maximum major device number
#define ROOTDEV 1                 // device number of file system root disk
#define MAXARG 32                 // max exec arguments
#define MAXOPBLOCKS 50            // max # of blocks any FS op writes
#define LOGSIZE (MAXOPBLOCKS * 3) // max data blocks in on-disk log
#define NBUF (MAXOPBLOCKS * 3)    // size of disk block cache
#define FSSIZE 1000               // size of the file system in blocks
