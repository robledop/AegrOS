#include <ata.h>
#include <io.h>
#include <printf.h>
#include <spinlock.h>
#include <status.h>

#define ATA_PRIMARY_CMD_BASE 0x1F0
#define ATA_PRIMARY_CTL_BASE 0x3F6
#define ATA_SECONDARY_CMD_BASE 0x170
#define ATA_SECONDARY_CTL_BASE 0x376

#define ATA_REG_DATA 0x00
#define ATA_REG_FEATURES 0x01
#define ATA_REG_SEC_COUNT 0x02
#define ATA_REG_LBA0 0x03
#define ATA_REG_LBA1 0x04
#define ATA_REG_LBA2 0x05
#define ATA_REG_DEVSEL 0x06
#define ATA_REG_STATUS 0x07
#define ATA_REG_CMD ATA_REG_STATUS

#define ATA_CMD_IDENTIFY 0xEC    // Identify drive
#define ATA_CMD_CACHE_FLUSH 0xE7 // Flush cache
#define ATA_CMD_READ_PIO 0x20    // Read PIO
#define ATA_CMD_WRITE_PIO 0x30   // Write PIO

#define ATA_STATUS_ERR 0x01   // Error bit
#define ATA_STATUS_DRQ 0x08   // Data request bit
#define ATA_STATUS_BUSY 0x80  // Busy bit
#define ATA_STATUS_FAULT 0x20 // Drive fault bit

#define ATA_MASTER 0xE0 // Select master drive

#define ATA_IO_TIMEOUT 1000000

struct spinlock ata_lock;

struct ata_channel_config {
    uint16_t cmd_base;
    uint16_t ctrl_base;
    const char *name;
};

static uint16_t ata_cmd_base   = ATA_PRIMARY_CMD_BASE;
static uint16_t ata_ctrl_base  = ATA_PRIMARY_CTL_BASE;
static const char *ata_channel = "primary";

static inline uint8_t ata_read_status(void)
{
    return inb(ata_cmd_base + ATA_REG_STATUS);
}

static inline uint16_t ata_read_data(void)
{
    return inw(ata_cmd_base + ATA_REG_DATA);
}

static inline void ata_write_data(uint16_t value)
{
    outw(ata_cmd_base + ATA_REG_DATA, value);
}

static inline void ata_write_reg(uint16_t offset, uint8_t value)
{
    outb(ata_cmd_base + offset, value);
}

static inline void ata_write_control(uint8_t value)
{
    outb(ata_ctrl_base, value);
}

static inline void ata_delay_400ns(void)
{
    inb(ata_ctrl_base);
    inb(ata_ctrl_base);
    inb(ata_ctrl_base);
    inb(ata_ctrl_base);
}

static int ata_poll(const bool require_drq, const char *ctx)
{
    unsigned int attempts = ATA_IO_TIMEOUT;

    while (attempts--) {
        const uint8_t status = ata_read_status();

        if (status & ATA_STATUS_ERR) {
            printf("[ATA] %s(%s): status=0x%02X (ERR)\n", ctx, ata_channel, status);
            return -EIO;
        }

        if (status & ATA_STATUS_FAULT) {
            printf("[ATA] %s(%s): status=0x%02X (FAULT)\n", ctx, ata_channel, status);
            return -EIO;
        }

        if (!(status & ATA_STATUS_BUSY)) {
            if (!require_drq) {
                return ALL_OK;
            }

            if (status & ATA_STATUS_DRQ) {
                return ALL_OK;
            }
        }
    }

    const uint8_t status = ata_read_status();
    printf("[ATA] %s(%s): timeout, final status=0x%02X require_drq=%d\n", ctx, ata_channel, status, require_drq);

    return -EIO;
}

static int ata_select_drive(const uint32_t lba)
{
    ata_write_reg(ATA_REG_DEVSEL, (uint8_t)((lba >> 24) & 0x0F) | ATA_MASTER);
    ata_delay_400ns();
    return ata_poll(false, "select");
}

