#pragma once
#include <stdint.h>

#define CR0_PE 0x00000001
#define CR0_MP 0x00000002
#define CR0_EM 0x00000004
#define CR0_TS 0x00000008
#define CR0_NE 0x00000020
#define CR0_WP 0x00010000
#define CR0_AM 0x00040000
#define CR0_NW 0x20000000
#define CR0_CD 0x40000000
#define CR0_PG 0x80000000

#define CR4_VME        0x00000001
#define CR4_PVI        0x00000002
#define CR4_TSD        0x00000004
#define CR4_DE         0x00000008
#define CR4_PSE        0x00000010
#define CR4_PAE        0x00000020
#define CR4_MCE        0x00000040
#define CR4_PGE        0x00000080
#define CR4_PCE        0x00000100
#define CR4_OSFXSR     0x00000200
#define CR4_OSXMMEXCPT 0x00000400
#define CR4_UMIP       0x00000800
#define CR4_LA57       0x00001000

#define MSR_IA32_PAT 0x00000277
#define MSR_IA32_APIC_BASE 0x0000001B

static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t value)
{
    uint32_t lo = (uint32_t)value;
    uint32_t hi = (uint32_t)(value >> 32);
    asm volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

// Interrupt flags enabled
#define EFLAGS_CF 0x00000001
#define EFLAGS_PF 0x00000004
#define EFLAGS_AF 0x00000010
#define EFLAGS_ZF 0x00000040
#define EFLAGS_SF 0x00000080
#define EFLAGS_TF 0x00000100
#define EFLAGS_IF 0x00000200
#define EFLAGS_DF 0x00000400
#define EFLAGS_OF 0x00000800
#define EFLAGS_IOPL 0x00003000
#define EFLAGS_NT 0x00004000
#define EFLAGS_RF 0x0001'0000
#define EFLAGS_VM 0x0002'0000
#define EFLAGS_AC 0x0004'0000
#define EFLAGS_VIF 0x0008'0000
#define EFLAGS_VIP 0x0010'0000
#define EFLAGS_ID 0x0020'0000
#define EFLAGS_AI 0x8000'0000
#define EFLAGS_ALL                                                                                                     \
    (EFLAGS_CF | EFLAGS_PF | EFLAGS_AF | EFLAGS_ZF | EFLAGS_SF | EFLAGS_TF | EFLAGS_IF | EFLAGS_DF | EFLAGS_OF |       \
     EFLAGS_IOPL | EFLAGS_NT | EFLAGS_RF | EFLAGS_VM | EFLAGS_AC | EFLAGS_VIF | EFLAGS_VIP | EFLAGS_ID | EFLAGS_AI)

static inline uint32_t read_eflags(void)
{
    uint32_t eflags;
    asm volatile("pushfl; popl %0" : "=r"(eflags));
    return eflags;
}

static inline uint32_t read_cr0(void)
{
    uint32_t val;
    asm volatile("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline void write_cr0(uint32_t val)
{
    asm volatile("mov %0, %%cr0" : : "r"(val) : "memory");
}

static inline uint32_t read_cr4(void)
{
    uint32_t val;
    asm volatile("mov %%cr4, %0" : "=r"(val));
    return val;
}

static inline void write_cr4(uint32_t val)
{
    asm volatile("mov %0, %%cr4" : : "r"(val) : "memory");
}

static inline uint32_t read_esp(void)
{
    uint32_t esp;
    asm volatile("movl %%esp, %0" : "=r"(esp));
    return esp;
}

static inline void load_gs(uint16_t v)
{
    asm volatile("movw %0, %%gs" : : "r"(v));
}

/// @brief Disable interrupts
static inline void cli(void)
{
    asm volatile("cli");
}

static inline void ltr(uint16_t sel)
{
    asm volatile("ltr %0" : : "r"(sel));
}

/// @brief Enable interrupts
static inline void sti(void)
{
    asm volatile("sti");
}

static inline void hlt(void)
{
    asm volatile("hlt");
}

static inline void pause(void)
{
    asm volatile("pause");
}

static inline void fninit(void)
{
    asm volatile("fninit");
}

static inline void fxsave(void *state)
{
    asm volatile("fxsave (%0)" : : "r"(state) : "memory");
}

static inline void fxrstor(const void *state)
{
    asm volatile("fxrstor (%0)" : : "r"(state) : "memory");
}

static inline uint32_t xchg(volatile uint32_t *addr, uint32_t newval)
{
    uint32_t result;

    asm volatile("lock; xchgl %0, %1" : "+m"(*addr), "=a"(result) : "1"(newval) : "cc");
    return result;
}

static inline uint32_t read_cr2(void)
{
    uint32_t val;
    asm volatile("movl %%cr2,%0" : "=r"(val));
    return val;
}

/// @brief Load a new page directory
static inline void lcr3(uint32_t val)
{
    asm volatile("movl %0,%%cr3" : : "r"(val));
}
