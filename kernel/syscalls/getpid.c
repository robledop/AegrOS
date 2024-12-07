#include <process.h>
#include <syscall.h>
#include <task.h>

void *sys_getpid(void)
{
    return (void *)(int)current_process()->pid;
}
