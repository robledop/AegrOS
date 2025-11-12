#include "param.h"
#include "types.h"
#include "user.h"
#include "fs.h"
#include "stat.h"
#include "file.h"
#include "fcntl.h"
#include "include/dirwalk.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"
#include "termcolors.h"

char buf[8192];
char name[3];
char *echoargv[] = {"echo", "ALL", "TESTS", "PASSED", nullptr};

static int framebuffer_mmap_supported(void)
{
    static int cached = -1;
    if (cached != -1) {
        return cached;
    }

    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        cached = 0;
        return 0;
    }

    void *map = mmap(nullptr, PGSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        cached = 0;
        return 0;
    }

    munmap(map, PGSIZE);
    close(fd);
    cached = 1;
    return 1;
}

static int open_framebuffer(void)
{
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        printf(KBYEL "\nfb test skipped: /dev/fb0 unavailable\n" KRESET);
    }
    return fd;
}

static int readfile(const char *path, char *out, int max)
{
    if (max <= 0) {
        return -1;
    }
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    int total = 0;
    int limit = max - 1;
    while (total < limit) {
        int n = read(fd, out + total, limit - total);
        if (n < 0) {
            close(fd);
            return -1;
        }
        if (n == 0) {
            break;
        }
        total += n;
    }
    close(fd);
    out[total] = '\0';
    return total;
}

