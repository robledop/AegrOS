#include <assert.h>
#include <debug.h>
#include <elf.h>
#include <idt.h>
#include <kernel.h>
#include <kernel_heap.h>
#include <memory.h>
#include <paging.h>
#include <process.h>
#include <rand.h>
#include <serial.h>
#include <spinlock.h>
#include <status.h>
#include <string.h>
#include <sys/stat.h>
#include <thread.h>
#include <vfs.h>

extern struct process_list process_list;

struct process *current_process(void)
{
    pushcli();
    const struct thread *current_task = get_current_thread();
    if (current_task) {
        popcli();
        return current_task->process;
    }

    popcli();
    return nullptr;
}

static int process_find_free_allocation_slot(const struct process *process)
{
    for (int i = 0; i < MAX_PROGRAM_ALLOCATIONS; i++) {
        if (process->allocations[i].ptr == nullptr) {
            return i;
        }
    }

    panic("Failed to find free allocation slot");
    return -ENOMEM;
}

static struct process_allocation *process_get_allocation_by_address(struct process *process, const void *address)
{
    for (int i = 0; i < MAX_PROGRAM_ALLOCATIONS; i++) {
        if (process->allocations[i].ptr == address) {
            return &process->allocations[i];
        }
    }

    return nullptr;
}

int process_free_allocations(struct process *process)
{
    for (int i = 0; i < MAX_PROGRAM_ALLOCATIONS; i++) {
        if (process->allocations[i].ptr == nullptr) {
            continue;
        }
        process_free(process, process->allocations[i].ptr);
    }

    return 0;
}

int process_free_program_data(const struct process *process)
{
    int res = 0;
    switch (process->file_type) {
    case PROCESS_FILE_TYPE_BINARY:
        if (process->pointer) {
            kfree(process->pointer);
        }
        break;

    case PROCESS_FILE_TYPE_ELF:
        if (process->elf_file) {
            elf_close(process->elf_file);
        }
        break;

    default:
        ASSERT(false, "Unknown process file type");
        res = -EINVARG;
    }
    return res;
}

void process_free_file_descriptors(struct process *process)
{
    for (int i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        if (process->file_descriptors[i]) {
            vfs_close(process, i);
        }
    }
}

/// @brief Tear down a process and release its resources.
/// Caller remains responsible for freeing the process struct itself when safe.
int process_zombify(struct process *process)
{
    int res = process_free_allocations(process);
    ASSERT(res == 0, "Failed to free allocations for process");

    process_free_file_descriptors(process);

    res = process_free_program_data(process);
    ASSERT(res == 0, "Failed to free program data for process");

    if (process->thread->user_stack) {
        kfree(process->thread->user_stack);
        process->thread->user_stack = nullptr;
    }

    if (process->thread) {
        if (process->thread->kernel_stack) {
            kfree(process->thread->kernel_stack);
        }
        kfree(process->thread);
        process->thread = nullptr;
    }

    if (process->page_directory) {
        paging_free_directory(process->page_directory);
        process->page_directory = nullptr;
    }

    return res;
}

int process_count_command_arguments(const struct command_argument *root_argument)
{
    int i                                  = 0;
    const struct command_argument *current = root_argument;
    while (current) {
        i++;
        current = current->next;
    }

    return i;
}

int process_inject_arguments(struct process *process, const struct command_argument *root_argument)
{
    int res                                = 0;
    const struct command_argument *current = root_argument;
    int i                                  = 0;

    const int argc = process_count_command_arguments(root_argument);

    if (argc == 0) {
        ASSERT(false, "No arguments to inject");
        res = -EINVARG;
        goto out;
    }

    char **argv = process_malloc(process, sizeof(const char *) * argc);
    if (!argv) {
        ASSERT(false, "Failed to allocate memory for arguments");
        res = -ENOMEM;
        goto out;
    }

    while (current) {
        char *argument_str = process_malloc(process, sizeof(current->argument));
        if (!argument_str) {
            ASSERT(false, "Failed to allocate memory for argument string");
            res = -ENOMEM;
            goto out;
        }

        strncpy(argument_str, current->argument, sizeof(current->argument));
        argv[i] = argument_str;
        current = current->next;
        i++;
    }

    process->arguments.argc = argc;
    process->arguments.argv = argv;
    process_set_current_directory(process, root_argument->current_directory);

out:
    return res;
}

