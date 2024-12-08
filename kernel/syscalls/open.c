#include <config.h>
#include <dirent.h>
#include <process.h>
#include <spinlock.h>
#include <syscall.h>
#include <thread.h>
#include <vfs.h>

struct spinlock open_lock = {};

void *sys_open(void)
{
    acquire(&open_lock);

    const void *file_name       = get_pointer_argument(1);
    char *name[MAX_PATH_LENGTH] = {nullptr};
    copy_string_from_thread(get_current_thread(), file_name, name, sizeof(name));
    const FILE_MODE mode = get_integer_argument(0);

    const int fd = vfs_open(current_process(), (const char *)name, mode);

    release(&open_lock);

    return (void *)(int)fd;
}
