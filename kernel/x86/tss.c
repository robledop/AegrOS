#include <gdt.h>
#include <memory.h>
#include <tss.h>

struct tss_entry tss_entry;

// Function to write the TSS descriptor into the GDT
void write_tss(const int num, const uint16_t ss0, const uint32_t esp0)
{
    const uint32_t base  = (uint32_t)&tss_entry;
    const uint32_t limit = base + sizeof(struct tss_entry);

    // Add TSS descriptor to the GDT
    gdt_set_gate(num, base, limit, 0xE9, 0x40);
    // 0x89: Present, ring 0, type 9 (available 32-bit TSS)
    // 0x40: Granularity

    memset(&tss_entry, 0, sizeof(struct tss_entry));

    tss_entry.ss0  = ss0;  // Kernel stack segment
    tss_entry.esp0 = esp0; // Kernel stack pointer

    // Set IO map base to the size of the TSS (no IO bitmap)
    tss_entry.iomap_base = sizeof(struct tss_entry);
}

void set_kernel_stack(uint32_t stack)
{
    tss_entry.esp0 = stack;
}