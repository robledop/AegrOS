#pragma once
#include "types.h"

struct buf;
struct context;
struct file;
struct inode;
struct pci_device;
struct pipe;
struct proc;
struct rtcdate;
struct spinlock;
struct sleeplock;
struct stat;
struct superblock;

typedef enum warning_level
{
    WARNING_LEVEL_INFO,
    WARNING_LEVEL_WARNING,
    WARNING_LEVEL_ERROR,
} warning_level_t;


// bio.c
void binit(void);
struct buf *bread(u32, u32);
void brelse(struct buf *);
void bwrite(struct buf *);

// console.c
void console_init(void);
// void cprintf(char *, ...);
void consputc(int c);
void console_input_handler(int (*)(void));
void panic(const char *fmt, ...) __attribute__((noreturn));
void boot_message(warning_level_t level, const char *fmt, ...);

// exec.c
int exec(char *, char **);

// file.c
struct file *file_alloc(void);
void file_close(struct file *);
struct file *file_dup(struct file *);
void file_init(void);
int file_read(struct file *, char *, int n);
int file_stat(struct file *, struct stat *);
int file_write(struct file *, char *, int n);

// fs.c
struct inode *idup(struct inode *);
struct inode *iget(u32 dev, u32 inum);
int namecmp(const char *, const char *);
struct inode *namei(char *);
struct inode *nameiparent(char *, char *);

// ide.c
void ideintr(void);
void iderw(struct buf *);
void ide_pci_init(struct pci_device device);

// ioapic.c
void enable_ioapic_interrupt(int irq, int cpu);
extern u8 ioapicid;
void ioapic_int(void);

// kalloc_page.c
char *kalloc_page(void);
void kfree_page(char *);
void kinit1(void *, void *);
void kalloc_enable_locking(void);

// kbd.c
void keyboard_interrupt_handler(void);

// lapic.c
void cmostime(struct rtcdate *r);
int lapicid(void);
extern volatile u32 *lapic;
void lapic_ack_interrupt(void);
void lapicinit(void);
void lapicstartap(u8, u32);
void microdelay(int);

// mp.c
extern int ismp;
void mpinit(void);
void mp_report_state(void);

// picirq.c
void picenable(int);
void picinit(void);

// pipe.c
int pipe_alloc(struct file **, struct file **);
void pipe_close(struct pipe *, int);
int pipe_read(struct pipe *, char *, int);
int pipe_write(struct pipe *, char *, int);

// proc.c
int cpu_index(void);
void exit(void);
int fork(void);
int resize_proc(int);
int kill(int);
struct cpu *current_cpu();
struct proc *current_process();
void pinit(void);
// void on_timer();
void setproc(struct proc *);
void sleep(void *, struct spinlock *);
void user_init(void);
int wait(void);
void wakeup(void *);
void yield(void);

// spinlock.c
void acquire(struct spinlock *);
void getcallerpcs(void *, u32 *);
int holding(struct spinlock *);
void initlock(struct spinlock *, char *);
void release(struct spinlock *);
void pushcli(void);
void popcli(void);

// sleeplock.c
void acquiresleep(struct sleeplock *);
void releasesleep(struct sleeplock *);
int holdingsleep(struct sleeplock *);
void initsleeplock(struct sleeplock *, char *);

// syscall.c
int argint(int, int *);
int argptr(int, char **, int);
int argstr(int, char **);
int fetchint(u32, int *);
int fetchstr(u32, char **);
void syscall(void);
int open_file(char *path, int omode);

// timer.c
void timerinit(void);

// trap.c
void idtinit(void);
extern volatile u32 ticks;
void tvinit(void);
extern struct spinlock tickslock;

// uart.c
void uart_init();
void uartintr();
void uartputc(int);

// vm.c
void seginit(void);
void kernel_page_directory_init();
pde_t *setup_kernel_page_directory();
char *uva2ka(pde_t *, char *);
int allocvm(pde_t *, u32, u32, int);
u32 deallocvm(pde_t *, u32, u32);
void freevm(pde_t *);
void inituvm(pde_t *, const char *, u32);
int map_physical_range(pde_t *pgdir, u32 va, u32 pa, u32 size, int perm);
int loaduvm(pde_t *, char *, struct inode *, u32, u32);
pde_t *copyuvm(pde_t *, u32);
void activate_process(struct proc *);
u32 resize_kernel_page_directory(int n);
void switch_kernel_page_directory();
int copyout(pde_t *, u32, void *, u32);
void clearpteu(pde_t *pgdir, const char *uva);
void *kernel_map_mmio(u32 pa, u32 size);
void *kernel_map_mmio_wc(u32 pa, u32 size);
void kernel_enable_mmio_propagation(void);
void unmap_vm_range(pde_t *pgdir, u32 start, u32 end, int free_frames);

void *kmalloc(u32 nbytes);
void *kzalloc(u32 nbytes);
void kfree(void *ap);
void memory_enable_sse(void);
void memory_enable_avx(void);
void memory_disable_avx(void);
int memory_sse_available(void);
int memory_avx_available(void);
void fpu_save_state(struct proc *p);
void fpu_restore_state(struct proc *p);

// number of elements in a fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))
