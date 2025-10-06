#pragma once
#include <mmu.h>
#include <config.h>
#include <stdint.h>
#include <x86.h>
#include <spinlock.h>

// Per-CPU state
struct cpu
{
    uint8_t apicid; // Local APIC ID
    struct context* scheduler; // swtch() here to enter scheduler
    struct task_state task_state; // Used by x86 to find stack for interrupt
    struct segdesc gdt[NSEGS]; // x86 global descriptor table
    volatile uint32_t started; // Has the CPU started?
    int ncli; // Depth of pushcli nesting.

    // intena
    int interrupts_enabled; // Were interrupts enabled before pushcli?
    struct proc* proc; // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc.),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context
{
    uint32_t edi;
    uint32_t esi;
    uint32_t ebx;
    uint32_t ebp;
    uint32_t eip;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc
{
    uint32_t size; // Size of process memory (bytes)
    uintptr_t * page_directory; // Page table
    char* kstack; // Bottom of the kernel stack for this process
    enum procstate state; // Process state
    int pid; // Process ID
    struct proc* parent; // Parent process
    struct trapframe* trap_frame; // Trap frame for current syscall
    struct context* context; // swtch() here to run process
    void* chan; // If non-zero, sleeping on chan
    int killed; // If non-zero, have been killed
    struct file* ofile[NOFILE]; // Open files
    struct inode* cwd; // Current directory
    char name[16]; // Process name (debugging)
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap

int cpuid(void);
void exit(void);
int fork(void);
int growproc(int);
int kill(int);
struct cpu* mycpu(void);
struct proc* myproc();
void pinit(void);
void procdump(void);
void scheduler(void) __attribute__((noreturn));
void sched(void);
void setproc(struct proc*);
void sleep(void*, struct spinlock*);
void user_init(void);
int wait(void);
void wakeup(void*);
void yield(void);


// swtch.S
// was swtch()
void switch_context(struct context**, struct context*);
