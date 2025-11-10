#include "types.h"
#include "defs.h"
#include "proc.h"
#include "mmu.h"
#include "string.h"
#include "x86.h"
#include "io.h"
#include "file.h"
#include "mman.h"
#include "framebuffer.h"
#include "memlayout.h"

/** @brief System call wrapper for fork. */
int sys_fork(void)
{
    return fork();
}

/** @brief System call wrapper for exit. */
int sys_exit(void)
{
    exit();
    return 0; // not reached
}

/** @brief System call wrapper for wait. */
int sys_wait(void)
{
    return wait();
}

/** @brief Terminate a process by PID (syscall handler). */
int sys_kill(void)
{
    int pid;

    if (argint(0, &pid) < 0)
        return -1;
    return kill(pid);
}

/** @brief Return the current process ID. */
int sys_getpid(void)
{
    return current_process()->pid;
}

int sys_getcwd(void)
{
    int n;
    char *p;

    if (argint(1, &n) < 0 || argptr(0, &p, n) < 0) {
        return -1;
    }
    if (p == nullptr || n <= 0) {
        return -1;
    }

    const char *cwd = current_process()->cwd_path;
    if (cwd[0] == '\0') {
        cwd = "/";
    }
    safestrcpy(p, cwd, n);

    return 0;
}

/**
 * @brief Adjust process memory size by a delta (syscall handler).
 *
 * @return Previous end-of-heap address or -1 on error.
 */
int sys_sbrk(void)
{
    int n;

    if (argint(0, &n) < 0) {
        return -1;
    }
    int addr = (int)current_process()->brk;
    if (resize_proc(n) < 0) {
        return -1;
    }
    return addr;
}

static int mmap_framebuffer(struct proc *p, u32 length, int prot, int flags, struct file *f, u32 offset)
{
#ifndef GRAPHICS
    return -1;
#else
    if (vbe_info == nullptr) {
        return -1;
    }
    if (offset != 0) {
        return -1;
    }
    if ((prot & PROT_WRITE) == 0 || (prot & PROT_READ) == 0) {
        return -1;
    }
    if ((flags & MAP_SHARED) == 0) {
        return -1;
    }
    if ((vbe_info->framebuffer & (PGSIZE - 1)) != 0) {
        return -1;
    }

    for (struct vm_area *v = p->vma_list; v != nullptr; v = v->next) {
        if (v->flags & VMA_FLAG_DEVICE) {
            return (int)v->start;
        }
    }

    u32 fb_size = (u32)vbe_info->pitch * vbe_info->height;
    if (length == 0 || length > fb_size) {
        length = fb_size;
    }
    length = PGROUNDUP(length);

    struct vm_area *vma = (struct vm_area *)kzalloc(sizeof(*vma));
    if (vma == nullptr) {
        return -1;
    }

    vma->start       = FB_MMAP_BASE;
    vma->end         = FB_MMAP_BASE + length;
    vma->prot        = prot;
    vma->flags       = VMA_FLAG_DEVICE;
    vma->file        = file_dup(f);
    vma->file_offset = 0;
    vma->phys_addr   = vbe_info->framebuffer;

    int perm = PTE_U | PTE_PCD | PTE_PWT;
    if (prot & PROT_WRITE) {
        perm |= PTE_W;
    }

    if (map_physical_range(p->page_directory, vma->start, vma->phys_addr, length, perm) < 0) {
        if (vma->file != nullptr) {
            file_close(vma->file);
        }
        kfree(vma);
        return -1;
    }

    vma->next   = p->vma_list;
    p->vma_list = vma;
    return (int)vma->start;
#endif
}

int sys_mmap(void)
{
    int addr;
    int length;
    int prot;
    int flags;
    int fd;
    int offset;

    if (argint(0, &addr) < 0 || argint(1, &length) < 0 || argint(2, &prot) < 0 || argint(3, &flags) < 0 ||
        argint(4, &fd) < 0 || argint(5, &offset) < 0) {
        return -1;
    }

    struct proc *p = current_process();
    if (fd < 0 || fd >= NOFILE) {
        return -1;
    }
    struct file *f = p->ofile[fd];
    if (f == nullptr) {
        return -1;
    }
    if (addr != 0 && addr != FB_MMAP_BASE) {
        return -1;
    }
    if (length <= 0) {
        return -1;
    }
    if (f->type != FD_INODE || f->ip == nullptr || f->ip->type != T_DEV) {
        return -1;
    }

    int major = devtab_lookup_major(f->ip);
    if (major == FRAMEBUFFER) {
        return mmap_framebuffer(p, (u32)length, prot, flags, f, offset);
    }

    return -1;
}

/**
 * @brief Sleep for a number of clock ticks (syscall handler).
 *
 * @return 0 on success, -1 if interrupted.
 */
int sys_sleep(void)
{
    int n;

    if (argint(0, &n) < 0) {
        return -1;
    }
    acquire(&tickslock);
    u32 ticks0 = ticks;
    while (ticks - ticks0 < (u32)n) {
        if (current_process()->killed) {
            release(&tickslock);
            return -1;
        }
        sleep((void *)&ticks, &tickslock);
    }
    release(&tickslock);
    return 0;
}

int sys_yield(void)
{
    yield();
    return 0;
}

/**
 * @brief Return the number of clock ticks since boot.
 */
int sys_uptime(void)
{
    acquire(&tickslock);
    u32 xticks = ticks;
    release(&tickslock);
    return (int)xticks;
}

int sys_reboot(void)
{
    u8 good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);
    return 0;
}

int sys_shutdown()
{
    outw(0x604, 0x2000);  // qemu
    outw(0x4004, 0x3400); // VirtualBox
    outw(0xB004, 0x2000); // Bochs
    outw(0x600, 0x34);    // Cloud hypervisors

    hlt();
    return 0;
}
