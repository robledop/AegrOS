#include "echo.hpp"
#include <stdio.h>

void Echo::print(int argc, char **argv)
{
    putchar('\n');
    for (int i = 1; i < argc; i++) {
        printf("%s ", argv[i]);
    }
}