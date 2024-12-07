#include <spinlock.h>
#include <syscall.h>
#include <task.h>

struct spinlock wait_lock = {};

void *sys_wait_pid(void)
{
    // TODO: Actually take the pid as an argument
    const int pid     = get_integer_argument(1);
    void *virtual_ptr = task_peek_stack_item(get_current_task(), 0);
    int *status_ptr   = nullptr;
    if (virtual_ptr) {
        status_ptr = thread_virtual_to_physical_address(get_current_task(), virtual_ptr);
    }
    const int status = wait();
    if (status_ptr) {
        *status_ptr = status;
    }

    return nullptr;
}
