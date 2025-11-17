#include "assert.h"
#include "debug.h"
#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "file.h"
#include "printf.h"
#include "string.h"

/** @brief End of kernel text; defined in linker script. */
extern char data[]; // defined by kernel.ld
/** @brief Kernel page directory shared across CPUs when idle. */
pde_t *kpgdir; // for use in scheduler()
u32 kpgdir_break;

extern struct ptable_t ptable;

#define MAX_KERNEL_MMIO_RANGES 16

struct kernel_mmio_range
{
    u32 start;
    u32 end;
};

static struct kernel_mmio_range kernel_mmio_ranges[MAX_KERNEL_MMIO_RANGES];
static int kernel_mmio_count;
static int mmio_propagation_enabled;

/**
 * @brief Replicate kernel mappings from kpgdir into another page directory.
 *
 * @param pgdir Target page directory.
 * @param start Starting virtual address of the kernel range.
 * @param end Ending virtual address of the kernel range.
 */
static void replicate_kernel_range(pde_t *pgdir, u32 start, u32 end)
{
    if (pgdir == nullptr || end <= start) {
        return;
    }

    start = PGROUNDDOWN(start);
    end   = PGROUNDUP(end);

    for (u32 va = start; va < end; va += (PGSIZE * NPTENTRIES)) {
        const u32 index = PDX(va);
        pgdir[index]    = kpgdir[index];
    }
}

/**
 * @brief Propagate kernel mappings to all active process page directories.
 *
 * @param start Starting virtual address of the kernel range.
 * @param end Ending virtual address of the kernel range.
 */
static void propagate_kernel_range(u32 start, u32 end)
{
    if (end <= start) {
        return;
    }

    start = PGROUNDDOWN(start);
    end   = PGROUNDUP(end);

    const int lock_ready = ptable.lock.name != nullptr;
    if (lock_ready) {
        acquire(&ptable.lock);
    }

    for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; ++p) {
        if (p->page_directory != nullptr && p->state != UNUSED) {
            for (u32 va = start; va < end; va += (PGSIZE * NPTENTRIES)) {
                const u32 index          = PDX(va);
                p->page_directory[index] = kpgdir[index];
            }
        }
    }
    if (lock_ready) {
        release(&ptable.lock);
    }

    struct proc *cur = current_process();
    if (cur != nullptr && cur->page_directory != nullptr) {
        lcr3(V2P(cur->page_directory));
    } else {
        switch_kernel_page_directory();
    }
}

/**
 * @brief Initialize the per-CPU segment descriptors.
 *
 * Must be invoked once on each CPU during startup to configure the GDT for
 * kernel and user segments before enabling interrupts.
 */
void seginit(void)
{
    // Map "logical" addresses to virtual addresses using the identity map.
    // Cannot share a CODE descriptor for both kernel and user
    // because it would have to have DPL_USR, but the CPU forbids
    // an interrupt from CPL=0 to DPL=3.
    struct cpu *c     = &cpus[cpu_index()];
    c->gdt[SEG_KCODE] = SEG(STA_X | STA_R, 0, 0xffffffff, 0);
    c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
    c->gdt[SEG_UCODE] = SEG(STA_X | STA_R, 0, 0xffffffff, DPL_USER);
    c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
    lgdt(c->gdt, sizeof(c->gdt));
}

/**
 * @brief Locate or optionally allocate the PTE for a virtual address.
 *
 * @param pgdir Page directory to search.
 * @param va Virtual address whose page-table entry is requested.
 * @param alloc When non-zero, allocate intermediate page tables on demand.
 * @return Pointer to the requested PTE, or 0 on allocation failure.
 */
static pte_t *walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
    pte_t *pgtab;

    pde_t *pde = &pgdir[PDX(va)];
    if (*pde & PTE_P) {
        pgtab = (pte_t *)P2V(PTE_ADDR(*pde));
    } else {
        if (!alloc || (pgtab = (pte_t *)kalloc_page()) == nullptr) {
            return nullptr;
        }
        // Make sure all those PTE_P bits are zero.
        memset(pgtab, 0, PGSIZE);
        // The permissions here are overly generous, but they can
        // be further restricted by the permissions in the page table
        // entries, if necessary.
        *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
    }
    return &pgtab[PTX(va)];
}

