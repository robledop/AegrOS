#pragma once

#include <attributes.h>
#include <config.h>
#include <paging.h>
#include <spinlock.h>
#include <stddef.h>
#include <stdint.h>

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
    bool interrupts_enabled;   // Were interrupts enabled before pushcli?
    _Alignas(16) uint8_t fpu_state[512];
    // struct process *proc;    // The process running on this cpu or null
};

enum thread_state {
    TASK_RUNNING = 0,
    TASK_READY   = 1,
    TASK_SLEEPING,
    TASK_BLOCKED,
    TASK_STOPPED,
    TASK_PAUSED,
};

enum thread_mode { KERNEL_MODE, USER_MODE };

struct thread {
    int priority;
    enum thread_state state;
    uint64_t time_used;
    struct interrupt_frame *trap_frame;
    struct thread *next;
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
    _Alignas(16) uint8_t fpu_state[512];
};

struct process_list {
    struct spinlock lock;
    struct process *processes[MAX_PROCESSES];
    int count;
    int active_count;
};

struct process_info {
    uint16_t pid;
    int priority;
    char file_name[MAX_PATH_LENGTH];
    enum thread_state state;
};

extern struct thread *current_thread;

#define TIME_SLICE_SIZE (10 * 1000 * 1000ULL)
#define DPL_USER 0x3 // User DPL;

void *thread_peek_stack_item(const struct thread *thread, int index);
void set_user_mode_segments(void);
int copy_string_from_thread(const struct thread *thread, const void *virtual, void *physical, size_t max);
NON_NULL struct thread *thread_create(struct process *process);
NON_NULL void *thread_virtual_to_physical_address(const struct thread *thread, void *virtual_address);

void current_thread_page();
void switch_to_scheduler(void);
int wait(void);
void sleep(void *chan, struct spinlock *lk);
void wakeup(const void *chan);
void yield(void);
void exit(void);
int kill(int pid);
void threads_init(void);
struct thread *get_current_thread(void);
struct cpu *get_cpu();
[[noreturn]] void scheduler(void);

void switch_context(struct context **, struct context *);
struct thread *thread_allocate(void (*entry)(void), enum thread_state state, const char *name, enum thread_mode mode);
int process_get_free_pid();

struct process *process_get(int pid);
void process_set(int pid, struct process *process);
void set_idle_thread(struct thread *thread);
int thread_init(struct thread *thread, struct process *process);
uint64_t get_cpu_time_ns();

int get_processes(struct process_info **proc_info);
