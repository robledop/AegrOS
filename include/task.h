#pragma once

#include <config.h>
#include <list.h>
#include <paging.h>
#include <spinlock.h>
#include <stdint.h>

// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebx;
    uint32_t ebp;
    uint32_t eip;
};

struct cpu {
    struct context *scheduler; // switch_context() here to enter scheduler
    int ncli;                  // Depth of pushcli nesting.

    bool interrupts_enabled; // Were interrupts enabled before pushcli?
    struct process *proc;    // The process running on this cpu or null
};

enum task_state {
    TASK_RUNNING = 0,
    TASK_READY   = 1,
    TASK_SLEEPING,
    TASK_BLOCKED,
    TASK_STOPPED,
    TASK_PAUSED,
    TASK_STATE_COUNT,
};

enum task_mode { KERNEL_MODE, USER_MODE };

struct task {
    int priority;
    enum task_state state;
    uint64_t time_used;
    struct interrupt_frame *trap_frame;
    struct task *next;
    uint64_t wakeup_time;
    void *wait_channel;
    const char *name;
    struct context *context;
    struct page_directory *page_directory;
    int rand_id;
    char file_name[MAX_PATH_LENGTH];
    int wait_pid;
    int exit_code;
    uint32_t size;
    struct process *process;
    void *kernel_stack;
};

static struct cpu *cpu = nullptr;
extern struct task *current_task;

#define TASK_ONLY if (current_task != NULL)
#define TIME_SLICE_SIZE (1 * 1000 * 1000ULL)
#define FL_IF 0x00000200 // Interrupt Enabled
#define DPL_USER 0x3     // User DPL;

void *task_peek_stack_item(const struct task *task, int index);
void set_user_mode_segments(void);
int copy_string_from_task(const struct task *task, const void *virtual, void *physical, size_t max);
__attribute__((nonnull)) struct task *thread_create(struct process *process);
__attribute__((nonnull)) void *thread_virtual_to_physical_address(const struct task *task, void *virtual_address);

void current_task_page();
void sched(void);
int wait(void);
void sleep(void *chan, struct spinlock *lk);
void wakeup(void *chan);
void yield(void);
void exit(void);
int kill(int pid);
void tasks_init(void);
struct task *get_current_task(void);
struct cpu *get_cpu();
[[noreturn]] void scheduler(void);

void switch_context(struct context **, struct context *);
struct task *create_task(void (*entry)(void), struct task *storage, enum task_state state, const char *name,
                         enum task_mode mode);
int process_get_free_pid();

struct process *process_get(int pid);
void process_set(int pid, struct process *process);
void tasks_set_idle_task(struct task *task);
