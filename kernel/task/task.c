#include <assert.h>
#include <config.h>
#include <elf.h>
#include <idt.h>
#include <kernel.h>
#include <kernel_heap.h>
#include <memory.h>
#include <paging.h>
#include <printf.h>
#include <process.h>
#include <serial.h>
#include <spinlock.h>
#include <status.h>
#include <string.h>
#include <task.h>
#include <termcolors.h>
#include <timer.h>
#include <tss.h>
#include <x86.h>
#include <x86gprintrin.h>

void task_starting(void);
extern void trap_return(void);

struct {
    struct spinlock lock;
    struct process *processes[MAX_PROCESSES];
    int count;
    int active_count;
} process_list;

extern struct tss_entry tss_entry;
extern struct page_directory *kernel_page_directory;

struct task *current_task = nullptr;
struct task *idle_task    = nullptr;
static uint64_t instr_per_ns;

void sched(void)
{

    ASSERT(holding(&process_list.lock));
    ASSERT(get_cpu()->ncli == 1);
    ASSERT(get_current_task()->state != TASK_RUNNING);
    ASSERT((read_eflags() & FL_IF) != FL_IF);

    const bool interrupts_enabled = get_cpu()->interrupts_enabled;
    switch_context(&current_task->context, get_cpu()->scheduler);
    get_cpu()->interrupts_enabled = interrupts_enabled;
}

struct task *get_current_task(void)
{
    return current_task;
}

static void discover_cpu_speed()
{
    sti();
    const uint32_t curr_tick = timer_tick;
    uint64_t curr_rtsc       = __rdtsc();
    while (timer_tick != curr_tick + 1) {
        // wait for the next tick
    }
    curr_rtsc    = __rdtsc() - curr_rtsc;
    instr_per_ns = curr_rtsc / 1000000;
    if (instr_per_ns == 0) {
        instr_per_ns = 1;
    }
    cli();
}

static inline uint64_t get_cpu_time_ns()
{
    return (__rdtsc()) / instr_per_ns;
}

static void on_timer();

static inline void stack_push_pointer(char **stack_pointer, const uintptr_t value)
{
    *(uintptr_t *)stack_pointer -= sizeof(uintptr_t); // make room for a pointer
    **(uintptr_t **)stack_pointer = value;            // push the pointer onto the stack
}

struct task *create_task(void (*entry)(void), struct task *storage, const enum task_state state, const char *name,
                         enum task_mode mode)
{
    struct task *new_task = storage;
    if (storage == nullptr) {
        new_task = (struct task *)kzalloc(sizeof(struct task));
    }
    if (new_task == NULL) {
        panic("Unable to allocate memory for new task struct.");
        return nullptr;
    }

    // ReSharper disable once CppDFAMemoryLeak
    uint8_t *kernel_stack = kzalloc(KERNEL_STACK_SIZE);
    if (kernel_stack == nullptr) {
        panic("Unable to allocate memory for new task stack.");
        return nullptr;
    }

    new_task->kernel_stack    = kernel_stack;
    auto kernel_stack_pointer = (char *)(kernel_stack + KERNEL_STACK_SIZE);

    if (mode == USER_MODE) {
        // When trap_return is called, it will use the trap frame to restore the state of the task and enter user mode
        // so we need to change cs, ds, es, ss, eflags, esp, and eip to point to the program we want to run
        kernel_stack_pointer -= sizeof(*new_task->trap_frame);
        new_task->trap_frame = (struct interrupt_frame *)kernel_stack_pointer;

        new_task->trap_frame->cs     = USER_CODE_SELECTOR;
        new_task->trap_frame->ds     = USER_DATA_SELECTOR;
        new_task->trap_frame->es     = USER_DATA_SELECTOR;
        new_task->trap_frame->ss     = USER_DATA_SELECTOR;
        new_task->trap_frame->eflags = EFLAGS_IF;
        new_task->trap_frame->esp    = USER_STACK_TOP;
        new_task->trap_frame->eip    = PROGRAM_VIRTUAL_ADDRESS;

        stack_push_pointer(&kernel_stack_pointer, (uintptr_t)trap_return);
    } else if (mode == KERNEL_MODE) {
        stack_push_pointer(&kernel_stack_pointer, (size_t)entry);

        struct page_directory *page_directory = paging_create_directory(
            PAGING_DIRECTORY_ENTRY_IS_WRITABLE | PAGING_DIRECTORY_ENTRY_IS_PRESENT | PAGING_DIRECTORY_ENTRY_SUPERVISOR);

        new_task->page_directory = page_directory;
    }

