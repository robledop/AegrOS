#pragma once

#include <stat.h>

#ifndef __AE_MKDIR_COMPAT
#define __AE_MKDIR_COMPAT

static inline int __ae_mkdir_nomode(const char *path)
{
    extern int mkdir(const char *);
    return mkdir(path);
}

static inline int __ae_mkdir_with_mode(const char *path, int mode)
{
    (void)mode;
    return __ae_mkdir_nomode(path);
}

#define __AE_MKDIR_GET_MACRO(_1,_2,NAME,...) NAME
#define mkdir(...) __AE_MKDIR_GET_MACRO(__VA_ARGS__, __ae_mkdir_with_mode, __ae_mkdir_nomode)(__VA_ARGS__)

#endif
