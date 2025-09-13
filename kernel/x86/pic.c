#include <assert.h>
#include <io.h>
#include <pic.h>

// https://wiki.osdev.org/8259_PIC

#define PIC_EOI 0x20 // End of interrupt command
#define PIC1 0x20    // Master PIC
#define PIC1_COMMAND PIC1
#define PIC2 0xA0 // Slave PIC
#define PIC2_COMMAND PIC2
#define PIC1_DATA (PIC1 + 1) // Data port for master PIC
#define PIC2_DATA (PIC2 + 1) // Data port for slave PIC

#define ICW1_ICW4 0x01      /* Indicates that ICW4 will be present */
#define ICW1_SINGLE 0x02    /* Single (cascade) mode */
#define ICW1_INTERVAL4 0x04 /* Call address interval 4 (8) */
#define ICW1_LEVEL 0x08     /* Level triggered (edge) mode */
#define ICW1_INIT 0x10      /* Initialization - required! */

#define ICW4_8086 0x01       /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO 0x02       /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE 0x08  /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C /* Buffered mode/master */
#define ICW4_SFNM 0x10       /* Special fully nested (not) */

#define CASCADE_IRQ 2

/**
 * @brief  reinitialize the PIC controllers, giving them specified vector offsets rather than 8h and 70h, as configured
 * by default
 * @param master_offset vector offset for the master PIC
 * @param slave_offset vector offset for the slave PIC
 */
void pic_remap(int master_offset, int slave_offset)
{
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4); // starts the initialization sequence (in cascade mode)
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC1_DATA, master_offset); // ICW2: Master PIC vector offset
    io_wait();
    outb(PIC2_DATA, slave_offset); // ICW2: Slave PIC vector offset
    io_wait();
    outb(PIC1_DATA, 1 << CASCADE_IRQ); // ICW3: tell Master PIC that there is a slave PIC at IRQ2
    io_wait();
    outb(PIC2_DATA, 2); // ICW3: tell Slave PIC its cascade identity (0000 0010)
    io_wait();

    outb(PIC1_DATA, ICW4_8086); // ICW4: have the PICs use 8086 mode (and not 8080 mode)
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    // Unmask both PICs. Enable all interrupts.
    outb(PIC1_DATA, 0);
    outb(PIC2_DATA, 0);
}

void pic_init()
{
    pic_remap(0x20, 0x28);
}

void pic_acknowledge(const int irq)
{
    ASSERT(irq >= 0x20 && irq < 0x30);

    // Acknowledge slave PIC if this is a slave interrupt.
    if (irq >= 0x28) {
        outb(PIC2, PIC_EOI);
    }

    // Acknowledge master PIC.
    outb(PIC1, PIC_EOI);
}

// Enable a specific IRQ line
void pic_enable_irq(int irq)
{
    if (irq < 8) {
        // Master PIC
        uint8_t mask = inb(PIC1_DATA);
        mask &= ~(1 << irq);
        outb(PIC1_DATA, mask);
    } else if (irq < 16) {
        // Slave PIC
        uint8_t mask = inb(PIC2_DATA);
        mask &= ~(1 << (irq - 8));
        outb(PIC2_DATA, mask);
    }
}

// Disable a specific IRQ line
void pic_disable_irq(int irq)
{
    if (irq < 8) {
        // Master PIC
        uint8_t mask = inb(PIC1_DATA);
        mask |= (1 << irq);
        outb(PIC1_DATA, mask);
    } else if (irq < 16) {
        // Slave PIC
        uint8_t mask = inb(PIC2_DATA);
        mask |= (1 << (irq - 8));
        outb(PIC2_DATA, mask);
    }
}

// Check if an IRQ is enabled
int pic_irq_enabled(int irq)
{
    if (irq < 8) {
        // Master PIC
        uint8_t mask = inb(PIC1_DATA);
        return !(mask & (1 << irq));
    } else if (irq < 16) {
        // Slave PIC
        uint8_t mask = inb(PIC2_DATA);
        return !(mask & (1 << (irq - 8)));
    }
    return 0;
}
