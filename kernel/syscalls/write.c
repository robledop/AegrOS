#include <kernel.h>
#include <process.h>
#include <status.h>
#include <syscall.h>
#include <vfs.h>
#include <x86.h>

void *sys_write(void)
{
    const size_t size     = (size_t)thread_peek_stack_item(get_current_thread(), 0);
    void *virtual_address = thread_peek_stack_item(get_current_thread(), 1);
    const int fd          = (int)thread_peek_stack_item(get_current_thread(), 2);

    if (virtual_address == nullptr) {
        return ERROR(-EFAULT);
    }

    char *buffer = thread_virtual_to_physical_address(get_current_thread(), virtual_address);
    if (buffer == nullptr) {
        return ERROR(-EFAULT);
    }

    const int res = vfs_write(current_process(), fd, buffer, size);

    return (void *)res;
}
