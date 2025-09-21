#include <cpuid.h>
#include <lapic.h>
#include <timer.h>
#include <x86.h>

#define LAPIC_DEFAULT_BASE 0xFEE00000U

#define LAPIC_REG_ID 0x020
#define LAPIC_REG_EOI 0x0B0
#define LAPIC_REG_SVR 0x0F0
#define LAPIC_REG_LVT_TIMER 0x320
#define LAPIC_REG_TIMER_INIT 0x380
#define LAPIC_REG_TIMER_CUR 0x390
#define LAPIC_REG_TIMER_DIV 0x3E0

#define LAPIC_SVR_ENABLE 0x100
#define LAPIC_TIMER_MODE_PERIODIC 0x00020000
#define LAPIC_TIMER_VECTOR 0x20
#define LAPIC_LVT_MASKED 0x00010000

static volatile uint32_t *lapic    = nullptr;
static uint32_t lapic_ticks_per_ms = 0;

/**
 * @brief Read a Local APIC register.
 *
 * @param offset Register offset from the LAPIC base address.
 * @return Current value of the register.
 */
static inline uint32_t lapic_read(uint32_t offset)
{
    return lapic[offset / 4];
}

/**
 * @brief Write a Local APIC register and perform a posted read for ordering.
 *
 * @param offset Register offset from the LAPIC base address.
 * @param value Value to store in the register.
 */
static inline void lapic_write(uint32_t offset, uint32_t value)
{
    lapic[offset / 4] = value;
    (void)lapic_read(LAPIC_REG_ID);
}

/**
 * @brief Determine whether the processor exposes a Local APIC implementation.
 *
 * @return true when LAPIC support is detected, otherwise false.
 */
bool lapic_available(void)
{
    unsigned int eax, ebx, ecx, edx;
    if (!__get_cpuid(0x01, &eax, &ebx, &ecx, &edx)) {
        return false;
    }
    return (edx & (1U << 9)) != 0U;
}

/**
 * @brief Enable the Local APIC and program the spurious interrupt vector.
 */
void lapic_init(void)
{
    if (!lapic_available()) {
        return;
    }

    uint64_t apic_base = rdmsr(MSR_IA32_APIC_BASE);
    apic_base |= (1ULL << 11);
    if ((apic_base & 0xFFFFF000ULL) == 0) {
        apic_base |= LAPIC_DEFAULT_BASE;
    }
    wrmsr(MSR_IA32_APIC_BASE, apic_base);

    uint32_t base = (uint32_t)(apic_base & 0xFFFFF000ULL);
    lapic         = (volatile uint32_t *)base;

    lapic_write(LAPIC_REG_SVR, LAPIC_SVR_ENABLE | LAPIC_TIMER_VECTOR);
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_TIMER_DIV, 0x3);
}

/**
 * @brief Measure the LAPIC timer tick rate using PIT ticks as a reference.
 *
 * The routine briefly enables interrupts to count a fixed number of PIT ticks while the
 * LAPIC timer free-runs, providing a calibration factor expressed as ticks per millisecond.
 *
 * @param hz Target timer frequency (currently advisory).
 */
void lapic_timer_calibrate(uint32_t hz)
{
    (void)hz;

    if (!lapic) {
        return;
    }

    constexpr uint32_t sample_ms = 10;

    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_REG_TIMER_DIV, 0x3);
    lapic_write(LAPIC_REG_TIMER_INIT, 0xFFFFFFFFU);

    const bool irq_enabled = (read_eflags() & EFLAGS_IF) != 0U;
    if (!irq_enabled) {
        sti();
    }

    uint32_t start = timer_tick;
    while ((timer_tick - start) < sample_ms) {
        __asm__ volatile("pause");
    }

    if (!irq_enabled) {
        cli();
    }

    uint32_t elapsed   = 0xFFFFFFFFU - lapic_read(LAPIC_REG_TIMER_CUR);
    lapic_ticks_per_ms = elapsed / sample_ms;
    if (lapic_ticks_per_ms == 0) {
        lapic_ticks_per_ms = 1;
    }

    lapic_write(LAPIC_REG_TIMER_INIT, 0);
}

/**
 * @brief Program the LAPIC timer to fire periodically at the requested rate.
 *
 * @param hz Desired interrupt rate in Hertz.
 */
void lapic_timer_set_periodic(uint32_t hz)
{
    if (!lapic || lapic_ticks_per_ms == 0) {
        return;
    }

    if (hz == 0) {
        hz = 1000;
    }

    uint32_t interval_ms = 1000U / hz;
    if (interval_ms == 0) {
        interval_ms = 1;
    }

    uint32_t initial = lapic_ticks_per_ms * interval_ms;
    if (initial == 0) {
        initial = lapic_ticks_per_ms;
    }

    lapic_write(LAPIC_REG_TIMER_DIV, 0x3);
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_MODE_PERIODIC | LAPIC_TIMER_VECTOR);
    lapic_write(LAPIC_REG_TIMER_INIT, initial);
}

/**
 * @brief Signal end-of-interrupt to the Local APIC.
 */
void lapic_eoi(void)
{
    if (!lapic) {
        return;
    }
    lapic_write(LAPIC_REG_EOI, 0);
}
