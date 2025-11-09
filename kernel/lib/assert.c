#include "assert.h"
#include <stdarg.h>
#include "defs.h"
#include "printf.h"

void assert(char *snippet, char *file, int line, char *message, ...)
{
    printf("\nassert failed %s:%d %s\n", file, line, snippet);

    if (*message) {
        va_list arg;
        va_start(arg, message);
        char *data = va_arg(arg, char *);
        printf(data, arg);
        panic(message);
    }
    panic("Assertion failed\n");
}