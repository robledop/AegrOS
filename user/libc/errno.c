#include "errno.h"
#include "printf.h"
#include "status.h"

int errno;

void perror(const char *s)
{
    printf("%s %s\n", s, strerror(errno));
}