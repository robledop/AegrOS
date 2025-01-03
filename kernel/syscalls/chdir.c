#include <config.h>
#include <process.h>
#include <syscall.h>
#include <thread.h>

/// @brief Change the current working directory
void *sys_chdir(void)
{
    const void *path_ptr = get_pointer_argument(0);
    char path[MAX_PATH_LENGTH];

    copy_string_from_thread(get_current_thread(), path_ptr, path, sizeof(path));
    return (void *)process_set_current_directory(get_current_thread()->process, path);
}
