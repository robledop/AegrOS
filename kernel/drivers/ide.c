// Simple PIO-based (non-DMA) IDE driver code.

#include "ahci.h"
#include "assert.h"
#include "buf.h"
#include "defs.h"
#include "fs.h"
#include "io.h"
#include "pci.h"
#include "proc.h"
#include "spinlock.h"
#include "status.h"
#include "traps.h"

/** @brief Size in bytes of a hardware IDE sector. */
#define SECTOR_SIZE 512
/** @brief Controller busy flag. */
#define IDE_BSY 0x80
/** @brief Device ready flag. */
#define IDE_DRDY 0x40
/** @brief Device fault flag. */
#define IDE_DF 0x20
/** @brief Generic error flag. */
#define IDE_ERR 0x01
/** @brief Data request ready flag. */
#define IDE_DRQ 0x08

/** @brief PIO read command for a single sector. */
#define IDE_CMD_READ 0x20
/** @brief PIO write command for a single sector. */
#define IDE_CMD_WRITE 0x30
/** @brief PIO read command for multiple sectors. */
#define IDE_CMD_RDMUL 0xc4
/** @brief PIO write command for multiple sectors. */
#define IDE_CMD_WRMUL 0xc5
/** @brief Command to set up multi-sector transfer count. */
#define IDE_CMD_SETMUL 0xc6

#define SECTOR_PER_BLOCK (BSIZE / SECTOR_SIZE)

/** @brief Protects access to the IDE request queue. */
static struct spinlock idelock;
/** @brief Linked list of pending IDE requests. */
static struct buf *idequeue;

/** @brief Tracks whether a second disk device responded. */
static int havedisk1;
static int ide_initialized;
static bool ide_controller_present;
static void ide_start(struct buf *);

/**
 * @brief Busy-wait for the IDE device to become ready.
 *
 * @param checkerr When non-zero, validate that no fault/error flags are set.
 * @return 0 when ready, -1 if an error was detected.
 */
static int ide_wait(int checkerr)
{
    int r       = 0;
    int timeout = 100000;

    while (--timeout >= 0) {
        r = inb(0x1f7);
        if (r == 0xFF) {
            return -1;
        }
        if ((r & (IDE_BSY | IDE_DRDY)) == IDE_DRDY) {
            break;
        }
        microdelay(1);
    }

    if (timeout < 0) {
        return -1;
    }

    if (checkerr && (r & (IDE_DF | IDE_ERR)) != 0) {
        return -1;
    }
    return 0;
}

static int idewait_drq(void)
{
    int r;
    while (((r = inb(0x1f7)) & (IDE_BSY | IDE_DRDY | IDE_DRQ)) != (IDE_DRDY | IDE_DRQ)) {
    }
    if (r & (IDE_DF | IDE_ERR)) {
        return -1;
    }
    return 0;
}

static void ide_initialize_hardware(void)
{
    if (ide_initialized) {
        return;
    }
    ide_initialized = 1;

    initlock(&idelock, "ide");
    if (ide_wait(0) < 0) {
        boot_message(WARNING_LEVEL_INFO, "IDE controller not responding; skipping legacy driver");
        return;
    }

    ide_controller_present = true;
    enable_ioapic_interrupt(IRQ_IDE, ncpu - 1);

    // Check if disk 1 is present
    outb(0x1f6, 0xe0 | (1 << 4));
    for (int i = 0; i < 1000; i++) {
        if (inb(0x1f7) != 0) {
            havedisk1 = 1;
            break;
        }
    }

    // Switch back to disk 0.
    outb(0x1f6, 0xe0 | (0 << 4));

#if SECTOR_PER_BLOCK > 1
    if (ide_wait(0) < 0) {
        boot_message(WARNING_LEVEL_WARNING, "ideinit: controller not ready for SETMUL; keeping single-sector PIO");
        return;
    }
    outb(0x1f2, SECTOR_PER_BLOCK);
    outb(0x1f7, IDE_CMD_SETMUL);
    if (ide_wait(1) < 0) {
        boot_message(WARNING_LEVEL_WARNING, "ideinit: set multiple failed");
    }
#endif
}

