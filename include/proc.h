#pragma once
#include "types.h"
#include "mmu.h"
#include "param.h"
#include "spinlock.h"

// Per-CPU state
struct cpu
{
    u8 apicid;                    // Local APIC ID
    struct context *scheduler;    // swtch() here to enter scheduler
    struct task_state task_state; // Used by x86 to find stack for interrupt
    struct segdesc gdt[NSEGS];    // x86 global descriptor table
    volatile u32 started;         // Has the CPU started?
    int ncli;                     // Depth of pushcli nesting.

    int interrupts_enabled; // Were interrupts enabled before pushcli?
    int time_slice_ticks;   // Remaining timer ticks before preemption
    struct proc *proc;      // The process running on this cpu or null
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
    u32 edi;
    u32 esi;
    u32 ebx;
    u32 ebp;
    u32 eip;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

#define VMA_FLAG_HEAP 0x1

struct vm_area
{
    u32 start;
    u32 end;
    int prot;
    int flags;
    struct file *file;
    u32 file_offset;
    struct vm_area *next;
};

// Per-process state
struct proc
{
    u32 brk;                      // Current end of process memory (heap)
    pde_t *page_directory;        // Page table
    char *kstack;                 // Bottom of the kernel stack for this process
    enum procstate state;         // Process state
    int pid;                      // Process ID
    struct proc *parent;          // Parent process
    struct trapframe *trap_frame; // Trap frame for current syscall
    struct context *context;      // switch_context() here to run process
    void *chan;                   // If non-zero, sleeping on chan
    int killed;                   // If non-zero, have been killed
    struct file *ofile[NOFILE];   // Open files
    struct inode *cwd;            // Current directory
    struct vm_area *vma_list;     // Head of VM area list
    char cwd_path[MAX_FILE_PATH];
    char name[16]; // Process name (debugging)
    struct proc *next;
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap


struct ptable_t
{
    struct spinlock lock;
    int active_count;
    struct proc proc[NPROC];
};

struct vm_area *proc_ensure_heap_vma(struct proc *p);
void proc_free_vmas(struct proc *p);
int proc_clone_vmas(struct proc *dst, struct proc *src);