/**
 * @brief Map a range of virtual addresses to physical memory.
 *
 * @param pgdir Page directory to modify.
 * @param va Starting virtual address; need not be page-aligned.
 * @param size Size in bytes of the region to map.
 * @param pa Starting physical address.
 * @param perm Permission bits to set on each mapping.
 * @return 0 on success or -1 if allocation fails.
 */
static int mappages(pde_t *pgdir, void *va, u32 size, u32 pa, int perm)
{
    pte_t *pte;

    const char *a    = (char *)PGROUNDDOWN((u32)va);
    const char *last = (char *)PGROUNDDOWN(((u32)va) + size - 1);
    for (;;) {
        if ((pte = walkpgdir(pgdir, a, 1)) == nullptr) {
            return -1;
        }
        if (*pte & PTE_P) {
            panic("remap");
        }
        *pte = pa | perm | PTE_P;
        if (a == last) {
            break;
        }
        a += PGSIZE;
        pa += PGSIZE;
    }
    return 0;
}

int map_physical_range(pde_t *pgdir, u32 va, u32 pa, u32 size, int perm)
{
    if ((va & (PGSIZE - 1)) != 0 || (pa & (PGSIZE - 1)) != 0) {
        panic("map_physical_range: unaligned");
    }
    return mappages(pgdir, (void *)va, size, pa, perm);
}

/**
 * @brief Identity-map an MMIO range into the kernel page directory and propagate it.
 *
 * @param pa Physical address of the MMIO region (must be page-aligned).
 * @param size Size in bytes of the region to map.
 */
static void kernel_map_mmio_range(u32 pa, u32 size, u32 flags)
{
    if (size == 0) {
        return;
    }

    u32 start = PGROUNDDOWN(pa);
    u32 end   = PGROUNDUP(pa + size);

    for (u32 a = start; a < end; a += PGSIZE) {
        pte_t *pte = walkpgdir(kpgdir, (void *)a, 0);
        if (pte != nullptr && (*pte & PTE_P) != 0) {
            continue;
        }
        if (mappages(kpgdir, (void *)a, PGSIZE, a, flags) < 0) {
            panic("kernel_map_mmio: mappages failed");
        }
    }

    if (mmio_propagation_enabled) {
        propagate_kernel_range(start, end);
    } else {
        switch_kernel_page_directory();
    }

    // Record the MMIO range so future page directories inherit the mapping.
    int merged = 0;
    for (int i = 0; i < kernel_mmio_count; ++i) {
        u32 range_start = kernel_mmio_ranges[i].start;
        u32 range_end   = kernel_mmio_ranges[i].end;
        if (start >= range_start && end <= range_end) {
            merged = 1;
            break;
        }
        if (start <= range_end && end >= range_start) {
            if (start < range_start) {
                kernel_mmio_ranges[i].start = start;
            }
            if (end > range_end) {
                kernel_mmio_ranges[i].end = end;
            }
            merged = 1;
            break;
        }
    }

    if (!merged) {
        if (kernel_mmio_count >= MAX_KERNEL_MMIO_RANGES) {
            panic("kernel_map_mmio: too many ranges");
        }
        kernel_mmio_ranges[kernel_mmio_count].start = start;
        kernel_mmio_ranges[kernel_mmio_count].end   = end;
        kernel_mmio_count++;
    }
}

void kernel_enable_mmio_propagation(void)
{
    mmio_propagation_enabled = 1;
}

void kernel_map_mmio(u32 pa, u32 size)
{
    kernel_map_mmio_range(pa, size, PTE_W | PTE_PCD | PTE_PWT);
}

