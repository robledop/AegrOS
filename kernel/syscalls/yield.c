#include <syscall.h>
#include <task.h>

void *sys_yield(void)
{
    yield();

    return nullptr;
}
