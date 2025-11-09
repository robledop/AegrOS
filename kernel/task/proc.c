#include "debug.h"
#include "string.h"
#include "termcolors.h"
#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "ext2.h"
#include "io.h"
#include "printf.h"
#include "scheduler.h"

extern struct ptable_t ptable;

/** @brief Pointer to the very first user process. */
struct proc *initproc;
extern pde_t *kpgdir;

/** @brief Next PID to assign during process creation. */
int nextpid = 1;
extern void forkret(void);
extern void trapret(void);


/**
 * @brief Obtain the currently running process structure.
 *
 * Interrupts are temporarily disabled to avoid rescheduling during the read.
 */
struct proc *current_process(void)
{
    pushcli();
    struct cpu *c  = current_cpu();
    struct proc *p = c->proc;
    popcli();
    return p;
}

static struct proc *init_proc(struct proc *p)
{
    // Allocate kernel stack.
    if ((p->kstack = kalloc_page()) == nullptr) {
        p->state = UNUSED;
        return nullptr;
    }
    char *stack_pointer = p->kstack + KSTACKSIZE;

    // Leave room for the trap frame.
    stack_pointer -= sizeof *p->trap_frame;
    p->trap_frame = (struct trapframe *)stack_pointer;

    // Set up new context to start executing at forkret,
    // which returns to trapret.
    stack_pointer -= 4;
    *(u32 *)stack_pointer = (u32)trapret;

    stack_pointer -= sizeof *p->context;
    p->context = (struct context *)stack_pointer;
    memset(p->context, 0, sizeof *p->context);
    p->context->eip = (u32)forkret;

    return p;
}

/**
 * @brief Allocate and partially initialize a process structure.
 *
 * @return Pointer to the new process or 0 if none are available.
 */
static struct proc *alloc_proc(void)
{
    struct proc *p;

    acquire(&ptable.lock);

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->state == UNUSED) {
            goto found;
        }
    }

    release(&ptable.lock);
    return nullptr;

found:
    p->state = EMBRYO;
    p->pid = nextpid++;

    release(&ptable.lock);

    return init_proc(p);
}

[[maybe_unused]] static struct proc *alloc_kernel_proc(struct proc *p, void (*entry_point)(void))
{
    if ((p->kstack = kalloc_page()) == nullptr) {
        p->state = UNUSED;
        return nullptr;
    }
    char *stack_pointer = p->kstack + KSTACKSIZE;

    stack_push_pointer(&stack_pointer, (u32)entry_point);
    p->page_directory = kpgdir;

    stack_pointer -= sizeof *p->context;
    p->context = (struct context *)stack_pointer;
    memset(p->context, 0, sizeof *p->context);
    p->context->eip = (u32)forkret;
    p->state        = EMBRYO;

    return p;
}

/**
 * @brief Create the initial user process containing initcode.
 */
void user_init()
{
    boot_message(WARNING_LEVEL_INFO, "Creating initial user process");

    // This name depends on the path where initcode.asm is built.
    extern char _binary_build_initcode_start[], _binary_build_initcode_size[]; // NOLINT(*-reserved-identifier)

    struct proc *p = alloc_proc();

    initproc = p;
    if ((p->page_directory = setup_kernel_page_directory()) == nullptr) {
        panic("user_init: out of memory?");
    }
    inituvm(p->page_directory, _binary_build_initcode_start, (int)_binary_build_initcode_size);
    p->size = PGSIZE;
    memset(p->trap_frame, 0, sizeof(*p->trap_frame));
    p->trap_frame->cs     = (SEG_UCODE << 3) | DPL_USER;
    p->trap_frame->ds     = (SEG_UDATA << 3) | DPL_USER;
    p->trap_frame->es     = p->trap_frame->ds;
    p->trap_frame->ss     = p->trap_frame->ds;
    p->trap_frame->eflags = FL_IF;
    p->trap_frame->esp    = PGSIZE;
    p->trap_frame->eip    = 0; // beginning of initcode.S

    safestrcpy(p->name, "initcode", sizeof(p->name));
    p->cwd = namei("/");
    strncpy(p->cwd_path, "/", MAX_FILE_PATH);

    // this assignment to p->state lets other cores
    // run this process. the acquire forces the above
    // writes to be visible, and the lock is also needed
    // because the assignment might not be atomic.
    // acquire(&ptable.lock);

    p->state = RUNNABLE;
    enqueue_runnable(p);

    // release(&ptable.lock);
}