void kernel_map_mmio_wc(u32 pa, u32 size)
{
    kernel_map_mmio_range(pa, size, PTE_W | PTE_PWT | PTE_PAT);
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

/** @brief Static kernel mapping template present in every page directory. */
static struct kmap
{
    void *virt;
    u32 phys_start;
    u32 phys_end;
    int perm;
} kmap[] = {
    {(void *)KERNBASE, 0, EXTMEM, PTE_W},                       // I/O space
    {(void *)KERNLINK, V2P(KERNLINK), V2P(data), 0},            // kern text+rodata
    {(void *)data, V2P(data), PHYSTOP, PTE_W},                  // kern data+memory
    {(void *)MMIOBASE, MMIOBASE, 0, PTE_W | PTE_PCD | PTE_PWT}, // MMIO devices (framebuffer, lapic, etc.)
};

/**
 * @brief Build the kernel portion of a new page directory.
 *
 * @return Pointer to the initialized page directory or 0 on failure.
 */
pde_t *setup_kernel_page_directory(void)
{
    pde_t *pgdir;

    if ((pgdir = (pde_t *)kalloc_page()) == nullptr) {
        return nullptr;
    }
    memset(pgdir, 0, PGSIZE);

#if (PHYSTOP + KERNBASE) > MMIOBASE
    panic("PHYSTOP too high");
#endif

    for (const struct kmap *k = kmap; k < &kmap[NELEM(kmap)]; k++) {
        if (mappages(pgdir,
                     k->virt,
                     k->phys_end - k->phys_start,
                     (u32)k->phys_start,
                     k->perm) < 0) {
            freevm(pgdir);
            return nullptr;
        }
    }

    if (kpgdir != nullptr) {
        replicate_kernel_range(pgdir, (u32)KHEAP_START, kpgdir_break);
        for (int i = 0; i < kernel_mmio_count; ++i) {
            replicate_kernel_range(pgdir, kernel_mmio_ranges[i].start, kernel_mmio_ranges[i].end);
        }
    }

    return pgdir;
}

/** @brief Allocate the kernel page directory and activate it. */
void kernel_page_directory_init(void)
{
    kpgdir       = setup_kernel_page_directory();
    kpgdir_break = (uptr)KHEAP_START;
    switch_kernel_page_directory();
}

/** @brief Switch to the kernel-only page table for the idle CPU. */
void switch_kernel_page_directory(void)
{
    lcr3(V2P(kpgdir)); // switch to the kernel page table
}

/**
 * @brief Grow or shrink a page directory's address space.
 *
 * @param n Positive delta to grow, negative to shrink.
 * @return 0 on success, -1 on failure.
 */
u32 resize_kernel_page_directory(int n)
{
    u32 sz        = kpgdir_break;
    u32 old_break = kpgdir_break;
    if (n > 0) {
        u32 requested = sz + (u32)n;
        if (requested < sz) {
            return -1;
        }
        if ((sz = allocvm(kpgdir, sz, requested, PTE_W)) == 0) {
            return -1;
        }
        propagate_kernel_range(old_break, sz);
    } else if (n < 0) {
        u32 delta = (u32)(-n);
        if (delta > sz) {
            return -1;
        }
        u32 target = sz - delta;
        if ((sz = deallocvm(kpgdir, sz, target)) == 0) {
            return -1;
        }
        propagate_kernel_range(sz, old_break);
    } else {
        return 0;
    }

    kpgdir_break = sz;
    return old_break;
}

/**
 * @brief Activate a process's address space and task state.
 *
 * @param p Process whose page table and TSS should become active.
 */
void activate_process(struct proc *p)
{
    ASSERT(p != nullptr, "activate_process: no process");
    ASSERT(p->kstack != nullptr, "activate_process: no kstack");
    ASSERT(p->page_directory != nullptr, "activate_process: no pgdir");

    pushcli();
    current_cpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &current_cpu()->task_state, sizeof(current_cpu()->task_state) - 1, 0);
    current_cpu()->gdt[SEG_TSS].s = 0;
    current_cpu()->task_state.ss0 = SEG_KDATA << 3;
    current_cpu()->task_state.esp0 = (u32)p->kstack + KSTACKSIZE;
    // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
    // forbids I/O instructions (e.g., inb and outb) from user space
    current_cpu()->task_state.iomb = (u16)0xFFFF;
    ltr(SEG_TSS << 3);
    lcr3(V2P(p->page_directory)); // switch to the process's address space
    popcli();
}

/**
 * @brief Load the initcode into the first page of a page directory.
 *
 * @param pgdir Destination page directory.
 * @param init Pointer to the initcode image.
 * @param sz Size of the image in bytes; must be less than a page.
 */
