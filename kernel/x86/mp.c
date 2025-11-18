// Multiprocessor support
// Search memory for MP description structures.
// http://developer.intel.com/design/pentium/datashts/24201606.pdf

#include "types.h"
#include "io.h"
#include "defs.h"
#include "string.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "mp.h"
#include "x86.h"
#include "proc.h"

struct cpu cpus[NCPU];
int ncpu;
u8 ioapicid;

enum mp_source
{
    MP_SOURCE_NONE,
    MP_SOURCE_ACPI,
    MP_SOURCE_LEGACY,
    MP_SOURCE_ACPI_LEGACY
};

static enum mp_source mp_source_state = MP_SOURCE_NONE;
static int mp_acpi_cpu_count;
static int mp_legacy_cpu_count;
static u32 acpi_rsdp_phys;
static u32 acpi_madt_phys;
static bool acpi_rsdp_found;
static bool acpi_rsdt_found;
static bool acpi_xsdt_found;
static bool acpi_madt_found;
static u32 acpi_rsdt_phys;
static u32 acpi_xsdt_phys;
static u32 acpi_rsdt_length;
static u32 acpi_xsdt_length;
static u8 acpi_rsdp_revision;
static u32 acpi_rsdt_addr_raw;
static unsigned long long acpi_xsdt_addr_raw;

enum mp_record_mode
{
    MP_RECORD_NONE,
    MP_RECORD_ACPI,
    MP_RECORD_LEGACY
};

static enum mp_record_mode mp_recording = MP_RECORD_NONE;
extern u32 boot_config_table_ptr;

static int mpinit_legacy(void);
static int acpi_init(void);

static void set_lapic_base(u32 phys_addr)
{
    void *va = kernel_map_mmio(phys_addr, PGSIZE);
    if (va == nullptr) {
        boot_message(WARNING_LEVEL_ERROR, "Failed to map LAPIC at 0x%x", phys_addr);
        lapic = nullptr;
        return;
    }
    lapic = (u32 *)va;
}

static void *acpi_map_range(u32 phys_addr, u32 length)
{
    if (length == 0) {
        return nullptr;
    }

    u32 end = phys_addr + length;
    if (end <= PHYSTOP) {
        return P2V(phys_addr);
    }

    return kernel_map_mmio(phys_addr, length);
}

struct acpi_rsdp
{
    char signature[8];
    u8 checksum;
    char oemid[6];
    u8 revision;
    u32 rsdt_addr;
} __attribute__((packed));

struct acpi_rsdp_v2
{
    struct acpi_rsdp v1;
    u32 length;
    unsigned long long xsdt_addr;
    u8 extended_checksum;
    u8 reserved[3];
} __attribute__((packed));

struct acpi_sdt_header
{
    char signature[4];
    u32 length;
    u8 revision;
    u8 checksum;
    char oemid[6];
    char oem_table_id[8];
    u32 oem_revision;
    u32 creator_id;
    u32 creator_revision;
} __attribute__((packed));

struct acpi_madt
{
    struct acpi_sdt_header header;
    u32 lapic_addr;
    u32 flags;
} __attribute__((packed));

struct acpi_madt_entry
{
    u8 type;
    u8 length;
} __attribute__((packed));

struct acpi_madt_lapic
{
    struct acpi_madt_entry header;
    u8 acpi_processor_id;
    u8 apic_id;
    u32 flags;
} __attribute__((packed));

struct acpi_madt_ioapic
{
    struct acpi_madt_entry header;
    u8 ioapic_id;
    u8 reserved;
    u32 ioapic_addr;
    u32 gsi_base;
} __attribute__((packed));

struct acpi_madt_lapic_override
{
    struct acpi_madt_entry header;
    u16 reserved;
    unsigned long long lapic_addr;
} __attribute__((packed));

struct acpi_madt_x2apic
{
    struct acpi_madt_entry header;
    u16 reserved;
    u32 x2apic_id;
    u32 flags;
    u32 acpi_processor_uid;
} __attribute__((packed));

