#include <printf.h>
#include <process.h>
#include <string.h>
#include <syscall.h>
#include <termcolors.h>

#include "kernel_heap.h"

// TODO: Re-implement this in userland using a device file
void *sys_ps(void)
{
    struct process_info *proc_info = nullptr;

    int count = get_processes(&proc_info);

    printf(KBBLU "\n %-5s%-15s%-12s%-12s\n" KWHT, "PID", "Name", "Priority", "State");
    for (int i = 0; i < count; i++) {
        constexpr int col               = 15;
        const struct process_info *info = &proc_info[i];

        char state[col + 1];
        switch (info->state) {
        case TASK_RUNNING:
            strncpy(state, "RUNNING", col);
            break;
        case TASK_STOPPED:
            strncpy(state, "ZOMBIE", col);
            break;
        case TASK_SLEEPING:
            strncpy(state, "SLEEPING", col);
            break;
        case TASK_BLOCKED:
            strncpy(state, "BLOCKED", col);
            break;
        case TASK_READY:
            strncpy(state, "READY", col);
            break;
        default:
            strncpy(state, "UNKNOWN", col);
            break;
        }

        printf(" %-5u%-15s%-12u%-12s\n", info->pid, info->file_name, info->priority, state);
    }

    kfree(proc_info);

    return nullptr;
}
