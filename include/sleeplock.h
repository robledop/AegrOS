#pragma once

#include <spinlock.h>
#include <stdint.h>

// Long-term locks for processes
struct sleeplock {
    uint32_t locked;    // Is the lock held?
    struct spinlock lk; // spinlock protecting this sleep lock

    // For debugging:
    char *name; // Name of lock.
    int pid;    // Process holding lock
};


void acquiresleep(struct sleeplock *);
void releasesleep(struct sleeplock *);
int holdingsleep(struct sleeplock *);
void initsleeplock(struct sleeplock *, char *);