/** @brief PCI driver hook for legacy IDE controllers. */
void ide_pci_init([[maybe_unused]] struct pci_device device)
{
    ide_initialize_hardware();
}

/**
 * @brief Issue a command to service the given buffer.
 *
 * Caller must hold idelock.
 */
static void ide_start(struct buf *b)
{
    if (b == nullptr) {
        panic("ide_start");
    }
    u32 sector    = b->blockno * SECTOR_PER_BLOCK;
    int read_cmd  = (SECTOR_PER_BLOCK == 1) ? IDE_CMD_READ : IDE_CMD_RDMUL;
    int write_cmd = (SECTOR_PER_BLOCK == 1) ? IDE_CMD_WRITE : IDE_CMD_WRMUL;

#if (SECTOR_PER_BLOCK > 7)
    panic("ide_start");
#endif

    if (ide_wait(0) < 0) {
        panic("ide_start: controller not ready");
    }
    outb(0x3f6, 0);                // generate interrupt
    outb(0x1f2, SECTOR_PER_BLOCK); // number of sectors
    outb(0x1f3, sector & 0xff);
    outb(0x1f4, (sector >> 8) & 0xff);
    outb(0x1f5, (sector >> 16) & 0xff);
    outb(0x1f6, 0xe0 | ((b->dev & 1) << 4) | ((sector >> 24) & 0x0f));

    if (b->flags & B_DIRTY) {
        outb(0x1f7, write_cmd);
        if (idewait_drq() < 0) {
            boot_message(WARNING_LEVEL_ERROR, "ide_start: write error before data transfer");
        }
        outsl(0x1f0, b->data, BSIZE / 4);
    } else {
        outb(0x1f7, read_cmd);
    }
}

/** @brief Interrupt handler that completes the active IDE request. */
void ideintr(void)
{
    struct buf *b;

    // First queued buffer is the active request.
    acquire(&idelock);

    if ((b = idequeue) == nullptr) {
        release(&idelock);
        return;
    }
    idequeue = b->qnext;

    // Read data if needed.
    if (!(b->flags & B_DIRTY) && ide_wait(1) >= 0) {
        insl(0x1f0, b->data, BSIZE / 4);
    }

    // Wake process waiting for this buf.
    b->flags |= B_VALID;
    b->flags &= ~B_DIRTY;
    wakeup(b);

    // Start disk on next buf in queue.
    if (idequeue != nullptr) {
        ide_start(idequeue);
    }

    release(&idelock);
}

/**
 * @brief Synchronize a buffer with disk, reading or writing as required.
 *
 * @param b Buffer to schedule; must be locked by the caller.
 */
void iderw(struct buf *b)
{
    struct buf **pp;

    ASSERT(holdingsleep(&b->lock), "iderw: buf not locked");
    ASSERT((b->flags & (B_VALID | B_DIRTY)) != B_VALID, "iderw: nothing to do");

    if (ahci_port_ready()) {
        const u32 sectors_per_block = SECTOR_PER_BLOCK;
        const u64 lba               = (u64)b->blockno * sectors_per_block;
        const u32 sector_count      = sectors_per_block;

        int rc;
        if (b->flags & B_DIRTY) {
            rc = ahci_write(lba, sector_count, b->data);
        } else {
            rc = ahci_read(lba, sector_count, b->data);
        }

        if (rc != ALL_OK) {
            panic("ahci %s failed for block %u: %s",
                  (b->flags & B_DIRTY) ? "write" : "read",
                  b->blockno,
                  strerror(rc));
        }

        b->flags |= B_VALID;
        b->flags &= ~B_DIRTY;
        return;
    }

    if (!ide_controller_present) {
        panic("iderw: no legacy IDE controller");
    }

    acquire(&idelock);

    // Append b to idequeue.
    b->qnext = nullptr;
    for (pp = &idequeue; *pp; pp = &(*pp)->qnext) {
    }
    *pp = b;

    // Start disk if necessary.
    if (idequeue == b) {
        ide_start(b);
    }

    // Wait for request to finish.
    while ((b->flags & (B_VALID | B_DIRTY)) != B_VALID) {
        sleep(b, &idelock);
    }

    release(&idelock);
}