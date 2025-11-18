#include "defs.h"
#include "string.h"
#include "types.h"

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.
//
// This is a simple free-list allocator that maintains a circular linked list
// of free memory blocks. Each block has a header containing the size and
// pointer to the next free block.

// Used to force proper alignment of header structures
typedef long Align;

// Each free block starts with a header that contains metadata
union header
{
    struct
    {
        union header *ptr; // Pointer to next free block in the circular list
        u32 size;          // Size of this block in Header-sized units
    } s;

    Align x; // Force alignment to long boundary
};

typedef union header Header;

// Base of the free list - initially forms a zero-size block
static Header base;

// Pointer to the current position in the free list (where last search ended)
static Header *freep;

// Free a previously allocated block of memory
// ap: pointer to the memory block (not including the header)

__attribute__((target("avx,sse2")))
void kfree(void *ap)
{
    Header *p;

    // Get pointer to the header (one unit before the user data)
    Header *bp = (Header *)ap - 1;

    // Find the insertion point in the free list
    // The list is kept sorted by address to enable coalescing
    // Loop until we find where bp fits: between p and p->s.ptr
    for (p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr) {
        // Special case: if p >= p->s.ptr, we've wrapped around the circular list
        // (p is the highest address block and p->s.ptr is the lowest)
        // In this case, bp should be inserted if it's either:
        // - Higher than p (at the end), or
        // - Lower than p->s.ptr (at the beginning)
        if (p >= p->s.ptr && (bp > p || bp < p->s.ptr)) {
            break;
        }
    }

    // Coalesce with next block if they're adjacent in memory
    if (bp + bp->s.size == p->s.ptr) {
        bp->s.size += p->s.ptr->s.size; // Merge sizes
        bp->s.ptr = p->s.ptr->s.ptr;    // Skip over the next block
    } else {
        // Not adjacent, just link to next block
        bp->s.ptr = p->s.ptr;
    }

    // Coalesce with previous block if they're adjacent in memory
    if (p + p->s.size == bp) {
        p->s.size += bp->s.size; // Merge sizes
        p->s.ptr = bp->s.ptr;    // Skip over bp
    } else {
        // Not adjacent, just link previous block to bp
        p->s.ptr = bp;
    }

    // Update freep to start next search here
    freep = p;
}

// Request more memory from the operating system
// nu: number of Header-sized units needed
// Returns: pointer to the free list, or nullptr on failure
__attribute__((target("avx,sse2")))
static Header *morecore(u32 nu)
{
    // Allocate at least 4096 units to reduce syscall overhead
    if (nu < 4096) {
        nu = 4096;
    }

    // char *p = sbrk(nu * sizeof(Header));
    char *p = (char *)resize_kernel_page_directory(nu * sizeof(Header));
    if (p == (char *)-1) {
        return nullptr; // sbrk failed
    }

    // Set up the header for the new memory block
    Header *hp = (Header *)p;
    hp->s.size = nu;

    // Add the new block to the free list by "freeing" it
    // This also handles coalescing with adjacent free blocks
    kfree((void *)(hp + 1));

    return freep;
}


void *kzalloc(u32 nbytes)
{
    void *ptr = kmalloc(nbytes);
    memset(ptr, 0, nbytes);
    return ptr;
}

// Allocate memory of at least nbytes size
// nbytes: number of bytes requested
// Returns: pointer to allocated memory, or nullptr if allocation fails
__attribute__((target("avx,sse2")))
void *kmalloc(u32 nbytes)
{
    Header *prevp;

    // Convert byte size to Header-sized units
    // +1 for the header itself, and round up for any remainder
    u32 nunits = (nbytes + sizeof(Header) - 1) / sizeof(Header) + 1;

    // Initialize the free list on first call
    if ((prevp = freep) == nullptr) {
        base.s.ptr  = freep = prevp = &base;
        base.s.size = 0;
    }

    // Search the free list for a block that's big enough
    // Start at freep (where last search ended) and scan circularly
    for (Header *p = prevp->s.ptr; ; prevp = p, p = p->s.ptr) {
        // Found a block big enough
        if (p->s.size >= nunits) {
            if (p->s.size == nunits) {
                // Exact fit: remove entire block from free list
                prevp->s.ptr = p->s.ptr;
            } else {
                // Block is larger: split it
                // Allocate from the tail end of the block
                p->s.size -= nunits; // Reduce free block size
                p += p->s.size;      // Move to tail of remaining block
                p->s.size = nunits;  // Set size of allocated block
            }
            freep = prevp;          // Update search start position
            return (void *)(p + 1); // Return pointer past the header
        }

        // Wrapped around to where we started - no suitable block found
        if (p == freep) {
            // Request more memory from OS
            if ((p = morecore(nunits)) == nullptr) {
                return nullptr; // Out of memory
            }
        }
    }
}