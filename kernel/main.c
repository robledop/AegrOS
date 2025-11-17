#include <cpuid.h>
#include "assert.h"
#include "termcolors.h"
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
#include "mbr.h"
#include "pci.h"
#include "scheduler.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "mouse.h"
#include "x86.h"
#include <cpuid.h>

/** @brief Start the non-boot (AP) processors. */
static void startothers(void);
/** @brief Common CPU setup code. */
static void mpmain(void) __attribute__((noreturn));

void set_vbe_info(const multiboot_info_t *mbd);
static bool enable_sse(struct cpu *cpu);
#ifdef GRAPHICS
static void map_framebuffer_mmio(void);
#endif
/** @brief Kernel page directory */
extern pde_t *kpgdir;
/** @brief First address after kernel loaded from ELF file */
extern char end[]; // first address after kernel loaded from ELF file. See kernel.ld

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
int main(multiboot_info_t *mbinfo, [[maybe_unused]] unsigned int magic)
{
    ASSERT(magic == MULTIBOOT_BOOTLOADER_MAGIC, "Invalid multiboot magic number: 0x%x", magic);

#ifdef GRAPHICS
    set_vbe_info(mbinfo);
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
    map_framebuffer_mmio();
    framebuffer_init(); // framebuffer device
#endif
    mpinit();       // detect other processors
    lapicinit();    // interrupt controller
    seginit();      // segment descriptors
    picinit();      // disable pic
    ioapic_int();   // another interrupt controller
    console_init(); // console hardware
    uart_init();    // serial port
    cpu_print_info();
    pinit();                                    // process table
    tvinit();                                   // trap vectors
    binit();                                    // buffer cache
    file_init();                                // file table
    startothers();                              // start other processors
    kinit2(P2V(8 * 1024 * 1024), P2V(PHYSTOP)); // must come after startothers()
    kernel_enable_mmio_propagation();
    pci_scan();
    user_init(); // first user process
    // ps2_keyboard_init();
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

    boot_message(WARNING_LEVEL_INFO, "%d cpu%s", ncpu, ncpu == 1 ? "" : "s");

    for (struct cpu *c = cpus; c < cpus + ncpu; c++) {
        if (c == current_cpu()) // We've started already.
            continue;

        // Tell entryother.S what stack to use, where to enter, and what
        // pgdir to use. We cannot use kpgdir yet, because the AP processor
        // is running in low  memory, so we use entrypgdir for the APs too.
        char *stack                  = kalloc_page();
        *(void **)(code - 4)         = stack + KSTACKSIZE;
        *(void (**)(void))(code - 8) = mpenter;
        *(int **)(code - 12)         = (void *)V2P(entrypgdir);

        lapicstartap(c->apicid, V2P(code));

        // wait for cpu to finish mpmain()
        while (c->started == 0);
    }
}

void set_vbe_info(const multiboot_info_t *mbd)
{
    vbe_info->height      = mbd->framebuffer_height;
    vbe_info->width       = mbd->framebuffer_width;
    vbe_info->bpp         = mbd->framebuffer_bpp;
    vbe_info->pitch       = mbd->framebuffer_pitch;
    vbe_info->framebuffer = (u32)mbd->framebuffer_addr;
}

#ifdef GRAPHICS
static bool framebuffer_map_with_pat(const u32 fb_addr, const u32 fb_size)
{
    u32 eax, ebx, ecx, edx;
    cpuid(0x01, &eax, &ebx, &ecx, &edx);
    if ((edx & CPUID_FEAT_EDX_PAT) == 0) {
        return false;
    }

    u64 pat           = rdmsr(MSR_IA32_PAT);
    const u64 wc_mask = 0xFFull << 8; // PAT entry 1 (PWT=1, PCD=0)
    const u64 wc_val  = 0x01ull << 8; // Write-combining encoding
    if ((pat & wc_mask) != wc_val) {
        pat = (pat & ~wc_mask) | wc_val;
        wrmsr(MSR_IA32_PAT, pat);
    }

    kernel_map_mmio_wc(fb_addr, fb_size);
    boot_message(WARNING_LEVEL_INFO, "Framebuffer write-combining enabled (PAT)");
    framebuffer_enable_write_combining();
    return true;
}

/**
 * @brief Identity-map the framebuffer provided by Multiboot so VirtualBox VMs
 *        (which place VRAM at 0xE0000000) don't fault when we touch it.
 */
static void map_framebuffer_mmio(void)
{
    if (vbe_info->framebuffer == 0 || vbe_info->pitch == 0 || vbe_info->height == 0) {
        return;
    }

    const u32 fb_size = (u32)vbe_info->pitch * (u32)vbe_info->height;
    if (fb_size == 0) {
        return;
    }

    if (!framebuffer_map_with_pat(vbe_info->framebuffer, fb_size)) {
        kernel_map_mmio(vbe_info->framebuffer, fb_size);
    }
}
#endif

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

    return true;
}

// /**
//  * @brief Boot page table image used while bringing up processors.
//  *
//  * Page directories (and tables) must start on page boundaries, hence the
//  * alignment attribute. The large-page bit (PTE_PS) allows identity mapping of
//  * the first 4 MiB of physical memory.
//  */
// __attribute__ ((__aligned__
// (PGSIZE)
// )
// )
// pde_t entrypgdir[NPDENTRIES] = {
//     // Map VA's [0, 4MB) to PA's [0, 4MB)
//     [0] = (0) | PTE_P | PTE_W | PTE_PS,
//     // Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
//     [KERNBASE >> PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
// };

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