static u8
sum(u8 *addr, int len)
{
    int sum = 0;
    for (int i = 0; i < len; i++)
        sum += addr[i];
    return sum;
}

// Look for an MP structure in the len bytes at addr.
static struct mp *mpsearch1(u32 a, int len)
{
    u8 *addr = P2V(a);
    u8 *e    = addr + len;
    for (u8 *p = addr; p < e; p += sizeof(struct mp))
        if (memcmp(p, "_MP_", 4) == 0 && sum(p, sizeof(struct mp)) == 0)
            return (struct mp *)p;
    return nullptr;
}

// Search for the MP Floating Pointer Structure, which according to the
// spec is in one of the following three locations:
// 1) in the first KB of the EBDA;
// 2) in the last KB of system base memory;
// 3) in the BIOS ROM between 0xE0000 and 0xFFFFF.
static struct mp *mpsearch(void)
{
    u32 p;
    struct mp *mp;

    u8 *bda = (u8 *)P2V(0x400);
    if ((p = ((bda[0x0F] << 8) | bda[0x0E]) << 4)) {
        if ((mp = mpsearch1(p, 1024)))
            return mp;
    } else {
        p = ((bda[0x14] << 8) | bda[0x13]) * 1024;
        if ((mp = mpsearch1(p - 1024, 1024)))
            return mp;
    }
    return mpsearch1(0xF0000, 0x10000);
}

// Search for an MP configuration table.  For now,
// don't accept the default configurations (physaddr == 0).
// Check for the correct signature, calculate the checksum and,
// if correct, check the version.
// To do: check extended table checksum.
static bool cpu_apic_exists(u32 apicid)
{
    for (int i = 0; i < ncpu; i++) {
        if (cpus[i].apicid == apicid) {
            return true;
        }
    }
    return false;
}

static void record_cpu_apicid(u32 apicid)
{
    if (cpu_apic_exists(apicid)) {
        return;
    }

    if (ncpu < NCPU) {
        cpus[ncpu++].apicid = apicid;
        if (mp_recording == MP_RECORD_ACPI)
            mp_acpi_cpu_count++;
        else if (mp_recording == MP_RECORD_LEGACY)
            mp_legacy_cpu_count++;
    }
}

static struct mpconf *mpconfig(struct mp **pmp)
{
    struct mp *mp;

    if ((mp = mpsearch()) == nullptr || mp->physaddr == nullptr)
        return nullptr;
    struct mpconf *conf = (struct mpconf *)P2V((u32)mp->physaddr);
    if (memcmp(conf, "PCMP", 4) != 0)
        return nullptr;
    if (conf->version != 1 && conf->version != 4)
        return nullptr;
    if (sum((u8 *)conf, conf->length) != 0)
        return nullptr;
    *pmp = mp;
    return conf;
}

static int mpinit_legacy(void)
{
    u8 *p, *e;
    struct mp *mp;
    struct mpconf *conf;

    if ((conf = mpconfig(&mp)) == nullptr)
        return 0;

    boot_message(WARNING_LEVEL_INFO,
                 "MP config table at 0x%p version %u entries %u",
                 conf,
                 conf->version,
                 conf->entry);

    set_lapic_base((u32)conf->lapicaddr);

    mp_recording = MP_RECORD_LEGACY;
    for (p = (u8 *)(conf + 1), e = (u8 *)conf + conf->length; p < e;) {
        switch (*p) {
        case MPPROC: {
            struct mpproc *proc = (struct mpproc *)p;
            boot_message(WARNING_LEVEL_INFO, "MP PROC apicid=%u flags=0x%x", proc->apicid, proc->flags);
            record_cpu_apicid(proc->apicid);
            p += sizeof(struct mpproc);
            continue;
        }
        case MPIOAPIC: {
            struct mpioapic *ioapic = (struct mpioapic *)p;
            ioapicid                = ioapic->apicno;
            p += sizeof(struct mpioapic);
            continue;
        }
        case MPBUS:
        case MPIOINTR:
        case MPLINTR:
            p += 8;
            continue;
        default:
            return 0;
        }
    }
    mp_recording = MP_RECORD_NONE;

    if (mp->imcrp) {
        outb(0x22, 0x70);
        outb(0x23, inb(0x23) | 1);
    }

    return ncpu > 0;
}

