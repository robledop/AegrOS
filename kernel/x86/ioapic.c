// The I/O APIC manages hardware interrupts for an SMP system.
// http://www.intel.com/design/chipsets/datashts/29056601.pdf
// See also picirq.c.

#include "assert.h"
#include "types.h"
#include "defs.h"
#include "printf.h"
#include "traps.h"
#include "proc.h"

#define IOAPIC 0xFEC00000 // Default physical address of IO APIC

#define REG_ID 0x00    // Register index: ID
#define REG_VER 0x01   // Register index: version
#define REG_TABLE 0x10 // Redirection table base

// The redirection table starts at REG_TABLE and uses
// two registers to configure each interrupt.
// The first (low) register in a pair contains configuration bits.
// The second (high) register contains a bitmask telling which
// CPUs can serve that interrupt.
#define INT_DISABLED 0x00010000  // Interrupt disabled
#define INT_LEVEL 0x00008000     // Level-triggered (vs edge-)
#define INT_ACTIVELOW 0x00002000 // Active low (vs high)
#define INT_LOGICAL 0x00000800   // Destination is CPU id (vs APIC ID)

/** @brief Memory-mapped base pointer to the I/O APIC. */
volatile struct ioapic *ioapic;

// IO APIC MMIO structure: write reg, then read or write data.
/** @brief Memory-mapped register layout of the I/O APIC. */
struct ioapic
{
    u32 reg;
    u32 pad[3];
    u32 data;
};

/**
 * @brief Read a 32-bit value from an I/O APIC register.
 *
 * @param reg Register index to read.
 * @return Contents of the requested register.
 */
static u32
ioapic_read(int reg)
{
    ioapic->reg = reg;
    return ioapic->data;
}

/**
 * @brief Write a 32-bit value to an I/O APIC register.
 *
 * @param reg Register index to write.
 * @param data Value to store.
 */
static void ioapic_write(int reg, u32 data)
{
    ioapic->reg  = reg;
    ioapic->data = data;
}

/** @brief Initialize the I/O APIC and mask all interrupts. */
void ioapic_int(void)
{
    void *va = kernel_map_mmio(IOAPIC, PGSIZE);
    if (va == nullptr) {
        boot_message(WARNING_LEVEL_ERROR, "Failed to map IOAPIC at 0x%x", IOAPIC);
        return;
    }
    ioapic      = (volatile struct ioapic *)va;
    int maxintr = (ioapic_read(REG_VER) >> 16) & 0xFF;
    int id      = ioapic_read(REG_ID) >> 24;
    if (id != ioapicid) {
        boot_message(WARNING_LEVEL_WARNING,
                     "ioapicinit: expected id %d got %d; continuing anyway",
                     ioapicid,
                     id);
    }

    // Mark all interrupts edge-triggered, active high, disabled,
    // and not routed to any CPUs.
    for (int i = 0; i <= maxintr; i++) {
        ioapic_write(REG_TABLE + 2 * i, INT_DISABLED | (T_IRQ0 + i));
        ioapic_write(REG_TABLE + 2 * i + 1, 0);
    }
}

/**
 * @brief Route an external interrupt to a specific CPU.
 *
 * @param irq IRQ line to enable.
 * @param cpunum Index of the destination CPU in the cpus[] array.
 */
void enable_ioapic_interrupt(int irq, int cpunum)
{
    ASSERT(cpunum >= 0 && cpunum < ncpu, "ioapicenable: invalid cpu");

    u32 apicid = cpus[cpunum].apicid;

    // Mark interrupt edge-triggered, active high,
    // enabled, and routed to the given cpunum,
    // which happens to be that cpu's APIC ID.
    ioapic_write(REG_TABLE + 2 * irq, T_IRQ0 + irq);
    ioapic_write(REG_TABLE + 2 * irq + 1, apicid << 24);
}