void inituvm(pde_t *pgdir, const char *init, u32 sz)
{
    ASSERT(sz < PGSIZE, "inituvm: more than a page");

    char *mem = kalloc_page();
    memset(mem, 0, PGSIZE);
    mappages(pgdir, nullptr, PGSIZE, V2P(mem), PTE_W | PTE_U);
    memmove(mem, init, sz);
}

/**
 * @brief Load a program segment from the disk into memory.
 *
 * @param pgdir Destination page directory.
 * @param addr Virtual address of the mapped destination region.
 * @param ip Inode providing the segment contents.
 * @param offset Byte offset into the file.
 * @param sz Number of bytes to read.
 * @return 0 on success, -1 if disk I/O fails.
 */
int loaduvm(pde_t *pgdir, char *addr, struct inode *ip, u32 offset, u32 sz)
{
    if (sz == 0) {
        return 0;
    }

    u32 va       = (u32)addr;
    u32 pagebase = PGROUNDDOWN(va);
    u32 pageoff  = va - pagebase;
    u32 copied   = 0;

    while (copied < sz) {
        pte_t *pte = walkpgdir(pgdir, (char *)pagebase, 0);
        if (pte == nullptr || (*pte & PTE_P) == 0) {
            panic("loaduvm: address should exist");
        }
        u32 pa     = PTE_ADDR(*pte);
        char *dest = (char *)P2V(pa) + pageoff;

        u32 chunk = PGSIZE - pageoff;
        if (chunk > sz - copied) {
            chunk = sz - copied;
        }

        if (ip->iops->readi(ip, (char *)dest, offset + copied, chunk) != (int)chunk) {
            return -1;
        }

        copied += chunk;
        pagebase += PGSIZE;
        pageoff = 0;
    }
    return 0;
}

/**
 * @brief Grow a page directory's address space.
 *
 * Allocates physical pages and maps them into the supplied page directory.
 *
 * @param pgdir Page directory to extend.
 * @param oldsz Current size in bytes.
 * @param newsz Requested new size in bytes.
 * @param perm Permission bits to set on each new mapping.
 * @return The resulting size on success, or 0 on failure.
 */
int allocvm(pde_t *pgdir, u32 oldsz, u32 newsz, int perm)
{
    if (newsz >= KERNBASE && perm & PTE_U) {
        return 0;
    }
    if (newsz < oldsz) {
        return (int)oldsz;
    }

    for (u32 a = PGROUNDUP(oldsz); a < newsz; a += PGSIZE) {
        char *mem = kalloc_page();
        if (mem == nullptr) {
            printf("allocvm out of memory\n");
            deallocvm(pgdir, newsz, oldsz);
            return 0;
        }
        memset(mem, 0, PGSIZE);
        if (mappages(pgdir, (char *)a, PGSIZE, V2P(mem), perm) < 0) {
            printf("allocvm out of memory (2)\n");
            deallocvm(pgdir, newsz, oldsz);
            kfree_page(mem);
            return 0;
        }
    }
    return (int)newsz;
}

/**
 * @brief Shrink a process's address space.
 *
 * Frees user pages between @p newsz and @p oldsz.
 *
 * @param pgdir Page directory to trim.
 * @param oldsz Current size in bytes.
 * @param newsz Desired size in bytes.
 * @return The resulting size after deallocation.
 */
u32 deallocvm(pde_t *pgdir, u32 oldsz, u32 newsz)
{
    if (newsz >= oldsz) {
        return oldsz;
    }

    for (u32 a = PGROUNDUP(newsz); a < oldsz; a += PGSIZE) {
        pte_t *pte = walkpgdir(pgdir, (char *)a, 0);
        if (!pte) {
            a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
        } else if ((*pte & PTE_P) != 0) {
            u32 pa = PTE_ADDR(*pte);
            if (pa == 0) {
                panic("kfree");
            }
            char *v = P2V(pa);
            kfree_page(v);
            *pte = 0;
        }
    }
    return newsz;
}

/** @brief Free a user page table and all associated physical pages. */
void freevm(pde_t *pgdir)
{
    if (pgdir == nullptr) {
        panic("freevm: no pgdir");
    }
    deallocvm(pgdir, KERNBASE, 0);
    for (u32 i = 0; i < NPDENTRIES; i++) {
        if (pgdir[i] & PTE_P) {
            u32 va = i << PDXSHIFT;
            if (va >= (u32)KHEAP_START) {
                continue;
            }
            char *v = P2V(PTE_ADDR(pgdir[i]));
            kfree_page(v);
        }
    }
    kfree_page((char *)pgdir);
}

