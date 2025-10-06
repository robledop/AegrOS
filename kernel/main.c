#include <stdint.h>
#include <multiboot.h>
#include <memlayout.h>
#include <x86.h>
#include <printf.h>
#include <termcolors.h>
#include <vga_buffer.h>
#include <assert.h>
#include <io.h>

extern char end[]; // first address after kernel loaded from ELF file

#define STACK_CHK_GUARD 0xe2dee396
uintptr_t __stack_chk_guard = STACK_CHK_GUARD; // NOLINT(*-reserved-identifier)

[[noreturn]]
void kernel_main(multiboot_info_t* mbd_phys, uint32_t magic)
{
    ASSERT(magic == MULTIBOOT_BOOTLOADER_MAGIC, "Invalid magic number: 0x%x\n", magic);
    // Store the physical address of the multiboot info structure before paging is enabled
    const multiboot_info_t* mbd = (multiboot_info_t*)P2V((uint32_t)mbd_phys);
    vga_buffer_init();
    printf(KGRN "\nWelcome to " KBBLU "AegrOS" KWHT "!\n");
    cpu_print_info();

    while (1)
    {
        hlt();
    }
}

[[noreturn]] void panic(const char* msg)
{
    // debug_stats();
    printf(KRED "\nKERNEL PANIC: " KWHT "%s\n", msg);

    while (1)
    {
        hlt();
    }

    __builtin_unreachable();
}