// does chdir() call iput(p->cwd) in a transaction?
void iputtest(void)
{
    printf("iput test");

    if (mkdir("iputdir") < 0) {
        printf(KBRED "\nmkdir failed\n" KRESET);
        exit();
    }
    if (chdir("iputdir") < 0) {
        printf(KBRED "\nchdir iputdir failed\n" KRESET);
        exit();
    }
    if (unlink("../iputdir") < 0) {
        printf(KBRED "\nunlink ../iputdir failed\n" KRESET);
        exit();
    }
    if (chdir("/") < 0) {
        printf(KBRED "\nchdir / failed\n" KRESET);
        exit();
    }
    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

// does exit() call iput(p->cwd) in a transaction?
void exitiputtest(void)
{
    printf("exitiput test");

    int pid = fork();
    if (pid < 0) {
        printf(KBRED "\nfork failed\n" KRESET);
        exit();
    }
    if (pid == 0) {
        if (mkdir("iputdir") < 0) {
            printf(KBRED "\nmkdir failed\n" KRESET);
            exit();
        }
        if (chdir("iputdir") < 0) {
            printf(KBRED "\nchild chdir failed\n" KRESET);
            exit();
        }
        if (unlink("../iputdir") < 0) {
            printf(KBRED "\nunlink ../iputdir failed\n" KRESET);
            exit();
        }
        exit();
    }
    wait();
    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

// does the error path in open() for attempt to write a
// directory call iput() in a transaction?
// needs a hacked kernel that pauses just after the namei()
// call in sys_open():
//    if((ip = namei(path)) == 0)
//      return -1;
//    {
//      int i;
//      for(i = 0; i < 10000; i++)
//        yield();
//    }
void openiputtest(void)
{
    printf("openiput test");
    if (mkdir("oidir") < 0) {
        printf(KBRED "\nmkdir oidir failed\n" KRESET);
        exit();
    }
    int pid = fork();
    if (pid < 0) {
        printf(KBRED "\nfork failed\n" KRESET);
        exit();
    }
    if (pid == 0) {
        int fd = open("oidir", O_RDWR);
        if (fd >= 0) {
            printf(KBRED "\nopen directory for write succeeded\n" KRESET);
            exit();
        }
        exit();
    }
    sleep(1);
    if (unlink("oidir") != 0) {
        printf(KBRED "\nunlink failed\n" KRESET);
        exit();
    }
    wait();
    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

// simple file system tests

void opentest(void)
{
    printf("open test");
    int fd = open("/bin/echo", 0);
    if (fd < 0) {
        printf(KBRED "\nopen echo failed!\n" KRESET);
        exit();
    }
    close(fd);
    fd = open("doesnotexist", 0);
    if (fd >= 0) {
        printf(KBRED "\nopen doesnotexist succeeded!\n" KRESET);
        exit();
    }
    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void writetest(void)
{
    int i;

    printf("small file test");
    int fd = open("small", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf(KBRED "\ncreat small failed!\n" KRESET);
        exit();
    }
    for (i = 0; i < 100; i++) {
        if (write(fd, "aaaaaaaaaa", 10) != 10) {
            printf(KBRED "\nwrite aa %d new file failed\n" KRESET, i);
            exit();
        }
        if (write(fd, "bbbbbbbbbb", 10) != 10) {
            printf(KBRED "\nwrite bb %d new file failed\n" KRESET, i);
            exit();
        }
    }
    close(fd);
    fd = open("small", O_RDONLY);
    if (fd < 0) {
        printf(KBRED "\nopen small failed!\n" KRESET);
        exit();
    }
    i = read(fd, buf, 2000);
    if (i != 2000) {
        printf(KBRED "\nread failed\n" KRESET);
        exit();
    }
    close(fd);

    if (unlink("small") < 0) {
        printf(KBRED "\nunlink small failed\n" KRESET);
        exit();
    }
    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void writetest1(void)
{
    int i;

    printf("big files test");

    int fd = open("big", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf(KBRED "\ncreat big failed!\n" KRESET);
        exit();
    }

    for (i = 0; i < MAXFILE; i++) {
        ((int *)buf)[0] = i;
        if (write(fd, buf, 512) != 512) {
            printf(KBRED "\nwrite big file failed\n" KRESET);
            exit();
        }
    }

    close(fd);

    fd = open("big", O_RDONLY);
    if (fd < 0) {
        printf(KBRED "\nopen big failed!\n" KRESET);
        exit();
    }

    int n = 0;
    for (;;) {
        i = read(fd, buf, 512);
        if (i == 0) {
            if (n == MAXFILE - 1) {
                printf(KBRED "\nread only %d blocks from big\n" KRESET, n);
                exit();
            }
            break;
        } else if (i != 512) {
            printf(KBRED "\nread failed %d\n" KRESET, i);
            exit();
        }
        if (((int *)buf)[0] != n) {
            printf(KBRED "\nread content of block %d is %d\n" KRESET, n, ((int *)buf)[0]);
            exit();
        }
        n++;
    }
    close(fd);
    if (unlink("big") < 0) {
        printf(KBRED "\nunlink big failed\n" KRESET);
        exit();
    }
    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void createtest(void)
{
    int i;

    printf("many creates, followed by unlink test");

    name[0] = 'a';
    name[2] = '\0';
    for (i = 0; i < 52; i++) {
        name[1] = '0' + i;
        int fd  = open(name, O_CREATE | O_RDWR);
        close(fd);
    }
    name[0] = 'a';
    name[2] = '\0';
    for (i = 0; i < 52; i++) {
        name[1] = '0' + i;
        unlink(name);
    }
    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void dirtest(void)
{
    printf("mkdir test");

    if (mkdir("dir0") < 0) {
        printf(KBRED "\nmkdir failed\n" KRESET);
        exit();
    }

    if (chdir("dir0") < 0) {
        printf(KBRED "\nchdir dir0 failed\n" KRESET);
        exit();
    }

    if (chdir("..") < 0) {
        printf(KBRED "\nchdir .. failed\n" KRESET);
        exit();
    }

    if (unlink("dir0") < 0) {
        printf(KBRED "\nunlink dir0 failed\n" KRESET);
        exit();
    }
    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void exectest(void)
{
    printf("exec test\n");
    if (exec("/bin/echo", echoargv) < 0) {
        printf(KBRED "\nexec echo failed\n" KRESET);
        exit();
    }
}

// simple fork and pipe read/write

void pipe1(void)
{
    int fds[2];
    int i, n;

    printf("pipe1 test");
    if (pipe(fds) != 0) {
        printf(KBRED "\npipe() failed\n" KRESET);
        exit();
    }
    int pid = fork();
    int seq = 0;
    if (pid == 0) {
        close(fds[0]);
        for (n = 0; n < 5; n++) {
            for (i     = 0; i < 1033; i++)
                buf[i] = seq++;
            if (write(fds[1], buf, 1033) != 1033) {
                printf(KBRED "\npipe1 oops 1\n" KRESET);
                exit();
            }
        }
        exit();
    } else if (pid > 0) {
        close(fds[1]);
        int total = 0;
        int cc    = 1;
        while ((n = read(fds[0], buf, cc)) > 0) {
            for (i = 0; i < n; i++) {
                if ((buf[i] & 0xff) != (seq++ & 0xff)) {
                    printf(KBRED "\npipe1 oops 2\n" KRESET);
                    return;
                }
            }
            total += n;
            cc = cc * 2;
            if (cc > sizeof(buf))
                cc = sizeof(buf);
        }
        if (total != 5 * 1033) {
            printf(KBRED "\npipe1 oops 3 total %d\n" KRESET, total);
            exit();
        }
        close(fds[0]);
        wait();
    } else {
        printf(KBRED "\nfork() failed\n" KRESET);
        exit();
    }
    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

// meant to be run w/ at most two CPUs
void preempt(void)
{
    int pfds[2];

    printf("preempt test");
    int pid1 = fork();
    if (pid1 == 0)
        for (;;) {
        }

    int pid2 = fork();
    if (pid2 == 0)
        for (;;) {
        }

    pipe(pfds);
    int pid3 = fork();
    if (pid3 == 0) {
        close(pfds[0]);
        if (write(pfds[1], "x", 1) != 1)
            printf(KBRED "\npreempt write error\n" KRESET);
        close(pfds[1]);
        for (;;) {
        }
    }

    close(pfds[1]);
    if (read(pfds[0], buf, sizeof(buf)) != 1) {
        printf(KBRED "\npreempt read error\n" KRESET);
        return;
    }
    close(pfds[0]);
    kill(pid1);
    kill(pid2);
    kill(pid3);
    wait();
    wait();
    wait();
    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

// try to find any races between exit and wait
void exitwait(void)
{
    printf("exitwait test");
    for (int i = 0; i < 100; i++) {
        int pid = fork();
        if (pid < 0) {
            printf(KBRED "\nfork failed\n" KRESET);
            return;
        }
        if (pid) {
            if (wait() != pid) {
                printf(KBRED "\nwait wrong pid\n" KRESET);
                return;
            }
        } else {
            exit();
        }
    }
    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void mem(void)
{
    void *m2;
    int pid;

    printf("mem test\n");
    int ppid = getpid();
    if ((pid = fork()) == 0) {
        void *m1 = nullptr;
        while ((m2 = malloc(10001)) != nullptr) {
            *(char **)m2 = m1;
            m1           = m2;
        }
        while (m1) {
            m2 = *(char **)m1;
            free(m1);
            m1 = m2;
        }
        m1 = malloc(1024 * 20);
        if (m1 == nullptr) {
            printf(KBRED "\ncouldn't allocate mem?!!\n" KRESET);
            kill(ppid);
            exit();
        }
        free(m1);
        printf("mem test [ " KBGRN "OK" KRESET " ]\n");
        exit();
    } else {
        wait();
    }
}

// More file system tests

// two processes write to the same file descriptor
// is the offset shared? does inode locking work?
void sharedfd(void)
{
    int i, n, np;
    char buf[10];

    printf("sharedfd test");

    unlink("sharedfd");
    int fd = open("sharedfd", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf(KBRED "\ncannot open sharedfd for writing\n" KRESET);
        return;
    }
    int pid = fork();
    memset(buf, pid == 0 ? 'c' : 'p', sizeof(buf));
    for (i = 0; i < 1000; i++) {
        if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
            printf(KBRED "\nwrite sharedfd failed\n" KRESET);
            break;
        }
    }
    if (pid == 0)
        exit();
    else
        wait();
    close(fd);
    fd = open("sharedfd", 0);
    if (fd < 0) {
        printf(KBRED "\ncannot open sharedfd for reading\n" KRESET);
        return;
    }
    int nc = np = 0;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (i = 0; i < sizeof(buf); i++) {
            if (buf[i] == 'c')
                nc++;
            if (buf[i] == 'p')
                np++;
        }
    }
    close(fd);
    unlink("sharedfd");
    if (nc == 10000 && np == 10000) {
        printf(" [ " KBGRN "OK" KRESET " ]\n");
    } else {
        printf(KBRED "\nsharedfd oops %d %d\n" KRESET, nc, np);
        exit();
    }
}

void duptest(void)
{
    printf("dup test");

    unlink("dupfile");
    int fd = open("dupfile", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf(KBRED "\nopen dupfile failed\n" KRESET);
        exit();
    }
    if (write(fd, "hello", 5) != 5) {
        printf(KBRED "\nwrite hello failed\n" KRESET);
        exit();
    }
    int dupfd = dup(fd);
    if (dupfd < 0) {
        printf(KBRED "\ndup failed\n" KRESET);
        exit();
    }
    if (write(dupfd, "world", 5) != 5) {
        printf(KBRED "\nwrite via dup failed\n" KRESET);
        exit();
    }
    if (close(dupfd) < 0) {
        printf(KBRED "\nclose dupfd failed\n" KRESET);
        exit();
    }
    if (write(fd, "!", 1) != 1) {
        printf(KBRED "\nwrite after closing dup failed\n" KRESET);
        exit();
    }
    if (close(fd) < 0) {
        printf(KBRED "\nclose fd failed\n" KRESET);
        exit();
    }

    int check = open("dupfile", O_RDONLY);
    if (check < 0) {
        printf(KBRED "\nopen dupfile for verify failed\n" KRESET);
        exit();
    }
    if (read(check, buf, sizeof(buf)) != 11 || strncmp(buf, "helloworld!", 11) != 0) {
        printf(KBRED "\ndupfile contents wrong\n" KRESET);
        exit();
    }
    close(check);

    int rfd = open("dupfile", O_RDONLY);
    if (rfd < 0) {
        printf(KBRED "\nopen dupfile for read failed\n" KRESET);
        exit();
    }
    int rdup = dup(rfd);
    if (rdup < 0) {
        printf(KBRED "\ndup read fd failed\n" KRESET);
        exit();
    }
    if (read(rfd, buf, 5) != 5 || strncmp(buf, "hello", 5) != 0) {
        printf(KBRED "\nread from original fd wrong\n" KRESET);
        exit();
    }
    if (read(rdup, buf, 5) != 5 || strncmp(buf, "world", 5) != 0) {
        printf(KBRED "\nread from dup fd wrong\n" KRESET);
        exit();
    }
    if (close(rfd) < 0) {
        printf(KBRED "\nclose original read fd failed\n" KRESET);
        exit();
    }
    if (read(rdup, buf, 1) != 1 || buf[0] != '!') {
        printf(KBRED "\nread after close wrong\n" KRESET);
        exit();
    }
    close(rdup);

    int limitfd = open("dupfile", O_RDONLY);
    if (limitfd < 0) {
        printf(KBRED "\nopen dupfile for limit test failed\n" KRESET);
        exit();
    }
    int copies[NOFILE];
    int copied = 0;
    for (; copied < NOFILE; copied++) {
        int d = dup(limitfd);
        if (d < 0) {
            break;
        }
        copies[copied] = d;
    }
    if (copied == 0) {
        printf(KBRED "\ndup produced no copies\n" KRESET);
        exit();
    }
    if (copied == NOFILE) {
        printf(KBRED "\ndup exceeded NOFILE limit\n" KRESET);
        exit();
    }
    if (dup(limitfd) >= 0) {
        printf(KBRED "\ndup unexpectedly succeeded with table full\n" KRESET);
        exit();
    }
    for (int i = 0; i < copied; i++) {
        close(copies[i]);
    }
    close(limitfd);

    if (unlink("dupfile") != 0) {
        printf(KBRED "\nunlink dupfile failed\n" KRESET);
        exit();
    }

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

// four processes write different files at the same
// time, to test block allocation.
void
fourfiles(void)
{
    int fd, i, n, pi;
    char *names[] = {"f0", "f1", "f2", "f3"};
    char *fname;

    printf("fourfiles test");

    for (pi = 0; pi < 4; pi++) {
        fname = names[pi];
        unlink(fname);

        int pid = fork();
        if (pid < 0) {
            printf(KBRED "\nfork failed\n" KRESET);
            exit();
        }

        if (pid == 0) {
            fd = open(fname, O_CREATE | O_RDWR);
            if (fd < 0) {
                printf(KBRED "\ncreate failed\n" KRESET);
                exit();
            }

            memset(buf, '0' + pi, 512);
            for (i = 0; i < 12; i++) {
                if ((n = write(fd, buf, 500)) != 500) {
                    printf(KBRED "\nwrite failed %d\n" KRESET, n);
                    exit();
                }
            }
            exit();
        }
    }

    for (pi = 0; pi < 4; pi++) {
        wait();
    }

    for (i = 0; i < 2; i++) {
        fname     = names[i];
        fd        = open(fname, 0);
        int total = 0;
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            for (int j = 0; j < n; j++) {
                if (buf[j] != '0' + i) {
                    printf(KBRED "\nwrong char\n" KRESET);
                    exit();
                }
            }
            total += n;
        }
        close(fd);
        if (total != 12 * 500) {
            printf(KBRED "\nwrong length %d\n" KRESET, total);
            exit();
        }
        unlink(fname);
    }

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

// four processes create and delete different files in same directory
void createdelete(void)
{
    enum { N = 20 };
    int i, fd, pi;
    char name[32];

    printf("createdelete test");

    for (pi = 0; pi < 4; pi++) {
        int pid = fork();
        if (pid < 0) {
            printf(KBRED "\nfork failed\n" KRESET);
            exit();
        }

        if (pid == 0) {
            name[0] = 'p' + pi;
            name[2] = '\0';
            for (i = 0; i < N; i++) {
                name[1] = '0' + i;
                fd      = open(name, O_CREATE | O_RDWR);
                if (fd < 0) {
                    printf(KBRED "\ncreate failed\n" KRESET);
                    exit();
                }
                close(fd);
                if (i > 0 && (i % 2) == 0) {
                    name[1] = '0' + (i / 2);
                    if (unlink(name) < 0) {
                        printf(KBRED "\nunlink failed\n" KRESET);
                        exit();
                    }
                }
            }
            exit();
        }
    }

    for (pi = 0; pi < 4; pi++) {
        wait();
    }

    name[0] = name[1] = name[2] = 0;
    for (i = 0; i < N; i++) {
        for (pi = 0; pi < 4; pi++) {
            name[0] = 'p' + pi;
            name[1] = '0' + i;
            fd      = open(name, 0);
            if ((i == 0 || i >= N / 2) && fd < 0) {
                printf(KBRED "\noops createdelete %s didn't exist\n" KRESET, name);
                exit();
            } else if ((i >= 1 && i < N / 2) && fd >= 0) {
                printf(KBRED "\noops createdelete %s did exist\n" KRESET, name);
                exit();
            }
            if (fd >= 0)
                close(fd);
        }
    }

    for (i = 0; i < N; i++) {
        for (pi = 0; pi < 4; pi++) {
            name[0] = 'p' + i;
            name[1] = '0' + i;
            unlink(name);
        }
    }

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

// can I unlink a file and still read it?
void unlinkread(void)
{
    printf("unlinkread test");
    int fd = open("unlinkread", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf(KBRED "\ncreate unlinkread failed\n" KRESET);
        exit();
    }
    write(fd, "hello", 5);
    close(fd);

    fd = open("unlinkread", O_RDWR);
    if (fd < 0) {
        printf(KBRED "\nopen unlinkread failed\n" KRESET);
        exit();
    }
    if (unlink("unlinkread") != 0) {
        printf(KBRED "\nunlink unlinkread failed\n" KRESET);
        exit();
    }

    int fd1 = open("unlinkread", O_CREATE | O_RDWR);
    write(fd1, "yyy", 3);
    close(fd1);

    if (read(fd, buf, sizeof(buf)) != 5) {
        printf(KBRED "\nunlinkread read failed\n" KRESET);
        exit();
    }
    if (buf[0] != 'h') {
        printf(KBRED "\nunlinkread wrong data\n" KRESET);
        exit();
    }
    if (write(fd, buf, 10) != 10) {
        printf(KBRED "\nunlinkread write failed\n" KRESET);
        exit();
    }
    close(fd);
    unlink("unlinkread");
    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void fstattest(void)
{
    struct stat st;
    printf("fstat test");

    unlink("fstatfile");
    unlink("fstatfile2");

    int fd = open("fstatfile", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf(KBRED "\ncreate fstatfile failed\n" KRESET);
        exit();
    }

    if (fstat(fd, &st) < 0) {
        printf(KBRED "\nfstat on new file failed\n" KRESET);
        exit();
    }
    if (st.type != T_FILE) {
        printf(KBRED "\nnew file wrong type %d\n" KRESET, st.type);
        exit();
    }
    if (st.size != 0) {
        printf(KBRED "\nnew file wrong size %u\n" KRESET, st.size);
        exit();
    }
    if (st.nlink != 1) {
        printf(KBRED "\nnew file wrong nlink %d\n" KRESET, st.nlink);
        exit();
    }

    if (write(fd, "abc", 3) != 3) {
        printf(KBRED "\nwrite fstatfile failed\n" KRESET);
        exit();
    }
    if (fstat(fd, &st) < 0 || st.size != 3) {
        printf(KBRED "\npost-write fstat wrong size %u\n" KRESET, st.size);
        exit();
    }

    if (link("fstatfile", "fstatfile2") != 0) {
        printf(KBRED "\nlink fstatfile2 failed\n" KRESET);
        exit();
    }
    if (fstat(fd, &st) < 0 || st.nlink != 2) {
        printf(KBRED "\nlink count not incremented (%d)\n" KRESET, st.nlink);
        exit();
    }

    if (unlink("fstatfile2") != 0) {
        printf(KBRED "\nunlink fstatfile2 failed\n" KRESET);
        exit();
    }
    if (fstat(fd, &st) < 0 || st.nlink != 1) {
        printf(KBRED "\nlink count not decremented (%d)\n" KRESET, st.nlink);
        exit();
    }

    int dirfd = open(".", 0);
    if (dirfd < 0) {
        printf(KBRED "\nopen . failed\n" KRESET);
        exit();
    }
    if (fstat(dirfd, &st) < 0 || st.type != T_DIR) {
        printf(KBRED "\nfstat on dir wrong type %d\n" KRESET, st.type);
        exit();
    }
    close(dirfd);

    int closedfd = fd;
    if (close(fd) != 0) {
        printf(KBRED "\nclose fstatfile failed\n" KRESET);
        exit();
    }
    if (fstat(closedfd, &st) >= 0) {
        printf(KBRED "\nfstat on closed fd succeeded\n" KRESET);
        exit();
    }

    fd = open("fstatfile", 0);
    if (fd < 0) {
        printf(KBRED "\nopen fstatfile for cleanup failed\n" KRESET);
        exit();
    }
    if (fstat(fd, &st) < 0 || st.size != 3 || st.nlink != 1) {
        printf(KBRED "\nfinal fstat mismatch size=%u nlink=%d\n" KRESET, st.size, st.nlink);
        exit();
    }
    close(fd);

    if (unlink("fstatfile") != 0) {
        printf(KBRED "\nunlink fstatfile failed\n" KRESET);
        exit();
    }

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void mknodtest(void)
{
    struct stat st;
    printf("mknod test\n");

    unlink("mknod.tmp");
    unlink("/dev/mknod-null");

    int fd = open("mknod.tmp", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf(KBRED "\ncreate mknod.tmp failed\n" KRESET);
        exit();
    }
    close(fd);
    if (mknod("mknod.tmp", 0, 0) >= 0) {
        printf(KBRED "\nmknod overwrote existing file\n" KRESET);
        exit();
    }
    unlink("mknod.tmp");

    if (mknod("missing/mknod", 0, 0) >= 0) {
        printf(KBRED "\nmknod unexpectedly succeeded in missing dir\n" KRESET);
        exit();
    }

    const char *devpath = "/dev/mknod-null";
    if (mknod(devpath, 0, 0) != 0) {
        printf(KBRED "\nmknod %s failed\n" KRESET, devpath);
        exit();
    }

    if (stat(devpath, &st) < 0) {
        printf(KBRED "\nstat %s failed\n" KRESET, devpath);
        exit();
    }
    if (st.type != T_DEV) {
        printf(KBRED "\n%s wrong type %d\n" KRESET, devpath, st.type);
        exit();
    }
    if (st.nlink != 1) {
        printf(KBRED "\n%s wrong nlink %d\n" KRESET, devpath, st.nlink);
        exit();
    }

    int devfd = open(devpath, O_RDWR);
    if (devfd < 0) {
        printf(KBRED "\nopen %s failed\n" KRESET, devpath);
        exit();
    }
    char ch = 'a';
    if (write(devfd, &ch, 1) >= 0) {
        printf(KBRED "\nwrite to %s unexpectedly succeeded\n" KRESET, devpath);
        exit();
    }
    if (read(devfd, &ch, 1) >= 0) {
        printf(KBRED "\nread from %s unexpectedly succeeded\n" KRESET, devpath);
        exit();
    }
    close(devfd);

    if (unlink(devpath) != 0) {
        printf(KBRED "\nunlink %s failed\n" KRESET, devpath);
        exit();
    }

    struct stat devtab_stat;
    if (stat("/etc/devtab", &devtab_stat) < 0) {
        printf(KBRED "\nstat /etc/devtab failed\n" KRESET);
        exit();
    }
    int devtab_backup_cap = devtab_stat.size + 1;
    if (devtab_backup_cap < 1) {
        devtab_backup_cap = 1;
    }
    char *devtab_backup = malloc(devtab_backup_cap);
    if (devtab_backup == nullptr) {
        printf(KBRED "\nmalloc devtab backup failed\n" KRESET);
        exit();
    }
    int devtab_backup_len = readfile("/etc/devtab", devtab_backup, devtab_backup_cap);
    if (devtab_backup_len < 0) {
        printf(KBRED "\nread /etc/devtab failed\n" KRESET);
        exit();
    }

    const char *badmajor = "/dev/mknod-badmajor";
    unlink(badmajor);
    if (mknod(badmajor, NDEV + 1, 0) != 0) {
        printf(KBRED "\nmknod %s with invalid major failed\n" KRESET, badmajor);
        exit();
    }
    int badfd = open(badmajor, O_RDWR);
    if (badfd < 0) {
        printf(KBRED "\nopen %s failed\n" KRESET, badmajor);
        exit();
    }
    if (write(badfd, &ch, 1) >= 0) {
        printf(KBRED "\nwrite to %s unexpectedly succeeded\n" KRESET, badmajor);
        exit();
    }
    if (read(badfd, &ch, 1) >= 0) {
        printf(KBRED "\nread from %s unexpectedly succeeded\n" KRESET, badmajor);
        exit();
    }
    close(badfd);
    if (unlink(badmajor) != 0) {
        printf(KBRED "\nunlink %s failed\n" KRESET, badmajor);
        exit();
    }

    char consoledev[64];
    snprintf(consoledev, sizeof(consoledev), "/dev/mknod-console.%d", getpid());
    unlink(consoledev);
    if (mknod(consoledev, CONSOLE, 0) != 0) {
        printf(KBRED "\nmknod %s console failed\n" KRESET, consoledev);
        exit();
    }
    int consolefd = open(consoledev, O_WRONLY);
    if (consolefd < 0) {
        printf(KBRED "\nopen %s failed\n" KRESET, consoledev);
        exit();
    }
    const char *msg = "console device test";
    if (write(consolefd, msg, strlen(msg)) != (int)strlen(msg)) {
        printf(KBRED "\nwrite to %s failed\n" KRESET, consoledev);
        exit();
    }
    close(consolefd);

    int devtab_contents_cap = devtab_backup_len + 128;
    if (devtab_contents_cap < devtab_backup_cap + 1) {
        devtab_contents_cap = devtab_backup_cap + 1;
    }
    char *devtab_contents = malloc(devtab_contents_cap);
    if (devtab_contents == nullptr) {
        printf(KBRED "\nmalloc devtab contents failed\n" KRESET);
        exit();
    }
    int devtab_len = readfile("/etc/devtab", devtab_contents, devtab_contents_cap);
    if (devtab_len < 0) {
        printf(KBRED "\nre-read /etc/devtab failed\n" KRESET);
        exit();
    }
    if (devtab_len <= devtab_backup_len) {
        printf(KBRED "\n/etc/devtab did not grow after mknod\n" KRESET);
        exit();
    }
    int found   = 0;
    int namelen = strlen(consoledev);
    for (int i = 0; i + namelen <= devtab_len; i++) {
        if (strncmp(devtab_contents + i, consoledev, namelen) == 0) {
            found = 1;
            break;
        }
    }
    if (!found) {
        printf(KBRED "\n/etc/devtab missing %s entry\n" KRESET, consoledev);
        exit();
    }

    if (unlink(consoledev) != 0) {
        printf(KBRED "\nunlink %s failed\n" KRESET, consoledev);
        exit();
    }
    free(devtab_contents);
    free(devtab_backup);

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void linktest(void)
{
    printf("linktest");

    unlink("lf1");
    unlink("lf2");

    int fd = open("lf1", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf(KBRED "\ncreate lf1 failed\n" KRESET);
        exit();
    }
    if (write(fd, "hello", 5) != 5) {
        printf(KBRED "\nwrite lf1 failed\n" KRESET);
        exit();
    }
    close(fd);

    if (link("lf1", "lf2") < 0) {
        printf(KBRED "\nlink lf1 lf2 failed\n" KRESET);
        exit();
    }
    unlink("lf1");

    if (open("lf1", 0) >= 0) {
        printf(KBRED "\nunlinked lf1 but it is still there!\n" KRESET);
        exit();
    }

    fd = open("lf2", 0);
    if (fd < 0) {
        printf(KBRED "\nopen lf2 failed\n" KRESET);
        exit();
    }
    if (read(fd, buf, sizeof(buf)) != 5) {
        printf(KBRED "\nread lf2 failed\n" KRESET);
        exit();
    }
    close(fd);

    if (link("lf2", "lf2") >= 0) {
        printf(KBRED "\nlink lf2 lf2 succeeded! oops\n" KRESET);
        exit();
    }

    unlink("lf2");
    if (link("lf2", "lf1") >= 0) {
        printf(KBRED "\nlink non-existant succeeded! oops\n" KRESET);
        exit();
    }

    if (link(".", "lf1") >= 0) {
        printf(KBRED "\nlink . lf1 succeeded! oops\n" KRESET);
        exit();
    }

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

// test concurrent create/link/unlink of the same file
struct concreate_ctx
{
    char *fa;
    int count;
    int error;
};

static int concreate_visit(const struct dirent_view *entry, void *arg)
{
    struct concreate_ctx *ctx = (struct concreate_ctx *)arg;
    if (entry->name_len != 2)
        return 0;
    if (entry->name[0] != 'C' || entry->name[2] != '\0')
        return 0;
    int idx = entry->name[1] - '0';
    if (idx < 0 || idx >= 40) {
        printf("concreate weird file %s\n", entry->name);
        ctx->error = 1;
        return -1;
    }
    if (ctx->fa[idx]) {
        printf("concreate duplicate file %s\n", entry->name);
        ctx->error = 1;
        return -1;
    }
    ctx->fa[idx] = 1;
    ctx->count++;
    return 0;
}

void
concreate(void)
{
    char file[3];
    int i, pid, fd;
    char fa[40];

    printf("concreate test");
    file[0] = 'C';
    file[2] = '\0';
    for (i = 0; i < 40; i++) {
        file[1] = '0' + i;
        unlink(file);
        pid = fork();
        if (pid && (i % 3) == 1) {
            link("C0", file);
        } else if (pid == 0 && (i % 5) == 1) {
            link("C0", file);
        } else {
            fd = open(file, O_CREATE | O_RDWR);
            if (fd < 0) {
                printf(KBRED "\nconcreate create %s failed\n" KRESET, file);
                exit();
            }
            close(fd);
        }
        if (pid == 0)
            exit();
        else
            wait();
    }

    memset(fa, 0, sizeof(fa));
    fd = open(".", 0);
    if (fd < 0) {
        printf(KBRED "\nconcreate: cannot open .\n" KRESET);
        exit();
    }
    struct concreate_ctx ctx = {
        .fa = fa,
        .count = 0,
        .error = 0,
    };
    int walk_rc = dirwalk(fd, concreate_visit, &ctx);
    close(fd);
    if (walk_rc < 0 || ctx.error) {
        if (!ctx.error)
            printf(KBRED "\nconcreate: dirwalk failed\n" KRESET);
        exit();
    }

    if (ctx.count != 40) {
        printf(KBRED "\nconcreate not enough files in directory listing: %d\n" KRESET, ctx.count);
        exit();
    }

    for (i = 0; i < 40; i++) {
        file[1] = '0' + i;
        pid     = fork();
        if (pid < 0) {
            printf(KBRED "\nfork failed\n" KRESET);
            exit();
        }
        if (((i % 3) == 0 && pid == 0) ||
            ((i % 3) == 1 && pid != 0)) {
            close(open(file, 0));
            close(open(file, 0));
            close(open(file, 0));
            close(open(file, 0));
        } else {
            unlink(file);
            unlink(file);
            unlink(file);
            unlink(file);
        }
        if (pid == 0)
            exit();
        else
            wait();
    }

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

// another concurrent link/unlink/create test,
// to look for deadlocks.
void
linkunlink()
{
    printf("linkunlink test");

    unlink("x");
    int pid = fork();
    if (pid < 0) {
        printf(KBRED "\nfork failed\n" KRESET);
        exit();
    }

    unsigned int x = (pid ? 1 : 97);
    for (int i = 0; i < 100; i++) {
        x = x * 1103515245 + 12345;
        if ((x % 3) == 0) {
            close(open("x", O_RDWR | O_CREATE));
        } else if ((x % 3) == 1) {
            link("cat", "x");
        } else {
            unlink("x");
        }
    }

    if (pid)
        wait();
    else
        exit();

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

// directory that uses indirect blocks
void bigdir(void)
{
    int i;
    char dirname[10];

    printf("bigdir test");
    unlink("bd");

    int fd = open("bd", O_CREATE);
    if (fd < 0) {
        printf(KBRED "\nbigdir create failed\n" KRESET);
        exit();
    }
    close(fd);

    for (i = 0; i < 500; i++) {
        dirname[0] = 'x';
        dirname[1] = '0' + (i / 64);
        dirname[2] = '0' + (i % 64);
        dirname[3] = '\0';
        if (link("bd", dirname) != 0) {
            printf(KBRED "\nbigdir link failed\n" KRESET);
            exit();
        }
    }

    unlink("bd");
    for (i = 0; i < 500; i++) {
        dirname[0] = 'x';
        dirname[1] = '0' + (i / 64);
        dirname[2] = '0' + (i % 64);
        dirname[3] = '\0';
        if (unlink(dirname) != 0) {
            printf(KBRED "\nbigdir unlink failed\n" KRESET);
            exit();
        }
    }

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void
subdir(void)
{
    printf("subdir test");

    unlink("ff");
    if (mkdir("dd") != 0) {
        printf(KBRED "\nsubdir mkdir dd failed\n" KRESET);
        exit();
    }

    int fd = open("dd/ff", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf(KBRED "\ncreate dd/ff failed\n" KRESET);
        exit();
    }
    write(fd, "ff", 2);
    close(fd);

    if (unlink("dd") >= 0) {
        printf(KBRED "\nunlink dd (non-empty dir) succeeded!\n" KRESET);
        exit();
    }

    if (mkdir("/dd/dd") != 0) {
        printf(KBRED "\nsubdir mkdir dd/dd failed\n" KRESET);
        exit();
    }

    fd = open("dd/dd/ff", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf(KBRED "\ncreate dd/dd/ff failed\n" KRESET);
        exit();
    }
    write(fd, "FF", 2);
    close(fd);

    fd = open("dd/dd/../ff", 0);
    if (fd < 0) {
        printf(KBRED "\nopen dd/dd/../ff failed\n" KRESET);
        exit();
    }
    int cc = read(fd, buf, sizeof(buf));
    if (cc != 2 || buf[0] != 'f') {
        printf(KBRED "\ndd/dd/../ff wrong content\n" KRESET);
        exit();
    }
    close(fd);

    if (link("dd/dd/ff", "dd/dd/ffff") != 0) {
        printf(KBRED "\nlink dd/dd/ff dd/dd/ffff failed\n" KRESET);
        exit();
    }

    if (unlink("dd/dd/ff") != 0) {
        printf(KBRED "\nunlink dd/dd/ff failed\n" KRESET);
        exit();
    }
    if (open("dd/dd/ff", O_RDONLY) >= 0) {
        printf(KBRED "\nopen (unlinked) dd/dd/ff succeeded\n" KRESET);
        exit();
    }

    if (chdir("dd") != 0) {
        printf(KBRED "\nchdir dd failed\n" KRESET);
        exit();
    }
    if (chdir("dd/../../dd") != 0) {
        printf(KBRED "\nchdir dd/../../dd failed\n" KRESET);
        exit();
    }
    if (chdir("dd/../../../dd") != 0) {
        printf(KBRED "\nchdir dd/../../dd failed\n" KRESET);
        exit();
    }
    if (chdir("./..") != 0) {
        printf(KBRED "\nchdir ./.. failed\n" KRESET);
        exit();
    }

    fd = open("dd/dd/ffff", 0);
    if (fd < 0) {
        printf(KBRED "\nopen dd/dd/ffff failed\n" KRESET);
        exit();
    }
    if (read(fd, buf, sizeof(buf)) != 2) {
        printf(KBRED "\nread dd/dd/ffff wrong len\n" KRESET);
        exit();
    }
    close(fd);

    if (open("dd/dd/ff", O_RDONLY) >= 0) {
        printf(KBRED "\nopen (unlinked) dd/dd/ff succeeded!\n" KRESET);
        exit();
    }

    if (open("dd/ff/ff", O_CREATE | O_RDWR) >= 0) {
        printf(KBRED "\ncreate dd/ff/ff succeeded!\n" KRESET);
        exit();
    }
    if (open("dd/xx/ff", O_CREATE | O_RDWR) >= 0) {
        printf(KBRED "\ncreate dd/xx/ff succeeded!\n" KRESET);
        exit();
    }
    if (open("dd", O_CREATE) >= 0) {
        printf(KBRED "\ncreate dd succeeded!\n" KRESET);
        exit();
    }
    if (open("dd", O_RDWR) >= 0) {
        printf(KBRED "\nopen dd rdwr succeeded!\n" KRESET);
        exit();
    }
    if (open("dd", O_WRONLY) >= 0) {
        printf(KBRED "\nopen dd wronly succeeded!\n" KRESET);
        exit();
    }
    if (link("dd/ff/ff", "dd/dd/xx") == 0) {
        printf(KBRED "\nlink dd/ff/ff dd/dd/xx succeeded!\n" KRESET);
        exit();
    }
    if (link("dd/xx/ff", "dd/dd/xx") == 0) {
        printf(KBRED "\nlink dd/xx/ff dd/dd/xx succeeded!\n" KRESET);
        exit();
    }
    if (link("dd/ff", "dd/dd/ffff") == 0) {
        printf(KBRED "\nlink dd/ff dd/dd/ffff succeeded!\n" KRESET);
        exit();
    }
    if (mkdir("dd/ff/ff") == 0) {
        printf(KBRED "\nmkdir dd/ff/ff succeeded!\n" KRESET);
        exit();
    }
    if (mkdir("dd/xx/ff") == 0) {
        printf(KBRED "\nmkdir dd/xx/ff succeeded!\n" KRESET);
        exit();
    }
    if (mkdir("dd/dd/ffff") == 0) {
        printf(KBRED "\nmkdir dd/dd/ffff succeeded!\n" KRESET);
        exit();
    }
    if (unlink("dd/xx/ff") == 0) {
        printf(KBRED "\nunlink dd/xx/ff succeeded!\n" KRESET);
        exit();
    }
    if (unlink("dd/ff/ff") == 0) {
        printf(KBRED "\nunlink dd/ff/ff succeeded!\n" KRESET);
        exit();
    }
    if (chdir("dd/ff") == 0) {
        printf(KBRED "\nchdir dd/ff succeeded!\n" KRESET);
        exit();
    }
    if (chdir("dd/xx") == 0) {
        printf(KBRED "\nchdir dd/xx succeeded!\n" KRESET);
        exit();
    }

    if (unlink("dd/dd/ffff") != 0) {
        printf(KBRED "\nunlink dd/dd/ff failed\n" KRESET);
        exit();
    }
    if (unlink("dd/ff") != 0) {
        printf(KBRED "\nunlink dd/ff failed\n" KRESET);
        exit();
    }
    if (unlink("dd") == 0) {
        printf(KBRED "\nunlink non-empty dd succeeded!\n" KRESET);
        exit();
    }
    if (unlink("dd/dd") < 0) {
        printf(KBRED "\nunlink dd/dd failed\n" KRESET);
        exit();
    }
    if (unlink("dd") < 0) {
        printf(KBRED "\nunlink dd failed\n" KRESET);
        exit();
    }

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void getcwdtest(void)
{
    printf("getcwd test");

    if (chdir("/") < 0) {
        printf(KBRED "\nchdir / failed\n" KRESET);
        exit();
    }

    char path[128];
    if (getcwd(path, sizeof(path)) < 0 || strcmp(path, "/") != 0) {
        printf(KBRED "\ngetcwd root mismatch\n" KRESET);
        exit();
    }

    char rootbuf[2];
    if (getcwd(rootbuf, sizeof(rootbuf)) < 0 || strcmp(rootbuf, "/") != 0) {
        printf(KBRED "\ngetcwd tiny buffer root mismatch\n" KRESET);
        exit();
    }

    char top[32];
    char child[64];
    char grand[96];
    int pid = getpid();
    snprintf(top, sizeof(top), "cwdtest.%d", pid);
    snprintf(child, sizeof(child), "%s/child", top);
    snprintf(grand, sizeof(grand), "%s/grand", child);

    char abs_top[64];
    char abs_child[96];
    char abs_grand[128];
    snprintf(abs_top, sizeof(abs_top), "/%s", top);
    snprintf(abs_child, sizeof(abs_child), "%s/child", abs_top);
    snprintf(abs_grand, sizeof(abs_grand), "%s/grand", abs_child);

    if (mkdir(top) != 0) {
        printf(KBRED "\nmkdir %s failed\n" KRESET, top);
        exit();
    }
    if (chdir(top) != 0) {
        printf(KBRED "\nchdir %s failed\n" KRESET, top);
        exit();
    }
    if (getcwd(path, sizeof(path)) < 0 || strcmp(path, abs_top) != 0) {
        printf(KBRED "\ngetcwd %s mismatch\n" KRESET, abs_top);
        exit();
    }

    if (mkdir("child") != 0 || chdir("child") != 0) {
        printf(KBRED "\nsetup child dir failed\n" KRESET);
        exit();
    }
    if (getcwd(path, sizeof(path)) < 0 || strcmp(path, abs_child) != 0) {
        printf(KBRED "\ngetcwd %s mismatch\n" KRESET, abs_child);
        exit();
    }

    if (mkdir("grand") != 0 || chdir("./grand") != 0) {
        printf(KBRED "\nsetup grand dir failed\n" KRESET);
        exit();
    }
    if (getcwd(path, sizeof(path)) < 0 || strcmp(path, abs_grand) != 0) {
        printf(KBRED "\ngetcwd %s mismatch\n" KRESET, abs_grand);
        exit();
    }

    if (chdir("../..") != 0) {
        printf(KBRED "\nchdir ../.. failed\n" KRESET);
        exit();
    }
    if (getcwd(path, sizeof(path)) < 0 || strcmp(path, abs_top) != 0) {
        printf(KBRED "\ngetcwd %s mismatch after ../..\n" KRESET, abs_top);
        exit();
    }

    if (chdir("/") != 0) {
        printf(KBRED "\nreturn to / failed\n" KRESET);
        exit();
    }

    if (chdir(child) != 0) {
        printf(KBRED "\nchdir %s failed\n" KRESET, child);
        exit();
    }
    char tiny[8];
    memset(tiny, 'x', sizeof(tiny));
    if (getcwd(tiny, sizeof(tiny)) < 0) {
        printf(KBRED "\ngetcwd tiny buffer failed\n" KRESET);
        exit();
    }
    if (tiny[sizeof(tiny) - 1] != '\0' || strncmp(tiny, abs_child, sizeof(tiny) - 1) != 0) {
        printf(KBRED "\ntiny buffer contents wrong\n" KRESET);
        exit();
    }

    if (chdir("/") != 0) {
        printf(KBRED "\nfinal return to / failed\n" KRESET);
        exit();
    }
    if (unlink(grand) != 0) {
        printf(KBRED "\nunlink %s failed\n" KRESET, grand);
        exit();
    }
    if (unlink(child) != 0) {
        printf(KBRED "\nunlink %s failed\n" KRESET, child);
        exit();
    }
    if (unlink(top) != 0) {
        printf(KBRED "\nunlink %s failed\n" KRESET, top);
        exit();
    }

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void cwdrobusttest(void)
{
    printf("cwd robust test");

    if (chdir("/") != 0) {
        printf(KBRED "\nchdir / failed\n" KRESET);
        exit();
    }

    char buf[32];
    if (getcwd(nullptr, sizeof(buf)) >= 0) {
        printf(KBRED "\ngetcwd accepted null buffer\n" KRESET);
        exit();
    }
    memset(buf, 'z', sizeof(buf));
    if (getcwd(buf, 0) >= 0) {
        printf(KBRED "\ngetcwd accepted zero length\n" KRESET);
        exit();
    }
    if (buf[0] != 'z') {
        printf(KBRED "\ngetcwd zero length corrupted buffer\n" KRESET);
        exit();
    }
    if (getcwd(buf, -1) >= 0) {
        printf(KBRED "\ngetcwd accepted negative length\n" KRESET);
        exit();
    }

    char fulldir[64];
    int pid = getpid();
    snprintf(fulldir, sizeof(fulldir), "cwdchild.%d", pid);
    unlink(fulldir);
    if (mkdir(fulldir) != 0) {
        printf(KBRED "\nmkdir %s failed\n" KRESET, fulldir);
        exit();
    }
    if (chdir(fulldir) != 0) {
        printf(KBRED "\nchdir %s failed\n" KRESET, fulldir);
        exit();
    }
    char expected[128];
    if (getcwd(expected, sizeof(expected)) < 0) {
        printf(KBRED "\ngetcwd in child setup failed\n" KRESET);
        exit();
    }
    int forkpid = fork();
    if (forkpid < 0) {
        printf(KBRED "\nfork failed\n" KRESET);
        exit();
    }
    if (forkpid == 0) {
        char childbuf[128];
        if (getcwd(childbuf, sizeof(childbuf)) < 0 || strcmp(childbuf, expected) != 0) {
            printf(KBRED "\nchild cwd mismatch\n" KRESET);
            exit();
        }
        exit();
    }
    wait();
    if (chdir("/") != 0) {
        printf(KBRED "\nparent return to / failed\n" KRESET);
        exit();
    }
    if (unlink(fulldir) != 0) {
        printf(KBRED "\nunlink %s failed\n" KRESET, fulldir);
        exit();
    }

    char before[128];
    if (getcwd(before, sizeof(before)) < 0) {
        printf(KBRED "\ngetcwd before long path failed\n" KRESET);
        exit();
    }
    char longpath[MAX_FILE_PATH + 32];
    for (int i = 0; i < (int)sizeof(longpath) - 1; i++) {
        longpath[i] = 'a';
    }
    longpath[sizeof(longpath) - 1] = '\0';
    if (chdir(longpath) >= 0) {
        printf(KBRED "\nchdir overly long path unexpectedly succeeded\n" KRESET);
        exit();
    }
    char after[128];
    if (getcwd(after, sizeof(after)) < 0 || strcmp(before, after) != 0) {
        printf(KBRED "\ngetcwd changed after failed chdir\n" KRESET);
        exit();
    }

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

// test writes that are larger than the log.
void
bigwrite(void)
{
    printf("bigwrite test");

    unlink("bigwrite");
    for (int sz = 499; sz < 12 * 512; sz += 471) {
        int fd = open("bigwrite", O_CREATE | O_RDWR);
        if (fd < 0) {
            printf(KBRED "\ncannot create bigwrite\n" KRESET);
            exit();
        }
        for (int i = 0; i < 2; i++) {
            int cc = write(fd, buf, sz);
            if (cc != sz) {
                printf(KBRED "\nwrite(%d) ret %d\n" KRESET, sz, cc);
                exit();
            }
        }
        close(fd);
        unlink("bigwrite");
    }

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void
bigfile(void)
{
    int i;

    printf("bigfile test");

    unlink("bigfile");
    int fd = open("bigfile", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf(KBRED "\ncannot create bigfile\n" KRESET);
        exit();
    }
    for (i = 0; i < 20; i++) {
        memset(buf, i, 600);
        if (write(fd, buf, 600) != 600) {
            printf(KBRED "\nwrite bigfile failed\n" KRESET);
            exit();
        }
    }
    close(fd);

    fd = open("bigfile", 0);
    if (fd < 0) {
        printf(KBRED "\ncannot open bigfile\n" KRESET);
        exit();
    }
    int total = 0;
    for (i = 0; ; i++) {
        int cc = read(fd, buf, 300);
        if (cc < 0) {
            printf(KBRED "\nread bigfile failed\n" KRESET);
            exit();
        }
        if (cc == 0)
            break;
        if (cc != 300) {
            printf(KBRED "\nshort read bigfile\n" KRESET);
            exit();
        }
        if (buf[0] != i / 2 || buf[299] != i / 2) {
            printf(KBRED "\nread bigfile wrong data\n" KRESET);
            exit();
        }
        total += cc;
    }
    close(fd);
    if (total != 20 * 600) {
        printf(KBRED "\nread bigfile wrong total\n" KRESET);
        exit();
    }
    unlink("bigfile");

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void
fourteen(void)
{
    printf("fourteen test");

    if (mkdir("12345678901234") != 0) {
        printf(KBRED "\nmkdir 12345678901234 failed\n" KRESET);
        exit();
    }
    if (mkdir("12345678901234/123456789012345") != 0) {
        printf(KBRED "\nmkdir 12345678901234/123456789012345 failed\n" KRESET);
        exit();
    }

    int fd = open("12345678901234/123456789012345/123456789012345", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf(KBRED "\ncreate 12345678901234/123456789012345/123456789012345 failed\n" KRESET);
        exit();
    }
    close(fd);

    fd = open("12345678901234/123456789012345/123456789012345", 0);
    if (fd < 0) {
        printf(KBRED "\nopen 12345678901234/123456789012345/123456789012345 failed\n" KRESET);
        exit();
    }
    close(fd);

    if (open("123456789012345/123456789012345/123456789012345", 0) >= 0) {
        printf(KBRED "\nopen 123456789012345/123456789012345/123456789012345 unexpectedly succeeded\n" KRESET);
        exit();
    }

    if (mkdir("12345678901234/12345678901234") != 0) {
        printf(KBRED "\nmkdir 12345678901234/12345678901234 failed\n" KRESET);
        exit();
    }
    if (mkdir("123456789012345/12345678901234") == 0) {
        printf(KBRED "\nmkdir 123456789012345/12345678901234 unexpectedly succeeded\n" KRESET);
        exit();
    }

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void
rmdot(void)
{
    printf("rmdot test");
    if (mkdir("dots") != 0) {
        printf(KBRED "\nmkdir dots failed\n" KRESET);
        exit();
    }
    if (chdir("dots") != 0) {
        printf(KBRED "\nchdir dots failed\n" KRESET);
        exit();
    }
    if (unlink(".") == 0) {
        printf(KBRED "\nrm . worked!\n" KRESET);
        exit();
    }
    if (unlink("..") == 0) {
        printf(KBRED "\nrm .. worked!\n" KRESET);
        exit();
    }
    if (chdir("/") != 0) {
        printf(KBRED "\nchdir / failed\n" KRESET);
        exit();
    }
    if (unlink("dots/.") == 0) {
        printf(KBRED "\nunlink dots/. worked!\n" KRESET);
        exit();
    }
    if (unlink("dots/..") == 0) {
        printf(KBRED "\nunlink dots/.. worked!\n" KRESET);
        exit();
    }
    if (unlink("dots") != 0) {
        printf(KBRED "\nunlink dots failed!\n" KRESET);
        exit();
    }
    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void
dirfile(void)
{
    printf("dir vs file");

    int fd = open("dirfile", O_CREATE);
    if (fd < 0) {
        printf(KBRED "\ncreate dirfile failed\n" KRESET);
        exit();
    }
    close(fd);
    if (chdir("dirfile") == 0) {
        printf(KBRED "\nchdir dirfile succeeded!\n" KRESET);
        exit();
    }
    fd = open("dirfile/xx", 0);
    if (fd >= 0) {
        printf(KBRED "\ncreate dirfile/xx succeeded!\n" KRESET);
        exit();
    }
    fd = open("dirfile/xx", O_CREATE);
    if (fd >= 0) {
        printf(KBRED "\ncreate dirfile/xx succeeded!\n" KRESET);
        exit();
    }
    if (mkdir("dirfile/xx") == 0) {
        printf(KBRED "\nmkdir dirfile/xx succeeded!\n" KRESET);
        exit();
    }
    if (unlink("dirfile/xx") == 0) {
        printf(KBRED "\nunlink dirfile/xx succeeded!\n" KRESET);
        exit();
    }
    if (link("README", "dirfile/xx") == 0) {
        printf(KBRED "\nlink to dirfile/xx succeeded!\n" KRESET);
        exit();
    }
    if (unlink("dirfile") != 0) {
        printf(KBRED "\nunlink dirfile failed!\n" KRESET);
        exit();
    }

    fd = open(".", O_RDWR);
    if (fd >= 0) {
        printf(KBRED "\nopen . for writing succeeded!\n" KRESET);
        exit();
    }
    fd = open(".", 0);
    if (write(fd, "x", 1) > 0) {
        printf(KBRED "\nwrite . succeeded!\n" KRESET);
        exit();
    }
    close(fd);

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

// test that iput() is called at the end of _namei()
void
iref(void)
{
    printf("empty file name");

    // the 50 is NINODE
    for (int i = 0; i < 50 + 1; i++) {
        if (mkdir("irefd") != 0) {
            printf(KBRED "\nmkdir irefd failed\n" KRESET);
            exit();
        }
        if (chdir("irefd") != 0) {
            printf(KBRED "\nchdir irefd failed\n" KRESET);
            exit();
        }

        mkdir("");
        link("README", "");
        int fd = open("", O_CREATE);
        if (fd >= 0)
            close(fd);
        fd = open("xx", O_CREATE);
        if (fd >= 0)
            close(fd);
        unlink("xx");
    }

    chdir("/");
    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

// test that fork fails gracefully
// the forktest binary also does this, but it runs out of proc entries first.
// inside the bigger usertests binary, we run out of memory first.
void
forktest(void)
{
    int n;

    printf("fork test");

    for (n = 0; n < 1000; n++) {
        int pid = fork();
        if (pid < 0)
            break;
        if (pid == 0)
            exit();
    }

    if (n == 1000) {
        printf(KBRED "\nfork claimed to work 1000 times!\n" KRESET);
        exit();
    }

    for (; n > 0; n--) {
        if (wait() < 0) {
            printf(KBRED "\nwait stopped early\n" KRESET);
            exit();
        }
    }

    if (wait() != -1) {
        printf(KBRED "\nwait got too many\n" KRESET);
        exit();
    }

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void sbrktest(void)
{
    int fds[2], pids[10];
    char scratch;

    printf("sbrk test\n");
    char *oldbrk = sbrk(0);

    // can one sbrk() less than a page?
    char *a = sbrk(0);
    int i;
    for (i = 0; i < 5000; i++) {
        char *b = sbrk(1);
        if (b != a) {
            printf("sbrk test failed %d %s %s\n", i, a, b);
            exit();
        }
        *b = 1;
        a  = b + 1;
    }
    int pid = fork();
    if (pid < 0) {
        printf("sbrk test fork failed\n");
        exit();
    }
    char *c = sbrk(1);
    c       = sbrk(1);
    if (c != a + 1) {
        printf("sbrk test failed post-fork\n");
        exit();
    }
    if (pid == 0)
        exit();
    wait();

    // can one grow address space to something big?
#define BIG (100*1024*1024)
    a       = sbrk(0);
    u32 amt = (BIG) - (u32)a;
    char *p = sbrk(amt);
    if (p != a) {
        printf("sbrk test failed to grow big address space; enough phys mem?\n");
        exit();
    }
    char *lastaddr = (char *)(BIG - 1);
    *lastaddr      = 99;

    // can one de-allocate?
    a = sbrk(0);
    c = sbrk(-4096);
    if (c == (char *)0xffffffff) {
        printf("sbrk could not deallocate\n");
        exit();
    }
    c = sbrk(0);
    if (c != a - 4096) {
        printf("sbrk deallocation produced wrong address, a %s c %s\n", a, c);
        exit();
    }

    // can one re-allocate that page?
    a = sbrk(0);
    c = sbrk(4096);
    if (c != a || sbrk(0) != a + 4096) {
        printf("sbrk re-allocation failed, a %s c %s\n", a, c);
        exit();
    }
    if (*lastaddr == 99) {
        // should be zero
        printf("sbrk de-allocation didn't really deallocate\n");
        exit();
    }

    a = sbrk(0);
    c = sbrk(-(sbrk(0) - oldbrk));
    if (c != a) {
        printf("sbrk downsize failed, a %s c %s\n", a, c);
        exit();
    }

    // can we read the kernel's memory?
    for (a = (char *)(KERNBASE); a < (char *)(KERNBASE + 2000000); a += 50000) {
        int ppid = getpid();
        pid      = fork();
        if (pid < 0) {
            printf("fork failed\n");
            exit();
        }
        if (pid == 0) {
            printf("oops could read %s = %x\n", a, *a);
            kill(ppid);
            exit();
        }
        wait();
    }

    // if we run the system out of memory, does it clean up the last
    // failed allocation?
    if (pipe(fds) != 0) {
        printf("pipe() failed\n");
        exit();
    }
    for (i = 0; i < sizeof(pids) / sizeof(pids[0]); i++) {
        if ((pids[i] = fork()) == 0) {
            // allocate a lot of memory
            sbrk(BIG - (u32)sbrk(0));
            write(fds[1], "x", 1);
            // sit around until killed
            for (;;)
                sleep(1000);
        }
        if (pids[i] != -1)
            read(fds[0], &scratch, 1);
    }
    // if those failed allocations freed up the pages they did allocate,
    // we'll be able to allocate here
    c = sbrk(4096);
    for (i = 0; i < sizeof(pids) / sizeof(pids[0]); i++) {
        if (pids[i] == -1)
            continue;
        kill(pids[i]);
        wait();
    }
    if (c == (char *)0xffffffff) {
        printf("failed sbrk leaked memory\n");
        exit();
    }

    if (sbrk(0) > oldbrk)
        sbrk(-(sbrk(0) - oldbrk));

    printf("sbrk test [ " KBGRN "OK" KRESET " ]\n");
}

void validateint(int *p)
{
    int res;
    __asm__("mov %%esp, %%ebx\n\t"
        "mov %3, %%esp\n\t"
        "int %2\n\t"
        "mov %%ebx, %%esp" :
        "=a" (res) :
        "a" (SYS_sleep), "n" (T_SYSCALL), "c" (p) :
        "ebx");
}

void validatetest(void)
{
    int pid;

    printf("validate test");
    int hi = 1100 * 1024;

    for (u32 p = 0; p <= (u32)hi; p += 4096) {
        if ((pid = fork()) == 0) {
            // try to crash the kernel by passing in a badly placed integer
            validateint((int *)p);
            exit();
        }
        sleep(0);
        sleep(0);
        kill(pid);
        wait();

        // try to crash the kernel by passing in a bad string pointer
        if (link("nosuchfile", (char *)p) != -1) {
            printf("link should not succeed\n");
            exit();
        }
    }

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void uptimeyieldtest(void)
{
    printf("uptime/yield test");

    int first = uptime();
    if (first < 0) {
        printf(KBRED "\nuptime initial call failed\n" KRESET);
        exit();
    }
    int second = uptime();
    if (second < first) {
        printf(KBRED "\nuptime went backwards\n" KRESET);
        exit();
    }

    const int sleep_ticks = 20;
    if (sleep(sleep_ticks) < 0) {
        printf(KBRED "\nsleep failed\n" KRESET);
        exit();
    }
    int after_sleep = uptime();
    if (after_sleep - first < sleep_ticks) {
        printf(KBRED "\nuptime did not advance enough (%d)\n" KRESET, after_sleep - first);
        exit();
    }

    int spin_goal = uptime() + 5;
    int yields    = 0;
    while (uptime() < spin_goal) {
        if (yield() < 0) {
            printf(KBRED "\nyield failed\n" KRESET);
            exit();
        }
        if (++yields > 1000000) {
            printf(KBRED "\nyield spin exceeded limit\n" KRESET);
            exit();
        }
    }

    int pfds[2];
    if (pipe(pfds) != 0) {
        printf(KBRED "\npipe failed\n" KRESET);
        exit();
    }
    int pid = fork();
    if (pid < 0) {
        printf(KBRED "\nfork failed\n" KRESET);
        exit();
    }
    if (pid == 0) {
        close(pfds[0]);
        for (int i = 0; i < 50; i++) {
            yield();
        }
        if (write(pfds[1], "y", 1) != 1) {
            printf(KBRED "\nchild pipe write failed\n" KRESET);
            exit();
        }
        close(pfds[1]);
        exit();
    }

    close(pfds[1]);
    for (int i = 0; i < 50; i++) {
        yield();
    }
    char ch;
    if (read(pfds[0], &ch, 1) != 1) {
        printf(KBRED "\nparent pipe read failed\n" KRESET);
        exit();
    }
    if (ch != 'y') {
        printf(KBRED "\nparent pipe wrong data\n" KRESET);
        exit();
    }
    close(pfds[0]);
    wait();

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

// does unintialized data start out zero?
char uninit[10000];

void bsstest(void)
{
    printf("bss test");
    for (int i = 0; i < sizeof(uninit); i++) {
        if (uninit[i] != '\0') {
            printf(KBRED "\nbss test failed\n" KRESET);
            exit();
        }
    }
    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

// does exec return an error if the arguments
// are larger than a page? or does it write
// below the stack and wreck the instructions/data?
void bigargtest(void)
{
    int fd;

    unlink("bigarg-ok");
    int pid = fork();
    if (pid == 0) {
        static char *args[MAXARG];
        for (int i  = 0; i < MAXARG - 1; i++)
            args[i] =
                "bigargs test: failed\n                                                                                                                                                                                                       ";
        args[MAXARG - 1] = 0;
        printf("bigarg test");
        exec("/bin/echo", args);
        printf(" [ " KBGRN "OK" KRESET " ]\n");
        fd = open("bigarg-ok", O_CREATE);
        close(fd);
        exit();
    } else if (pid < 0) {
        printf(KBRED "\nbigargtest: fork failed\n" KRESET);
        exit();
    }
    wait();
    fd = open("bigarg-ok", 0);
    if (fd < 0) {
        printf(KBRED "\nbigarg test failed!\n" KRESET);
        exit();
    }
    close(fd);
    unlink("bigarg-ok");
}

// what happens when the file system runs out of blocks?
// answer: balloc panics, so this test is not useful.
void fsfull()
{
    int nfiles;
    int fsblocks = 0;

    printf("fsfull test");

    for (nfiles = 0; ; nfiles++) {
        char name[64];
        name[0] = 'f';
        name[1] = '0' + nfiles / 1000;
        name[2] = '0' + (nfiles % 1000) / 100;
        name[3] = '0' + (nfiles % 100) / 10;
        name[4] = '0' + (nfiles % 10);
        name[5] = '\0';
        printf("writing %s\n", name);
        int fd = open(name, O_CREATE | O_RDWR);
        if (fd < 0) {
            printf("open %s failed\n", name);
            break;
        }
        int total = 0;
        while (1) {
            int cc = write(fd, buf, 512);
            if (cc < 512)
                break;
            total += cc;
            fsblocks++;
        }
        printf("wrote %d bytes\n", total);
        close(fd);
        if (total == 0)
            break;
    }

    while (nfiles >= 0) {
        char name[64];
        name[0] = 'f';
        name[1] = '0' + nfiles / 1000;
        name[2] = '0' + (nfiles % 1000) / 100;
        name[3] = '0' + (nfiles % 100) / 10;
        name[4] = '0' + (nfiles % 10);
        name[5] = '\0';
        unlink(name);
        nfiles--;
    }

    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void uio()
{
#define RTC_ADDR 0x70
#define RTC_DATA 0x71

    u16 port = 0;
    u8 val   = 0;
    int pid;

    printf("uio test\n");
    pid = fork();
    if (pid == 0) {
        port = RTC_ADDR;
        val  = 0x09; /* year */
        /* http://wiki.osdev.org/Inline_Assembly/Examples */
        __asm__ volatile("outb %0,%1"::"a"(val), "d" (port));
        port = RTC_DATA;
        __asm__ volatile("inb %1,%0" : "=a" (val) : "d" (port));
        printf(KBRED "\nuio: uio succeeded; test FAILED\n" KRESET);
        exit();
    } else if (pid < 0) {
        printf(KBRED "\nfork failed\n" KRESET);
        exit();
    }
    wait();
    printf("uio test [ " KBGRN "OK" KRESET " ]\n");
}

void argptest()
{
    printf("arg test");
    int fd = open("/bin/init", O_RDONLY);
    if (fd < 0) {
        printf(KBRED "\nopen failed\n" KRESET);
        exit();
    }
    read(fd, sbrk(0) - 1, -1);
    close(fd);
    printf(" [ " KBGRN "OK" KRESET " ]\n");
}

void fb_mmap_basic_test(void)
{
    printf("fb mmap basic");
    int fd = open_framebuffer();
    if (fd < 0) {
        return;
    }

    volatile u32 *fb = mmap(nullptr, PGSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) {
        printf(KBRED "\nfb mmap basic: mmap failed\n" KRESET);
        close(fd);
        exit();
    }

    fb[0] = 0x00FF00;
    fb[1] = 0x0000FF;
    printf(" [ " KBGRN "OK" KRESET " ]\n");
    munmap((void *)fb, PGSIZE);
    close(fd);
}

void fb_mmap_multi_test(void)
{
    printf("fb mmap multi");
    int fd = open_framebuffer();
    if (fd < 0) {
        return;
    }

    volatile u32 *first = mmap(nullptr, PGSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (first == MAP_FAILED) {
        printf(KBRED "\nfb mmap multi: first mmap failed\n" KRESET);
        close(fd);
        exit();
    }
    volatile u32 *second = mmap(nullptr, PGSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (second == MAP_FAILED || second != first) {
        printf(KBRED "\nfb mmap multi: second mmap mismatch\n" KRESET);
        close(fd);
        exit();
    }

    second[2] = 0x00ffff;

    u32 color = 0xff00ff;
    if (write(fd, &color, sizeof(color)) != sizeof(color)) {
        printf(KBRED "\nfb mmap multi: write failed\n" KRESET);
        close(fd);
        exit();
    }

    printf(" [ " KBGRN "OK" KRESET " ]\n");
    munmap((void *)first, PGSIZE);
    close(fd);
}

unsigned long randstate = 1;

unsigned int
rand()
{
    randstate = randstate * 1664525 + 1013904223;
    return randstate;
}

int main(int argc, char *argv[])
{
    printf("usertests starting\n");

    if (open("usertests.ran", 0) >= 0) {
        printf("already ran user tests -- rebuild fs.img\n");
        exit();
    }
    close(open("usertests.ran", O_CREATE));

    argptest();
    createdelete();
    linkunlink();
    concreate();
    fourfiles();
    sharedfd();
    duptest();

    bigargtest();
    bigwrite();
    bigargtest();
    bsstest();
    sbrktest();
    validatetest();
    uptimeyieldtest();

    opentest();
    writetest();
    writetest1();
    createtest();

    openiputtest();
    exitiputtest();
    iputtest();

    mem();
    pipe1();
    preempt();
    exitwait();
    if (framebuffer_mmap_supported()) {
        fb_mmap_basic_test();
        fb_mmap_multi_test();
    } else {
        printf(KBYEL "\nfb mmap tests skipped: graphics mode unavailable\n" KRESET);
    }

    rmdot();
    fourteen();
    bigfile();
    subdir();
    getcwdtest();
    cwdrobusttest();
    linktest();
    unlinkread();
    fstattest();
    mknodtest();
    dirfile();
    iref();
    forktest();
    bigdir(); // slow

    uio();

    exectest();
}
