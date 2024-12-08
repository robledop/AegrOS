#include <process.h>
#include <syscall.h>
#include <thread.h>

void *sys_get_program_arguments(void)
{
    const struct process *process = get_current_thread()->process;
    void *virtual_address         = thread_peek_stack_item(get_current_thread(), 0);
    if (!virtual_address) {
        return nullptr;
    }
    struct process_arguments *arguments = thread_virtual_to_physical_address(get_current_thread(), virtual_address);

    arguments->argc = process->arguments.argc;
    arguments->argv = process->arguments.argv;

    return nullptr;
}
