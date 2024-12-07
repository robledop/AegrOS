#pragma once

#include <config.h>
#include <task.h>

#define PROCESS_FILE_TYPE_ELF 0
#define PROCESS_FILE_TYPE_BINARY 1
typedef unsigned char PROCESS_FILE_TYPE;
enum PROCESS_STATE {
    EMPTY,
    /// The process is ready to run
    RUNNING,
    /// The process has been deallocated, but the parent process has not yet waited for it
    ZOMBIE,
    /// The process is waiting for a child process to exit
    WAITING,
    /// The process is waiting for a signal
    SLEEPING
};

struct command_argument {
    char argument[512];
    struct command_argument *next;
    char current_directory[MAX_PATH_LENGTH];
};

struct process_allocation {
    void *ptr;
    size_t size;
};

struct process_arguments {
    int argc;
    char **argv;
};

struct process {
    uint16_t pid;
    int rand_id; // For debugging purposes
    int priority;
    char file_name[MAX_PATH_LENGTH];
    struct page_directory *page_directory;
    struct process *parent;
    struct task *thread;
    int wait_pid;
    int exit_code;
    bool killed;
    struct process_allocation allocations[MAX_PROGRAM_ALLOCATIONS];
    PROCESS_FILE_TYPE file_type;
    union {
        void *pointer;
        struct elf_file *elf_file;
    };

    struct file *file_descriptors[MAX_FILE_DESCRIPTORS];
    void *user_stack;
    uint32_t size;
    struct process_arguments arguments;
    char *current_directory;
};

__attribute__((nonnull)) int process_load_enqueue(const char file_name[static 1], struct process **process);
__attribute__((nonnull)) int process_load(const char file_name[static 1], struct process **process);
__attribute__((nonnull)) int process_load_for_slot(const char file_name[static 1], struct process **process,
                                                   uint16_t pid);
__attribute__((nonnull)) void *process_malloc(struct process *process, size_t size);
__attribute__((nonnull)) void *process_calloc(struct process *process, size_t nmemb, size_t size);
__attribute__((nonnull)) void process_free(struct process *process, void *ptr);
__attribute__((nonnull)) void process_get_arguments(struct process *process, int *argc, char ***argv);
__attribute__((nonnull)) int process_inject_arguments(struct process *process,
                                                      const struct command_argument *root_argument);
__attribute__((nonnull)) int process_zombify(struct process *process);
__attribute__((nonnull)) int process_set_current_directory(struct process *process, const char directory[static 1]);
__attribute__((nonnull)) struct process *process_clone(struct process *process);
__attribute__((nonnull)) int process_load_data(const char file_name[static 1], struct process *process);
__attribute__((nonnull)) int process_map_memory(struct process *process);
__attribute__((nonnull)) int process_unmap_memory(const struct process *process);
__attribute__((nonnull)) int process_free_allocations(struct process *process);
__attribute__((nonnull)) int process_free_program_data(const struct process *process);
__attribute__((nonnull)) void process_command_argument_free(struct command_argument *argument);

struct file *process_get_file_descriptor(const struct process *process, uint32_t index);
int process_new_file_descriptor(struct process *process, struct file **desc_out);
void process_free_file_descriptor(struct process *process, struct file *desc);
struct process *current_process(void);
// int process_get_free_pid();
// void process_set(int pid, struct process *process);
// struct process *process_get(int pid);
int process_wait_pid(int child_pid);
void process_wakeup(const void *wait_channel);

void process_free_file_descriptors(struct process *process);
void process_copy_file_descriptors(struct process *dest, struct process *src);
