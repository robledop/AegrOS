#include <gdt.h>
#include <tss.h>

struct gdt_entry gdt_entries[6];
struct gdt_ptr gdt_ptr;

extern uint32_t kernel_stack_top;

void gdt_init()
{
    gdt_ptr.limit = (sizeof(struct gdt_entry) * 6) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    // Null segment
    gdt_set_gate(0, 0, 0, 0, 0);
    // Kernel code segment
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    // Kernel data segment
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    // User code segment
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    // User data segment
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);
    // TSS segment
    write_tss(5, 0x10, (uint32_t)&kernel_stack_top);
    // Load the new GDT
    gdt_flush((uint32_t)&gdt_ptr);
    // Load the TSS
    tss_flush();
}

// Set up a GDT entry
void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = ((limit >> 16) & 0x0F);

    gdt_entries[num].granularity |= (gran & 0xF0);
    gdt_entries[num].access = access;
}