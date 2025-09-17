#pragma once

typedef void (*putchar_func_t)(char c);
typedef void (*clear_screen_func_t)();

void vesa_terminal_init(putchar_func_t func, clear_screen_func_t clear_func);
#ifdef PIXEL_RENDERING
void putchar(char c);
#endif
