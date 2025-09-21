#include <spinlock.h>
#include <syscall.h>
#include <thread.h>
#include "serial.h"

struct spinlock wait_lock = {
    .name = "waitpid",
};

void *sys_wait_pid(void)
{
    // TODO: Actually take the pid as an argument
    const int pid = get_integer_argument(1);
    dbgprintf("Waiting for pid %d\n", pid);
    void *virtual_ptr = thread_peek_stack_item(get_current_thread(), 0);
    int *status_ptr   = nullptr;
    if (virtual_ptr) {
        status_ptr = thread_virtual_to_physical_address(get_current_thread(), virtual_ptr);
    }
    const int status = wait();
    if (status_ptr) {
        *status_ptr = status;
    }

    return nullptr;
}
