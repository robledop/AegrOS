#include <assert.h>
#include <kernel.h>
#include <memory.h>
#include <paging.h>
#include <x86.h>
#include "kernel_heap.h"
#include "serial.h"
#include "spinlock.h"
#include "status.h"

// https://wiki.osdev.org/Paging

struct page_directory *kernel_page_directory = nullptr;
static uint32_t *current_page_directory      = nullptr;

bool paging_is_video_memory(const uint32_t address)
{
    return address >= 0xB8000 && address <= 0xBFFFF;
}
/// @brief Set the kernel mode segments and switch to the kernel page directory
void kernel_page()
{
    set_kernel_mode_segments();
    paging_switch_directory(kernel_page_directory);
}

struct page_directory *paging_create_directory(const uint8_t flags)
{
    uint32_t *directory_entry = kzalloc(sizeof(uint32_t) * PAGING_ENTRIES_PER_DIRECTORY);
    if (!directory_entry) {
        panic("Failed to allocate page directory\n");
        return nullptr;
    }

    uint32_t offset = 0;
    for (size_t i = 0; i < PAGING_ENTRIES_PER_TABLE; i++) {
        // The table is freed in paging_free_directory
        // ReSharper disable once CppDFAMemoryLeak
        uint32_t *table = kzalloc(sizeof(uint32_t) * PAGING_ENTRIES_PER_TABLE);
        if (!table) {
            panic("Failed to allocate page table\n");
            return nullptr;
        }

        for (size_t j = 0; j < PAGING_ENTRIES_PER_TABLE; j++) {
            table[j] = (offset + (j * PAGING_PAGE_SIZE)) | flags;
        }
        offset             = offset + (PAGING_ENTRIES_PER_TABLE * PAGING_PAGE_SIZE);
        directory_entry[i] = (uint32_t)table | flags | PDE_IS_WRITABLE;
    }

    struct page_directory *directory = kzalloc(sizeof(struct page_directory));
    if (!directory) {
        panic("Failed to allocate page directory\n");
        // ReSharper disable once CppDFAMemoryLeak
        return nullptr;
    }

    directory->directory_entry = directory_entry;
    dbgprintf("Page directory allocated at %x\n", directory->directory_entry);
    // ReSharper disable once CppDFAMemoryLeak
    return directory;
}

void paging_free_directory(struct page_directory *page_directory)
{
    for (int i = 0; i < PAGING_ENTRIES_PER_DIRECTORY; i++) {
        const uint32_t entry = page_directory->directory_entry[i];
        auto const table     = (uint32_t *)(entry & 0xfffff000);
        kfree(table);
    }

    kfree(page_directory->directory_entry);
    kfree(page_directory);
}

void paging_switch_directory(const struct page_directory *directory)
{
    ASSERT(directory->directory_entry);

    if (current_page_directory == directory->directory_entry) {
        return;
    }

    lcr3((uint32_t)directory->directory_entry);
    current_page_directory = directory->directory_entry;
}

uint32_t *paging_get_directory(const struct page_directory *directory)
{
    return directory->directory_entry;
}

bool paging_is_aligned(void *address)
{
    return ((uint32_t)address % PAGING_PAGE_SIZE) == 0;
}

int paging_get_indexes(void *virtual_address, uint32_t *directory_index_out, uint32_t *table_index_out)
{
    int res = 0;
    if (!paging_is_aligned(virtual_address)) {
        ASSERT(false, "Virtual address is not aligned");
        res = -EINVARG;
        goto out;
    }

    *directory_index_out = ((uint32_t)virtual_address / (PAGING_ENTRIES_PER_TABLE * PAGING_PAGE_SIZE));
    *table_index_out = ((uint32_t)virtual_address % (PAGING_ENTRIES_PER_TABLE * PAGING_PAGE_SIZE)) / PAGING_PAGE_SIZE;

out:
    return res;
}

/// @brief Align the address to the next page
void *paging_align_address(void *address)
{
    if (paging_is_aligned(address)) {
        return address;
    }

    return (void *)((uint32_t)address + PAGING_PAGE_SIZE - ((uint32_t)address % PAGING_PAGE_SIZE));
}

/// @brief Align the address to the lower page
void *paging_align_to_lower_page(void *address)
{
    return (void *)((uint32_t)address - ((uint32_t)address % PAGING_PAGE_SIZE));
}

int paging_kernel_map(void *virtual_address, void *physical_address, const int flags)
{
    return paging_map(kernel_page_directory, virtual_address, physical_address, flags);
}

