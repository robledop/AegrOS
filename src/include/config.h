#ifndef CONFIG_H
#define CONFIG_H

#define TOTAL_INTERRUPTS 512
#define CODE_SELECTOR 0x08
#define DATA_SELECTOR 0x10

#define HEAP_SIZE_BYTES 104857600 // 100MB
#define HEAP_BLOCK_SIZE 4096

// https://wiki.osdev.org/Memory_Map_(x86)
#define HEAP_ADDRESS 0x01000000
#define HEAP_TABLE_ADDRESS 0x00007E00

#define VGA_WIDTH 80 
#define VGA_HEIGHT 25 

#define SECTOR_SIZE 512
#define MAX_PATH_LENGTH 108
#define MAX_FILE_SYSTEMS 10
#define MAX_FILE_DESCRIPTORS 512

#define KDEBUG_SERIAL

#define MAX_FMT_STR 256

#endif