#include "defs.h"
#include "proc.h"
#include "x86.h"
#include "file.h"
#include "string.h"

struct ptable_t ptable;
extern struct proc *initproc;

/**
 * @brief Per-CPU scheduler loop that selects and runs processes.
 *
 * Never returns; invoked once per CPU during initialization.
 */
void scheduler(void)
{
    // TODO: Cleanup ZOMBIE processes that have not been waited on.
    struct cpu *cpu = current_cpu();
    cpu->proc       = nullptr;

    for (;;) {
        // Enable interrupts on this processor.
        sti();

        // Loop over the process table looking for a process to run.
        acquire(&ptable.lock);
        ptable.active_count = 0;
        for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->state != RUNNABLE)
                continue;

            // Account for runnable processes for idle detection.
            ptable.active_count++;

            // Switch to the chosen process.  It is the process's job
            // to release ptable.lock and then reacquire it
            // before jumping back to us.
            cpu->proc = p;
            activate_process(p);
            p->state = RUNNING;

            switch_context(&(cpu->scheduler), p->context);
            switch_kernel_page_directory();

            // Process is done running for now.
            // It should have changed its p->state before coming back.
            cpu->proc = nullptr;
        }

        release(&ptable.lock);

        // Idle "thread"
        if (ptable.active_count == 0) {
            sti();
            hlt();
        }
    }
}

/**
 * @brief Enter the scheduler after marking the current process non-running.
 *
 * Requires ptable.lock to be held and saves/restores interrupt state so the
 * process can resume correctly.
 */
void switch_to_scheduler(void)
{
    struct proc *p = current_process();

    if (!holding(&ptable.lock)) {
        panic("switch_to_scheduler ptable.lock");
    }
    if (current_cpu()->ncli != 1) {
        panic("switch_to_scheduler locks");
    }
    if (p->state == RUNNING) {
        panic("switch_to_scheduler running");
    }
    if (read_eflags() & FL_IF) {
        panic("switch_to_scheduler interruptible");
    }

    const int interrupts_enabled = current_cpu()->interrupts_enabled;
    switch_context(&p->context, current_cpu()->scheduler);
    current_cpu()->interrupts_enabled = interrupts_enabled;
}

/**
 * @brief Wait for a child process to exit and return its PID.
 *
 * @return Child PID on success, -1 if no children are alive.
 */
int wait(void)
{
    struct proc *curproc = current_process();

    acquire(&ptable.lock);
    for (;;) {
        // Scan through table looking for exited children.
        int havekids = 0;
        for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->parent != curproc) {
                continue;
            }
            havekids = 1;
            if (p->state == ZOMBIE) {
                // Found one.
                int pid = p->pid;
                kfree(p->kstack);
                p->kstack = nullptr;
                freevm(p->page_directory);
                p->pid     = 0;
                p->parent  = nullptr;
                p->name[0] = 0;
                p->killed  = 0;
                p->state   = UNUSED;
                release(&ptable.lock);
                return pid;
            }
        }

        // No point waiting if we don't have any children.
        if (!havekids || curproc->killed) {
            release(&ptable.lock);
            return -1;
        }

        // Wait for children to exit.  (See wakeup1 call in proc_exit.)
        sleep(curproc, &ptable.lock);
    }
}

/**
 * @brief Wake all processes sleeping on a channel.
 *
 * Requires ptable.lock to be held.
 */
static void wakeup1(void *chan)
{
    for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->state == SLEEPING && p->chan == chan) {
            p->state = RUNNABLE;
        }
    }
}

/**
 * @brief Wake any processes sleeping on @p chan.
 *
 * Acquires and releases ptable.lock internally.
 */
void wakeup(void *chan)
{
    acquire(&ptable.lock);
    wakeup1(chan);
    release(&ptable.lock);
}

/**
 * @brief Terminate the current process and release associated resources.
 *
 * Transitions the process to ZOMBIE until the parent collects the status with
 * wait.
 */
void exit(void)
{
    struct proc *curproc = current_process();

    if (curproc == initproc) {
        panic("init exiting");
    }

    // Close all open files.
    for (int fd = 0; fd < NOFILE; fd++) {
        if (curproc->ofile[fd]) {
            fileclose(curproc->ofile[fd]);
            curproc->ofile[fd] = nullptr;
        }
    }

    curproc->cwd->iops->iput(curproc->cwd);
    curproc->cwd = nullptr;
    memset(curproc->cwd_path, 0, MAX_FILE_PATH);

    acquire(&ptable.lock);

    // Parent might be sleeping in wait().
    wakeup1(curproc->parent);

    // Pass abandoned children to init.
    for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->parent == curproc) {
            p->parent = initproc;
            if (p->state == ZOMBIE) {
                wakeup1(initproc);
            }
        }
    }

    // Jump into the scheduler, never to return.
    curproc->state = ZOMBIE;
    switch_to_scheduler();
    panic("zombie exit");
}

/**
 * @brief Request termination of the process with the given PID.
 *
 * The process transitions to killed state and exits upon returning to user
 * space.
 *
 * @param pid Process identifier to terminate.
 * @return 0 on success, -1 if no such process exists.
 */
int kill(int pid)
{
    acquire(&ptable.lock);
    for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->pid == pid) {
            p->killed = 1;
            // Wake process from sleep if necessary.
            if (p->state == SLEEPING) {
                p->state = RUNNABLE;
            }
            release(&ptable.lock);
            return 0;
        }
    }
    release(&ptable.lock);
    return -1;
}

/**
 * @brief Atomically release a lock and put the current process to sleep.
 *
 * @param chan Sleep channel identifier.
 * @param lk Lock currently held by the caller.
 */
void sleep(void *chan, struct spinlock *lk)
{
    struct proc *p = current_process();

    if (p == nullptr) {
        sti();
        return;
    }

    if (lk == nullptr) {
        panic("sleep without lk");
    }

    // Must acquire ptable.lock in order to
    // change p->state and then call switch_to_scheduler.
    // Once we hold ptable.lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup runs with ptable.lock locked),
    // so it's okay to release lk.
    if (lk != &ptable.lock) {
        acquire(&ptable.lock);
        release(lk);
    }
    // Go to sleep.
    p->chan  = chan;
    p->state = SLEEPING;

    switch_to_scheduler();

    // Tidy up.
    p->chan = nullptr;

    // Reacquire original lock.
    if (lk != &ptable.lock) {
        release(&ptable.lock);
        acquire(lk);
    }
}

/** @brief Give up the CPU for one scheduling round. */
void yield(void)
{
    acquire(&ptable.lock);
    current_process()->state = RUNNABLE;
    switch_to_scheduler();
    release(&ptable.lock);
}

/**
 * @brief Return the index of the current CPU.
 *
 * Must be called with interrupts disabled.
 */
int cpu_index()
{
    return current_cpu() - cpus;
}

/**
 * @brief Return a pointer to the cpu structure for the running CPU.
 *
 * Interrupts must be disabled to prevent migration during lookup.
 */
struct cpu *current_cpu(void)
{
    if (read_eflags() & FL_IF) {
        panic("current_cpu called with interrupts enabled\n");
    }

    int apicid = lapicid();
    // APIC IDs are not guaranteed to be contiguous. Maybe we should have
    // a reverse map, or reserve a register to store &cpus[i].
    for (int i = 0; i < ncpu; ++i) {
        if (cpus[i].apicid == apicid) {
            return &cpus[i];
        }
    }
    panic("unknown apicid\n");
}

/** @brief Initialize the process table lock. */
void pinit(void)
{
    initlock(&ptable.lock, "ptable");
}