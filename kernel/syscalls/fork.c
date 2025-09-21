#include <idt.h>
#include <process.h>
#include <spinlock.h>
#include <syscall.h>
#include "serial.h"

struct spinlock fork_lock = {};

void *sys_fork(void)
{
    acquire(&fork_lock);

    struct process *parent = current_process();
    if (!parent) {
        release(&fork_lock);
        return (void *)-1;
    }

    warningf("Forking %s, pid: %d\n", parent->file_name, parent->pid);

    auto const child = process_clone(parent);
    if (!child) {
        release(&fork_lock);
        return (void *)-1; // Fork failed
    }

    // Set return value for child process
    child->thread->trap_frame->eax = 0;

    release(&fork_lock);

    return (void *)(int)child->pid;
}
