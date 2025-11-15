#pragma once
#include <stddef.h>

#if !defined(__cplusplus) && !defined(__bool_true_false_are_defined)
#if defined(__STDC_VERSION__)
#if __STDC_VERSION__ >= 202000L
/* C23 provides bool/true/false natively. */
#define __bool_true_false_are_defined 1
#endif
#endif
#if !defined(__bool_true_false_are_defined)
#if defined(__is_identifier)
#if __is_identifier(bool)
typedef _Bool bool;
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif
#define __bool_true_false_are_defined 1
#endif
#else
#ifndef bool
typedef _Bool bool;
#endif
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif
#define __bool_true_false_are_defined 1
#endif
#endif
#endif

typedef unsigned long long int u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef long long int i64;
typedef int i32;
typedef short i16;
typedef char i8;
typedef int ssize_t;
typedef u32 pde_t;
typedef u32 uptr;