/**
 * @brief Grow or shrink the current process's address space.
 *
 * @param n Positive delta to grow, negative to shrink.
 * @return 0 on success, -1 on failure.
 */
int resize_proc(int n)
{
    struct proc *curproc = current_process();

    u32 sz = curproc->size;
    if (n > 0) {
        if ((sz = allocvm(curproc->page_directory, sz, sz + n, PTE_W | PTE_U)) == 0) {
            return -1;
        }
    } else if (n < 0) {
        if ((sz = deallocvm(curproc->page_directory, sz, sz + n)) == 0) {
            return -1;
        }
    }
    curproc->size = sz;
    activate_process(curproc);
    return 0;
}


/**
 * @brief Create a child process that duplicates the current process.
 *
 * The caller must mark the returned process RUNNABLE.
 *
 * @return Child PID in the parent, 0 in the child, or -1 on failure.
 */
int fork(void)
{
    struct proc *np;
    struct proc *curproc = current_process();

    // Allocate process.
    if ((np = alloc_proc()) == nullptr) {
        return -1;
    }

    // Copy process state from proc.
    if ((np->page_directory = copyuvm(curproc->page_directory, curproc->size)) == nullptr) {
        kfree_page(np->kstack);
        np->kstack = nullptr;
        np->state  = UNUSED;
        return -1;
    }
    np->size        = curproc->size;
    np->parent      = curproc;
    *np->trap_frame = *curproc->trap_frame;

    // Clear %eax so that fork returns 0 in the child.
    np->trap_frame->eax = 0;

    for (int i = 0; i < NOFILE; i++)
        if (curproc->ofile[i])
            np->ofile[i] = filedup(curproc->ofile[i]);
    np->cwd = idup(curproc->cwd);
    memset(np->cwd_path, 0, MAX_FILE_PATH);
    strncpy(np->cwd_path, curproc->cwd_path, MAX_FILE_PATH);

    safestrcpy(np->name, curproc->name, sizeof(curproc->name));

    int pid = np->pid;

    acquire(&ptable.lock);

    np->state = RUNNABLE;
    enqueue_runnable(np);

    release(&ptable.lock);

    return pid;
}

/**
 * @brief Entry point for forked children on their first scheduled run.
 *
 * Releases ptable.lock and performs late initialization before returning to
 * user space via trapret.
 */
void forkret(void)
{
    static int first = 1;
    // Still holding ptable.lock from scheduler.
    release(&ptable.lock);

    if (first) {
        // Some initialization functions must be run in the context
        // of a regular process (e.g., they call sleep), and thus cannot
        // be run from main().
        first = 0;
        // iinit(ROOTDEV);
        // initlog(ROOTDEV);
        ext2fs_iinit(ROOTDEV);
    }

    // Return to "caller", actually trapret (see allocproc).
}

/**
 * @brief Emit a process table listing for debugging purposes.
 *
 * Invoked via the console ^P handler without acquiring locks to avoid deadlock on
 * wedged systems.
 */
void procdump(void)
{
    static char *states[] = {
        [UNUSED] = "unused",
        [EMBRYO] = "embryo",
        [SLEEPING] = "sleep ",
        [RUNNABLE] = "runnable",
        [RUNNING] = "running",
        [ZOMBIE] = "zombie"
    };
    char *state;
    u32 pc[10];


    for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->state == UNUSED) {
            continue;
        }
        if (p->state >= 0 && p->state < NELEM(states) && states[p->state]) {
            state = states[p->state];
        } else {
            state = "???";
        }
        printf("\n");
        printf(KYEL "%s " KWHT "pid: %d, state: %s", p->name, p->pid, state);
        if (p->state == SLEEPING) {
            printf("\nStack trace:\n");
            getcallerpcs((u32 *)p->context->ebp + 2, pc);
            for (int i = 0; i < 10 && pc[i] != 0; i++) {
                struct symbol symbol = debug_function_symbol_lookup(pc[i]);
                printf(KBWHT " %s " KBBLU "[" KWHT "%x" KBBLU "]" KWHT,
                       (symbol.name == nullptr) ? KBRED "[" KWHT "unknown" KBRED "]" KWHT : symbol.name,
                       pc[i]);
            }
        }
        printf("\n");
    }
}