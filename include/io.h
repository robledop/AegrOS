#pragma once

#include "types.h"

static inline u8 read8(const u64 addr)
{
    return *(volatile u8 *)(uptr)addr;
}

static inline u16 read16(const u64 addr)
{
    return *(volatile u16 *)(uptr)addr;
}

static inline u32 read32(const u64 addr)
{
    return *(volatile u32 *)(uptr)addr;
}

static inline u64 read64(const u64 addr)
{
    return *(volatile u64 *)(uptr)addr;
}

static inline void write8(const u64 addr, const u8 data)
{
    *(volatile u8 *)(uptr)addr = data;
}

static inline void write16(const u64 addr, const u16 data)
{
    *(volatile u16 *)(uptr)addr = data;
}

static inline void write32(const u64 addr, const u32 data)
{
    *(volatile u64 *)(uptr)addr = data;
}

static inline void write64(const u64 addr, const u64 data)
{
    *(volatile u64 *)(uptr)addr = data;
}

static inline void stack_push_pointer(char **stack_pointer, const u32 value)
{
    *(u32 *)stack_pointer -= sizeof(u32); // make room for a pointer
    **(u32 **)stack_pointer = value;      // push the pointer onto the stack
}

static inline u8 inb(u16 port)
{
    u8 data;

    __asm__ volatile("in %1,%0" : "=a" (data) : "d" (port));
    return data;
}

static inline u32 inl(u16 p)
{
    u32 r;
    asm volatile("inl %%dx, %%eax" : "=a"(r) : "d"(p));
    return r;
}


static inline u16 inw(u16 p)
{
    u16 r;
    __asm__ volatile("inw %%dx, %%ax" : "=a"(r) : "d"(p));
    return r;
}

static inline void insl(int port, void *addr, int cnt)
{
    __asm__ volatile("cld; rep insl" :
        "=D" (addr), "=c" (cnt) :
        "d" (port), "0" (addr), "1" (cnt) :
        "memory", "cc");
}

static inline void outb(u16 port, u8 data)
{
    __asm__ volatile("out %0,%1" : : "a" (data), "d" (port));
}

static inline void outw(u16 port, u16 data)
{
    __asm__ volatile("out %0,%1" : : "a" (data), "d" (port));
}

static inline void outsl(int port, const void *addr, int cnt)
{
    __asm__ volatile("cld; rep outsl" :
        "=S" (addr), "=c" (cnt) :
        "d" (port), "0" (addr), "1" (cnt) :
        "cc");
}

static inline void outl(u16 portid, u32 value)
{
    asm volatile("outl %%eax, %%dx" ::"d"(portid), "a"(value));
}
