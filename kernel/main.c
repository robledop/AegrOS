#include <stdint.h>
#include <multiboot.h>
#include <memlayout.h>

extern char end[]; // first address after kernel loaded from ELF file


void kernel_main(multiboot_info_t *mbd_phys, uint32_t magic)
{
    // Store the physical address of the multiboot info structure before paging is enabled
    const multiboot_info_t *mbd = (multiboot_info_t *)P2V((uint32_t)mbd_phys);
}