void process_free(struct process *process, void *ptr)
{
    struct process_allocation *primary = process_get_allocation_by_address(process, ptr);
    if (!primary) {
        ASSERT(false, "Failed to find allocation for address");
        return;
    }

    const size_t allocation_size = primary->size;

    const int res = paging_map_to(
        process->page_directory, ptr, ptr, paging_align_address((char *)ptr + allocation_size), PDE_UNMAPPED);

    if (res < 0) {
        ASSERT(false, "Failed to unmap memory");
        return;
    }

    for (size_t i = 0; i < MAX_PROGRAM_ALLOCATIONS; i++) {
        if (process->allocations[i].ptr == ptr) {
            process->allocations[i].ptr  = nullptr;
            process->allocations[i].size = 0;
        }
    }

    kfree(ptr);
}

void *process_calloc(struct process *process, const size_t nmemb, const size_t size)
{
    void *ptr = process_malloc(process, nmemb * size);
    if (!ptr) {
        return nullptr;
    }

    memset(ptr, 0x00, nmemb * size);
    return ptr;
}

// Allocate memory accessible by the process
void *process_malloc(struct process *process, const size_t size)
{
    void *ptr = kmalloc(size);
    if (!ptr) {
        ASSERT(false, "Failed to allocate memory for process");
        goto out_error;
    }

    const int index = process_find_free_allocation_slot(process);
    if (index < 0) {
        ASSERT(false, "Failed to find free allocation slot");
        goto out_error;
    }

    const int res =
        paging_map_to(process->page_directory,
                      ptr,
                      ptr,
                      paging_align_address((char *)ptr + size),
                      PDE_IS_PRESENT | PDE_IS_WRITABLE | PDE_SUPERVISOR); // TODO: Get rid of supervisor flag
    if (res < 0) {
        ASSERT(false, "Failed to map memory for process");
        goto out_error;
    }

    process->allocations[index].ptr  = ptr;
    process->allocations[index].size = size;

    return ptr;

out_error:
    if (ptr) {
        kfree(ptr);
    }
    return nullptr;
}

void *process_realloc(struct process *process, void *ptr, const size_t size)
{
    struct process_allocation *allocation = process_get_allocation_by_address(process, ptr);
    if (!allocation) {
        ASSERT(false, "Failed to find allocation for address");
        return nullptr;
    }

    void *new_ptr = process_malloc(process, size);
    if (!new_ptr) {
        ASSERT(false, "Failed to allocate memory for reallocation");
        return nullptr;
    }

    memcpy(new_ptr, ptr, allocation->size);
    process_free(process, ptr);

    return new_ptr;
}

// ReSharper disable once CppDFAUnreachableFunctionCall
static int process_load_binary(const char *file_name, struct process *process)
{
    dbgprintf("Loading binary %s\n", file_name);

    void *program = nullptr;

    int res      = 0;
    const int fd = vfs_open(nullptr, file_name, O_RDONLY);
    if (fd < 0) {
        warningf("Failed to open file %s\n", file_name);
        res = -EIO;
        goto out;
    }

    struct stat fstat;
    res = vfs_stat(nullptr, fd, &fstat);
    if (res != ALL_OK) {
        warningf("Failed to get file stat\n");
        res = -EIO;
        goto out;
    }

    program = kzalloc(fstat.st_size);
    if (program == nullptr) {
        ASSERT(false, "Failed to allocate memory for program");
        res = -ENOMEM;
        goto out;
    }

    if (vfs_read(nullptr, program, fstat.st_size, 1, fd) != (int)fstat.st_size) {
        warningf("Failed to read file\n");
        res = -EIO;
        goto out;
    }

    process->file_type = PROCESS_FILE_TYPE_BINARY;
    process->pointer   = program;
    process->size      = fstat.st_size;

out:
    if (res < 0) {
        if (program != nullptr) {
            kfree(program);
        }
    }
    vfs_close(process, fd);
    return res;
}

