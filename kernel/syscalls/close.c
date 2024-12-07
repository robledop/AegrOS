#include <process.h>
#include <spinlock.h>
#include <syscall.h>
#include <vfs.h>
#include <x86.h>

struct spinlock close_lock = {};

void *sys_close(void)
{
    acquire(&close_lock);

    const int fd  = get_integer_argument(0);
    const int res = vfs_close(current_process(), fd);

    release(&close_lock);

    return (void *)res;
}
