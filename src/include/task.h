#ifndef TASK_H
#define TASK_H

#include "config.h"
#include "paging.h"
#include "process.h"

struct registers
{
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;

    uint32_t ip;
    uint32_t cs;
    uint32_t flags;
    uint32_t esp;
    uint32_t ss;
};

struct process;
struct interrupt_frame;

struct task
{
    struct page_directory *page_directory;
    struct registers registers;
    struct process *process;
    struct task *next;
    struct task *prev;
};

void task_run_first_ever_task();

struct task *task_create(struct process *process);
int task_free(struct task *task);
struct task *task_current();
struct task *task_get_next();
void restore_general_purpose_registers(struct registers *registers);
void task_return(struct registers *registers);
void user_registers();
int task_switch(struct task *task);
int task_page();
void task_current_save_state(struct interrupt_frame *interrupt_frame);

#endif