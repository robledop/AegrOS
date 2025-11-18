#include <ahci.h>
#include <printf.h>
#include <spinlock.h>
#include <status.h>
#include <string.h>
#include <types.h>
#include <defs.h>
#include <memlayout.h>
#include <mmu.h>

#define AHCI_GHC_ENABLE (1u << 31)

#define AHCI_DET_NO_DEVICE 0x0
#define AHCI_DET_DEVICE_PRESENT 0x1
#define AHCI_DET_DEVICE_PRESENT_ACTIVE 0x3

#define AHCI_IPM_NOT_PRESENT 0x0
#define AHCI_IPM_ACTIVE 0x1
#define AHCI_IPM_PARTIAL 0x2
#define AHCI_IPM_SLUMBER 0x6

#define AHCI_HBA_PxCMD_ST (1u << 0)
#define AHCI_HBA_PxCMD_FRE (1u << 4)
#define AHCI_HBA_PxCMD_FR (1u << 14)
#define AHCI_HBA_PxCMD_CR (1u << 15)

#define AHCI_PORT_IS_TFES (1u << 30)

#define AHCI_TFD_ERR 0x01
#define AHCI_TFD_DRQ 0x08
#define AHCI_TFD_BUSY 0x80

#define AHCI_COMMAND_LIST_BYTES 1024u
#define AHCI_RECEIVED_FIS_BYTES 256u
#define AHCI_PRDT_MAX_BYTES (4u * 1024u * 1024u)
#define AHCI_MAX_SECTORS_PER_CMD (AHCI_PRDT_MAX_BYTES / AHCI_SECTOR_SIZE)
#define AHCI_CMD_SLOT 0u
#define AHCI_GENERIC_TIMEOUT 1000000u
#define AHCI_MMIO_BYTES 0x1100u

struct ahci_prdt_entry
{
    u32 dba;
    u32 dbau;
    u32 reserved;
    u32 dbc;
} __attribute__((packed));

struct ahci_command_header
{
    u16 flags;
    u16 prdtl;
    u32 prdbc;
    u32 ctba;
    u32 ctbau;
    u32 reserved[4];
} __attribute__((packed));

struct ahci_command_table
{
    u8 cfis[64];
    u8 acmd[16];
    u8 reserved0[48];
    struct ahci_prdt_entry prdt[1];
} __attribute__((packed));

struct ahci_port_state
{
    bool configured;
    u8 port_index;
    volatile struct ahci_port *port;
    struct ahci_command_header *command_list;
    struct ahci_command_table *command_table;
    u8 *fis;
    u8 *bounce_buffer;
    uptr bounce_phys;
};

static volatile struct ahci_memory *hba_memory;
static struct ahci_port_state active_port;
static struct spinlock ahci_lock;
static bool ahci_lock_initialized;

extern pde_t *kpgdir;

static const char *ahci_det_to_string(const u8 det)
{
    switch (det) {
    case AHCI_DET_NO_DEVICE:
        return "no device";
    case AHCI_DET_DEVICE_PRESENT:
        return "device present";
    case AHCI_DET_DEVICE_PRESENT_ACTIVE:
        return "device active";
    default:
        return "reserved";
    }
}

static const char *ahci_ipm_to_string(const u8 ipm)
{
    switch (ipm) {
    case AHCI_IPM_NOT_PRESENT:
        return "not present";
    case AHCI_IPM_ACTIVE:
        return "active";
    case AHCI_IPM_PARTIAL:
        return "partial";
    case AHCI_IPM_SLUMBER:
        return "slumber";
    default:
        return "reserved";
    }
}

static bool ahci_port_device_present(const u8 det)
{
    return det == AHCI_DET_DEVICE_PRESENT || det == AHCI_DET_DEVICE_PRESENT_ACTIVE;
}

static uptr ahci_virt_to_phys(const void *virt)
{
    if (!virt) {
        return 0;
    }

    const u32 va    = (u32)(uptr)virt;
    const pde_t pde = kpgdir[PDX(va)];
    if ((pde & PTE_P) == 0) {
        return 0;
    }

    pte_t *const pgtab = (pte_t *)P2V(PTE_ADDR(pde));
    const pte_t pte    = pgtab[PTX(va)];
    if ((pte & PTE_P) == 0) {
        return 0;
    }

    return (uptr)(PTE_ADDR(pte) | (va & (PGSIZE - 1)));
}

#if UINTPTR_MAX > 0xFFFFFFFFu
static inline u32 ahci_upper32(const uptr value)
{
    return (u32)(value >> 32);
}
#else
static inline u32 ahci_upper32(uptr value)
{
    (void)value;
    return 0u;
}
#endif

