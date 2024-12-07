#include <idt.h>
#include <io.h>
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
    pit_set_interval(freq);

    idt_register_interrupt_callback(0x20, timer_callback);
}

void timer_callback(struct interrupt_frame *frame)
{
    acquire(&tickslock);
    timer_tick++;
    release(&tickslock);

    for (size_t i = 0; i < callback_count; i++) {
        callbacks[i]();
    }
}

void timer_register_callback(void (*func)())
{
    if (callback_count < MAX_CALLBACKS - 1) {
        callbacks[callback_count] = func;
        callback_count++;
    }
}
