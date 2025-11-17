#pragma once
#include "mmu.h"
#include "types.h"
// Routines to let C code use special x86 instructions.

#define MSR_IA32_PAT 0x277

// Vendor strings from CPUs.
#define CPUID_VENDOR_AMD "AuthenticAMD"
#define CPUID_VENDOR_AMD_OLD "AMDisbetter!" // Early engineering samples of AMD K5 processor
#define CPUID_VENDOR_INTEL "GenuineIntel"
#define CPUID_VENDOR_VIA "VIA VIA VIA "
#define CPUID_VENDOR_TRANSMETA "GenuineTMx86"
#define CPUID_VENDOR_TRANSMETA_OLD "TransmetaCPU"
#define CPUID_VENDOR_CYRIX "CyrixInstead"
#define CPUID_VENDOR_CENTAUR "CentaurHauls"
#define CPUID_VENDOR_NEXGEN "NexGenDriven"
#define CPUID_VENDOR_UMC "UMC UMC UMC "
#define CPUID_VENDOR_SIS "SiS SiS SiS "
#define CPUID_VENDOR_NSC "Geode by NSC"
#define CPUID_VENDOR_RISE "RiseRiseRise"
#define CPUID_VENDOR_VORTEX "Vortex86 SoC"
#define CPUID_VENDOR_AO486 "MiSTer AO486"
#define CPUID_VENDOR_AO486_OLD "GenuineAO486"
#define CPUID_VENDOR_ZHAOXIN "  Shanghai  "
#define CPUID_VENDOR_HYGON "HygonGenuine"
#define CPUID_VENDOR_ELBRUS "E2K MACHINE "
#define CPUID_VENDOR_QEMU "TCGTCGTCGTCG"
#define CPUID_VENDOR_KVM " KVMKVMKVM  "
#define CPUID_VENDOR_VMWARE "VMwareVMware"
#define CPUID_VENDOR_VIRTUALBOX "VBoxVBoxVBox"
#define CPUID_VENDOR_XEN "XenVMMXenVMM"
#define CPUID_VENDOR_HYPERV "Microsoft Hv"
#define CPUID_VENDOR_PARALLELS " prl hyperv "
#define CPUID_VENDOR_BHYVE "bhyve bhyve "
#define CPUID_VENDOR_QNX " QNXQVMBSQG "

enum
{
    CPUID_FEAT_ECX_SSE3 = 1 << 0,
    CPUID_FEAT_ECX_PCLMUL = 1 << 1,
    CPUID_FEAT_ECX_DTES64 = 1 << 2,
    CPUID_FEAT_ECX_MONITOR = 1 << 3,
    CPUID_FEAT_ECX_DS_CPL = 1 << 4,
    CPUID_FEAT_ECX_VMX = 1 << 5,
    CPUID_FEAT_ECX_SMX = 1 << 6,
    CPUID_FEAT_ECX_EST = 1 << 7,
    CPUID_FEAT_ECX_TM2 = 1 << 8,
    CPUID_FEAT_ECX_SSSE3 = 1 << 9,
    CPUID_FEAT_ECX_CID = 1 << 10,
    CPUID_FEAT_ECX_SDBG = 1 << 11,
    CPUID_FEAT_ECX_FMA = 1 << 12,
    CPUID_FEAT_ECX_CX16 = 1 << 13,
    CPUID_FEAT_ECX_XTPR = 1 << 14,
    CPUID_FEAT_ECX_PDCM = 1 << 15,
    CPUID_FEAT_ECX_PCID = 1 << 17,
    CPUID_FEAT_ECX_DCA = 1 << 18,
    CPUID_FEAT_ECX_SSE4_1 = 1 << 19,
    CPUID_FEAT_ECX_SSE4_2 = 1 << 20,
    CPUID_FEAT_ECX_X2APIC = 1 << 21,
    CPUID_FEAT_ECX_MOVBE = 1 << 22,
    CPUID_FEAT_ECX_POPCNT = 1 << 23,
    CPUID_FEAT_ECX_TSC = 1 << 24,
    CPUID_FEAT_ECX_AES = 1 << 25,
    CPUID_FEAT_ECX_XSAVE = 1 << 26,
    CPUID_FEAT_ECX_OSXSAVE = 1 << 27,
    CPUID_FEAT_ECX_AVX = 1 << 28,
    CPUID_FEAT_ECX_F16C = 1 << 29,
    CPUID_FEAT_ECX_RDRAND = 1 << 30,
    CPUID_FEAT_EDX_FPU = 1 << 0,
    CPUID_FEAT_EDX_VME = 1 << 1,
    CPUID_FEAT_EDX_DE = 1 << 2,
    CPUID_FEAT_EDX_PSE = 1 << 3,
    CPUID_FEAT_EDX_TSC = 1 << 4,
    CPUID_FEAT_EDX_MSR = 1 << 5,
    CPUID_FEAT_EDX_PAE = 1 << 6,
    CPUID_FEAT_EDX_MCE = 1 << 7,
    CPUID_FEAT_EDX_CX8 = 1 << 8,
    CPUID_FEAT_EDX_APIC = 1 << 9,
    CPUID_FEAT_EDX_SEP = 1 << 11,
    CPUID_FEAT_EDX_MTRR = 1 << 12,
    CPUID_FEAT_EDX_PGE = 1 << 13,
    CPUID_FEAT_EDX_MCA = 1 << 14,
    CPUID_FEAT_EDX_CMOV = 1 << 15,
    CPUID_FEAT_EDX_PAT = 1 << 16,
    CPUID_FEAT_EDX_PSE36 = 1 << 17,
    CPUID_FEAT_EDX_PSN = 1 << 18,
    CPUID_FEAT_EDX_CLFLUSH = 1 << 19,
    CPUID_FEAT_EDX_DS = 1 << 21,
    CPUID_FEAT_EDX_ACPI = 1 << 22,
    CPUID_FEAT_EDX_MMX = 1 << 23,
    CPUID_FEAT_EDX_FXSR = 1 << 24,
    CPUID_FEAT_EDX_SSE = 1 << 25,
    CPUID_FEAT_EDX_SSE2 = 1 << 26,
    CPUID_FEAT_EDX_SS = 1 << 27,
    CPUID_FEAT_EDX_HTT = 1 << 28,
    CPUID_FEAT_EDX_TM = 1 << 29,
    CPUID_FEAT_EDX_IA64 = 1 << 30,
};