static struct acpi_rsdp *acpi_rsdp_search(u32 phys_addr, int len)
{
    u8 *addr = P2V(phys_addr);
    u8 *end  = addr + len;
    for (u8 *p = addr; p < end; p += 16)
        if (memcmp(p, "RSD PTR ", 8) == 0) {
            struct acpi_rsdp *rsdp = (struct acpi_rsdp *)p;
            u32 length             = sizeof(struct acpi_rsdp);
            if (rsdp->revision >= 2) {
                struct acpi_rsdp_v2 *rsdp2 = (struct acpi_rsdp_v2 *)p;
                if (rsdp2->length >= sizeof(struct acpi_rsdp))
                    length = rsdp2->length;
            }
            if (sum(p, length) == 0) {
                acpi_rsdp_phys     = phys_addr + (u32)(p - addr);
                acpi_rsdp_found    = true;
                acpi_rsdp_revision = rsdp->revision;
                acpi_rsdt_addr_raw = rsdp->rsdt_addr;
                acpi_xsdt_addr_raw = (rsdp->revision >= 2) ? ((struct acpi_rsdp_v2 *)rsdp)->xsdt_addr : 0;
                return rsdp;
            }
        }
    return nullptr;
}

static struct acpi_rsdp *acpi_find_rsdp(void)
{
    u8 *bda          = (u8 *)P2V(0x400);
    u32 ebda_segment = (bda[0x0F] << 8) | bda[0x0E];
    if (ebda_segment) {
        u32 addr = ebda_segment << 4;
        boot_message(WARNING_LEVEL_INFO, "ACPI: scanning EBDA at 0x%x", addr);
        struct acpi_rsdp *rsdp = acpi_rsdp_search(addr, 1024);
        if (rsdp)
            return rsdp;
    }

    u32 base_mem_kb = (bda[0x14] << 8) | bda[0x13];
    if (base_mem_kb >= 1024) {
        u32 addr = base_mem_kb * 1024 - 1024;
        boot_message(WARNING_LEVEL_INFO, "ACPI: scanning top of base memory at 0x%x", addr);
        struct acpi_rsdp *rsdp = acpi_rsdp_search(addr, 1024);
        if (rsdp)
            return rsdp;
    }

    extern u32 boot_config_table_ptr;
    if (boot_config_table_ptr != 0) {
        struct acpi_rsdp *rsdp = (struct acpi_rsdp *)boot_config_table_ptr;
        if (memcmp(rsdp->signature, "RSD PTR ", 8) == 0) {
            u32 length = sizeof(*rsdp);
            if (rsdp->revision >= 2) {
                length = ((struct acpi_rsdp_v2 *)rsdp)->length;
            }
            if (sum((u8 *)rsdp, length) == 0) {
                acpi_rsdp_phys     = boot_config_table_ptr;
                acpi_rsdp_found    = true;
                acpi_rsdp_revision = rsdp->revision;
                acpi_rsdt_addr_raw = rsdp->rsdt_addr;
                acpi_xsdt_addr_raw = (rsdp->revision >= 2) ? ((struct acpi_rsdp_v2 *)rsdp)->xsdt_addr : 0;
                return rsdp;
            }
        }
        boot_message(WARNING_LEVEL_WARNING,
                     "ACPI: multiboot config table pointer 0x%x did not look like an RSDP",
                     boot_config_table_ptr);
    }

    boot_message(WARNING_LEVEL_INFO, "ACPI: scanning BIOS ROM 0xE0000-0xFFFFF");
    return acpi_rsdp_search(0xE0000, 0x20000);
}

