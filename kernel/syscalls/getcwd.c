#include <process.h>
#include <syscall.h>

/// @brief Get the current working directory
void *sys_getcwd(void)
{
    return (void *)current_process()->current_directory;
}
