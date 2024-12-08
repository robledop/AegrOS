#include <process.h>
#include <syscall.h>
#include <thread.h>

// TODO: Implement mmap/munmap and use it instead of this
void *sys_free(void)
{
    void *ptr = get_pointer_argument(0);
    process_free(get_current_thread()->process, ptr);
    return nullptr;
}
