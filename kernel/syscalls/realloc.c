#include <process.h>
#include <stdint.h>
#include <syscall.h>

void *sys_realloc(void)
{
    const uintptr_t ptr  = (uintptr_t)get_pointer_argument(0);
    const uintptr_t size = (uintptr_t)get_pointer_argument(1);
    return process_realloc(current_process(), (void *)ptr, size);
}