    kernel_stack_pointer -= sizeof *new_task->context;
    new_task->context = (struct context *)kernel_stack_pointer;
    memset(new_task->context, 0, sizeof *new_task->context);
    new_task->context->eip = (uintptr_t)task_starting;

    new_task->next      = nullptr;
    new_task->state     = state;
    new_task->time_used = 0;
    new_task->name      = name;
    return new_task;
}

void tasks_set_idle_task(struct task *task)
{
    idle_task = task;
}

void task_starting(void)
{
    // Still holding process_list.lock from scheduler.
    release(&process_list.lock);

    // We can run some initialization code here.
    // This gets executed in the context of the new task.

    // Return to "caller", actually trap_return
}

struct cpu *get_cpu()
{
    return cpu;
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void wakeup1(void *chan)
{
    for (int i = 0; i < MAX_PROCESSES; i++) {
        struct process *p = process_list.processes[i];
        if (p && p->thread && p->thread->state == TASK_SLEEPING && p->thread->wait_channel == chan) {
            p->thread->state = TASK_READY;
        }
    }

    if (current_task == idle_task) {
        idle_task->state = TASK_READY;
        sched();
    }
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan)
{
    acquire(&process_list.lock);
    wakeup1(chan);
    release(&process_list.lock);
}


// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void)
{
    struct process *curproc = current_process();
    struct process *p;
    int fd;

    // Close all open files.
    // for (fd = 0; fd < NOFILE; fd++) {
    //     if (curproc->ofile[fd]) {
    //         fileclose(curproc->ofile[fd]);
    //         curproc->ofile[fd] = 0;
    //     }
    // }

    // iput(curproc->cwd);
    curproc->current_directory = nullptr;

    acquire(&process_list.lock);

    // Parent might be sleeping in wait().
    wakeup1(curproc->parent);

    // Pass abandoned children to init.
    // for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    //     if (p->parent == curproc) {
    //         p->parent = initproc;
    //         if (p->state == ZOMBIE)
    //             wakeup1(initproc);
    //     }
    // }

    // Jump into the scheduler, never to return.
    curproc->thread->state = TASK_STOPPED;
    sched();
    panic("zombie exit");
}


// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
    struct process *p = current_process();

    if (p == nullptr) {
        panic("sleep");
    }

    if (lk == nullptr) {
        panic("sleep without lk");
    }

    // Must acquire ptable.lock in order to
    // change p->state and then call sched.
    // Once we hold ptable.lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup runs with ptable.lock locked),
    // so it's okay to release lk.
    if (lk != &process_list.lock) {
        // DOC: sleeplock0
        acquire(&process_list.lock); // DOC: sleeplock1
        release(lk);
    }
    // Go to sleep.
    p->thread->wait_channel = chan;
    p->thread->state        = TASK_SLEEPING;

    sched();

    // Tidy up.
    p->thread->wait_channel = nullptr;

    // Reacquire original lock.
    if (lk != &process_list.lock) {
        // DOC: sleeplock2
        release(&process_list.lock);
        acquire(lk);
    }
}


// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void)
{
    struct process *curproc = current_process();

    acquire(&process_list.lock);
    for (;;) {
        // Scan through table looking for exited children.
        int havekids = 0;
        for (int i = 0; i < MAX_PROCESSES - 1; i++) {
            struct process *p = process_list.processes[i];
            if (!p || p->parent != curproc) {
                continue;
            }
            havekids = 1;
            if (p->thread->state == TASK_STOPPED) {
                // Found one.
                int pid = p->pid;
                process_zombify(p);
                process_set(pid, nullptr);
                release(&process_list.lock);
                return pid;
            }
        }

        // No point waiting if we don't have any children.
        if (!havekids || curproc->killed) {
            release(&process_list.lock);
            return -1;
        }

        // Wait for children to exit.  (See wakeup1 call in proc_exit.)
        sleep(curproc, &process_list.lock); // DOC: wait-sleep
    }
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid)
{
    struct process *p;

    acquire(&process_list.lock);
    for (int i = 0; i < MAX_PROCESSES - 1; i++) {
        p = process_list.processes[i];
        if (p && p->pid == pid) {
            p->killed = true;
            if (p->thread->state == TASK_SLEEPING) {
                p->thread->state = TASK_READY;
            }
            if (p == current_process()) {
                p->thread->state = TASK_READY;
                sched();
            }

            release(&process_list.lock);
            return 0;
        }
    }
    release(&process_list.lock);
    return -1;
}


// Give up the CPU for one scheduling round.
void yield(void)
{
    acquire(&process_list.lock);
    get_current_task()->state = TASK_READY;
    sched();
    release(&process_list.lock);
}


void scheduler(void)
{
    struct process *p;
    struct cpu *current_cpu = get_cpu();
    // current_cpu->proc       = nullptr;
    current_task = nullptr;

    // ReSharper disable once CppDFAEndlessLoop
    for (;;) {
        // Enable interrupts on this processor.
        sti();
        process_list.active_count = 0;

        // Loop over process table looking for process to run.
        acquire(&process_list.lock);
        for (int i = 0; i < MAX_PROCESSES - 1; i++) {
            p = process_list.processes[i];
            if (p && p->killed && p->parent == nullptr) {
                int pid = p->pid;
                process_zombify(p);
                process_set(pid, nullptr);
            }

            if (!p || !p->thread || p->thread->state != TASK_READY) {
                continue;
            }

            process_list.active_count++;

            // Switch to chosen process.  It is the process's job
            // to release process_list.lock and then reacquire it
            // before jumping back to us.
            // current_cpu->proc = p;
            current_task = p->thread;
            // switch_uvm(p);
            pushcli();
            write_tss(5, KERNEL_DATA_SELECTOR, (uintptr_t)p->thread->kernel_stack + KERNEL_STACK_SIZE);
            paging_switch_directory(p->page_directory);
            popcli();

            p->thread->state = TASK_RUNNING;

            switch_context(&(current_cpu->scheduler), p->thread->context);

            kernel_page();

            // Process is done running for now.
            // It should have changed its p->state before coming back.
            // current_cpu->proc = 0;
            current_task = nullptr;
        }

        if (process_list.active_count == 0) {
            if (idle_task) {
                current_task        = idle_task;
                current_task->state = TASK_RUNNING;
                switch_context(&(current_cpu->scheduler), current_task->context);
                current_task = nullptr;
            }
        }

        release(&process_list.lock);
    }
}

static void on_timer()
{
    // if (cpuid() == 0)
    // {
    //     acquire(&tickslock);
    //     ticks++;
    //     wakeup(&ticks);
    //     release(&tickslock);
    // }


    if (process_list.count == 0) {
        start_shell(0);
    }

    // Force process exit if it has been killed and is in user space.
    // (If it is still executing in the kernel, let it keep running
    // until it gets to the regular system call return.)
    if (current_process() && current_process()->killed &&
        current_process()->thread->trap_frame->cs == USER_CODE_SELECTOR) {
        exit();
    }

    // Force process to give up CPU on clock tick.
    // If interrupts were on while locks held, would need to check nlock.
    if (current_process() && current_process()->thread->state == TASK_RUNNING) {
        yield();
    }

    // Check if the process has been killed since we yielded
    if (current_process() && current_process()->killed) {
        exit();
    }
}