static inline void stosb(void *addr, int data, int cnt)
{
    __asm__ volatile("cld; rep stosb" :
        "=D" (addr), "=c" (cnt) :
        "0" (addr), "1" (cnt), "a" (data) :
        "memory", "cc");
}

static inline void stosl(void *addr, int data, int cnt)
{
    __asm__ volatile("cld; rep stosl" :
        "=D" (addr), "=c" (cnt) :
        "0" (addr), "1" (cnt), "a" (data) :
        "memory", "cc");
}

struct segdesc;

extern void gdt_flush();

static inline void lgdt(struct segdesc *p, int size)
{
    volatile u16 pd[3];

    pd[0] = size - 1;
    pd[1] = (u32)p;
    pd[2] = (u32)p >> 16;

    __asm__ volatile("lgdt (%0)" : : "r" (pd));
    gdt_flush();
}


struct gate_desc;

static inline void lidt(struct gate_desc *p, int size)
{
    volatile u16 pd[3];

    pd[0] = size - 1;
    pd[1] = (u32)p;
    pd[2] = (u32)p >> 16;

    __asm__ volatile("lidt (%0)" : : "r" (pd));
}

static inline void ltr(u16 sel)
{
    __asm__ volatile("ltr %0" : : "r" (sel));
}

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

static inline u32 read_eflags(void)
{
    u32 eflags;
    __asm__ volatile("pushfl; popl %0" : "=r" (eflags));
    return eflags;
}

static inline void load_gs(u16 v)
{
    __asm__ volatile("movw %0, %%gs" : : "r" (v));
}

static inline void cli(void)
{
    __asm__ volatile("cli");
}

static inline void sti(void)
{
    __asm__ volatile("sti");
}

static inline u64 rdmsr(u32 msr)
{
    u32 lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((u64)hi << 32) | lo;
}

static inline void wrmsr(u32 msr, u64 value)
{
    u32 lo = (u32)value;
    u32 hi = (u32)(value >> 32);
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static inline void fxsave(void *state)
{
    __asm__ volatile("fxsave (%0)" : : "r"(state) : "memory");
}

static inline void fxrstor(const void *state)
{
    __asm__ volatile("fxrstor (%0)" : : "r"(state) : "memory");
}

static inline u32 rcr0(void)
{
    u32 val;
    __asm__ volatile("movl %%cr0,%0" : "=r"(val));
    return val;
}

static inline void lcr0(u32 val)
{
    __asm__ volatile("movl %0,%%cr0" : : "r"(val));
}

static inline u32 rcr4(void)
{
    u32 val;
    __asm__ volatile("movl %%cr4,%0" : "=r"(val));
    return val;
}

static inline void lcr4(u32 val)
{
    __asm__ volatile("movl %0,%%cr4" : : "r"(val));
}

static inline u32 xchg(volatile u32 *addr, u32 newval)
{
    u32 result;

    // The + in "+m" denotes a read-modify-write operand.
    __asm__ volatile("lock; xchgl %0, %1" :
        "+m" (*addr), "=a" (result) :
        "1" (newval) :
        "cc");
    return result;
}

static inline u32 rcr2(void)
{
    u32 val;
    __asm__ volatile("movl %%cr2,%0" : "=r" (val));
    return val;
}

static inline void lcr3(u32 val)
{
    __asm__ volatile("movl %0,%%cr3" : : "r" (val));
}

static inline void clts(void)
{
    __asm__ volatile("clts");
}

static inline void hlt(void)
{
    __asm__ volatile("hlt");
}

static inline void cpuid(u32 reg, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx)
{
    asm volatile("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "0"(reg));
}

static int cpuid_string(int code, int where[4])
{
    asm volatile("cpuid" : "=a"(*where), "=b"(*(where + 0)), "=d"(*(where + 1)), "=c"(*(where + 2)) : "a"(code));
    return (int)where[0];
}

static inline char *cpu_string()
{
    static char s[16] = "CPUID_ERROR!";
    cpuid_string(0, (int *)(s));
    return s;
}


void cpu_print_info();

// Layout of the trap frame built on the stack by the
// hardware and by trapasm.S, and passed to trap().
struct trapframe
{
    // registers as pushed by pusha
    u32 edi;
    u32 esi;
    u32 ebp;
    u32 oesp; // useless & ignored
    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;

    // rest of trap frame
    u16 gs;
    u16 padding1;
    u16 fs;
    u16 padding2;
    u16 es;
    u16 padding3;
    u16 ds;
    u16 padding4;
    u32 trapno;

    // below here defined by x86 hardware
    u32 err;
    u32 eip;
    u16 cs;
    u16 padding5;
    u32 eflags;

    // below here only when crossing rings, such as from user to kernel
    u32 esp;
    u16 ss;
    u16 padding6;
};