int paging_kernel_map_range(void *virtual_address, void *physical_start_address, const int total_pages, const int flags)
{
    return paging_map_range(kernel_page_directory, virtual_address, physical_start_address, total_pages, flags);
}

int paging_map(const struct page_directory *directory, void *virtual_address, void *physical_address, const int flags)
{
    ASSERT(!paging_is_video_memory((uint32_t)physical_address), "Trying to map video memory");
    dbgprintf("Mapping virtual address %x to physical address %x\n", virtual_address, physical_address);

    if (!paging_is_aligned(virtual_address)) {
        warningf("Virtual address %x is not page aligned\n", (uint32_t)virtual_address);
        ASSERT(false, "Virtual address is not aligned");
        return -EINVARG;
    }

    if (!paging_is_aligned(physical_address)) {
        warningf("Physical address %x is not page aligned\n", (uint32_t)physical_address);
        ASSERT(false, "Physical address is not aligned");
        return -EINVARG;
    }

    return paging_set(directory, virtual_address, (uint32_t)physical_address | flags);
}

int paging_map_range(const struct page_directory *directory, void *virtual_address, void *physical_start_address,
                     const int total_pages, const int flags)
{
    ASSERT(!paging_is_video_memory((uint32_t)physical_start_address), "Trying to map video memory");

    void *current_virtual  = virtual_address;
    void *current_physical = physical_start_address;

    for (int mapped_pages = 0; mapped_pages < total_pages; mapped_pages++) {
        const int res = paging_map(directory, current_virtual, current_physical, flags);
        if (res < 0) {
            warningf("Failed to map page %d\n", mapped_pages);
            for (int rollback = mapped_pages - 1; rollback >= 0; rollback--) {
                void *rollback_virtual  = (char *)virtual_address + (rollback * PAGING_PAGE_SIZE);
                void *rollback_physical = (char *)physical_start_address + (rollback * PAGING_PAGE_SIZE);
                paging_map(directory, rollback_virtual, rollback_physical, PDE_UNMAPPED);
            }
            ASSERT(false, "Failed to map page");
            return res;
        }

        current_virtual  = (char *)current_virtual + PAGING_PAGE_SIZE;
        current_physical = (char *)current_physical + PAGING_PAGE_SIZE;
    }

    return 0;
}

int paging_map_to(const struct page_directory *directory, void *virtual_address, void *physical_start_address,
                  void *physical_end_address, const int flags)
{
    ASSERT(!paging_is_video_memory((uint32_t)physical_start_address), "Trying to map video memory");
    ASSERT(!paging_is_video_memory((uint32_t)physical_end_address), "Trying to map video memory");
    ASSERT((uint32_t)virtual_address % PAGING_PAGE_SIZE == 0, "Virtual address is not aligned");
    ASSERT(paging_is_aligned(physical_start_address), "Physical start address is not aligned");
    ASSERT(paging_is_aligned(physical_end_address), "Physical end address is not aligned");
    ASSERT((uint32_t)physical_end_address >= (uint32_t)physical_start_address,
           "Physical end address is less than physical start address");

    int res               = 0;
    const int total_bytes = (char *)physical_end_address - (char *)physical_start_address;
    const int total_pages = total_bytes / PAGING_PAGE_SIZE;
    res                   = paging_map_range(directory, virtual_address, physical_start_address, total_pages, flags);

    return res;
}

void *paging_get_physical_address(const struct page_directory *directory, void *virtual_address)
{
    auto const virt_address_new = (void *)paging_align_to_lower_page(virtual_address);
    auto const offset           = (void *)((uint32_t)virtual_address - (uint32_t)virt_address_new);

    return (void *)((paging_get(directory, virt_address_new) & 0xFFFFF000) + (uint32_t)offset);
}

/// @brief Get the physical address of a page
uint32_t paging_get(const struct page_directory *directory, void *virtual_address)
{
    uint32_t directory_index = 0;
    uint32_t table_index     = 0;
    const int res            = paging_get_indexes(virtual_address, &directory_index, &table_index);
    if (res < 0) {
        ASSERT(false, "Failed to get indexes");
        return 0;
    }

    const uint32_t entry  = directory->directory_entry[directory_index];
    const uint32_t *table = (uint32_t *)(entry & 0xFFFFF000); // get the address without the flags
    return table[table_index];
}

