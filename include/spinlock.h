#pragma once
#include <stdint.h>
#define NAME_LEN 25

// Mutual exclusion lock.
struct spinlock {
    uint32_t locked; // Is the lock held?

    // For debugging:
    char name[NAME_LEN]; // Name of lock.
    char file[100];
    int line;
};

#define acquire(lk) acquire_(lk, __FILE__, __LINE__)


void acquire_(struct spinlock *, const char *, int);
int holding(struct spinlock *);
void initlock(struct spinlock *, char *);
void release(struct spinlock *);
void pushcli(void);
void popcli(void);
