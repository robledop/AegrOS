#include "physmem.h"
#include "defs.h"
#include "memlayout.h"
#include "mmu.h"

#define MAX_USABLE_MEMORY_RANGES 32

/** @brief Top of physical memory mapped into the kernel's direct map */
u32 phys_mem_top = 0x20000000; // default to 512MB until Multiboot data is parsed
/** @brief Highest usable RAM address provided by firmware */
u32 phys_ram_end = 0x20000000; // default until overwritten by Multiboot data
/** @brief First address after kernel loaded from ELF file */
extern char kernel_end[]; // first address after kernel loaded from ELF file. See kernel.ld

extern void freerange(void *vstart, void *vend);


static struct phys_mem_range usable_phys_ranges[MAX_USABLE_MEMORY_RANGES];
static int usable_phys_range_count;

static struct phystop_boot_info phystop_boot_state = {
    .source = PHYSTOP_SOURCE_FALLBACK,
    .reported_total_bytes = 0x20000000ull,
    .reported_usable_bytes = 0x20000000ull,
    .final_total_bytes = 0x20000000,
    .final_usable_bytes = 0x20000000,
    .total_clamped = false,
    .usable_clamped = false,
    .heap_reservation_active = false,
    .heap_reserve_bytes = 0,
};

static void reset_usable_phys_ranges(void)
{
    usable_phys_range_count = 0;
}

static void record_usable_phys_range(u32 start, u32 end)
{
    if (start >= end) {
        return;
    }
    if (usable_phys_range_count >= MAX_USABLE_MEMORY_RANGES) {
        boot_message(WARNING_LEVEL_WARNING,
                     "Too many usable memory ranges; dropping 0x%x-0x%x",
                     start,
                     end);
        return;
    }
    usable_phys_ranges[usable_phys_range_count].start = start;
    usable_phys_ranges[usable_phys_range_count].end   = end;
    usable_phys_range_count++;
}

static void collect_usable_ranges_from_mmap(const multiboot_info_t *mbinfo, u32 usable_limit)
{
    if (mbinfo == nullptr || (mbinfo->flags & MULTIBOOT_INFO_MEM_MAP) == 0 || mbinfo->mmap_length == 0) {
        return;
    }

    const u32 mmap_end                  = mbinfo->mmap_addr + mbinfo->mmap_length;
    const multiboot_memory_map_t *entry = (const multiboot_memory_map_t *)(u32)mbinfo->mmap_addr;

    while ((u32)entry < mmap_end) {
        if (entry->len == 0) {
            entry = (const multiboot_memory_map_t *)((u32)entry + entry->size + sizeof(entry->size));
            continue;
        }
        if (entry->type != MULTIBOOT_MEMORY_AVAILABLE) {
            entry = (const multiboot_memory_map_t *)((u32)entry + entry->size + sizeof(entry->size));
            continue;
        }

        u64 start = entry->addr;
        u64 end   = start + entry->len;
        if (start >= usable_limit) {
            entry = (const multiboot_memory_map_t *)((u32)entry + entry->size + sizeof(entry->size));
            continue;
        }
        if (end > usable_limit) {
            end = usable_limit;
        }
        if (end > start) {
            record_usable_phys_range((u32)start, (u32)end);
        }

        entry = (const multiboot_memory_map_t *)((u32)entry + entry->size + sizeof(entry->size));
    }
}

static void ensure_default_usable_range(u32 usable_limit)
{
    if (usable_phys_range_count == 0 && usable_limit > 0) {
        record_usable_phys_range(0, usable_limit);
    }
}

static void sort_and_merge_usable_ranges(void)
{
    if (usable_phys_range_count <= 1) {
        return;
    }
    for (int i = 1; i < usable_phys_range_count; ++i) {
        struct phys_mem_range key = usable_phys_ranges[i];
        int j                     = i - 1;
        while (j >= 0 && usable_phys_ranges[j].start > key.start) {
            usable_phys_ranges[j + 1] = usable_phys_ranges[j];
            --j;
        }
        usable_phys_ranges[j + 1] = key;
    }

    int write = 0;
    for (int i = 1; i < usable_phys_range_count; ++i) {
        struct phys_mem_range *cur  = &usable_phys_ranges[write];
        struct phys_mem_range *next = &usable_phys_ranges[i];
        if (next->start <= cur->end) {
            if (next->end > cur->end) {
                cur->end = next->end;
            }
        } else {
            ++write;
            usable_phys_ranges[write] = *next;
        }
    }
    usable_phys_range_count = write + 1;
}

