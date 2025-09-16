#pragma once
#include "gui/window.h"

typedef void (*putchar_func_t)(char c);

void vesa_terminal_init(putchar_func_t func);
#ifdef PIXEL_RENDERING
void putchar(char c);
#endif