static int process_load_elf(const char *file_name, struct process *process)
{
    int res                   = 0;
    struct elf_file *elf_file = nullptr;

    res = elf_load(file_name, &elf_file);
    if (ISERR(res)) {
        warningf("Failed to load ELF file\n");
        warningf("Error code: %d\n", res);
        goto out;
    }

    process->file_type = PROCESS_FILE_TYPE_ELF;
    process->elf_file  = elf_file;
    process->size      = elf_file->in_memory_size;

out:
    return res;
}

int process_load_data(const char file_name[static 1], struct process *process)
{
    int res = 0;

    res = process_load_elf(file_name, process);
    // ReSharper disable once CppDFAConstantConditions
    if (res == -EINFORMAT) {
        warningf("Failed to load ELF file, trying to load as binary\n");
        res = process_load_binary(file_name, process);
    }

    return res;
}

static int process_map_binary(const struct process *process)
{
    return paging_map_to(process->page_directory,
                         (void *)PROGRAM_VIRTUAL_ADDRESS,
                         process->pointer,
                         paging_align_address((char *)process->pointer + process->size),
                         PDE_IS_PRESENT | PDE_IS_WRITABLE | PDE_SUPERVISOR);
}

static int process_unmap_binary(const struct process *process)
{
    return paging_map_to(process->page_directory,
                         (void *)PROGRAM_VIRTUAL_ADDRESS,
                         process->pointer,
                         paging_align_address((char *)process->pointer + process->size),
                         PDE_UNMAPPED);
}

static int process_map_elf(struct process *process)
{
    int res                         = 0;
    const struct elf_file *elf_file = process->elf_file;
    struct elf_header *header       = elf_header(elf_file);
    const struct elf32_phdr *phdrs  = elf_pheader(header);

    for (int i = 0; i < header->e_phnum; i++) {
        const struct elf32_phdr *phdr = &phdrs[i];

        if (phdr->p_type != PT_LOAD) {
            continue; // Skip non-loadable segments
        }

        // Allocate new physical memory for the segment
        void *phys_addr = process_malloc(process, phdr->p_memsz);
        if (phys_addr == nullptr) {
            return -ENOMEM;
        }

        int flags = PDE_IS_PRESENT | PDE_SUPERVISOR; // TODO: Get rid of supervisor

        if (phdr->p_flags & PF_W) {
            flags |= PDE_IS_WRITABLE;
        }

        res = paging_map_to(process->page_directory,
                            paging_align_to_lower_page((void *)phdr->p_vaddr),
                            paging_align_to_lower_page(phys_addr),
                            paging_align_address((char *)phys_addr + phdr->p_memsz),
                            flags);
        if (ISERR(res)) {
            ASSERT(false, "Failed to map ELF segment");
            kfree(phys_addr);
            break;
        }

        // Copy segment data from the ELF file to the allocated memory
        if (phdr->p_filesz > 0) {
            memcpy(phys_addr, (char *)elf_memory(elf_file) + phdr->p_offset, phdr->p_filesz);
        }

        // Zero-initialize the BSS section (p_memsz > p_filesz)
        if (phdr->p_memsz > phdr->p_filesz) {
            memset((char *)phys_addr + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);
        }
    }

    return res;
}

static int process_unmap_elf(const struct process *process)
{
    int res                         = 0;
    const struct elf_file *elf_file = process->elf_file;
    struct elf_header *header       = elf_header(elf_file);
    const struct elf32_phdr *phdrs  = elf_pheader(header);

    for (int i = 0; i < header->e_phnum; i++) {
        const struct elf32_phdr *phdr = &phdrs[i];
        if (phdr->p_type != PT_LOAD) {
            continue; // Skip non-loadable segments
        }
        void *phdr_phys_address = elf_phdr_phys_address(elf_file, phdr);

        res = paging_map_to(process->page_directory,
                            paging_align_to_lower_page((void *)phdr->p_vaddr),
                            paging_align_to_lower_page(phdr_phys_address),
                            paging_align_address((char *)phdr_phys_address + phdr->p_memsz),
                            PDE_UNMAPPED);
        if (ISERR(res)) {
            ASSERT(false, "Failed to unmap ELF file");
            break;
        }
    }

    return res;
}

