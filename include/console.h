#pragma once
#include "spinlock.h"

struct console_lock
{
    struct spinlock lock;
    int locking;
};