static u32 ahci_calculate_chunk(const u8 *buffer, const u32 requested_sectors, uptr *phys_out,
                                bool *needs_bounce)
{
    const uptr phys = ahci_virt_to_phys(buffer);
    if (phys == 0) {
        *phys_out     = active_port.bounce_phys;
        *needs_bounce = true;
        return 1;
    }

    const size_t offset     = phys & (PGSIZE - 1u);
    size_t contiguous_bytes = PGSIZE - offset;
    if (contiguous_bytes > AHCI_PRDT_MAX_BYTES) {
        contiguous_bytes = AHCI_PRDT_MAX_BYTES;
    }

    const size_t requested_bytes = (size_t)requested_sectors * AHCI_SECTOR_SIZE;

    if (contiguous_bytes >= AHCI_SECTOR_SIZE) {
        if (contiguous_bytes > requested_bytes) {
            contiguous_bytes = requested_bytes;
        }

        u32 sectors = (u32)(contiguous_bytes / AHCI_SECTOR_SIZE);
        if (sectors == 0) {
            sectors = 1;
        }

        if (sectors > AHCI_MAX_SECTORS_PER_CMD) {
            sectors = AHCI_MAX_SECTORS_PER_CMD;
        }

        *phys_out     = phys;
        *needs_bounce = false;
        return sectors;
    }

    // Crosses a page with less than a full sector remaining; fall back to the bounce buffer.
    *phys_out     = active_port.bounce_phys;
    *needs_bounce = true;
    return 1;
}

static void ahci_init_lock()
{
    if (!ahci_lock_initialized) {
        initlock(&ahci_lock, "ahci");
        ahci_lock_initialized = true;
    }
}

static void *ahci_alloc_aligned(const size_t size, const size_t alignment)
{
    const size_t total = size + alignment - 1;
    u8 *raw            = kmalloc(total);
    if (!raw) {
        return NULL;
    }

    const uptr addr    = (uptr)raw;
    const uptr aligned = (addr + alignment - 1) & ~(alignment - 1);
    u8 *const ptr      = (u8 *)(uptr)aligned;
    memset(ptr, 0, size);
    return ptr;
}

static int ahci_port_wait(const volatile struct ahci_port *port, const u32 mask)
{
    u32 timeout = AHCI_GENERIC_TIMEOUT;
    while ((port->tfd & mask) != 0 && timeout-- > 0) {
        // busy wait
    }

    if (timeout == 0) {
        return -EIO;
    }

    return ALL_OK;
}

static int ahci_port_stop(volatile struct ahci_port *port)
{
    port->cmd &= ~AHCI_HBA_PxCMD_ST;

    u32 timeout = AHCI_GENERIC_TIMEOUT;
    while ((port->cmd & AHCI_HBA_PxCMD_CR) != 0 && timeout-- > 0) {
        // busy wait
    }
    if (timeout == 0) {
        return -EIO;
    }

    port->cmd &= ~AHCI_HBA_PxCMD_FRE;
    timeout = AHCI_GENERIC_TIMEOUT;
    while ((port->cmd & AHCI_HBA_PxCMD_FR) != 0 && timeout-- > 0) {
        // busy wait
    }

    return timeout == 0 ? -EIO : ALL_OK;
}

static int ahci_port_start(volatile struct ahci_port *port)
{
    u32 timeout = AHCI_GENERIC_TIMEOUT;
    while ((port->cmd & (AHCI_HBA_PxCMD_CR | AHCI_HBA_PxCMD_FR)) != 0 && timeout-- > 0) {
        // busy wait
    }
    if (timeout == 0) {
        return -EIO;
    }

    port->cmd |= AHCI_HBA_PxCMD_FRE;
    port->cmd |= AHCI_HBA_PxCMD_ST;
    return ALL_OK;
}