int paging_set(const struct page_directory *directory, void *virtual_address, const uint32_t value)
{
    dbgprintf("Setting page at virtual address %x to value %x\n", virtual_address, value);

    if (!paging_is_aligned(virtual_address)) {
        warningf("Virtual address %x is not page aligned\n", (uint32_t)virtual_address);
        ASSERT(false, "Virtual address is not aligned");
        return -EINVARG;
    }

    uint32_t directory_index = 0;
    uint32_t table_index     = 0;
    const int res            = paging_get_indexes(virtual_address, &directory_index, &table_index);
    if (res < 0) {
        warningf("Failed to get indexes\n");
        ASSERT(false, "Failed to get indexes");
        return res;
    }

    const uint32_t entry = directory->directory_entry[directory_index];
    auto const table     = (uint32_t *)(entry & 0xFFFFF000); // The address without the flags
    table[table_index]   = value;

    return 0;
}

void paging_init()
{
    kernel_page_directory = paging_create_directory(PDE_IS_WRITABLE | PDE_IS_PRESENT | PDE_SUPERVISOR);
    paging_switch_directory(kernel_page_directory);
    enable_paging();
}

////////////////////////////////////////////////////////////////////////////////
/// Memory overhaul


char *kalloc(void);
void new_kfree(char *v);
static uint32_t *walkpgdir(uint32_t *pgdir, const void *va, int alloc);

/** @brief End of kernel text; defined in linker script. */
// extern char data[]; // defined by kernel.ld
char data[]; // defined by kernel.ld

/** @brief First address after kernel loaded from ELF file */
// extern char end[]; // first address after kernel loaded from ELF file
char end[]; // first address after kernel loaded from ELF file

#define EXTMEM 0x100000     // Start of extended memory (1MB)
#define PHYSTOP 0x20000000  // Top physical memory (512MB)
#define DEVSPACE 0xFE000000 // Other devices are at high addresses (3.75GB)

// Key addresses for address space layout (see kmap in vm.c for layout)
#define KERNBASE 0x80000000          // First kernel virtual address (2GB)
#define KERNLINK (KERNBASE + EXTMEM) // Address where kernel is linked (2GB + 1MB))

#define V2P(a) ((uint32_t)(a) - KERNBASE)
#define P2V(a) ((void *)((uint32_t)(a) + KERNBASE))

#define V2P_WO(x) ((x) - KERNBASE) // same as V2P, but without casts
#define P2V_WO(x) ((x) + KERNBASE) // same as P2V, but without casts

// number of elements in a fixed-size array
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

// Page table/directory entry flags.
#define PTE_P 0x001  // Present
#define PTE_W 0x002  // Writeable
#define PTE_U 0x004  // User
#define PTE_PS 0x080 // Page Size (4MB pages)

// Page directory and page table constants.
#define NPDENTRIES 1024 // # directory entries per page directory
#define NPTENTRIES 1024 // # PTEs per page table
#define PGSIZE 4096     // bytes mapped by a page

#define PTXSHIFT 12 // offset of PTX in a linear address
#define PDXSHIFT 22 // offset of PDX in a linear address


#define PGROUNDUP(sz) (((sz) + PGSIZE - 1) & ~(PGSIZE - 1)) // round up to the next page boundary
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE - 1))              // round down to the page boundary

// Address in the page table or page directory entry
#define PTE_ADDR(pte) ((uint32_t)(pte) & ~0xFFF)
#define PTE_FLAGS(pte) ((uint32_t)(pte) & 0xFFF)

// page directory index
#define PDX(va) (((uint32_t)(va) >> PDXSHIFT) & 0x3FF)

// page table index
#define PTX(va) (((uint32_t)(va) >> PTXSHIFT) & 0x3FF)

// construct virtual address from indexes and offset
#define PGADDR(d, t, o) ((uint32_t)((d) << PDXSHIFT | (t) << PTXSHIFT | (o)))