void *task_peek_stack_item(const struct task *task, const int index)
{
    const uintptr_t *stack_pointer = (uintptr_t *)task->trap_frame->esp;
    current_task_page();
    auto const result = (void *)stack_pointer[index];
    kernel_page();
    return result;
}

int copy_string_from_task(const struct task *task, const void *virtual, void *physical, const size_t max)
{
    int res   = 0;
    char *tmp = kzalloc(max);
    if (!tmp) {
        dbgprintf("Failed to allocate memory for string\n");
        res = -ENOMEM;
        goto out;
    }

    const uint32_t old_entry = paging_get(task->page_directory, tmp);

    paging_map(task->page_directory,
               tmp,
               tmp,
               PAGING_DIRECTORY_ENTRY_IS_WRITABLE | PAGING_DIRECTORY_ENTRY_IS_PRESENT |
                   PAGING_DIRECTORY_ENTRY_SUPERVISOR);
    paging_switch_directory(task->page_directory);
    strncpy(tmp, virtual, max);
    kernel_page();
    res = paging_set(task->page_directory, tmp, old_entry);
    if (res < 0) {
        dbgprintf("Failed to set page\n");
        res = -EIO;
        goto out_free;
    }

    strncpy(physical, tmp, max);

out_free:
    kfree(tmp);
out:
    return res;
}

int thread_init(struct task *thread, struct process *process)
{
    thread->process = process;
    thread->process->page_directory =
        paging_create_directory(PAGING_DIRECTORY_ENTRY_IS_PRESENT | PAGING_DIRECTORY_ENTRY_SUPERVISOR);
    thread->page_directory = thread->process->page_directory;

    if (!thread->process->page_directory) {
        panic("Failed to create page directory");
        return -ENOMEM;
    }

    switch (process->file_type) {
    case PROCESS_FILE_TYPE_BINARY:
        thread->trap_frame->eip = PROGRAM_VIRTUAL_ADDRESS;
        break;
    case PROCESS_FILE_TYPE_ELF:
        thread->trap_frame->eip = elf_header(process->elf_file)->e_entry;
        break;
    default:
        panic("Unknown process file type");
        break;
    }

    return ALL_OK;
}

struct task *thread_create(struct process *process)
{
    int res = 0;

    struct task *thread = create_task(nullptr, nullptr, TASK_READY, process->file_name, USER_MODE);

    if (!thread) {
        panic("Failed to allocate memory for thread\n");
        res = -ENOMEM;
        goto out;
    }

    res = thread_init(thread, process);
    if (res != ALL_OK) {
        panic("Failed to initialize thread\n");
        goto out;
    }

out:
    if (ISERR(res)) {
        // thread_free(thread);
        return ERROR(res);
    }

    return thread;
}

void *thread_virtual_to_physical_address(const struct task *thread, void *virtual_address)
{
    return paging_get_physical_address(thread->process->page_directory, virtual_address);
}


void current_task_page()
{
    if (current_task) {
        set_user_mode_segments();
        paging_switch_directory(current_task->page_directory);
    }
}

void tasks_init(void)
{
    cpu = kzalloc(sizeof(struct cpu));
    initlock(&process_list.lock, "process table");
    timer_register_callback(on_timer);
}

int process_get_free_pid()
{
    acquire(&process_list.lock);

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_list.processes[i] == nullptr) {
            release(&process_list.lock);
            return i;
        }
    }

    release(&process_list.lock);

    return -EINSTKN;
}

struct process *process_get(const int pid)
{
    return process_list.processes[pid];
}

void process_set(const int pid, struct process *process)
{
    bool to_acquire = false;
    if (!holding(&process_list.lock)) {
        to_acquire = true;
    }
    if (to_acquire) {
        acquire(&process_list.lock);
    }

    process_list.processes[pid] = process;
    process_list.count += process ? 1 : -1;

    if (to_acquire) {
        release(&process_list.lock);
    }
}
