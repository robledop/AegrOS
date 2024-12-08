#include <syscall.h>
#include <thread.h>

void *sys_yield(void)
{
    yield();

    return nullptr;
}
