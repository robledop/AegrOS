#include "misc.h"
#include "idt.h"
#include "keyboard.h"
#include "task.h"
#include "terminal.h"
#include "process.h"
#include <stdint.h>
#include "kernel.h"
#include "serial.h"
#include "string.h"

void *isr80h_command0_sum(struct interrupt_frame *frame)
{
    int value2 = (int)task_get_stack_item(task_current(), 1);
    int value1 = (int)task_get_stack_item(task_current(), 0);
    return (void *)(value1 + value2);
}

void *isr80h_command1_print(struct interrupt_frame *frame)
{
    void *message = task_get_stack_item(task_current(), 0);
    char buffer[1024];

    copy_string_from_task(task_current(), message, buffer, sizeof(buffer));

    kprintf("%s", buffer);

    return NULL;
}

void *isr80h_command2_getkey(struct interrupt_frame *frame)
{
    char c = keyboard_pop();
    return (void *)(int)c;
}

void *isr80h_command3_putchar(struct interrupt_frame *frame)
{
    char c = (char)(int)task_get_stack_item(task_current(), 0);
    terminal_write_char(c, 0x0F, 0x00);
    return NULL;
}

void *isr80h_command4_malloc(struct interrupt_frame *frame)
{
    size_t size = (size_t)task_get_stack_item(task_current(), 0);
    return process_malloc(task_current()->process, size);
}

void *isr80h_command5_free(struct interrupt_frame *frame)
{
    void *ptr = task_get_stack_item(task_current(), 0);
    process_free(task_current()->process, ptr);
    return NULL;
}

void *isr80h_command6_process_start(struct interrupt_frame *frame)
{
    void *process_file = task_get_stack_item(task_current(), 0);
    char *file_name[MAX_PATH_LENGTH];
    int res = copy_string_from_task(task_current(), process_file, file_name, sizeof(file_name));
    if (res < 0)
    {
        dbgprintf("Failed to copy string from task\n");
        goto out;
    }

    // TODO: Handle paths properly
    char path[MAX_PATH_LENGTH];
    strncpy(path, "0:/", sizeof(path));
    strncpy(path + 3, (const char *)file_name, sizeof(path) - 3);

    struct process *process = NULL;
    res = process_load_switch((const char *)path, &process);
    if (res < 0)
    {
        dbgprintf("Failed to load process %s\n", file_name);
        kprintf("\nFailed to load process %s\n", path);
        goto out;
    }

    task_switch(process->task);
    task_return(&process->task->registers);

out:
    return (void *)res;
}