int process_map_memory(struct process *process)
{
    int res = 0;
    switch (process->file_type) {
    case PROCESS_FILE_TYPE_ELF:
        res = process_map_elf(process);
        break;
    case PROCESS_FILE_TYPE_BINARY:
        res = process_map_binary(process);
        break;
    default:
        panic("Unknown process file type");
        break;
    }

    ASSERT(res >= 0, "Failed to map memory for process");

    // Map stack
    res = paging_map_to(process->page_directory,
                        (char *)USER_STACK_BOTTOM, // stack grows down
                        process->thread->user_stack,
                        paging_align_address((char *)process->thread->user_stack + USER_STACK_SIZE),
                        PDE_IS_PRESENT | PDE_IS_WRITABLE | PDE_SUPERVISOR);


    return res;
}

int process_unmap_memory(const struct process *process)
{
    int res = 0;
    switch (process->file_type) {
    case PROCESS_FILE_TYPE_ELF:
        res = process_unmap_elf(process);
        break;
    case PROCESS_FILE_TYPE_BINARY:
        res = process_unmap_binary(process);
        break;
    default:
        panic("Unknown process file type");
        break;
    }

    ASSERT(res >= 0, "Failed to unmap memory for process");

    if (process->thread->user_stack == nullptr) {
        return res;
    }

    res = paging_map_to(process->page_directory,
                        (char *)USER_STACK_BOTTOM, // stack grows down
                        process->thread->user_stack,
                        paging_align_address((char *)process->thread->user_stack + USER_STACK_SIZE),
                        PDE_UNMAPPED);
    return res;
}


int process_load_enqueue(const char file_name[static 1], struct process **process)
{
    const int res = process_load(file_name, process);
    if (res == 0) {
        (*process)->thread->wakeup_time = -1;
    }

    return res;
}

int process_load(const char file_name[static 1], struct process **process)
{
    int res       = 0;
    const int pid = process_get_free_pid();
    if (pid < 0) {
        warningf("Failed to get free process slot\n");
        ASSERT(false, "Failed to get free process slot");
        res = -EINSTKN;
        goto out;
    }

    res = process_load_for_slot(file_name, process, pid);

    acquire(&process_list.lock);
    if (*process) {
        process_set(pid, *process);
    }
    release(&process_list.lock);
out:
    return res;
}

int process_load_for_slot(const char file_name[static 1], struct process **process, const uint16_t pid)
{
    int res               = 0;
    struct thread *thread = nullptr;
    struct process *proc  = nullptr;

    if (process_get(pid) != nullptr) {
        panic("Process slot is not empty\n");
        res = -EINSTKN;
        goto out;
    }

    proc = kzalloc(sizeof(struct process));
    if (!proc) {
        panic("Failed to allocate memory for process\n");
        res = -ENOMEM;
        goto out;
    }

    res = process_load_data(file_name, proc);
    if (res < 0) {
        warningf("Failed to load data for process\n");
        goto out;
    }

    strncpy(proc->file_name, file_name, sizeof(proc->file_name));
    proc->pid = pid;

    thread = thread_create(proc);
    if (ISERR(thread)) {
        panic("Failed to create thread\n");
        res = -ENOMEM;
        goto out;
    }

    proc->thread  = thread;
    proc->rand_id = (int)get_random();

    res = process_map_memory(proc);
    if (res < 0) {
        panic("Failed to map memory for process\n");
        goto out;
    }

    memset(proc->file_descriptors, 0, sizeof(proc->file_descriptors));
    *process = proc;

out:
    if (ISERR(res)) {
        if (proc) {
            process_zombify(proc);
            kfree(proc);
        }
    }
    return res;
}