void physmem_build_ranges(const multiboot_info_t *mbinfo, u32 usable_limit)
{
    reset_usable_phys_ranges();
    collect_usable_ranges_from_mmap(mbinfo, usable_limit);
    ensure_default_usable_range(usable_limit);
    sort_and_merge_usable_ranges();
}

void physmem_for_each_range(physmem_range_callback cb, void *ctx)
{
    if (cb == nullptr) {
        return;
    }
    for (int i = 0; i < usable_phys_range_count; ++i) {
        cb(usable_phys_ranges[i].start, usable_phys_ranges[i].end, ctx);
    }
}

static struct memory_limits memory_limits_from_mmap(const multiboot_info_t *mbinfo)
{
    struct memory_limits limits = {0, 0};
    if ((mbinfo->flags & MULTIBOOT_INFO_MEM_MAP) == 0) {
        return limits;
    }

    constexpr u64 max_supported = (u64)MMIOBASE - KERNBASE;
    const u32 mmap_end          = mbinfo->mmap_addr + mbinfo->mmap_length;
    auto entry                  = (const multiboot_memory_map_t *)(u32)mbinfo->mmap_addr;

    while ((u32)entry < mmap_end) {
        if (entry->len == 0) {
            entry = (const multiboot_memory_map_t *)((u32)entry + entry->size + sizeof(entry->size));
            continue;
        }

        u64 addr = entry->addr;
        if (addr >= max_supported) {
            entry = (const multiboot_memory_map_t *)((u32)entry + entry->size + sizeof(entry->size));
            continue;
        }

        u64 len           = entry->len;
        const u64 max_len = max_supported - addr;
        if (len > max_len) {
            len = max_len;
        }
        if (len == 0) {
            entry = (const multiboot_memory_map_t *)((u32)entry + entry->size + sizeof(entry->size));
            continue;
        }

        const u64 entry_end = addr + len;
        if (entry_end > limits.total_bytes) {
            limits.total_bytes = entry_end;
        }
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE && entry_end > limits.usable_bytes) {
            limits.usable_bytes = entry_end;
        }

        entry = (const multiboot_memory_map_t *)((u32)entry + entry->size + sizeof(entry->size));
    }

    return limits;
}

static struct memory_limits memory_limits_from_basic_info(const multiboot_info_t *mbinfo)
{
    struct memory_limits limits = {0, 0};
    if ((mbinfo->flags & MULTIBOOT_INFO_MEMORY) == 0) {
        return limits;
    }

    // mem_upper is reported in kilobytes starting at 1MB
    const u64 total_bytes = ((u64)mbinfo->mem_upper + 1024ull) << 10u;
    limits.total_bytes    = total_bytes;
    limits.usable_bytes   = total_bytes;
    return limits;
}


void init_physical_memory_limit(const multiboot_info_t *mbinfo)
{
    constexpr u32 heap_reserve = 128 * 1024 * 1024; // Leave 128MB of VA space for the kernel heap.

    struct memory_limits limits = memory_limits_from_mmap(mbinfo);
    enum phystop_source source  = PHYSTOP_SOURCE_MMAP;
    if (limits.total_bytes == 0 || limits.usable_bytes == 0) {
        limits = memory_limits_from_basic_info(mbinfo);
        source = PHYSTOP_SOURCE_BASIC;
    }
    if (limits.total_bytes == 0 || limits.usable_bytes == 0) {
        constexpr u32 fallback_limit = 0x20000000;
        limits.total_bytes           = fallback_limit;
        limits.usable_bytes          = fallback_limit;
        source                       = PHYSTOP_SOURCE_FALLBACK;
    }

    if (limits.total_bytes < limits.usable_bytes) {
        limits.total_bytes = limits.usable_bytes;
    }

    const u64 reported_total  = limits.total_bytes;
    const u64 reported_usable = limits.usable_bytes;

    constexpr u64 direct_map_window = (u64)MMIOBASE - KERNBASE;
    if (direct_map_window <= heap_reserve) {
        panic("Heap reserve too large");
    }
    constexpr u64 max_supported = direct_map_window - heap_reserve;
    bool total_clamped          = false;
    bool usable_clamped         = false;
    bool used_heap_reserve      = false;

    if (limits.total_bytes > max_supported) {
        limits.total_bytes = max_supported;
        total_clamped      = true;
        used_heap_reserve  = true;
    }
    if (limits.usable_bytes > max_supported) {
        limits.usable_bytes = max_supported;
        usable_clamped      = true;
        used_heap_reserve   = true;
    }

    u32 mapped32 = (u32)limits.total_bytes;
    u32 usable32 = (u32)limits.usable_bytes;

    u32 final_usable = PGROUNDDOWN(usable32);
    u32 final_total  = PGROUNDUP(mapped32);
    if (final_total > (u32)max_supported) {
        final_total = (u32)max_supported;
    }
    if (final_total < final_usable) {
        final_total = final_usable;
    }

    const u32 kernel_end_px = PGROUNDUP(V2P(kernel_end));
    if (final_usable <= kernel_end_px) {
        panic("Not enough usable RAM (0x%x bytes); kernel ends at 0x%x",
              final_usable,
              kernel_end_px);
    }

    phys_mem_top = final_total;
    phys_ram_end = final_usable;

    phystop_boot_state.source                  = source;
    phystop_boot_state.reported_total_bytes    = reported_total;
    phystop_boot_state.reported_usable_bytes   = reported_usable;
    phystop_boot_state.final_total_bytes       = final_total;
    phystop_boot_state.final_usable_bytes      = final_usable;
    phystop_boot_state.total_clamped           = total_clamped;
    phystop_boot_state.usable_clamped          = usable_clamped;
    phystop_boot_state.heap_reservation_active = used_heap_reserve;
    phystop_boot_state.heap_reserve_bytes      = heap_reserve;

    physmem_build_ranges(mbinfo, phys_ram_end);
}

