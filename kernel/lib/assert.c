#include "assert.h"
#include <stdarg.h>
#include "defs.h"

void assert(char *snippet, char *file, int line, char *message, ...)
{
    cprintf("\nassert failed %s:%d %s\n", file, line, snippet);

    if (*message) {
        va_list arg;
        va_start(arg, message);
        char *data = va_arg(arg, char *);
        cprintf(data, arg);
        panic(message);
    }
    panic("Assertion failed\n");
}