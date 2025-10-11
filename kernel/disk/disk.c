#include <ahci.h>
#include <assert.h>
#include <ata.h>
#include <attributes.h>
#include <disk.h>
#include <kernel.h>
#include <mbr.h>
#include <memory.h>
#include <printf.h>
#include <status.h>
#include <stdbool.h>

NON_NULL struct file_system *vfs_resolve(struct disk *disk);
struct disk disk;
// static bool disk_use_ahci;

void disk_init()
{
    ata_init();
    memset(&disk, 0, sizeof(disk));
    disk.type = DISK_TYPE_PHYSICAL;
    disk.id   = 0;

    // disk_use_ahci = ahci_port_ready();
    printf("[DISK] using %s for disk operations\n", ahci_port_ready() ? "AHCI" : "legacy ATA");
    disk.sector_size = (uint16_t)(ahci_port_ready() ? AHCI_SECTOR_SIZE : (unsigned int)ata_get_sector_size());

    // Validate the sector size
    if (disk.sector_size != 512 && disk.sector_size != 1024 && disk.sector_size != 2048 && disk.sector_size != 4096) {
        panic("Invalid sector size detected\n");
        return;
    }

    disk.fs = vfs_resolve(&disk);
}

struct disk *disk_get(const int index)
{
    if (index != 0) {
        return nullptr;
    }

    return &disk;
}

int disk_read_block(const uint32_t lba, const int total, void *buffer)
{
    if (total <= 0) {
        return -EINVARG;
    }

    if (ahci_port_ready()) {
        const int status = ahci_read(lba, (uint32_t)total, buffer);
        if (status == ALL_OK) {
            return ALL_OK;
        }

        printf("[DISK] AHCI read failed with status %d; falling back to legacy ATA\n", status);
    }

    return ata_read_sectors(lba, total, buffer);
}

int disk_read_sector(const uint32_t sector, void *buffer)
{
    return disk_read_block(sector, 1, buffer);
}

int disk_write_block(const uint32_t lba, const int total, void *buffer)
{
    if (total <= 0) {
        return -EINVARG;
    }

    if (ahci_port_ready()) {
        const int status = ahci_write(lba, (uint32_t)total, buffer);
        if (status == ALL_OK) {
            return ALL_OK;
        }

        printf("[DISK] AHCI write failed with status %d; falling back to legacy ATA\n", status);
    }

    return ata_write_sectors(lba, total, buffer);
}

int disk_write_sector(const uint32_t sector, uint8_t *buffer)
{
    return disk_write_block(sector, 1, buffer);
}

int disk_write_sector_offset(const void *data, const int size, const int offset, const int sector)
{
    ASSERT(size <= 512 - offset);

    uint8_t buffer[512];
    disk_read_sector(sector, buffer);

    memcpy(&buffer[offset], data, size);
    return disk_write_sector(sector, buffer);
}
