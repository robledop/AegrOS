#pragma once
#include <stdint.h>

// typedef uint32_t spinlock_t;
//
//
// __attribute__((nonnull)) void spinlock_init(spinlock_t *lock);
// __attribute__((nonnull)) void spin_lock(spinlock_t *lock);
// __attribute__((nonnull)) void spin_unlock(spinlock_t *lock);


// Mutual exclusion lock.
struct spinlock {
    uint32_t locked; // Is the lock held?

    // For debugging:
    char *name;       // Name of lock.
    struct cpu *cpu;  // The cpu holding the lock.
    uint32_t pcs[10]; // The call stack (an array of program counters)
                      // that locked the lock.
};

void acquire(struct spinlock *);
int holding(struct spinlock *);
void initlock(struct spinlock *, char *);
void release(struct spinlock *);
void pushcli(void);
void popcli(void);
