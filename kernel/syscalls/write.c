#include <kernel_heap.h>
#include <process.h>
#include <syscall.h>
#include <vfs.h>

void *sys_write(void)
{
    const int fd      = get_integer_argument(2);
    const size_t size = get_integer_argument(0);
    char *buffer      = get_string_argument(1, size + 1);

    const int res = vfs_write(current_process(), fd, buffer, size);

    kfree(buffer);

    return (void *)res;
}
