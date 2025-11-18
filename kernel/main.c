#include "assert.h"
#include "types.h"
#include "string.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "multiboot.h"
#include "debug.h"
#include "pci.h"
#include "scheduler.h"
#include "framebuffer.h"
#include "mouse.h"
#include "physmem.h"

/** @brief Start the non-boot (AP) processors. */
static void startothers(void);
/** @brief Common CPU setup code. */
static void mpmain(void) __attribute__((noreturn));

static bool enable_sse(struct cpu *cpu);
/** @brief Kernel page directory */
extern pde_t *kpgdir;
u32 boot_config_table_ptr;

#define STACK_CHK_GUARD 0xe2dee396
u32 __stack_chk_guard = STACK_CHK_GUARD; // NOLINT(*-reserved-identifier)


/**
 * @brief Bootstrap processor entry point.
 *
 * Allocates an initial stack, sets up essential subsystems, and
 * launches the first user process before transitioning into the scheduler.
 *
 * @return This function does not return; it hands control to the scheduler.
 */
NO_SSE int main(multiboot_info_t *mbinfo, [[maybe_unused]] unsigned int magic)
{
    ASSERT(magic == MULTIBOOT_BOOTLOADER_MAGIC, "Invalid multiboot magic number: 0x%x", magic);
    if ((mbinfo->flags & MULTIBOOT_INFO_CONFIG_TABLE) != 0 && mbinfo->config_table != 0) {
        boot_config_table_ptr = mbinfo->config_table;
    } else {
        boot_config_table_ptr = 0;
    }
    init_physical_memory_limit(mbinfo);

#ifdef GRAPHICS
    framebuffer_set_vbe_info(mbinfo);
#endif

    init_symbols(mbinfo);
    kinit1(debug_reserved_end(), P2V(8 * 1024 * 1024)); // phys page allocator for kernel
    kernel_page_directory_init();                       // kernel page table
    if (enable_sse(&cpus[0])) {
        memory_enable_sse();
        if (cpus[0].has_avx) {
            memory_enable_avx();
        } else {
            memory_disable_avx();
        }
    }
#ifdef GRAPHICS
    framebuffer_map_boot_framebuffer(&cpus[0]);
    framebuffer_init(); // framebuffer device
#endif
    mpinit();       // detect other processors
    lapicinit();    // interrupt controller
    seginit();      // segment descriptors
    picinit();      // disable pic
    ioapic_int();   // another interrupt controller
    console_init(); // console hardware
    report_physical_memory_limit();
    uart_init(); // serial port
    mp_report_state();
    cpu_print_info();
    pinit();       // process table
    tvinit();      // trap vectors
    binit();       // buffer cache
    file_init();   // file table
    startothers(); // start other processors
    release_usable_memory_ranges();
    kalloc_enable_locking(); // enable allocator locking after free lists are built
    kernel_enable_mmio_propagation();
    pci_scan();
    user_init(); // first user process
    mouse_init(nullptr);
    mpmain(); // finish this processor's setup
}

/**
 * @brief Application processor entry point used by entryother.S.
 *
 * Switches to the kernel's page tables and continues CPU initialization
 * via mpmain.
 */
static void mpenter(void)
{
    switch_kernel_page_directory();
    seginit();
    lapicinit();
    mpmain();
}

/**
 * @brief Complete per-CPU initialization and enter the scheduler.
 */
static void mpmain(void)
{
    enable_sse(current_cpu());
    // boot_message(WARNING_LEVEL_INFO, "cpu%d: starting %d", cpu_index(), cpu_index());
    idtinit();                          // load idt register
    xchg(&(current_cpu()->started), 1); // tell startothers() we're up
    scheduler();                        // start running processes
}

/** @brief Boot-time page directory referenced from assembly. */
pde_t entrypgdir[]; // For entry.asm

static bool wait_for_ap_start(struct cpu *cpu, u32 timeout_us)
{
    const u32 poll_interval_us = 100;
    for (u32 waited = 0; waited < timeout_us; waited += poll_interval_us) {
        if (cpu->started != 0) {
            return true;
        }
        microdelay((int)poll_interval_us);
    }
    return false;
}

static bool bringup_application_processor(struct cpu *cpu, u8 *code)
{
    char *stack = kalloc_page();
    if (stack == nullptr) {
        panic("startothers: failed to allocate AP stack");
    }
    cpu->boot_stack              = stack;
    *(void **)(code - 4)         = stack + KSTACKSIZE;
    *(void (**)(void))(code - 8) = mpenter;
    *(int **)(code - 12)         = (void *)V2P(entrypgdir);

    const u32 timeout_us = 1000000; // Allow up to ~1s for each AP to signal readiness.
    for (int attempt = 0; attempt < 2; ++attempt) {
        lapicstartap(cpu->apicid, V2P(code));
        if (wait_for_ap_start(cpu, timeout_us)) {
            return true;
        }
        microdelay(1000);
    }

    return false;
}

static void disable_cpu_slot(int index)
{
    if (index < 0 || index >= ncpu) {
        return;
    }
    struct cpu *victim = &cpus[index];
    if (victim->boot_stack != nullptr) {
        kfree_page(victim->boot_stack);
        victim->boot_stack = nullptr;
    }
    if (index != ncpu - 1) {
        cpus[index] = cpus[ncpu - 1];
    }
    memset(&cpus[ncpu - 1], 0, sizeof(struct cpu));
    --ncpu;
}