static struct acpi_sdt_header *acpi_map_table(u32 phys_addr)
{
    struct acpi_sdt_header *hdr = (struct acpi_sdt_header *)acpi_map_range(phys_addr,
                                                                           sizeof(struct acpi_sdt_header));
    if (hdr == nullptr) {
        boot_message(WARNING_LEVEL_WARNING, "ACPI: failed to map table header at 0x%x", phys_addr);
        return nullptr;
    }
    struct acpi_sdt_header *full = (struct acpi_sdt_header *)acpi_map_range(phys_addr, hdr->length);
    if (full == nullptr) {
        boot_message(WARNING_LEVEL_WARNING,
                     "ACPI: failed to map table body at 0x%x length=%u",
                     phys_addr,
                     hdr->length);
    }
    return full;
}

static int acpi_parse_madt(struct acpi_madt *madt)
{
    if (!madt || madt->header.length < sizeof(struct acpi_madt))
        return 0;

    boot_message(WARNING_LEVEL_INFO,
                 "ACPI: parsing MADT lapic=0x%x flags=0x%x length=%u",
                 madt->lapic_addr,
                 madt->flags,
                 madt->header.length);
    acpi_madt_found = true;
    set_lapic_base(madt->lapic_addr);

    u8 *p        = (u8 *)madt + sizeof(struct acpi_madt);
    u8 *end      = (u8 *)madt + madt->header.length;
    mp_recording = MP_RECORD_ACPI;
    while (p + sizeof(struct acpi_madt_entry) <= end) {
        struct acpi_madt_entry *entry = (struct acpi_madt_entry *)p;
        if (entry->length < sizeof(struct acpi_madt_entry) || p + entry->length > end)
            break;

        switch (entry->type) {
        case 0: // Processor Local APIC
        {
            struct acpi_madt_lapic *lapic_entry = (struct acpi_madt_lapic *)p;
            if (lapic_entry->flags & 0x01) {
                boot_message(WARNING_LEVEL_INFO,
                             "ACPI MADT LAPIC id=%u flags=0x%x",
                             lapic_entry->apic_id,
                             lapic_entry->flags);
                record_cpu_apicid(lapic_entry->apic_id);
            }
            break;
        }
        case 1: // I/O APIC
        {
            struct acpi_madt_ioapic *ioapic_entry = (struct acpi_madt_ioapic *)p;
            ioapicid                              = ioapic_entry->ioapic_id;
            break;
        }
        case 5: // Local APIC address override
        {
            struct acpi_madt_lapic_override *override_entry = (struct acpi_madt_lapic_override *)p;
            set_lapic_base((u32)override_entry->lapic_addr);
            break;
        }
        case 9: // Processor Local x2APIC
        {
            struct acpi_madt_x2apic *x2apic_entry = (struct acpi_madt_x2apic *)p;
            if (x2apic_entry->flags & 0x01)
                record_cpu_apicid(x2apic_entry->x2apic_id);
            break;
        }
        default:
            break;
        }

        p += entry->length;
    }
    mp_recording = MP_RECORD_NONE;

    return ncpu > 0 && lapic != nullptr;
}