static int ahci_configure_active_port(volatile struct ahci_memory *memory, const u32 port_index)
{
    volatile struct ahci_port *port = &memory->ports[port_index];

    int status = ahci_port_stop(port);
    if (status != ALL_OK) {
        boot_message(WARNING_LEVEL_ERROR,
                     "[AHCI] failed to stop command engine on port %lu",
                     (unsigned long)port_index);
        return status;
    }

    struct ahci_command_header *const command_list =
        (struct ahci_command_header *)ahci_alloc_aligned(AHCI_COMMAND_LIST_BYTES, 1024);
    u8 *const fis                                  = ahci_alloc_aligned(AHCI_RECEIVED_FIS_BYTES, 256);
    struct ahci_command_table *const command_table =
        (struct ahci_command_table *)ahci_alloc_aligned(sizeof(struct ahci_command_table), 128);
    u8 *const bounce_buffer = ahci_alloc_aligned(AHCI_SECTOR_SIZE, AHCI_SECTOR_SIZE);

    if (!command_list || !fis || !command_table || !bounce_buffer) {
        boot_message(WARNING_LEVEL_ERROR,
                     "[AHCI] failed to allocate command structures for port %lu",
                     (unsigned long)port_index);
        return -ENOMEM;
    }

    memset(command_list, 0, AHCI_COMMAND_LIST_BYTES);
    memset(fis, 0, AHCI_RECEIVED_FIS_BYTES);
    memset(command_table, 0, sizeof(struct ahci_command_table));
    memset(bounce_buffer, 0, AHCI_SECTOR_SIZE);

    const uptr clb_phys    = ahci_virt_to_phys(command_list);
    const uptr fb_phys     = ahci_virt_to_phys(fis);
    const uptr ct_phys     = ahci_virt_to_phys(command_table);
    const uptr bounce_phys = ahci_virt_to_phys(bounce_buffer);

    if (clb_phys == 0 || fb_phys == 0 || ct_phys == 0 || bounce_phys == 0) {
        boot_message(WARNING_LEVEL_ERROR, "[AHCI] failed to resolve physical addresses for command buffers");
        return -EFAULT;
    }

    port->clb  = (u32)clb_phys;
    port->clbu = ahci_upper32(clb_phys);
    port->fb   = (u32)fb_phys;
    port->fbu  = ahci_upper32(fb_phys);

    command_list[AHCI_CMD_SLOT].ctba  = (u32)ct_phys;
    command_list[AHCI_CMD_SLOT].ctbau = ahci_upper32(ct_phys);
    command_list[AHCI_CMD_SLOT].prdtl = 1;

    port->serr = 0xFFFFFFFF;
    port->is   = 0xFFFFFFFF;

    status = ahci_port_start(port);
    if (status != ALL_OK) {
        boot_message(WARNING_LEVEL_ERROR,
                     "[AHCI] failed to start command engine on port %lu",
                     (unsigned long)port_index);
        return status;
    }

    active_port.configured    = true;
    active_port.port_index    = (u8)port_index;
    active_port.port          = port;
    active_port.command_list  = command_list;
    active_port.command_table = command_table;
    active_port.fis           = fis;
    active_port.bounce_buffer = bounce_buffer;
    active_port.bounce_phys   = bounce_phys;

    ahci_init_lock();

    boot_message(WARNING_LEVEL_INFO, "[AHCI] using port %lu for DMA transfers", (unsigned long)port_index);
    return ALL_OK;
}

