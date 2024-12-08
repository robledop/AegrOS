#include <printf.h>
#include <serial.h>
#include <stdint.h>
#include <syscall.h>
#include <thread.h>

void *sys_print(void)
{
    uint32_t size       = get_integer_argument(0);
    const void *message = get_pointer_argument(1);
    if (!message) {
        warningf("message is null\n");
        return nullptr;
    }

    char buffer[size];

    copy_string_from_thread(current_thread, message, buffer, sizeof(buffer));

    printf(buffer);

    return nullptr;
}
