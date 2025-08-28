#include <process.h>
#include <syscall.h>
#include <vfs.h>

void *sys_ioctl(void)
{
    int file_descriptor = get_integer_argument(2);
    int request         = get_integer_argument(1);
    void *arg           = get_pointer_argument(0);

    return (void *)(long)vfs_ioctl(current_process(), file_descriptor, request, arg);
}