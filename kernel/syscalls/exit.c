#include <kernel.h>
#include <process.h>
#include <syscall.h>
#include <thread.h>

#include "kernel_heap.h"

extern struct process_list process_list;

[[noreturn]] void *sys_exit(void)
{
    // yield may still be holding the lock
    if (holding(&process_list.lock)) {
        current_thread->state   = TASK_STOPPED;
        auto p                  = current_thread->process;
        int pid                 = p->pid;
        current_thread->process = nullptr;
        process_zombify(p);
        process_set(pid, nullptr);

        switch_to_scheduler();
    }

    exit();

    panic("Trying to schedule a dead thread");

    __builtin_unreachable();
}
