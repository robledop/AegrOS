#include <idt.h>
#include <io.h>
#include <lapic.h>
#include <pic.h>
#include <pit.h>
#include <spinlock.h>
#include <stddef.h>
#include <timer.h>

void timer_callback(struct interrupt_frame *frame);
struct spinlock tickslock;
volatile uint32_t timer_tick;

typedef void (*voidfunc_t)();

#define MAX_CALLBACKS 8
static size_t callback_count = 0;
static voidfunc_t callbacks[MAX_CALLBACKS];

void timer_init(const uint32_t freq)
{
    initlock(&tickslock, "ticks");

    pit_set_interval(freq);

    idt_register_interrupt_callback(0x20, timer_callback);

    if (lapic_available()) {
        lapic_init();
        lapic_timer_calibrate(freq);
        lapic_timer_set_periodic(freq);
        pic_disable_irq(0);
    }
}

void timer_callback([[maybe_unused]] struct interrupt_frame *frame)
{
    acquire(&tickslock);
    timer_tick++;
    release(&tickslock);

    for (size_t i = 0; i < callback_count; i++) {
        callbacks[i]();
    }

    lapic_eoi();
}

void timer_register_callback(void (*func)())
{
    if (callback_count < MAX_CALLBACKS - 1) {
        callbacks[callback_count] = func;
        callback_count++;
    }
}