static bool ata_try_channel(const struct ata_channel_config *cfg)
{
    ata_cmd_base  = cfg->cmd_base;
    ata_ctrl_base = cfg->ctrl_base;
    ata_channel   = cfg->name;

    ata_write_reg(ATA_REG_DEVSEL, ATA_MASTER);
    ata_delay_400ns();

    const uint8_t status = ata_read_status();
    if (status == 0xFF || status == 0x00) {
        return false;
    }

    printf("[ATA] using %s channel (cmd=0x%X ctrl=0x%X status=0x%02X)\n",
           ata_channel,
           ata_cmd_base,
           ata_ctrl_base,
           status);
    return true;
}

/**
 * @brief Initialise the ATA driver and its lock state and pick a legacy channel.
 */
void ata_init()
{
    initlock(&ata_lock, "ata");

    const struct ata_channel_config channels[] = {
        {ATA_PRIMARY_CMD_BASE,   ATA_PRIMARY_CTL_BASE,   "primary"  },
        {ATA_SECONDARY_CMD_BASE, ATA_SECONDARY_CTL_BASE, "secondary"},
    };

    bool found = false;
    for (unsigned int i = 0; i < sizeof(channels) / sizeof(channels[0]); i++) {
        if (ata_try_channel(&channels[i])) {
            found = true;
            break;
        }
    }

    if (!found) {
        printf("[ATA] no ATA channels found\n");
    }
}

int ata_get_sector_size()
{
    return 512;
}

int ata_read_sectors(const uint32_t lba, const int total, void *buffer)
{
    ata_write_control(0x02); // Polling, disable interrupts

    int result = ata_select_drive(lba);
    if (result != ALL_OK) {
        ata_write_control(0x00);
        return result;
    }

    ata_write_reg(ATA_REG_SEC_COUNT, total);
    ata_write_reg(ATA_REG_LBA0, lba & 0xFF);
    ata_write_reg(ATA_REG_LBA1, (lba >> 8) & 0xFF);
    ata_write_reg(ATA_REG_LBA2, (lba >> 16) & 0xFF);
    ata_write_reg(ATA_REG_CMD, ATA_CMD_READ_PIO);

    uint16_t *ptr = (uint16_t *)buffer;
    for (int sector = 0; sector < total; sector++) {
        ata_delay_400ns();
        result = ata_poll(true, "read");
        if (result != ALL_OK) {
            ata_write_control(0x00);
            return result;
        }

        for (int i = 0; i < 256; i++) {
            ptr[i] = ata_read_data();
        }
        ptr += 256;
    }

    ata_write_control(0x00);
    return ALL_OK;
}

int ata_write_sectors(const uint32_t lba, const int total, void *buffer)
{
    acquire(&ata_lock);

    ata_write_control(0x02); // Polling, disable interrupts

    int result = ata_select_drive(lba);
    if (result != ALL_OK) {
        ata_write_control(0x00);
        release(&ata_lock);
        return result;
    }

    ata_write_reg(ATA_REG_FEATURES, 0);
    ata_write_reg(ATA_REG_SEC_COUNT, total);
    ata_write_reg(ATA_REG_LBA0, lba & 0xFF);
    ata_write_reg(ATA_REG_LBA1, (lba >> 8) & 0xFF);
    ata_write_reg(ATA_REG_LBA2, (lba >> 16) & 0xFF);
    ata_write_reg(ATA_REG_CMD, ATA_CMD_WRITE_PIO);

    ata_delay_400ns();
    result = ata_poll(true, "write setup");
    if (result != ALL_OK) {
        ata_write_control(0x00);
        release(&ata_lock);
        return result;
    }

    uint8_t *ptr = (uint8_t *)buffer;
    for (int sector = 0; sector < total; sector++) {
        for (int i = 0; i < 512; i += 2) {
            uint16_t word = ptr[i];
            word |= (uint16_t)ptr[i + 1] << 8;
            ata_write_data(word);
        }

        if (sector + 1 < total) {
            ata_delay_400ns();
            result = ata_poll(true, "write next");
            if (result != ALL_OK) {
                ata_write_control(0x00);
                release(&ata_lock);
                return result;
            }
        }

        ptr += 512;
    }

    ata_write_reg(ATA_REG_CMD, ATA_CMD_CACHE_FLUSH);
    ata_delay_400ns();
    result = ata_poll(false, "write flush");
    if (result != ALL_OK) {
        ata_write_control(0x00);
        release(&ata_lock);
        return result;
    }

    ata_write_control(0x00);
    release(&ata_lock);
    return ALL_OK;
}
