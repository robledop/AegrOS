#include <assert.h>
#include <config.h>
#include <elf.h>
#include <idt.h>
#include <kernel.h>
#include <kernel_heap.h>
#include <memory.h>
#include <paging.h>
#include <process.h>
#include <serial.h>
#include <spinlock.h>
#include <status.h>
#include <string.h>
#include <thread.h>
#include <timer.h>
#include <tss.h>
#include <x86.h>
#include <x86gprintrin.h>

extern void trap_return(void);
static void on_timer();
void thread_starting(void);

struct process_list process_list;
extern struct tss_entry tss_entry;
extern struct page_directory *kernel_page_directory;
static uint64_t instr_per_ns;
struct thread *current_thread = nullptr;
struct thread *idle_thread    = nullptr;
static uint64_t last_time     = 0;
static uint64_t idle_time     = 0;
static uint64_t idle_start    = 0;

/// @brief Nanoseconds since boot
uint64_t get_cpu_time_ns()
{
    return (__rdtsc()) / instr_per_ns;
}

int get_processes(struct process_info **proc_info)
{
    int count = 0;
    for (int i = 0; i < MAX_PROCESSES - 1; i++) {
        struct process *p = process_list.processes[i];
        if (p) {
            count++;
        }
    }

    *proc_info = kzalloc(count * sizeof(struct process_info));

    for (int i = 0; i < MAX_PROCESSES - 1; i++) {
        struct process *p = process_list.processes[i];
        if (!p) {
            continue;
        }
        struct process_info info = {
            .pid      = p->pid,
            .priority = p->priority,
            .state    = p->thread->state,
        };
        strncpy(info.file_name, p->file_name, MAX_PATH_LENGTH);
        memcpy(*proc_info + i, &info, sizeof(struct process_info));
    }

    return count;
}

/// @brief Switch to the scheduler context (the scheduler() function)
void switch_to_scheduler(void)
{
    ASSERT(holding(&process_list.lock));
    ASSERT(get_cpu()->ncli == 1);
    ASSERT(get_current_thread()->state != TASK_RUNNING);
    ASSERT((read_eflags() & EFLAGS_IF) != EFLAGS_IF);

    const bool interrupts_enabled = get_cpu()->interrupts_enabled;
    switch_context(&current_thread->context, get_cpu()->scheduler);
    get_cpu()->interrupts_enabled = interrupts_enabled;
}

struct thread *get_current_thread(void)
{
    return current_thread;
}

/// @brief Try to discover how many instructions are executed per nanosecond
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

static inline void stack_push_pointer(char **stack_pointer, const uintptr_t value)
{
    *(uintptr_t *)stack_pointer -= sizeof(uintptr_t); // make room for a pointer
    **(uintptr_t **)stack_pointer = value;            // push the pointer onto the stack
}

struct thread *thread_allocate(void (*entry)(void), const enum thread_state state, const char *name,
                               enum thread_mode mode)
{
    struct thread *new_thread = kzalloc(sizeof(struct thread));
    if (new_thread == NULL) {
        panic("Unable to allocate memory for new thread struct.");
        return nullptr;
    }

    // ReSharper disable once CppDFAMemoryLeak
    uint8_t *kernel_stack = kzalloc(KERNEL_STACK_SIZE);
    if (kernel_stack == nullptr) {
        kfree(new_thread);
        panic("Unable to allocate memory for new thread stack.");
        return nullptr;
    }

    new_thread->kernel_stack  = kernel_stack;
    auto kernel_stack_pointer = (char *)(kernel_stack + KERNEL_STACK_SIZE);

    if (mode == USER_MODE) {
        // When trap_return is called, it will use the trap frame to restore the state of the thread and enter user mode
        // so we need to change cs, ds, es, ss, eflags, esp, and eip to point to the program we want to run
        kernel_stack_pointer -= sizeof(*new_thread->trap_frame);
        new_thread->trap_frame = (struct interrupt_frame *)kernel_stack_pointer;

        new_thread->trap_frame->cs     = USER_CODE_SELECTOR;
        new_thread->trap_frame->ds     = USER_DATA_SELECTOR;
        new_thread->trap_frame->es     = USER_DATA_SELECTOR;
        new_thread->trap_frame->ss     = USER_DATA_SELECTOR;
        new_thread->trap_frame->eflags = EFLAGS_IF;
        new_thread->trap_frame->esp    = USER_STACK_TOP;
        new_thread->trap_frame->eip    = PROGRAM_VIRTUAL_ADDRESS;

        stack_push_pointer(&kernel_stack_pointer, (uintptr_t)trap_return);
    } else if (mode == KERNEL_MODE) {
        stack_push_pointer(&kernel_stack_pointer, (size_t)entry);

        struct page_directory *page_directory =
            paging_create_directory(PDE_IS_WRITABLE | PDE_IS_PRESENT | PDE_SUPERVISOR);

        new_thread->page_directory = page_directory;
    }

