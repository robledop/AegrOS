#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include <stddef.h>

void terminal_clear();
void print(const char *str);
void print_line(const char *str);

#endif