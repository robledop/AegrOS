#include <mmu.h>
#include <memlayout.h>
#include <stdint.h>

/** @brief Boot-time page directory referenced from assembly.
* Boot-time page directory used while paging is being enabled.
 *
 * Map the first 4 MiB of physical memory twice: once at VA 0 so we can keep
 * running the low-level bootstrap code, and once at KERNBASE so the kernel can
 * execute from its linked virtual addresses (0x80100000 and above).
 */
__attribute__((aligned(PGSIZE))) uint32_t entrypgdir[NPDENTRIES] = {
    [0]                          = PTE_P | PTE_W | PTE_PS,                   // maps 0x0000_0000 → 0x0000_0000
    [1]                          = (1 << PDXSHIFT) | PTE_P | PTE_W | PTE_PS, // maps 0x0040_0000 → 0x0040_0000
    [KERNBASE >> PDXSHIFT]       = PTE_P | PTE_W | PTE_PS,                   // maps 0x8000_0000 → phys 0
    [(KERNBASE >> PDXSHIFT) + 1] = (1 << PDXSHIFT) | PTE_P | PTE_W | PTE_PS,
};


// __attribute__((aligned(PGSIZE))) uint32_t entrypgdir[NPDENTRIES] = {
//     [0]                    = PTE_P | PTE_W | PTE_PS,
//     [KERNBASE >> PDXSHIFT] = PTE_P | PTE_W | PTE_PS,
// };
