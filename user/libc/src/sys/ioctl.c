#include <stdarg.h>
#include <sys/ioctl.h>
#include <syscall.h>

int ioctl(int file_descriptor, int request, ...)
{
    va_list args;
    va_start(args, request);
    void *arg = va_arg(args, void *);
    va_end(args);
    return syscall3(SYSCALL_IOCTL, file_descriptor, request, arg);
}