int process_set_current_directory(struct process *process, const char directory[static 1])
{
    if (strlen(directory) == 0) {
        return -EINVARG;
    }

    if (process->current_directory == nullptr) {
        process->current_directory = process_malloc(process, MAX_PATH_LENGTH);
    }

    strncpy(process->current_directory, directory, MAX_PATH_LENGTH);

    return ALL_OK;
}

int process_copy_allocations(struct process *dest, const struct process *src)
{
    memset(dest->allocations, 0, sizeof(dest->allocations));

    for (size_t i = 0; i < MAX_PROGRAM_ALLOCATIONS; i++) {
        if (!src->allocations[i].ptr || src->allocations[i].size == 0) {
            continue;
        }

        void *ptr = process_malloc(dest, src->allocations[i].size);
        if (!ptr) {
            ASSERT(false, "Failed to allocate memory for allocation");
            return -ENOMEM;
        }

        memcpy(ptr, src->allocations[i].ptr, src->allocations[i].size);
        struct process_allocation *allocation = process_get_allocation_by_address(dest, ptr);
        if (!allocation) {
            ASSERT(false, "Failed to get allocation for pointer");
            return -EFAULT;
        }

        dest->allocations[i].ptr  = allocation->ptr;
        dest->allocations[i].size = allocation->size;

        if (&dest->allocations[i] != allocation) {
            ASSERT(false, "Failed to copy allocation");
            allocation->ptr  = nullptr;
            allocation->size = 0;
        }
    }

    return ALL_OK;
}

void process_copy_stack(struct process *dest, const struct process *src)
{
    dest->thread->user_stack = kzalloc(USER_STACK_SIZE);
    memcpy(dest->thread->user_stack, src->thread->user_stack, USER_STACK_SIZE);
    memcpy(dest->thread->kernel_stack, src->thread->kernel_stack, KERNEL_STACK_SIZE);
}

void process_copy_file_info(struct process *dest, const struct process *src)
{
    memcpy(dest->file_name, src->file_name, sizeof(src->file_name));
    dest->file_type = src->file_type;
    if (dest->file_type == PROCESS_FILE_TYPE_ELF) {
        dest->elf_file             = kzalloc(sizeof(struct elf_file));
        dest->elf_file->elf_memory = kzalloc(src->elf_file->in_memory_size);
        memcpy(dest->elf_file->elf_memory, src->elf_file->elf_memory, src->elf_file->in_memory_size);
        dest->elf_file->in_memory_size = src->elf_file->in_memory_size;
        strncpy(dest->elf_file->filename, src->elf_file->filename, sizeof(src->elf_file->filename));
        dest->elf_file->virtual_base_address = src->elf_file->virtual_base_address;
        dest->elf_file->virtual_end_address  = src->elf_file->virtual_end_address;

        const int res = elf_process_loaded(dest->elf_file);
        if (res < 0) {
            panic("Failed to process loaded ELF file");
        }

    } else {
        dest->pointer = kzalloc(src->size);
        memcpy(dest->pointer, src->pointer, src->size);
    }
    dest->size = src->size;
}

void process_copy_arguments(struct process *dest, const struct process *src)
{
    dest->current_directory = process_malloc(dest, MAX_PATH_LENGTH);
    memcpy(dest->current_directory, src->current_directory, MAX_PATH_LENGTH);

    dest->arguments.argv = process_malloc(dest, sizeof(char *) * src->arguments.argc);
    for (int i = 0; i < src->arguments.argc; i++) {
        dest->arguments.argv[i] = process_malloc(dest, strlen(src->arguments.argv[i]) + 1);
        strncpy(dest->arguments.argv[i], src->arguments.argv[i], strlen(src->arguments.argv[i]) + 1);
    }
}

