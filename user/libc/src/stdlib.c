#include <config.h>
#include <errno.h>
#include <os.h>
#include <status.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

extern atexit_function atexit_functions[32];

void *malloc(size_t size)
{
    return (void *)syscall1(SYSCALL_MALLOC, size);
}

/// @brief Allocate memory for an array of elements and set the memory to zero
/// @param number_of_items number of elements
/// @param size size of each element
void *calloc(const int number_of_items, const int size)
{
    return (void *)syscall2(SYSCALL_CALLOC, number_of_items, size);
}

void *realloc(void *ptr, int size)
{
    return (void *)syscall2(SYSCALL_REALLOC, ptr, size);
}

void free(void *ptr)
{
    syscall1(SYSCALL_FREE, ptr);
}

int waitpid(const int pid, const int *return_status)
{
    return syscall2(SYSCALL_WAIT_PID, pid, return_status);
}

int wait(const int *return_status)
{
    return syscall2(SYSCALL_WAIT_PID, -1, return_status);
}

void reboot()
{
    syscall0(SYSCALL_REBOOT);
}

void shutdown()
{
    syscall0(SYSCALL_SHUTDOWN);
}

int fork()
{
    return syscall0(SYSCALL_FORK);
}

int exec(const char path[static 1], const char **args)
{
    return syscall2(SYSCALL_EXEC, path, args);
}

int getpid()
{
    return syscall0(SYSCALL_GET_PID);
}

int create_process(const char path[static 1], const char *current_directory)
{
    char buffer[512];
    strncpy(buffer, path, sizeof(buffer));
    struct command_argument *root_command_argument = os_parse_command(buffer, sizeof(buffer));
    if (root_command_argument == NULL) {
        return -EBADPATH;
    }

    strncpy(root_command_argument->current_directory, current_directory, MAX_PATH_LENGTH);

    return syscall1(SYSCALL_CREATE_PROCESS, root_command_argument);
}

void sleep(const uint32_t milliseconds)
{
    syscall1(SYSCALL_SLEEP, milliseconds);
}

void yield()
{
    syscall0(SYSCALL_YIELD);
}

void ps()
{
    syscall0(SYSCALL_PS);
}

void memstat()
{
    syscall0(SYSCALL_MEMSTAT);
}

void exit(int status)
{
    for (int i = 0; i < 32; i++) {
        if (atexit_functions[i] != nullptr) {
            atexit_functions[i]();
        }
    }

    errno = status;
    syscall0(SYSCALL_EXIT);
}

int atexit(void (*function)())
{
    for (int i = 0; i < 32; i++) {
        if (atexit_functions[i] == nullptr) {
            atexit_functions[i] = function;
            return 0;
        }
    }

    return -1;
}


void abort(void)
{
    syscall0(SYSCALL_EXIT);
}