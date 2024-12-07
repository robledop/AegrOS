#include <process.h>
#include <syscall.h>
#include <task.h>

void *sys_getcwd(void)
{
    return (void *)current_process()->current_directory;
}
