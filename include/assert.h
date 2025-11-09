#pragma once

void assert(char *snippet, char *file, int line, char *message, ...);

#ifdef DEBUG
#define ASSERT(cond, ...)                                                                                              \
if (!(cond))                                                                                                       \
assert(#cond, __FILE__, __LINE__, #__VA_ARGS__ __VA_OPT__(, )##__VA_ARGS__)
#else
#define ASSERT(cond, ...)
#endif