#include <kernel.h>
#include <kernel_heap.h>
#include <printf.h>
#include <process.h>
#include <spinlock.h>
#include <string.h>
#include <syscall.h>
#include <thread.h>
#include <tss.h>
#include <vfs.h>

struct spinlock exec_lock = {};

static void free_command_arguments(struct command_argument *argument)
{
    while (argument) {
        struct command_argument *next = argument->next;
        kfree(argument);
        argument = next;
    }
}


// TODO: Simplify this
// TODO: Fix memory leaks
void *sys_exec(void)
{
    pushcli();

    const void *path_ptr       = thread_peek_stack_item(get_current_thread(), 1);
    void *argv_virtual_address = thread_peek_stack_item(get_current_thread(), 0);

    char **argv_ptr = nullptr;

    if (argv_virtual_address) {
        argv_ptr = thread_virtual_to_physical_address(get_current_thread(), argv_virtual_address);
    }

    char path[MAX_PATH_LENGTH] = {0};
    copy_string_from_thread(get_current_thread(), path_ptr, path, sizeof(path));

    char *args[256] = {nullptr};
    if (argv_ptr) {
        int argc = 0;
        for (int i = 0; i < 256; i++) {
            if (argv_ptr[i] == nullptr) {
                break;
            }
            char *arg = kzalloc(256);
            copy_string_from_thread(get_current_thread(), argv_ptr[i], arg, 256);
            args[i] = arg;
            argc++;
        }
    }

    struct command_argument *root_argument = kzalloc(sizeof(struct command_argument));
    strncpy(root_argument->argument, path, sizeof(root_argument->argument));
    strncpy(root_argument->current_directory,
            current_process()->current_directory,
            sizeof(root_argument->current_directory));

    struct command_argument *arguments = parse_command(args);
    root_argument->next                = arguments;

    // Free the arguments
    for (int i = 0; i < 256; i++) {
        if (args[i] == nullptr) {
            break;
        }
        kfree(args[i]);
    }

    struct process *process = current_process();
    process_free_allocations(process);
    process->current_directory = nullptr;
    process->arguments.argv    = nullptr;
    process->arguments.argc    = 0;
    process_free_program_data(process);
    process_unmap_memory(process);
    kfree(process->user_stack);
    process->user_stack                       = nullptr;
    void *old_kernel_stack                    = process->thread->kernel_stack;
    struct page_directory *old_page_directory = process->page_directory;
    struct interrupt_frame *old_trap_frame    = process->thread->trap_frame;
    process->page_directory                   = nullptr;
    kfree(process->thread);
    process->thread = nullptr;

    char full_path[MAX_PATH_LENGTH] = {0};
    if (istrncmp(path, "/", 1) != 0) {
        strcat(full_path, "/bin/");
        strcat(full_path, path);
    } else {
        strcat(full_path, path);
    }

    const int res = process_load_data(full_path, process);
    if (res < 0) {
        printf("Result: %d\n", res);
        free_command_arguments(root_argument);
        if (old_page_directory) {
            paging_free_directory(old_page_directory);
        }
        release(&exec_lock);
        return (void *)res;
    }
    void *program_stack_pointer = kzalloc(USER_STACK_SIZE);
    strncpy(process->file_name, full_path, sizeof(process->file_name));
    process->user_stack = program_stack_pointer; // Physical address of the stack for the process

    struct thread *thread = kzalloc(sizeof(struct thread));
    thread->kernel_stack  = old_kernel_stack;
    thread->trap_frame    = old_trap_frame;
    thread_init(thread, process);
    if (ISERR(thread)) {
        panic("Failed to create thread");
    }
    process->thread = thread;

    if (old_page_directory) {
        paging_free_directory(old_page_directory);
    }

    process_map_memory(process);
    const int inject_res = process_inject_arguments(process, root_argument);
    free_command_arguments(root_argument);
    if (inject_res < 0) {
        release(&exec_lock);
        return (void *)inject_res;
    }

    process_set(process->pid, process);
    process->thread->state = TASK_READY;
    write_tss(5, KERNEL_DATA_SELECTOR, (uintptr_t)process->thread->kernel_stack + KERNEL_STACK_SIZE);
    paging_switch_directory(process->page_directory);

    current_thread = process->thread;


    popcli();
    return nullptr;
}
