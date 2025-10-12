#pragma once

#ifndef __KERNEL__
#error "This is a kernel header, and should not be included in userspace"
#endif
#include <attributes.h>
#include <bio.h>
#include <stdint.h>

typedef unsigned int DISK_TYPE;

#define DISK_TYPE_PHYSICAL 0

struct disk {
    int id;
    DISK_TYPE type;
    uint16_t sector_size;
    struct file_system *fs;
    void *fs_private;
};


void disk_init(void);
struct disk *disk_get(int index);
void disk_sync_buffer(struct buf *b);