/**
 * @brief Start all application processors (APs).
 *
 * Copies the AP bootstrap code to low memory, provides each processor with a
 * stack, entry point, and temporary page directory, then issues INIT/SIPI
 * sequences until every CPU reports as started.
 */
static void startothers(void)
{
    // This name depends on the path of the entryohter file.
    extern u8 _binary_build_x86_entryother_start[], _binary_build_x86_entryother_size[]; // NOLINT(*-reserved-identifier)

    // Write entry code to unused memory at 0x7000.
    // The linker has placed the image of entryother.S in
    // _binary_entryother_start.
    u8 *code = P2V(0x7000);
    memmove(code, _binary_build_x86_entryother_start, (u32)_binary_build_x86_entryother_size);

    const int initial_cpu_count = ncpu;
    const u8 bsp_apicid         = lapicid();
    boot_message(WARNING_LEVEL_INFO, "%d cpu%s", initial_cpu_count, initial_cpu_count == 1 ? "" : "s");

    for (int idx = 0; idx < ncpu; ++idx) {
        struct cpu *cpu = &cpus[idx];
        if (cpu->apicid == bsp_apicid) {
            continue; // BSP already running.
        }

        if (bringup_application_processor(cpu, code)) {
            continue;
        }

        boot_message(WARNING_LEVEL_WARNING,
                     "CPU with APIC ID %u failed to start; disabling it",
                     cpu->apicid);
        disable_cpu_slot(idx);
        idx--; // Retry this slot after collapsing the array.
    }

    if (ncpu != initial_cpu_count) {
        boot_message(WARNING_LEVEL_WARNING,
                     "Proceeding with %d CPU(s) after disabling %d that failed to start",
                     ncpu,
                     initial_cpu_count - ncpu);
    }
}

static bool enable_sse(struct cpu *cpu)
{
    u32 eax, ebx, ecx, edx;
    cpuid(0x01, &eax, &ebx, &ecx, &edx);
    if ((edx & CPUID_FEAT_EDX_SSE) == 0) {
        boot_message(WARNING_LEVEL_WARNING, "CPU lacks SSE support");
        return false;
    }
    if ((edx & CPUID_FEAT_EDX_SSE2) == 0) {
        boot_message(WARNING_LEVEL_WARNING, "CPU lacks SSE2 support");
        return false;
    }

    u32 cr0 = rcr0();
    cr0 &= ~CR0_EM;
    cr0 |= CR0_MP | CR0_NE;
    lcr0(cr0);

    cpu->xsave_features_low  = 0;
    cpu->xsave_features_high = 0;
    cpu->xsave_area_size     = 0;
    cpu->has_xsave           = false;
    cpu->has_avx             = false;

    u32 cr4 = rcr4();
    cr4 |= CR4_OSFXSR | CR4_OSXMMEXCPT;

    const bool has_xsave = (ecx & CPUID_FEAT_ECX_XSAVE) != 0;
    if (has_xsave) {
        cr4 |= CR4_OSXSAVE;
    }
    lcr4(cr4);

    __asm__ volatile("fninit");

    if (has_xsave) {
        u64 xcr0_mask      = XCR0_X87 | XCR0_SSE;
        const bool has_avx = (ecx & CPUID_FEAT_ECX_AVX) != 0;
        if (has_avx) {
            xcr0_mask |= XCR0_AVX;
            cpu->has_avx = true;
        } else {
            cpu->has_avx = false;
        }

        xsetbv(0, (u32)xcr0_mask, (u32)(xcr0_mask >> 32));

        u32 xax, xbx, xcx, xdx;
        cpuid_count(0x0D, 0, &xax, &xbx, &xcx, &xdx);
        if (xax <= sizeof(((struct proc *)0)->fpu_state)) {
            cpu->xsave_features_low  = (u32)xcr0_mask;
            cpu->xsave_features_high = (u32)(xcr0_mask >> 32);
            cpu->xsave_area_size     = xax;
            cpu->has_xsave           = true;
        } else {
            boot_message(WARNING_LEVEL_WARNING,
                         "XSAVE area too large (%u bytes); falling back to FXSAVE",
                         xax);
        }
    } else {
        cpu->has_avx   = false;
        cpu->has_xsave = false;
    }

    framebuffer_prepare_cpu(cpu);
    return true;
}

/** @brief Boot-time page directory referenced from assembly.
 * Boot-time page directory used while paging is being enabled.
 *
 * Map the first 8 MiB of physical memory twice: once at VA 0 so we can keep
 * running the low-level bootstrap code, and once at KERNBASE so the kernel can
 * execute from its linked virtual addresses (0x80100000 and above).
 */
__attribute__((aligned(PGSIZE)))
u32 entrypgdir[NPDENTRIES] = {
    [0] = PTE_P | PTE_W | PTE_PS,                   // maps 0x0000_0000 → 0x0000_0000
    [1] = (1 << PDXSHIFT) | PTE_P | PTE_W | PTE_PS, // maps 0x0040_0000 → 0x0040_0000
    [KERNBASE >>
        PDXSHIFT] = PTE_P | PTE_W | PTE_PS, // maps 0x8000_0000 → phys 0
    [(KERNBASE >> PDXSHIFT) + 1] = (1 << PDXSHIFT) | PTE_P | PTE_W | PTE_PS,
};
