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
#include "file.h"
#include "mman.h"

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
static int map_device_vma(struct proc *p, struct vm_area *vma);

struct proc *current_process(void)
{
    pushcli();
    struct cpu *c  = current_cpu();
    struct proc *p = c->proc;
    popcli();
    return p;
}

static void free_vma_chain(struct vm_area *head)
{
    struct vm_area *vma = head;
    while (vma != nullptr) {
        struct vm_area *next = vma->next;
        if (vma->file != nullptr) {
            file_close(vma->file);
        }
        kfree(vma);
        vma = next;
    }
}

void proc_free_vmas(struct proc *p)
{
    if (p == nullptr || p->vma_list == nullptr) {
        return;
    }
    struct vm_area *vma = p->vma_list;
    while (vma != nullptr) {
        if ((vma->flags & VMA_FLAG_DEVICE) && p->page_directory != nullptr) {
            unmap_vm_range(p->page_directory, vma->start, vma->end, 0);
        }
        vma = vma->next;
    }
    free_vma_chain(p->vma_list);
    p->vma_list = nullptr;
}

static struct vm_area *find_vma_with_flag(struct proc *p, int flag)
{
    for (struct vm_area *vma = p->vma_list; vma != nullptr; vma = vma->next) {
        if ((vma->flags & flag) != 0) {
            return vma;
        }
    }
    return nullptr;
}

struct vm_area *proc_ensure_heap_vma(struct proc *p)
{
    struct vm_area *heap = find_vma_with_flag(p, VMA_FLAG_HEAP);
    if (heap != nullptr) {
        return heap;
    }

    heap = (struct vm_area *)kzalloc(sizeof(*heap));
    if (heap == nullptr) {
        return nullptr;
    }

    heap->start       = 0;
    heap->end         = p->brk;
    heap->prot        = PTE_W | PTE_U;
    heap->flags       = VMA_FLAG_HEAP;
    heap->file        = nullptr;
    heap->file_offset = 0;
    heap->phys_addr   = 0;
    heap->next        = p->vma_list;
    p->vma_list       = heap;
    return heap;
}

static int map_device_vma(struct proc *p, struct vm_area *vma)
{
    if ((vma->flags & VMA_FLAG_DEVICE) == 0) {
        return 0;
    }

    u32 perm = PTE_U | PTE_PCD | PTE_PWT;
    if (vma->prot & PROT_WRITE) {
        perm |= PTE_W;
    }
    if (map_physical_range(p->page_directory, vma->start, vma->phys_addr, vma->end - vma->start, perm) < 0) {
        return -1;
    }
    return 0;
}

int proc_clone_vmas(struct proc *dst, struct proc *src)
{
    if (dst == nullptr || src == nullptr) {
        return -1;
    }

    struct vm_area *new_head = nullptr;
    struct vm_area **tail    = &new_head;

    for (struct vm_area *cur = src->vma_list; cur != nullptr; cur = cur->next) {
        struct vm_area *copy = (struct vm_area *)kzalloc(sizeof(*copy));
        if (copy == nullptr) {
            free_vma_chain(new_head);
            return -1;
        }
        *copy = *cur;
        if (copy->file != nullptr) {
            copy->file = file_dup(copy->file);
        }
        copy->next = nullptr;
        *tail      = copy;
        tail       = &copy->next;
    }

    proc_free_vmas(dst);
    dst->vma_list = new_head;

    for (struct vm_area *cur = dst->vma_list; cur != nullptr; cur = cur->next) {
        if (map_device_vma(dst, cur) < 0) {
            proc_free_vmas(dst);
            return -1;
        }
    }
    return 0;
}

static struct proc *init_proc(struct proc *p)
{
    // Allocate kernel stack.
    if ((p->kstack = kalloc_page()) == nullptr) {
        p->state = UNUSED;
        return nullptr;
    }
    proc_free_vmas(p);
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

static struct proc *alloc_kernel_proc(struct proc *p, void (*entry_point)(void))
{
    if ((p->kstack = kalloc_page()) == nullptr) {
        p->state = UNUSED;
        return nullptr;
    }
    proc_free_vmas(p);
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
    p->brk = PGSIZE;
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

    if (proc_ensure_heap_vma(p) == nullptr) {
        panic("user_init: heap vma");
    }

    // this assignment to p->state lets other cores
    // run this process. the acquire forces the above
    // writes to be visible, and the lock is also needed
    // because the assignment might not be atomic.
    // acquire(&ptable.lock);

    p->state = RUNNABLE;
    enqueue_runnable(p);
}

/**
 * @brief Grow or shrink the current process's address space.
 *
 * @param n Positive delta to grow, negative to shrink.
 * @return 0 on success, -1 on failure.
 */
int resize_proc(int n)
{
    struct proc *curproc     = current_process();
    struct vm_area *heap_vma = proc_ensure_heap_vma(curproc);
    if (heap_vma == nullptr) {
        return -1;
    }

    u32 sz = curproc->brk;
    if (n > 0) {
        if ((sz = allocvm(curproc->page_directory, sz, sz + n, PTE_W | PTE_U)) == 0) {
            return -1;
        }
    } else if (n < 0) {
        if ((sz = deallocvm(curproc->page_directory, sz, sz + n)) == 0) {
            return -1;
        }
    }
    curproc->brk    = sz;
    heap_vma->start = 0;
    heap_vma->end   = sz;
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
    if ((np->page_directory = copyuvm(curproc->page_directory, curproc->brk)) == nullptr) {
        kfree_page(np->kstack);
        np->kstack = nullptr;
        np->state  = UNUSED;
        return -1;
    }
    np->brk = curproc->brk;
    if (proc_clone_vmas(np, curproc) < 0) {
        freevm(np->page_directory);
        proc_free_vmas(np);
        kfree_page(np->kstack);
        np->kstack = nullptr;
        np->state  = UNUSED;
        return -1;
    }
    np->parent      = curproc;
    *np->trap_frame = *curproc->trap_frame;

    // Clear %eax so that fork returns 0 in the child.
    np->trap_frame->eax = 0;

    for (int i = 0; i < NOFILE; i++) {
        if (curproc->ofile[i]) {
            np->ofile[i] = file_dup(curproc->ofile[i]);
        }
    }
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