static int acpi_visit_sdt(struct acpi_sdt_header *table, int entry_size)
{
    if (!table || table->length < sizeof(struct acpi_sdt_header))
        return 0;
    if (sum((u8 *)table, table->length) != 0)
        return 0;

    int count   = (table->length - sizeof(struct acpi_sdt_header)) / entry_size;
    u8 *entries = (u8 *)table + sizeof(struct acpi_sdt_header);

    for (int i = 0; i < count; i++) {
        unsigned long long addr = 0;
        if (entry_size == 8) {
            memmove(&addr, entries + (u32)i * entry_size, sizeof(addr));
        } else {
            u32 addr32 = 0;
            memmove(&addr32, entries + (u32)i * entry_size, sizeof(addr32));
            addr = addr32;
        }
        if (addr == 0)
            continue;
        if (entry_size == 8 && (addr >> 32) != 0) {
            boot_message(WARNING_LEVEL_WARNING, "ACPI: ignoring 64-bit table above 4GiB (0x%llx)", addr);
            continue;
        }

        struct acpi_sdt_header *entry = acpi_map_table((u32)addr);
        if (entry == nullptr) {
            continue;
        }

        boot_message(WARNING_LEVEL_INFO,
                     "ACPI: found table %.4s at 0x%llx length %u",
                     entry->signature,
                     addr,
                     entry->length);

        if (memcmp(entry->signature, "APIC", 4) == 0) {
            if (sum((u8 *)entry, entry->length) != 0) {
                boot_message(WARNING_LEVEL_WARNING, "ACPI: MADT checksum mismatch");
                continue;
            }
            if (acpi_parse_madt((struct acpi_madt *)entry)) {
                acpi_madt_phys = (u32)addr;
                return 1;
            }
        }
    }

    return 0;
}

static int acpi_init(void)
{
    struct acpi_rsdp *rsdp = acpi_find_rsdp();
    if (!rsdp) {
        boot_message(WARNING_LEVEL_WARNING, "ACPI: RSDP not found");
        return 0;
    }

    boot_message(WARNING_LEVEL_INFO,
                 "ACPI: RSDP at 0x%x revision %u rsdt=0x%x xsdt=0x%llx",
                 acpi_rsdp_phys,
                 rsdp->revision,
                 rsdp->rsdt_addr,
                 (rsdp->revision >= 2) ? ((struct acpi_rsdp_v2 *)rsdp)->xsdt_addr : 0ULL);

    if (rsdp->rsdt_addr) {
        struct acpi_sdt_header *rsdt = acpi_map_table(rsdp->rsdt_addr);
        if (rsdt != nullptr && memcmp(rsdt->signature, "RSDT", 4) == 0) {
            boot_message(WARNING_LEVEL_INFO,
                         "ACPI: RSDT at 0x%x length=%u",
                         rsdp->rsdt_addr,
                         rsdt->length);
            acpi_rsdt_found  = true;
            acpi_rsdt_phys   = rsdp->rsdt_addr;
            acpi_rsdt_length = rsdt->length;
            if (acpi_visit_sdt(rsdt, 4)) {
                return lapic != nullptr && ncpu > 0;
            }
        } else {
            boot_message(WARNING_LEVEL_WARNING, "ACPI: failed to map RSDT at 0x%x", rsdp->rsdt_addr);
        }
    }

    if (rsdp->revision >= 2) {
        struct acpi_rsdp_v2 *rsdp2 = (struct acpi_rsdp_v2 *)rsdp;
        if (rsdp2->xsdt_addr && (rsdp2->xsdt_addr >> 32) == 0) {
            u32 xsdt_phys                = (u32)rsdp2->xsdt_addr;
            struct acpi_sdt_header *xsdt = acpi_map_table(xsdt_phys);
            if (xsdt != nullptr && memcmp(xsdt->signature, "XSDT", 4) == 0) {
                boot_message(WARNING_LEVEL_INFO,
                             "ACPI: XSDT at 0x%x length=%u",
                             xsdt_phys,
                             xsdt->length);
                acpi_xsdt_found  = true;
                acpi_xsdt_phys   = xsdt_phys;
                acpi_xsdt_length = xsdt->length;
                if (acpi_visit_sdt(xsdt, 8)) {
                    return lapic != nullptr && ncpu > 0;
                }
            } else {
                boot_message(WARNING_LEVEL_WARNING, "ACPI: failed to map XSDT at 0x%x", xsdt_phys);
            }
        }
    }

    return 0;
}

