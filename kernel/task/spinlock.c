#include <kernel.h>
#include <kernel_heap.h>
#include <memory.h>
#include <printf.h>
#include <spinlock.h>
#include <string.h>
#include <task.h>
#include <vfs.h>
#include <x86.h>

void initlock(struct spinlock *lk, char *name)
{
    lk->name   = name;
    lk->locked = 0;
    lk->cpu    = nullptr;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
// Holding a lock for a long time may cause
// other CPUs to waste time spinning to acquire it.
void acquire_(struct spinlock *lk, const char *file, int line)
{
    pushcli(); // disable interrupts to avoid deadlock.
    if (holding(lk)) {
        char *buf = kzalloc(256);
        snprintf(buf, 256, "acquire: %s. Held by %s:%d", lk->name, lk->file, lk->line);
        panic(buf);
    }

    // The xchg is atomic.
    while (xchg(&lk->locked, 1) != 0)
        ;

    // Tell the C compiler and the processor to not move loads or stores
    // past this point, to ensure that the critical section's memory
    // references happen after the lock is acquired.
    __sync_synchronize();

    // Record info about lock acquisition for debugging.
    lk->cpu = get_cpu();

    memcpy(lk->file, file, strlen(file));
    lk->line = line;
}

// Release the lock.
void release(struct spinlock *lk)
{
    if (!holding(lk)) {
        panic("release");
    }

    lk->pcs[0] = 0;
    lk->cpu    = nullptr;

    // Tell the C compiler and the processor to not move loads or stores
    // past this point, to ensure that all the stores in the critical
    // section are visible to other cores before the lock is released.
    // Both the C compiler and the hardware may re-order loads and
    // stores; __sync_synchronize() tells them both not to.
    __sync_synchronize();

    // Release the lock, equivalent to lk->locked = 0.
    // This code can't use a C assignment, since it might
    // not be atomic. A real OS would use C atomics here.
    asm volatile("movl $0, %0" : "+m"(lk->locked) :);
    memset(lk->file, 0, 100);

    popcli();
}

// Check whether this cpu is holding the lock.
int holding(struct spinlock *lock)
{
    pushcli();
    const int r = lock->locked && lock->cpu == get_cpu();
    popcli();
    return r;
}


// Pushcli/popcli are like cli/sti except that they are matched:
// it takes two popcli to undo two pushcli.  Also, if interrupts
// are off, then pushcli, popcli leaves them off.
void pushcli(void)
{
    const uint32_t eflags = read_eflags();
    cli();
    auto const cpu = get_cpu();
    if (cpu->ncli == 0) {
        cpu->interrupts_enabled = eflags & FL_IF;
    }
    cpu->ncli += 1;
}

void popcli(void)
{
    if (read_eflags() & FL_IF) {
        panic("popcli - interruptible");
    }
    auto const cpu = get_cpu();
    if (--cpu->ncli < 0) {
        panic("popcli");
    }
    if (cpu->ncli == 0 && cpu->interrupts_enabled) {
        sti();
    }
}