void report_physical_memory_limit(void)
{
    const u32 reported_total_mb  = (u32)(phystop_boot_state.reported_total_bytes >> 20);
    const u32 reported_usable_mb = (u32)(phystop_boot_state.reported_usable_bytes >> 20);
    const u32 used_mb            = phystop_boot_state.final_usable_bytes >> 20;
    const u32 mapped_mb          = phystop_boot_state.final_total_bytes >> 20;

    switch (phystop_boot_state.source) {
    case PHYSTOP_SOURCE_MMAP:
        if (reported_total_mb == reported_usable_mb) {
            boot_message(WARNING_LEVEL_INFO,
                         "Multiboot memory map reports %u MB; using %u MB",
                         reported_total_mb,
                         used_mb);
        } else {
            boot_message(WARNING_LEVEL_INFO,
                         "Multiboot memory map reports %u MB total (%u MB usable); using %u MB",
                         reported_total_mb,
                         reported_usable_mb,
                         used_mb);
        }
        break;
    case PHYSTOP_SOURCE_BASIC:
        boot_message(WARNING_LEVEL_INFO,
                     "mem_upper reports %u MB; using %u MB",
                     reported_total_mb,
                     used_mb);
        break;
    case PHYSTOP_SOURCE_FALLBACK:
    default:
        boot_message(WARNING_LEVEL_WARNING,
                     "Bootloader did not supply memory info; defaulting to %u MB",
                     used_mb);
        break;
    }

    if (mapped_mb > used_mb) {
        boot_message(WARNING_LEVEL_INFO,
                     "Mapping %u MB to cover firmware and reserved regions",
                     mapped_mb);
    }

    if (phystop_boot_state.total_clamped || phystop_boot_state.usable_clamped) {
        boot_message(WARNING_LEVEL_WARNING,
                     "Limiting memory to %u MB to preserve MMIO space",
                     mapped_mb);
    }

    if (phystop_boot_state.heap_reservation_active) {
        boot_message(WARNING_LEVEL_INFO,
                     "Reserved %u MB of kernel VA space for heap growth",
                     phystop_boot_state.heap_reserve_bytes >> 20);
    }
}

static void release_range_cb(u32 start, u32 end, void *opaque)
{
    struct physmem_release_ctx *ctx = opaque;
    if (end <= start || end <= ctx->min_free) {
        return;
    }
    if (start < ctx->min_free) {
        start = ctx->min_free;
    }

    freerange(P2V(start), P2V(end));
}

void release_usable_memory_ranges(void)
{
    const u32 kernel_guard  = PGROUNDUP(V2P(kernel_end));
    constexpr u32 low_limit = 8 * 1024 * 1024;
    const u32 min_free      = kernel_guard > low_limit ? kernel_guard : low_limit;

    struct physmem_release_ctx ctx = {.min_free = min_free};
    physmem_for_each_range(release_range_cb, &ctx);
}