    kernel_stack_pointer -= sizeof *new_thread->context;
    new_thread->context = (struct context *)kernel_stack_pointer;
    memset(new_thread->context, 0, sizeof *new_thread->context);
    new_thread->context->eip = (uintptr_t)thread_starting;

    new_thread->next      = nullptr;
    new_thread->state     = state;
    new_thread->time_used = 0;
    new_thread->name      = name;
    return new_thread;
}

void set_idle_thread(struct thread *thread)
{
    idle_thread = thread;
}

/// @brief Executed at the beginning of each thread
/// It runs in the context of the thread
void thread_starting(void)
{
    // Still holding process_list.lock from scheduler.
    release(&process_list.lock);

    // We can run some initialization code here.
    // This gets executed in the context of the new thread.

    // Return to "caller", actually trap_return
}

struct cpu *get_cpu()
{
    return cpu;
}

// Wake up all processes sleeping on chan.
// The process_list lock must be held.
static void wakeup1(const void *chan)
{
    kernel_page();
    for (int i = 0; i < MAX_PROCESSES; i++) {
        struct process *p = process_list.processes[i];
        if (p && p->thread && p->thread->state == TASK_SLEEPING && p->thread->wait_channel == chan) {
            p->thread->state        = TASK_READY;
            p->thread->wait_channel = nullptr;
        }
    }

    if (current_thread && current_thread == idle_thread) {
        idle_thread->state = TASK_READY;
        switch_to_scheduler();
    }
}

// Wake up all processes sleeping on chan.
void wakeup(const void *chan)
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

    // TODO: Close all open files.

    curproc->current_directory = nullptr;

    acquire(&process_list.lock);

    // Parent might be sleeping in wait().
    wakeup1(curproc->parent);

    // TODO: Pass abandoned children to init.

    // Jump into the scheduler, never to return.
    curproc->thread->state = TASK_STOPPED;
    switch_to_scheduler();

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

    // Must acquire process_list.lock in order to
    // change p->state and then call sched.
    // Once we hold process_list.lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup runs with process_list.lock locked),
    // so it's okay to release lk.
    if (lk != &process_list.lock) {
        // DOC: sleeplock0
        acquire(&process_list.lock); // DOC: sleeplock1
        release(lk);
    }
    // Go to sleep.
    p->thread->wait_channel = chan;
    p->thread->state        = TASK_SLEEPING;

    switch_to_scheduler();

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
                curproc->thread->wait_channel = nullptr;
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