void ahci_init(struct pci_device device)
{
    boot_message(WARNING_LEVEL_INFO,
                 "[AHCI] controller %04X:%04X at %lu:%lu.%lu",
                 device.header.vendor_id,
                 device.header.device_id,
                 (unsigned long)device.bus,
                 (unsigned long)device.slot,
                 (unsigned long)device.function);

    if (device.header.prog_if != 0x01) {
        boot_message(WARNING_LEVEL_WARNING,
                     "[AHCI] controller is not in AHCI mode (prog_if=0x%02X)",
                     device.header.prog_if);
        return;
    }

    pci_enable_bus_mastering(device);

    u32 abar = device.header.bars[5] & ~0x0F;
    if (abar == 0) {
        abar = pci_get_bar(device, PCI_BAR_MEM) & ~0x0F;
    }

    if (abar == 0) {
        boot_message(WARNING_LEVEL_ERROR, "[AHCI] controller missing ABAR; cannot continue");
        return;
    }

    void *abar_va = kernel_map_mmio(abar, AHCI_MMIO_BYTES);
    if (abar_va == nullptr) {
        boot_message(WARNING_LEVEL_ERROR, "[AHCI] failed to map ABAR 0x%08lX", (unsigned long)abar);
        return;
    }
    hba_memory = (volatile struct ahci_memory *)abar_va;

    // Ensure AHCI mode is enabled.
    hba_memory->ghc |= AHCI_GHC_ENABLE;

    const u32 version = hba_memory->vs;
    const u32 cap     = hba_memory->cap;
    const u32 ports   = hba_memory->pi;

    const u32 port_count    = (cap & 0x1F) + 1;
    const u32 version_major = (version >> 16) & 0xFFFF;
    const u32 version_minor = version & 0xFFFF;

    boot_message(WARNING_LEVEL_INFO,
                 "[AHCI] ABAR=0x%08lX version %lu.%lu cap=0x%08lX ports mask=0x%08lX",
                 (unsigned long)abar,
                 (unsigned long)version_major,
                 (unsigned long)version_minor,
                 (unsigned long)cap,
                 (unsigned long)ports);

    u32 port_mask = ports;
    if (port_mask == 0) {
        if (port_count == 0 || port_count > 32) {
            boot_message(WARNING_LEVEL_ERROR,
                         "[AHCI] invalid port count reported in CAP (NP=%lu)",
                         (unsigned long)port_count);
            return;
        }

        port_mask = (port_count == 32) ? 0xFFFFFFFFu : ((1u << port_count) - 1u);
        boot_message(WARNING_LEVEL_ERROR,
                     "[AHCI] controller reports empty PI; using CAP.NP derived mask=0x%08lX",
                     (unsigned long)port_mask);
    }

    if (port_mask == 0) {
        boot_message(WARNING_LEVEL_ERROR, "[AHCI] no ports implemented");
        return;
    }

    bool device_present_found = false;
    bool link_active_found    = false;

    for (u32 i = 0; i < 32; i++) {
        if ((port_mask & (1u << i)) == 0) {
            continue;
        }

        volatile struct ahci_port *const port = &hba_memory->ports[i];
        const u32 ssts                        = port->ssts;
        const u8 det                          = (u8)(ssts & 0x0F);
        const u8 ipm                          = (u8)((ssts >> 8) & 0x0F);

        const bool device_present = ahci_port_device_present(det);
        const bool link_active    = det == AHCI_DET_DEVICE_PRESENT_ACTIVE && ipm == AHCI_IPM_ACTIVE;

        if (device_present) {
            device_present_found = true;
        }
        if (link_active) {
            link_active_found = true;
        }

        boot_message(WARNING_LEVEL_INFO,
                     "[AHCI] port %lu: det=%s(%u) ipm=%s(%u) sig=0x%08lX%s%s",
                     (unsigned long)i,
                     ahci_det_to_string(det),
                     det,
                     ahci_ipm_to_string(ipm),
                     ipm,
                     (unsigned long)port->sig,
                     link_active ? " [link-up]" : "",
                     device_present && !link_active ? " [present]" : "");

        if (!active_port.configured && link_active) {
            if (ahci_configure_active_port(hba_memory, i) != ALL_OK) {
                boot_message(WARNING_LEVEL_ERROR, "[AHCI] failed to configure port %lu for DMA", (unsigned long)i);
            }
        }
    }

    if (!device_present_found) {
        boot_message(WARNING_LEVEL_WARNING, "[AHCI] no SATA devices detected on implemented ports");
    } else if (!link_active_found) {
        boot_message(WARNING_LEVEL_WARNING,
                     "[AHCI] SATA device presence detected but links are not active (DET != 3 or IPM != 1)");
    }
}

bool ahci_port_ready(void)
{
    return active_port.configured;
}

