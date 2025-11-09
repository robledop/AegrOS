#pragma once
#include "types.h"

struct buf;
struct context;
struct file;
struct inode;
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
void consoleinit(void);
// void cprintf(char *, ...);
void consputc(int c);
void consoleintr(int (*)(void));
void panic(const char *fmt, ...) __attribute__((noreturn));
void boot_message(warning_level_t level, const char *fmt, ...);

// exec.c
int exec(char *, char **);

// file.c
struct file *filealloc(void);
void fileclose(struct file *);
struct file *filedup(struct file *);
void fileinit(void);
int fileread(struct file *, char *, int n);
int filestat(struct file *, struct stat *);
int filewrite(struct file *, char *, int n);

// fs.c
struct inode *idup(struct inode *);
struct inode *iget(u32 dev, u32 inum);
int namecmp(const char *, const char *);
struct inode *namei(char *);
struct inode *nameiparent(char *, char *);

// ide.c
void ideinit(void);
void ideintr(void);
void iderw(struct buf *);

// ioapic.c
void ioapicenable(int irq, int cpu);
extern u8 ioapicid;
void ioapicinit(void);

// kalloc_page.c
char *kalloc_page(void);
void kfree_page(char *);
void kinit1(void *, void *);
void kinit2(void *, void *);

// kbd.c
void kbdintr(void);

// lapic.c
void cmostime(struct rtcdate *r);
int lapicid(void);
extern volatile u32 *lapic;
void lapiceoi(void);
void lapicinit(void);
void lapicstartap(u8, u32);
void microdelay(int);

// mp.c
extern int ismp;
void mpinit(void);

// picirq.c
void picenable(int);
void picinit(void);

// pipe.c
int pipealloc(struct file **, struct file **);
void pipeclose(struct pipe *, int);
int piperead(struct pipe *, char *, int);
int pipewrite(struct pipe *, char *, int);

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
void uartinit();
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
int loaduvm(pde_t *, char *, struct inode *, u32, u32);
pde_t *copyuvm(pde_t *, u32);
void activate_process(struct proc *);
u32 resize_kernel_page_directory(int n);
void switch_kernel_page_directory();
int copyout(pde_t *, u32, void *, u32);
void clearpteu(pde_t *pgdir, const char *uva);
void kernel_map_mmio(u32 pa, u32 size);
void kernel_enable_mmio_propagation(void);

void *kmalloc(u32 nbytes);
void *kzalloc(u32 nbytes);
void kfree(void *ap);

// number of elements in a fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))