// Kill the process with the given pid.  Process won't exit until it returns  to user space.
int kill(const int pid)
{
    acquire(&process_list.lock);
    for (int i = 0; i < MAX_PROCESSES - 1; i++) {
        struct process *p = process_list.processes[i];
        if (p && p->pid == pid) {
            p->killed = true;
            if (p->thread->state == TASK_SLEEPING) {
                p->thread->state = TASK_READY;
            }
            if (p == current_process()) {
                p->thread->state = TASK_READY;
                switch_to_scheduler();
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
    get_current_thread()->state     = TASK_READY;
    get_current_thread()->time_used = 0;

    switch_to_scheduler();
    release(&process_list.lock);
}


void scheduler(void)
{
    struct cpu *current_cpu = get_cpu();
    // current_cpu->proc       = nullptr;
    current_thread = nullptr;

    // ReSharper disable once CppDFAEndlessLoop
    for (;;) {
        // Enable interrupts on this processor.
        sti();
        process_list.active_count = 0;

        // Loop over process table looking for process to run.
        acquire(&process_list.lock);
        for (int i = 0; i < MAX_PROCESSES - 1; i++) {
            struct process *p = process_list.processes[i];
            if ((p && p->killed && p->parent == nullptr) || (p && p->parent && p->parent->thread == nullptr)) {
                const int pid = p->pid;
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
            current_thread = p->thread;

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
            current_thread = nullptr;
        }

        if (process_list.active_count == 0) {
            if (idle_thread) {
                current_thread        = idle_thread;
                current_thread->state = TASK_RUNNING;
                idle_start            = get_cpu_time_ns();
                switch_context(&(current_cpu->scheduler), current_thread->context);
                idle_start = idle_start - get_cpu_time_ns();
                idle_time += idle_start;
                current_thread = nullptr;
            }
        }

        release(&process_list.lock);
    }
}

void thread_update_time()
{
    const uint64_t current_time = get_cpu_time_ns();
    const uint64_t delta        = current_time - last_time;
    if (current_thread == idle_thread) {
        idle_time += delta;
    } else if (current_thread) {
        current_thread->time_used += delta;
    }
    last_time = current_time;
}

static void on_timer()
{
    thread_update_time();

    // if (process_list.count == 0) {
    //     start_shell(0);
    // }

    // Force process exit if it has been killed and is in user space.
    // (If it is still executing in the kernel, let it keep running
    // until it gets to the regular system call return.)
    if (current_process() && current_process()->killed &&
        current_process()->thread->trap_frame->cs == USER_CODE_SELECTOR) {
        exit();
    }

    // Force process to give up CPU on clock tick.
    // If interrupts were on while locks held, would need to check nlock.
    if (current_process() && current_process()->thread->state == TASK_RUNNING &&
        current_thread->time_used >= TIME_SLICE_SIZE) {
        yield();
    }

    // Check if the process has been killed since we yielded
    if (current_process() && current_process()->killed) {
        exit();
    }

    // If no one is holding the process_list lock, wake up the sleeping processes
    if (!holding(&process_list.lock)) {
        wakeup((void *)&timer_tick);
    }
}

void *thread_peek_stack_item(const struct thread *thread, const int index)
{
    const uintptr_t *stack_pointer = (uintptr_t *)thread->trap_frame->esp;
    current_thread_page();
    auto const result = (void *)stack_pointer[index];
    kernel_page();
    return result;
}

int copy_string_from_thread(const struct thread *thread, const void *virtual, void *physical, const size_t max)
{
    int res   = 0;
    char *tmp = kzalloc(max);
    if (!tmp) {
        dbgprintf("Failed to allocate memory for string\n");
        res = -ENOMEM;
        goto out;
    }

    const uint32_t old_entry = paging_get(thread->page_directory, tmp);

    paging_map(thread->page_directory, tmp, tmp, PDE_IS_WRITABLE | PDE_IS_PRESENT | PDE_SUPERVISOR);
    paging_switch_directory(thread->page_directory);
    strncpy(tmp, virtual, max);
    kernel_page();
    res = paging_set(thread->page_directory, tmp, old_entry);
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

int thread_init(struct thread *thread, struct process *process)
{
    thread->process                 = process;
    thread->process->page_directory = paging_create_directory(PDE_IS_PRESENT | PDE_SUPERVISOR);
    thread->page_directory          = thread->process->page_directory;

    ASSERT(thread->page_directory == thread->process->page_directory);
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

struct thread *thread_create(struct process *process)
{
    int res = 0;

    struct thread *thread = thread_allocate(nullptr, TASK_READY, process->file_name, USER_MODE);

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
        if (thread && thread->kernel_stack) {
            kfree(thread->kernel_stack);
        }
        if (thread) {
            kfree(thread);
        }
        return ERROR(res);
    }

    return thread;
}

void *thread_virtual_to_physical_address(const struct thread *thread, void *virtual_address)
{
    return paging_get_physical_address(thread->process->page_directory, virtual_address);
}

void current_thread_page()
{
    if (current_thread) {
        set_user_mode_segments();
        paging_switch_directory(current_thread->page_directory);
    }
}

void threads_init(void)
{
    cpu = kzalloc(sizeof(struct cpu));
    initlock(&process_list.lock, "process table");
    discover_cpu_speed();

    last_time = get_cpu_time_ns();

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
    ASSERT(process_list.lock.locked);

    process_list.processes[pid] = process;
    process_list.count += process ? 1 : -1;
}
