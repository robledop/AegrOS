#include <memory.h>
#include <syscall.h>
#include <task.h>
#include <vfs.h>

void *sys_read(void)
{
    void *virtual_address    = task_peek_stack_item(get_current_task(), 3);
    void *task_file_contents = thread_virtual_to_physical_address(get_current_task(), virtual_address);

    const unsigned int size  = (unsigned int)task_peek_stack_item(get_current_task(), 2);
    const unsigned int nmemb = (unsigned int)task_peek_stack_item(get_current_task(), 1);
    const int fd             = (int)task_peek_stack_item(get_current_task(), 0);

    // memset(task_file_contents, 0xFF, size * nmemb);
    const int res = vfs_read((void *)task_file_contents, size, nmemb, fd);

    return (void *)res;
}