void unmap_vm_range(pde_t *pgdir, u32 start, u32 end, int free_frames)
{
    if (pgdir == nullptr || start >= end) {
        return;
    }

    start = PGROUNDDOWN(start);
    end   = PGROUNDDOWN(end + PGSIZE - 1);

    for (u32 a = start; a <= end; a += PGSIZE) {
        pte_t *pte = walkpgdir(pgdir, (char *)a, 0);
        if (pte == nullptr) {
            a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
            continue;
        }
        if ((*pte & PTE_P) == 0) {
            continue;
        }
        if (free_frames) {
            u32 pa = PTE_ADDR(*pte);
            if (pa == 0) {
                panic("unmap_vm_range: zero pa");
            }
            if (pa < PHYSTOP) {
                char *v = P2V(pa);
                kfree_page(v);
            }
        }
        *pte = 0;
    }
}

/**
 * @brief Clear the user-accessible bit on a page-table entry.
 *
 * @param pgdir Page directory containing the mapping.
 * @param uva User virtual address whose entry should become supervisor-only.
 */
void clearpteu(pde_t *pgdir, const char *uva)
{
    pte_t *pte = walkpgdir(pgdir, uva, 0);
    if (pte == nullptr) {
        panic("clearpteu");
    }
    *pte &= ~PTE_U;
}

/**
 * @brief Clone a process address space for fork().
 *
 * @param pgdir Parent page directory.
 * @param sz Size in bytes of the address space to copy.
 * @return Newly allocated page directory on success, or 0 on failure.
 */
pde_t *copyuvm(pde_t *pgdir, u32 sz)
{
    pde_t *d;
    if ((d = setup_kernel_page_directory()) == nullptr) {
        return nullptr;
    }

    for (u32 i = 0; i < sz; i += PGSIZE) {
        pte_t *pte;
        if ((pte = walkpgdir(pgdir, (void *)i, 0)) == nullptr) {
            panic("copyuvm: pte should exist");
        }
        if (!(*pte & PTE_P)) {
            panic("copyuvm: page not present");
        }

        char *mem;
        if ((mem = kalloc_page()) == nullptr) {
            goto bad;
        }

        u32 pa    = PTE_ADDR(*pte);
        int flags = PTE_FLAGS(*pte);
        memmove(mem, (char *)P2V(pa), PGSIZE);
        if (mappages(d, (void *)i, PGSIZE, V2P(mem), flags) < 0) {
            kfree_page(mem);
            goto bad;
        }
    }
    return d;

bad:
    freevm(d);
    return nullptr;
}

/**
 * @brief Translate a user virtual address to a kernel-mapped pointer.
 *
 * @param pgdir Page directory containing the mapping.
 * @param uva User virtual address.
 * @return Kernel virtual address if accessible, otherwise 0.
 */
char *uva2ka(pde_t *pgdir, char *uva)
{
    pte_t *pte = walkpgdir(pgdir, uva, 0);
    if ((*pte & PTE_P) == 0)
        return nullptr;
    if ((*pte & PTE_U) == 0)
        return nullptr;
    return (char *)P2V(PTE_ADDR(*pte));
}

/**
 * @brief Copy data from kernel space to user memory.
 *
 * @param pgdir Target page directory.
 * @param va User virtual address to begin writing.
 * @param p Source buffer in kernel space.
 * @param len Number of bytes to copy.
 * @return 0 on success, -1 if a mapping is inaccessible.
 */
int copyout(pde_t *pgdir, u32 va, void *p, u32 len)
{
    char *buf = (char *)p;
    while (len > 0) {
        u32 va0   = (u32)PGROUNDDOWN(va);
        char *pa0 = uva2ka(pgdir, (char *)va0);
        if (pa0 == nullptr)
            return -1;
        u32 n = PGSIZE - (va - va0);
        if (n > len)
            n = len;
        memmove(pa0 + (va - va0), buf, n);
        len -= n;
        buf += n;
        va = va0 + PGSIZE;
    }
    return 0;
}
