#include <kernel.h>
#include <process.h>
#include <syscall.h>
#include <thread.h>

extern struct process_list process_list;

[[noreturn]] void *sys_exit(void)
{
    // yield may still be holding the lock
    if (holding(&process_list.lock)) {
        current_process()->killed = true;
        current_thread->state       = TASK_STOPPED;
        switch_to_scheduler();
    }

    exit();

    panic("Trying to schedule a dead thread");

    __builtin_unreachable();
}
