#include <assert.h>
#include <stdarg.h>
#include <termcolors.h>
#include <kernel.h>
#include <printf.h>

void assert(const char* snippet, const char* file, int line, const char* message, ...)
{
    printf(KBWHT "\nassert failed %s:%d %s " KWHT, file, line, snippet);

    if (*message)
    {
        va_list arg;
        va_start(arg, message);
        const char* data = va_arg(arg, char *);
        printf(data, arg);
        panic(message);
    }
    panic("Assertion failed");
}
