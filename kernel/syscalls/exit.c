#include <kernel.h>
#include <process.h>
#include <syscall.h>
#include <task.h>

extern struct process_list process_list;

[[noreturn]] void *sys_exit(void)
{
    // yield may still be holding the lock
    if (holding(&process_list.lock)) {
        current_process()->killed = true;
        current_task->state       = TASK_STOPPED;
        sched();
    }

    exit();

    panic("Trying to schedule a dead thread");

    __builtin_unreachable();
}
