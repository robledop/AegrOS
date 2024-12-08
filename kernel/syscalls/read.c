#include <memory.h>
#include <process.h>
#include <syscall.h>
#include <thread.h>
#include <vfs.h>

void *sys_read(void)
{
    void *virtual_address    = thread_peek_stack_item(get_current_thread(), 3);
    void *task_file_contents = thread_virtual_to_physical_address(get_current_thread(), virtual_address);

    const unsigned int size  = (unsigned int)thread_peek_stack_item(get_current_thread(), 2);
    const unsigned int nmemb = (unsigned int)thread_peek_stack_item(get_current_thread(), 1);
    const int fd             = (int)thread_peek_stack_item(get_current_thread(), 0);

    const int res = vfs_read(current_process(), (void *)task_file_contents, size, nmemb, fd);

    return (void *)res;
}