static int ahci_issue_dma(u64 lba, const uptr buffer_phys, const u32 sector_count, const bool write)
{
    if (!active_port.configured) {
        return -ENOTSUP;
    }

    volatile struct ahci_port *const port = active_port.port;

    int status = ahci_port_wait(port, AHCI_TFD_BUSY | AHCI_TFD_DRQ);
    if (status != ALL_OK) {
        return status;
    }

    port->serr = 0xFFFFFFFF;
    port->is   = 0xFFFFFFFF;

    struct ahci_command_header *const header = &active_port.command_list[AHCI_CMD_SLOT];
    struct ahci_command_table *const table   = active_port.command_table;

    memset(table, 0, sizeof(*table));

    header->flags = 5; // CFL = 5 (20 bytes)
    if (write) {
        header->flags |= 1u << 6; // write
    } else {
        header->flags &= ~(1u << 6);
    }
    header->prdtl = 1;
    header->prdbc = 0;

    struct ahci_prdt_entry *const prdt = &table->prdt[0];
    const u32 bytes                    = sector_count * AHCI_SECTOR_SIZE;

    prdt->dba  = (u32)buffer_phys;
    prdt->dbau = ahci_upper32(buffer_phys);
    prdt->dbc  = (bytes - 1) | (1u << 31); // Interrupt on completion

    u8 *const cfis = table->cfis;
    memset(cfis, 0, sizeof(table->cfis));

    cfis[0]  = 0x27; // FIS type: Register Host to Device
    cfis[1]  = 1u << 7;
    cfis[2]  = write ? 0x35 : 0x25; // WRITE/READ DMA EXT
    cfis[3]  = 0;
    cfis[4]  = (u8)(lba & 0xFF);
    cfis[5]  = (u8)((lba >> 8) & 0xFF);
    cfis[6]  = (u8)((lba >> 16) & 0xFF);
    cfis[7]  = (u8)(0x40 | ((lba >> 24) & 0x0F));
    cfis[8]  = (u8)((lba >> 24) & 0xFF);
    cfis[9]  = (u8)((lba >> 32) & 0xFF);
    cfis[10] = (u8)((lba >> 40) & 0xFF);
    cfis[12] = (u8)(sector_count & 0xFF);
    cfis[13] = (u8)((sector_count >> 8) & 0xFF);

    port->ci = 1u << AHCI_CMD_SLOT;

    u32 timeout = AHCI_GENERIC_TIMEOUT;
    while ((port->ci & (1u << AHCI_CMD_SLOT)) != 0 && timeout-- > 0) {
        if (port->is & AHCI_PORT_IS_TFES) {
            const u32 is   = port->is;
            const u32 serr = port->serr;
            const u32 tfd  = port->tfd;

            boot_message(WARNING_LEVEL_ERROR,
                         "[AHCI] DMA taskfile error during %s: LBA=%llu count=%u IS=0x%08X SERR=0x%08X TFD=0x%08X",
                         write ? "write" : "read",
                         (unsigned long long)lba,
                         sector_count,
                         is,
                         serr,
                         tfd);
            port->is = AHCI_PORT_IS_TFES;
            return -EIO;
        }
    }

    if (timeout == 0) {
        const u32 is   = port->is;
        const u32 serr = port->serr;
        const u32 tfd  = port->tfd;
        boot_message(WARNING_LEVEL_ERROR,
                     "[AHCI] DMA timeout during %s: LBA=%llu count=%u IS=0x%08X SERR=0x%08X TFD=0x%08X",
                     write ? "write" : "read",
                     (unsigned long long)lba,
                     sector_count,
                     is,
                     serr,
                     tfd);
        port->is = 0xFFFFFFFF;
        return -EIO;
    }

    if (port->tfd & AHCI_TFD_ERR) {
        const u32 serr = port->serr;
        const u32 is   = port->is;
        const u32 tfd  = port->tfd;
        boot_message(WARNING_LEVEL_ERROR,
                     "[AHCI] DMA taskfile status error during %s: LBA=%llu count=%u IS=0x%08X SERR=0x%08X TFD=0x%08X",
                     write ? "write" : "read",
                     (unsigned long long)lba,
                     sector_count,
                     is,
                     serr,
                     tfd);
        port->is = 0xFFFFFFFF;
        return -EIO;
    }

    return ALL_OK;
}

int ahci_read(u64 lba, u32 sector_count, void *buffer)
{
    if (!buffer || sector_count == 0) {
        return -EINVARG;
    }

    if (!active_port.configured) {
        return -ENOTSUP;
    }

    acquire(&ahci_lock);

    u8 *byte_buffer = (u8 *)buffer;
    u32 remaining   = sector_count;
    int result      = ALL_OK;

    while (remaining > 0) {
        uptr buffer_phys  = 0;
        bool needs_bounce = false;
        u32 chunk         = ahci_calculate_chunk(byte_buffer, remaining, &buffer_phys, &needs_bounce);

        result = ahci_issue_dma(lba, buffer_phys, chunk, false);
        if (result != ALL_OK) {
            break;
        }

        if (needs_bounce) {
            memcpy(byte_buffer, active_port.bounce_buffer, AHCI_SECTOR_SIZE);
        }

        lba += chunk;
        byte_buffer += chunk * AHCI_SECTOR_SIZE;
        remaining -= chunk;
    }

    release(&ahci_lock);
    return result;
}

int ahci_write(u64 lba, u32 sector_count, const void *buffer)
{
    if (!buffer || sector_count == 0) {
        return -EINVARG;
    }

    if (!active_port.configured) {
        return -ENOTSUP;
    }

    acquire(&ahci_lock);

    const u8 *byte_buffer_const = (const u8 *)buffer;
    u32 remaining               = sector_count;
    int result                  = ALL_OK;

    while (remaining > 0) {
        uptr buffer_phys  = 0;
        bool needs_bounce = false;
        u32 chunk         = ahci_calculate_chunk(byte_buffer_const, remaining, &buffer_phys, &needs_bounce);

        if (needs_bounce) {
            memcpy(active_port.bounce_buffer, byte_buffer_const, AHCI_SECTOR_SIZE);
        }

        result = ahci_issue_dma(lba, buffer_phys, chunk, true);
        if (result != ALL_OK) {
            break;
        }

        lba += chunk;
        byte_buffer_const += chunk * AHCI_SECTOR_SIZE;
        remaining -= chunk;
    }

    release(&ahci_lock);
    return result;
}
