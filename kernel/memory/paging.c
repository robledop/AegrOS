// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"
#include "string.h"
#include "param.h"

void freerange(void *vstart, void *vend);
/** @brief First address after kernel loaded from ELF file */
extern char kernel_end[]; // first address after kernel loaded from ELF file
// defined by the kernel linker script in kernel.ld

/** @brief Linked list node for free pages */
struct run
{
    struct run *next;
};

/** @brief Kernel memory allocator state */
struct
{
    struct spinlock lock;
    int use_lock;
    struct run *freelist;
} kmem;

/** @brief Initialize kernel memory allocator phase 1 */

NO_SSE void init_memory_range(void *vstart, void *vend)
{
    initlock(&kmem.lock, "kmem");
    kmem.use_lock = 0;
    freerange(vstart, vend);
}

/** @brief Initialize kernel memory allocator phase 2 */
void kalloc_enable_locking(void)
{
    kmem.use_lock = 1;
}

/** @brief Free a range of memory */
NO_SSE void freerange(void *vstart, void *vend)
{
    char *p = (char *)PGROUNDUP((u32)vstart);
    for (; p + PGSIZE <= (char *)vend; p += PGSIZE) {
        kfree_page(p);
    }
}

/** @brief Free the page of physical memory pointed at by v,
 * which normally should have been returned by a
 * call to kalloc().  (The exception is when
 * initializing the allocator; see kinit)
 */
void kfree_page(char *v)
{
    if ((u32)v % PGSIZE || v < kernel_end || V2P(v) >= PHYSTOP) {
        panic("kfree_page");
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
char *kalloc_page(void)
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
