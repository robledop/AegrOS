#include <syscall.h>
#include <vfs.h>

void *sys_lseek(void)
{
    const int fd     = get_integer_argument(2);
    const int offset = get_integer_argument(1);
    const int whence = get_integer_argument(0);

    return (void *)vfs_lseek(fd, offset, whence);
}
