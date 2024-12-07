#include <idt.h>
#include <process.h>
#include <spinlock.h>
#include <syscall.h>

struct spinlock fork_lock = {};

void *sys_fork(void)
{
    acquire(&fork_lock);

    auto const parent              = get_current_task()->process;
    auto const child               = process_clone(parent);
    child->thread->trap_frame->eax = 0;

    release(&fork_lock);

    return (void *)(int)child->pid;
}
