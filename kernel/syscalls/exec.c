#include "types.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"
#include "string.h"
#include "file.h"
#include "printf.h"
#include "status.h"


constexpr char elf_signature[] = {0x7f, 'E', 'L', 'F'};

static bool elf_valid_signature(const void *buffer)
{
    return memcmp(buffer, (void *)elf_signature, sizeof(elf_signature)) == 0;
}

static bool elf_valid_class(const struct elf_header *header)
{
    // We only support 32 bit binaries.
    return header->e_ident[EI_CLASS] == ELFCLASSNONE || header->e_ident[EI_CLASS] == ELFCLASS32;
}

static bool elf_valid_encoding(const struct elf_header *header)
{
    return header->e_ident[EI_DATA] == ELFDATANONE || header->e_ident[EI_DATA] == ELFDATA2LSB;
}

static bool elf_has_program_header(const struct elf_header *header)
{
    return header->e_phoff != 0;
}

int elf_validate_loaded(const struct elf_header *header)
{
    if (header == nullptr) {
        return -EINFORMAT;
    }
    return (elf_valid_signature(header) && elf_valid_class(header) && elf_valid_encoding(header) &&
            elf_has_program_header(header))
        ? ALL_OK
        : -EINFORMAT;
}

/**
 * @brief Replace the current process image with a new program.
 *
 * @param path Path to the executable file.
 * @param argv Argument vector terminated by a null pointer.
 * @return 0 on success, -1 on failure.
 */
int exec(char *path, char **argv)
{
    char path_buffer[MAX_FILE_PATH] = {0};
    if (!starts_with("/bin", path)) {
        strcat(path_buffer, "/bin/");
    }
    strcat(path_buffer, path);

    struct inode *ip;
    if ((ip = namei(path_buffer)) == nullptr) {
        printf("exec: fail\n");
        return -1;
    }

    ip->iops->ilock(ip);

    struct elf_header elf;
    pde_t *pgdir = nullptr;
    // Check ELF header
    if (ip->iops->readi(ip, (char *)&elf, 0, sizeof(elf)) != sizeof(elf)) {
        goto bad;
    }
    if (elf_validate_loaded(&elf) < 0) {
        goto bad;
    }

    if ((pgdir = setup_kernel_page_directory()) == nullptr) {
        goto bad;
    }

    // Load program into memory.
    int sz = 0;
    u32 i, off;
    struct elf32_phdr ph;
    for (i = 0, off = elf.e_phoff; i < elf.e_phnum; i++, off += sizeof(ph)) {
        if (ip->iops->readi(ip, (char *)&ph, off, sizeof(ph)) != sizeof(ph)) {
            goto bad;
        }
        if (ph.p_type != PT_LOAD) {
            continue;
        }
        if (ph.p_memsz < ph.p_filesz) {
            goto bad;
        }
        if (ph.p_vaddr + ph.p_memsz < ph.p_vaddr) {
            goto bad;
        }

        u32 seg_end   = ph.p_vaddr + ph.p_memsz;
        u32 alloc_end = PGROUNDUP(seg_end);
        if (alloc_end < seg_end) {
            goto bad;
        }
        if ((sz = allocvm(pgdir, sz, alloc_end, PTE_W | PTE_U)) == 0) {
            goto bad;
        }

        if (loaduvm(pgdir, (char *)ph.p_vaddr, ip, ph.p_offset, ph.p_filesz) < 0) {
            goto bad;
        }
    }
    ip->iops->iunlockput(ip);
    ip = nullptr;

    // Allocate two pages at the next page boundary.
    // Make the first inaccessible (stack guard page). Use the second as the user stack.
    sz = PGROUNDUP(sz);
    if ((sz = allocvm(pgdir, sz, sz + 2 * PGSIZE, PTE_W | PTE_U)) == 0) {
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
    for (last = s = path_buffer; *s; s++) {
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
    curproc->trap_frame->eip = elf.e_entry; // main
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