/** @brief Static kernel mapping template present in every page directory. */
static struct kmap {
    void *virt;
    uint32_t phys_start;
    uint32_t phys_end;
    int perm;
} kmap[] = {
    {(void *)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
    {(void *)KERNLINK, V2P(KERNLINK), V2P(data), 0    }, // kern text+rodata
    {(void *)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
    {(void *)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

/** @brief Linked list node for free pages */
struct run {
    struct run *next;
};

/** @brief Kernel memory allocator state */
struct {
    struct spinlock lock;
    int use_lock;
    struct run *freelist;
} kmem;

uint32_t *kpgdir;


/** @brief Free a range of memory */
void freerange(void *vstart, void *vend)
{
    char *p = (char *)PGROUNDUP((uint32_t)vstart);
    for (; p + PGSIZE <= (char *)vend; p += PGSIZE)
        new_kfree(p);
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
int deallocuvm(uint32_t *pgdir, uint32_t oldsz, uint32_t newsz)
{
    if (newsz >= oldsz) {
        return oldsz;
    }

    uint32_t a = PGROUNDUP(newsz);
    for (; a < oldsz; a += PGSIZE) {
        uint32_t *pte = walkpgdir(pgdir, (char *)a, 0);
        if (!pte) {
            a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
        } else if ((*pte & PTE_P) != 0) {
            uint32_t pa = PTE_ADDR(*pte);
            if (pa == 0) {
                panic("kfree");
            }
            char *v = P2V(pa);
            new_kfree(v);
            *pte = 0;
        }
    }
    return newsz;
}


/** @brief Free a user page table and all associated physical pages. */
void freevm(uint32_t *pgdir)
{
    if (pgdir == nullptr) {
        panic("freevm: no pgdir");
    }
    deallocuvm(pgdir, KERNBASE, 0);
    for (uint32_t i = 0; i < NPDENTRIES; i++) {
        if (pgdir[i] & PTE_P) {
            char *v = P2V(PTE_ADDR(pgdir[i]));
            new_kfree(v);
        }
    }
    new_kfree((char *)pgdir);
}


/**
 * @brief Locate or optionally allocate the PTE for a virtual address.
 *
 * @param pgdir Page directory to search.
 * @param va Virtual address whose page-table entry is requested.
 * @param alloc When non-zero, allocate intermediate page tables on demand.
 * @return Pointer to the requested PTE, or 0 on allocation failure.
 */
static uint32_t *walkpgdir(uint32_t *pgdir, const void *va, int alloc)
{
    uint32_t *pgtab;

    uint32_t *pde = &pgdir[PDX(va)];
    if (*pde & PTE_P) {
        pgtab = (uint32_t *)P2V(PTE_ADDR(*pde));
    } else {
        if (!alloc || (pgtab = (uint32_t *)kalloc()) == nullptr) {
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
static int mappages(uint32_t *pgdir, void *va, uint32_t size, uint32_t pa, int perm)
{
    uint32_t *pte;

    const char *a    = (char *)PGROUNDDOWN((uint32_t)va);
    const char *last = (char *)PGROUNDDOWN(((uint32_t)va) + size - 1);
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

/**
 * @brief Build the kernel portion of a new page directory.
 *
 * @return Pointer to the initialized page directory or 0 on failure.
 */
uint32_t *setupkvm(void)
{
    uint32_t *pgdir;

    if ((pgdir = (uint32_t *)kalloc()) == nullptr) {
        return nullptr;
    }
    memset(pgdir, 0, PGSIZE);
    if (P2V(PHYSTOP) > (void *)DEVSPACE) {
        panic("PHYSTOP too high");
    }
    for (const struct kmap *k = kmap; k < &kmap[NELEM(kmap)]; k++)
        if (mappages(pgdir, k->virt, k->phys_end - k->phys_start, (uint32_t)k->phys_start, k->perm) < 0) {
            freevm(pgdir);
            return nullptr;
        }
    return pgdir;
}

/** @brief Switch to the kernel-only page table for the idle CPU. */
void switch_kvm(void)
{
    lcr3(V2P(kpgdir)); // switch to the kernel page table
}


/** @brief Allocate the kernel page directory and activate it. */
void kvmalloc(void)
{
    kpgdir = setupkvm();
    switch_kvm();
}


/** @brief Free the page of physical memory pointed at by v,
 * which normally should have been returned by a
 * call to kalloc().  (The exception is when
 * initializing the allocator; see kinit)
 */
void new_kfree(char *v)
{
    if ((uint32_t)v % PGSIZE || v < end || V2P(v) >= PHYSTOP) {
        panic("kfree");
    }

    // Fill with junk to catch dangling refs.
    memset(v, 1, PGSIZE);

    if (kmem.use_lock) {
        acquire(&kmem.lock);
    }
    struct run *r = (struct run *)v;
    r->next       = kmem.freelist; // The current head of the free list becomes the next of this page
    kmem.freelist = r;             // This page becomes the head of the free list
    if (kmem.use_lock) {
        release(&kmem.lock);
    }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
/** @brief Allocate one 4096-byte page of physical memory */
char *kalloc(void)
{
    if (kmem.use_lock) {
        acquire(&kmem.lock);
    }
    struct run *r = kmem.freelist; // Gets the first free page
    if (r) {
        kmem.freelist = r->next; // The next free page becomes the head of the list
    }
    if (kmem.use_lock) {
        release(&kmem.lock);
    }
    return (char *)r; // Returns the first free page (or 0 if none)
}
