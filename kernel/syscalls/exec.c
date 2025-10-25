#include "types.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"
#include "string.h"
#include "file.h"

/**
 * @brief Replace the current process image with a new program.
 *
 * @param path Path to the executable file.
 * @param argv Argument vector terminated by a null pointer.
 * @return 0 on success, -1 on failure.
 */
int exec(char *path, char **argv)
{
    struct inode *ip;
    if ((ip = namei(path)) == nullptr) {
        cprintf("exec: fail\n");
        return -1;
    }

    ip->iops->ilock(ip);

    // NOTE: All the ELF loading is done here, but it is done in a simplified way.
    // This only works if the executables are linked with the -N flag (no paging).
    // ld’s -N flag (aka --omagic) forces the linker to put all loadable sections into
    // a single RWX segment that starts at virtual address 0. That gives us an ELF
    // where the one and only PT_LOAD entry is page aligned and covers .text, .data,
    // and .bss contiguously (readelf -l user/build/_echo shows this when -N is present).
    // The xv6 loader leans on that simplification, it insists that
    // every ELF loadable segment satisfy ph.vaddr % PGSIZE == 0, and the inner loop
    // in loaduvm() copies whole pages at a time starting on page
    // boundaries. Once you drop -N, ld emits a second PT_LOAD for .data whose
    // p_vaddr is something like 0x1824, and the loader immediately rejects the
    // program with the page-alignment check.

    struct elfhdr elf;
    pde_t *pgdir = nullptr;
    // Check ELF header
    if (ip->iops->readi(ip, (char *)&elf, 0, sizeof(elf)) != sizeof(elf)) {
        goto bad;
    }
    if (elf.magic != ELF_MAGIC) {
        goto bad;
    }

    if ((pgdir = setup_kernel_page_directory()) == nullptr) {
        goto bad;
    }

    // Load program into memory.
    int sz = 0;
    u32 i, off;
    struct proghdr ph;
    for (i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph)) {
        if (ip->iops->readi(ip, (char *)&ph, off, sizeof(ph)) != sizeof(ph)) {
            goto bad;
        }
        if (ph.type != ELF_PROG_LOAD) {
            continue;
        }
        if (ph.memsz < ph.filesz) {
            goto bad;
        }
        if (ph.vaddr + ph.memsz < ph.vaddr) {
            goto bad;
        }
        if ((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0) {
            goto bad;
        }
        if (ph.vaddr % PGSIZE != 0) {
            goto bad;
        }
        if (loaduvm(pgdir, (char *)ph.vaddr, ip, ph.off, ph.filesz) < 0) {
            goto bad;
        }
    }
    ip->iops->iunlockput(ip);
    ip = nullptr;

    // Allocate two pages at the next page boundary.
    // Make the first inaccessible.  Use the second as the user stack.
    sz = PGROUNDUP(sz);
    if ((sz = allocuvm(pgdir, sz, sz + 2 * PGSIZE)) == 0) {
        goto bad;
    }
    clearpteu(pgdir, (char *)(sz - 2 * PGSIZE));

    u32 argc;
    u32 sp = sz;

    u32 ustack[3 + MAXARG + 1];
    // Push argument strings, prepare rest of stack in ustack.
    for (argc = 0; argv[argc]; argc++) {
        if (argc >= MAXARG) {
            goto bad;
        }
        sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
        if (copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0) {
            goto bad;
        }
        ustack[3 + argc] = sp;
    }
    ustack[3 + argc] = 0;

    ustack[0] = 0xffffffff; // fake return PC
    ustack[1] = argc;
    ustack[2] = sp - (argc + 1) * 4; // argv pointer

    sp -= (3 + argc + 1) * 4;
    if (copyout(pgdir, sp, ustack, (3 + argc + 1) * 4) < 0) {
        goto bad;
    }

    char *s, *last;
    // Save the program name for debugging.
    for (last = s = path; *s; s++) {
        if (*s == '/') {
            last = s + 1;
        }
    }

    struct proc *curproc = current_process();
    safestrcpy(curproc->name, last, sizeof(curproc->name));

    // Commit to the user image.

    pde_t *oldpgdir          = curproc->page_directory;
    curproc->page_directory  = pgdir;
    curproc->size            = sz;
    curproc->trap_frame->eip = elf.entry; // main
    curproc->trap_frame->esp = sp;
    activate_process(curproc);
    freevm(oldpgdir);
    return 0;

bad:
    if (pgdir) {
        freevm(pgdir);
    }
    if (ip) {
        ip->iops->iunlockput(ip);
    }
    return -1;
}