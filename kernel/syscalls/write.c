#include <kernel.h>
#include <process.h>
#include <status.h>
#include <syscall.h>
#include <vfs.h>

void *sys_write(void)
{
    // const int fd      = get_integer_argument(2);
    // const size_t size = get_integer_argument(0);
    // char *buffer      = get_string_argument(1, size + 1);
    //
    // const int res = vfs_write(current_process(), fd, buffer, size);
    //
    // kfree(buffer);

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