void smp_init(void)
{
    ncpu                = 0;
    lapic               = nullptr;
    ioapicid            = 0;
    acpi_rsdp_found     = false;
    acpi_rsdt_found     = false;
    acpi_xsdt_found     = false;
    acpi_madt_found     = false;
    acpi_rsdt_phys      = 0;
    acpi_xsdt_phys      = 0;
    acpi_rsdt_length    = 0;
    acpi_xsdt_length    = 0;
    acpi_madt_phys      = 0;
    acpi_rsdp_revision  = 0;
    acpi_rsdt_addr_raw  = 0;
    acpi_xsdt_addr_raw  = 0;
    mp_acpi_cpu_count   = 0;
    mp_legacy_cpu_count = 0;

    bool acpi_ok   = acpi_init() != 0;
    bool legacy_ok = false;

    if (!acpi_ok || ncpu <= 1) {
        if (!acpi_ok) {
            boot_message(WARNING_LEVEL_WARNING,
                         "ACPI multiprocessor initialization failed, falling back to legacy MP tables");
        } else {
            boot_message(WARNING_LEVEL_WARNING,
                         "ACPI reported only %d CPU(s); attempting legacy MP tables for additional processors",
                         ncpu);
        }
        legacy_ok = mpinit_legacy() != 0;
    }

    if (ncpu == 0) {
        panic("Failed to initialize multiprocessor support");
    }

    if (legacy_ok && acpi_ok) {
        mp_source_state = MP_SOURCE_ACPI_LEGACY;
    } else if (legacy_ok) {
        mp_source_state = MP_SOURCE_LEGACY;
    } else if (acpi_ok) {
        mp_source_state = MP_SOURCE_ACPI;
    }
}

void mp_report_state(void)
{
    const char *source = "unknown";
    switch (mp_source_state) {
    case MP_SOURCE_ACPI:
        source = "ACPI";
        break;
    case MP_SOURCE_LEGACY:
        source = "legacy MP";
        break;
    case MP_SOURCE_ACPI_LEGACY:
        source = "ACPI + legacy MP";
        break;
    default:
        break;
    }

    boot_message(WARNING_LEVEL_INFO, "Detected %d CPU(s) via %s", ncpu, source);
    if (acpi_rsdp_found) {
        boot_message(WARNING_LEVEL_INFO,
                     " ACPI RSDP rev %u at 0x%x (RSDT=0x%x XSDT=0x%llx)",
                     acpi_rsdp_revision,
                     acpi_rsdp_phys,
                     acpi_rsdt_addr_raw,
                     acpi_xsdt_addr_raw);
    } else {
        boot_message(WARNING_LEVEL_WARNING, " ACPI RSDP not located via standard scan");
        for (u32 addr = 0xE0000; addr < 0x100000; addr += 16) {
            const char *sig = (const char *)P2V(addr);
            if (memcmp(sig, "RSD PTR ", 8) == 0) {
                boot_message(WARNING_LEVEL_WARNING, "  Found RSDP signature at 0x%x (checksum not verified)", addr);
                break;
            }
        }
    }
    if (acpi_rsdt_found) {
        boot_message(WARNING_LEVEL_INFO, " ACPI RSDT at 0x%x length=%u", acpi_rsdt_phys, acpi_rsdt_length);
    } else if (acpi_rsdp_found) {
        boot_message(WARNING_LEVEL_WARNING, " ACPI RSDT not mapped");
    }
    if (acpi_xsdt_found) {
        boot_message(WARNING_LEVEL_INFO, " ACPI XSDT at 0x%x length=%u", acpi_xsdt_phys, acpi_xsdt_length);
    }
    if (mp_acpi_cpu_count > 0) {
        boot_message(WARNING_LEVEL_INFO,
                     " ACPI enumerated %d CPU(s) (MADT=0x%x)",
                     mp_acpi_cpu_count,
                     acpi_madt_phys);
    } else if (acpi_rsdp_found) {
        boot_message(WARNING_LEVEL_WARNING, " ACPI did not enumerate any CPUs");
    }
    if (mp_legacy_cpu_count > 0) {
        boot_message(WARNING_LEVEL_INFO, " Legacy MP enumerated %d CPU(s)", mp_legacy_cpu_count);
    }
}
