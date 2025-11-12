#pragma once
#include "types.h"

void vesa_terminal_init(void);
void putchar(char c);
void vesa_write(const char *data, u32 length);
u16 vesa_terminal_columns(void);
u16 vesa_terminal_rows(void);
