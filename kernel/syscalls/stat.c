#include <process.h>
#include <syscall.h>
#include <thread.h>
#include <vfs.h>

void *sys_stat(void)
{
    const int fd          = get_integer_argument(1);
    void *virtual_address = thread_peek_stack_item(get_current_thread(), 0);

    struct stat *stat = thread_virtual_to_physical_address(get_current_thread(), virtual_address);

    return (void *)vfs_stat(current_process(), fd, stat);
}
