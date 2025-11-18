#pragma once

#include "types.h"
#include "multiboot.h"

struct memory_limits
{
    u64 total_bytes;
    u64 usable_bytes;
};

enum phystop_source
{
    PHYSTOP_SOURCE_MMAP,
    PHYSTOP_SOURCE_BASIC,
    PHYSTOP_SOURCE_FALLBACK,
};

struct phystop_boot_info
{
    enum phystop_source source;
    u64 reported_total_bytes;
    u64 reported_usable_bytes;
    u32 final_total_bytes;
    u32 final_usable_bytes;
    bool total_clamped;
    bool usable_clamped;
    bool heap_reservation_active;
    u32 heap_reserve_bytes;
};

struct phys_mem_range
{
    u32 start;
    u32 end;
};

struct physmem_release_ctx
{
    u32 min_free;
};

typedef void (*physmem_range_callback)(u32 start, u32 end, void *ctx);

void physmem_build_ranges(const multiboot_info_t *mbinfo, u32 usable_limit);
void physmem_for_each_range(physmem_range_callback cb, void *ctx);
void init_physical_memory_limit(const multiboot_info_t *mbinfo);
void report_physical_memory_limit(void);
void release_usable_memory_ranges(void);
