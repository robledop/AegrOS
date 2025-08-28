#include <stdio.h>
#include <sys/ioctl.h>
#include <syscall.h>
#include <termios.h>
#include <unistd.h>

int open(const char name[static 1], const int mode)
{
    return syscall2(SYSCALL_OPEN, name, mode);
}

int close(int fd)
{
    return syscall1(SYSCALL_CLOSE, fd);
}

int read(int fd, void *ptr, unsigned int size)
{
    return syscall4(SYSCALL_READ, ptr, size, 1, fd);
}

int write(int fd, const char *buffer, size_t size)
{
    return syscall3(SYSCALL_WRITE, fd, buffer, size);
}

int lseek(int fd, int offset, int whence)
{
    return syscall3(SYSCALL_LSEEK, fd, offset, whence);
}

int ftruncate(int fd, int length)
{

    return 0;
}

int isatty(int fd)
{
    int result = ioctl(fd, TCGETS);
    return result == 0;
}