void process_copy_thread(struct process *dest, const struct process *src)
{
    struct thread *thread = thread_create(dest);
    if (ISERR(thread)) {
        panic("Failed to create thread");
    }

    dest->thread            = thread;
    dest->thread->process   = dest;
    dest->thread->time_used = 0;

    dest->thread->context->ebp = src->thread->context->ebp;
    dest->thread->context->ebx = src->thread->context->ebx;
    dest->thread->context->esi = src->thread->context->esi;
    dest->thread->context->edi = src->thread->context->edi;

    dest->thread->trap_frame->cs     = src->thread->trap_frame->cs;
    dest->thread->trap_frame->ds     = src->thread->trap_frame->ds;
    dest->thread->trap_frame->es     = src->thread->trap_frame->es;
    dest->thread->trap_frame->fs     = src->thread->trap_frame->fs;
    dest->thread->trap_frame->gs     = src->thread->trap_frame->gs;
    dest->thread->trap_frame->ss     = src->thread->trap_frame->ss;
    dest->thread->trap_frame->eflags = src->thread->trap_frame->eflags;
    // dest->thread->trap_frame->eip              = src->thread->trap_frame->eip;
    dest->thread->trap_frame->esp              = src->thread->trap_frame->esp;
    dest->thread->trap_frame->interrupt_number = src->thread->trap_frame->interrupt_number;
}

void process_copy_file_descriptors(struct process *dest, struct process *src)
{
    for (int i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        if (src->file_descriptors[i]) {
            struct file *desc = kzalloc(sizeof(struct file));
            if (!desc) {
                panic("Failed to allocate memory for file descriptor");
            }

            memcpy(desc, src->file_descriptors[i], sizeof(struct file));
            dest->file_descriptors[i] = desc;
        }
    }
}

struct process *process_clone(struct process *process)
{
    struct process *clone = kzalloc(sizeof(struct process));

    if (!clone) {
        panic("Failed to allocate memory for process clone");
        return nullptr;
    }

    const int pid = process_get_free_pid();
    if (pid < 0) {
        kfree(clone);
        panic("No free process slot");
        return nullptr;
    }

    clone->pid    = pid;
    clone->parent = process;

    // This is not super efficient

    process_copy_file_descriptors(clone, process);
    process_copy_file_info(clone, process);
    process_copy_thread(clone, process);
    process_copy_stack(clone, process);
    process_copy_allocations(clone, process); // Must come before copy_arguments
    process_copy_arguments(clone, process);
    process_map_memory(clone);

    acquire(&process_list.lock);
    process_set(clone->pid, clone);
    release(&process_list.lock);

    clone->rand_id = (int)get_random();

    return clone;
}

void process_command_argument_free(struct process *process, struct command_argument *argument)
{
    struct command_argument *current_virtual = argument;
    while (current_virtual) {
        struct command_argument *current_physical =
            paging_get_physical_address(process->page_directory, current_virtual);
        ASSERT(current_physical, "Failed to translate command argument to physical address");

        struct command_argument *next_virtual = nullptr;
        if (current_physical) {
            next_virtual = current_physical->next;
        }

        process_free(process, current_virtual);
        current_virtual = next_virtual;
    }
}

void process_free_file_descriptor(struct process *process, struct file *desc)
{
    process->file_descriptors[desc->index] = nullptr;
    // Do not free device inodes
    if (desc->inode && desc->inode->type != INODE_DEVICE && desc->fs_type != FS_TYPE_RAMFS) {
        if (desc->inode->data) {
            kfree(desc->inode->data);
        }
        kfree(desc->inode);
    }
    kfree(desc);
}

int process_new_file_descriptor(struct process *process, struct file **desc_out)
{
    int res = -ENOMEM;
    for (int i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        if (process->file_descriptors[i] == nullptr) {
            struct file *desc = kzalloc(sizeof(struct file));
            if (desc == nullptr) {
                panic("Failed to allocate memory for file descriptor\n");
                res = -ENOMEM;
                break;
            }

            desc->index                  = i;
            process->file_descriptors[i] = desc;
            *desc_out                    = desc;
            res                          = 0;
            break;
        }
    }

    return res;
}

struct file *process_get_file_descriptor(const struct process *process, const uint32_t index)
{
    if (index > MAX_FILE_DESCRIPTORS - 1) {
        return nullptr;
    }

    return process->file_descriptors[index];
}
