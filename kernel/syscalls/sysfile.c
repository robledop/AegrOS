//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "assert.h"
#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "proc.h"
#include "ext2.h"
#include "file.h"
#include "fcntl.h"
#include "printf.h"
#include "string.h"

extern struct inode devtab[NDEV];
bool devtab_parsed = false;
void parse_devtab();

/**
 * @brief Normalize an absolute path by collapsing '.', '..', and duplicate '/'.
 *
 * @param path Absolute path to normalize.
 * @param output Destination buffer.
 * @param outsz Size of destination buffer.
 * @return 0 on success, -1 if the path cannot be normalized.
 */
static int canonicalize_absolute_path(const char *path, char *output, int outsz)
{
    if (path == nullptr || output == nullptr || outsz <= 1) {
        return -1;
    }
    if (*path != '/') {
        return -1;
    }

    int stack_depth = 0;
    int stack[MAX_FILE_PATH];
    int len   = 1;
    output[0] = '/';
    output[1] = '\0';

    const char *p = path;
    while (*p == '/') {
        p++;
    }

    while (*p != '\0') {
        const char *start = p;
        while (*p != '/' && *p != '\0') {
            p++;
        }
        const int segment_len = p - start;
        if (segment_len == 0) {
            while (*p == '/') {
                p++;
            }
            continue;
        }
        if (segment_len == 1 && start[0] == '.') {
            while (*p == '/') {
                p++;
            }
            continue;
        }
        if (segment_len == 2 && start[0] == '.' && start[1] == '.') {
            if (stack_depth > 0) {
                len         = stack[--stack_depth];
                output[len] = '\0';
            }
            while (*p == '/') {
                p++;
            }
            continue;
        }
        if (segment_len > EXT2_NAME_LEN) {
            return -1;
        }
        if (stack_depth >= MAX_FILE_PATH) {
            return -1;
        }

        const int prev_len = len;
        if (len > 1) {
            if (len + 1 >= outsz) {
                return -1;
            }
            output[len++] = '/';
        }
        if (len + segment_len >= outsz) {
            return -1;
        }
        memmove(output + len, start, segment_len);
        len += segment_len;
        output[len]          = '\0';
        stack[stack_depth++] = prev_len;

        while (*p == '/') {
            p++;
        }
    }

    if (len == 0) {
        output[0] = '/';
        output[1] = '\0';
    }
    return 0;
}

/**
 * @brief Build an absolute, canonical path for the requested directory change.
 *
 * @param proc Current process (provides cwd fallback).
 * @param request User-supplied path (absolute or relative).
 * @param resolved Output buffer receiving a canonical absolute path.
 * @return 0 on success, -1 on failure.
 */
static int resolve_cwd_path(struct proc *proc, const char *request, char *resolved)
{
    if (request == nullptr || request[0] == '\0' || resolved == nullptr) {
        return -1;
    }

    if (request[0] == '/') {
        if (canonicalize_absolute_path(request, resolved, MAX_FILE_PATH) < 0) {
            resolved[0] = '\0';
            return 1;
        }
        return 0;
    }

    if (proc->cwd_path[0] == '\0') {
        resolved[0] = '\0';
        return 1;
    }

    char combined[MAX_FILE_PATH];
    const char *base = proc->cwd_path;
    if (base[0] != '/') {
        base = "/";
    }
    const int base_len = strnlen(base, MAX_FILE_PATH);
    const int req_len  = strnlen(request, MAX_FILE_PATH);
    if (base_len < 0 || req_len < 0) {
        return -1;
    }

    safestrcpy(combined, base, MAX_FILE_PATH);
    int len = base_len;
    if (len == 0) {
        combined[0] = '/';
        combined[1] = '\0';
        len         = 1;
    }
    if (len > 1 && combined[len - 1] != '/') {
        if (len + 1 >= MAX_FILE_PATH) {
            resolved[0] = '\0';
            return 1;
        }
        combined[len++] = '/';
        combined[len]   = '\0';
    }
    if (len + req_len >= MAX_FILE_PATH) {
        resolved[0] = '\0';
        return 1;
    }
    memmove(combined + len, request, req_len);
    len += req_len;
    combined[len] = '\0';

    if (canonicalize_absolute_path(combined, resolved, MAX_FILE_PATH) < 0) {
        resolved[0] = '\0';
        return 1;
    }
    return 0;
}

/**
 * @brief Fetch a file descriptor argument and return its struct file.
 *
 * @param n Argument index.
 * @param pfd Optional destination for the descriptor value.
 * @param pf Optional destination for the file pointer.
 * @return 0 on success, -1 if the descriptor is invalid.
 */
static int argfd(int n, int *pfd, struct file **pf)
{
    int fd;
    struct file *f;

    if (argint(n, &fd) < 0) {
        return -1;
    }
    if (fd < 0 || fd >= NOFILE || (f = current_process()->ofile[fd]) == nullptr) {
        return -1;
    }
    if (pfd) {
        *pfd = fd;
    }
    if (pf) {
        *pf = f;
    }
    return 0;
}

/**
 * @brief Allocate a new descriptor slot for an open file.
 *
 * Takes ownership of the reference on success.
 *
 * @param f File to install.
 * @return Descriptor index or -1 if the table is full.
 */
static int fdalloc(struct file *f)
{
    struct proc *curproc = current_process();

    for (int fd = 0; fd < NOFILE; fd++) {
        if (curproc->ofile[fd] == nullptr) {
            curproc->ofile[fd] = f;
            return fd;
        }
    }
    return -1;
}

/** @brief Duplicate a file descriptor (syscall handler). */
int sys_dup(void)
{
    struct file *f;
    int fd;

    if (argfd(0, nullptr, &f) < 0) {
        return -1;
    }
    if ((fd = fdalloc(f)) < 0) {
        return -1;
    }
    filedup(f);
    return fd;
}

/** @brief Read from a file descriptor into user memory. */
int sys_read(void)
{
    struct file *f;
    int n;
    char *p;

    if (argfd(0, nullptr, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0) {
        return -1;
    }
    return fileread(f, p, n);
}

/** @brief Write user memory to a file descriptor. */
int sys_write(void)
{
    struct file *f;
    int n;
    char *p;

    if (argfd(0, nullptr, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0) {
        return -1;
    }
    return filewrite(f, p, n);
}

/** @brief Close a file descriptor. */
int sys_close(void)
{
    int fd;
    struct file *f;

    if (argfd(0, &fd, &f) < 0) {
        return -1;
    }
    current_process()->ofile[fd] = nullptr;
    fileclose(f);
    return 0;
}

/** @brief Retrieve file metadata for a descriptor. */
int sys_fstat(void)
{
    struct file *f;
    struct stat *st;

    if (argfd(0, nullptr, &f) < 0 || argptr(1, (void *)&st, sizeof(*st)) < 0) {
        return -1;
    }
    return filestat(f, st);
}

/**
 * @brief Create a new hard link to an existing inode.
 *
 * @return 0 on success, -1 on error.
 */
int sys_link(void)
{
    char name[EXT2_NAME_LEN + 1], *
         new, *old;
    struct inode *dp, *ip;

    if (argstr(0, &old) < 0 || argstr(1, &new) < 0) {
        return -1;
    }

    if ((ip = namei(old)) == nullptr) {
        return -1;
    }

    ip->iops->ilock(ip);
    if (ip->type == T_DIR) {
        ip->iops->iunlockput(ip);
        return -1;
    }

    ip->nlink++;
    ip->iops->iupdate(ip);
    ip->iops->iunlock(ip);

    if ((dp = nameiparent(new, name)) == nullptr)
        goto bad;
    ip->iops->ilock(dp);
    if (dp->dev != ip->dev || ip->iops->dirlink(dp, name, ip->inum) < 0) {
        ip->iops->iunlockput(dp);
        goto bad;
    }
    ip->iops->iunlockput(dp);
    ip->iops->iput(ip);


    return 0;

bad:
    ip->iops->ilock(ip);
    ip->nlink--;
    ip->iops->iupdate(ip);
    ip->iops->iunlockput(ip);
    return -1;
}

/**
 * @brief Determine whether a directory contains entries other than '.' and '..'.
 *
 * @param dp Directory inode.
 * @return Non-zero if empty, zero otherwise.
 */
static int isdirempty(struct inode *dp)
{
    struct ext2_dir_entry_2 de;

    for (u32 off = 0; off < dp->size;) {
        memset(&de, 0, sizeof(de));
        if (dp->iops->readi(dp, (char *)&de, off, 8) != 8)
            panic("isdirempty: read header");
        if (de.rec_len < 8 || de.rec_len > EXT2_BSIZE)
            panic("isdirempty: bad rec_len");
        if (de.name_len > EXT2_NAME_LEN)
            panic("isdirempty: bad name_len");
        if (de.name_len > 0) {
            if (dp->iops->readi(dp, de.name, off + 8, de.name_len) != de.name_len)
                panic("isdirempty: read name");
        }
        if (de.inode != 0) {
            int isdot    = (de.name_len == 1 && de.name[0] == '.');
            int isdotdot = (de.name_len == 2 && de.name[0] == '.' && de.name[1] == '.');
            if (!isdot && !isdotdot)
                return 0;
        }
        off += de.rec_len;
    }
    return 1;
}

/**
 * @brief Remove a directory entry.
 *
 * @return 0 on success, -1 on error.
 */
int sys_unlink(void)
{
    struct inode *ip, *dp;
    char name[EXT2_NAME_LEN + 1], *path;
    u32 off;

    if (argstr(0, &path) < 0)
        return -1;

    if ((dp = nameiparent(path, name)) == nullptr) {
        return -1;
    }

    dp->iops->ilock(dp);

    // Cannot unlink "." or "..".
    if ((name[0] == '.' && name[1] == '\0') ||
        (name[0] == '.' && name[1] == '.' && name[2] == '\0'))
        goto bad;

    if ((ip = dp->iops->dirlookup(dp, name, &off)) == nullptr)
        goto bad;
    ip->iops->ilock(ip);

    if (ip->nlink < 1)
        panic("unlink: nlink < 1");
    if (ip->type == T_DIR && !isdirempty(ip)) {
        ip->iops->iunlockput(ip);
        goto bad;
    }

    u32 zero = 0;
    if (dp->iops->writei(dp, (char *)&zero, off, sizeof(zero)) != sizeof(zero))
        panic("unlink: write inode");
    if (ip->type == T_DIR) {
        dp->nlink--;
        ip->iops->iupdate(dp);
    }
    dp->iops->iunlockput(dp);

    ip->nlink--;
    ip->iops->iupdate(ip);
    ip->iops->iunlockput(ip);

    return 0;

bad:
    dp->iops->iunlockput(dp);
    // end_op();
    return -1;
}

/**
 * @brief Create a new inode and directory entry.
 *
 * @param path Full path to the new entry.
 * @param type Inode type (T_DIR, T_FILE, etc.).
 * @param major Device major number (for T_DEV).
 * @param minor Device minor number (for T_DEV).
 * @return Referenced inode on success, or 0 on failure.
 */
static struct inode *create(char *path, short type, short major, short minor)
{
    struct inode *ip, *dp;
    char name[EXT2_NAME_LEN + 1];

    if ((dp = nameiparent(path, name)) == nullptr)
        return nullptr;
    dp->iops->ilock(dp);

    if ((ip = dp->iops->dirlookup(dp, name, nullptr)) != nullptr) {
        dp->iops->iunlockput(dp);
        ip->iops->ilock(ip);
        if (type == T_FILE && ip->type == T_FILE)
            return ip;
        ip->iops->iunlockput(ip);
        return nullptr;
    }

    if ((ip = dp->iops->ialloc(dp->dev, type)) == nullptr) {
        panic("create: ialloc");
    }

    ASSERT(ip->addrs != nullptr, "ip->addrs is null in create");
    ip->iops->ilock(ip);
    ip->major = major;
    ip->minor = minor;
    ip->nlink = 1;
    ip->iops->iupdate(ip);

    if (type == T_DIR) {
        // Create . and .. entries.
        dp->nlink++; // for ".."
        dp->iops->iupdate(dp);
        // No ip->nlink++ for ".": avoid cyclic ref count.
        if (ip->iops->dirlink(ip, ".", ip->inum) < 0 || ip->iops->dirlink(ip, "..", dp->inum) < 0)
            panic("create dots");
    }

    if (dp->iops->dirlink(dp, name, ip->inum) < 0)
        panic("create: dirlink");

    dp->iops->iunlockput(dp);

    if (ip->type == T_DEV && (major != 0 || minor != 0)) {
        const int fd      = open_file("/etc/devtab", O_RDWR);
        struct file *file = current_process()->ofile[fd];
        char buf[64];
        const int n = snprintf(buf, sizeof(buf), "%d\tchar\t%d\t%d\t#%s\n", ip->inum, major, minor, path);

        filewrite(file, buf, n);
        fileclose(file);
        current_process()->ofile[fd] = nullptr;

        bool found = false;
        for (int i = 0; i < NDEV; i++) {
            if (devtab[i].inum == ip->inum) {
                devtab[i].inum  = ip->inum;
                devtab[i].dev   = ip->dev;
                devtab[i].type  = ip->type;
                devtab[i].major = ip->major;
                devtab[i].minor = ip->minor;

                found = true;
                break;
            }
        }

        if (!found) {
            for (int i = 0; i < NDEV; i++) {
                if (devtab[i].inum == 0) {
                    devtab[i].inum  = ip->inum;
                    devtab[i].dev   = ip->dev;
                    devtab[i].type  = ip->type;
                    devtab[i].major = ip->major;
                    devtab[i].minor = ip->minor;
                    break;
                }
            }
        }
    }

    return ip;
}

void parse_devtab()
{
    const int fd      = open_file("/etc/devtab", O_RDWR);
    struct file *file = current_process()->ofile[fd];
    char buf[512];
    struct stat st;
    filestat(file, &st);
    const int n = fileread(file, buf, st.size);
    buf[n]      = '\0';

    for (char *line = strtok(buf, "\n"); line != nullptr; line = strtok(nullptr, "\n")) {
        u32 inum, major, minor;
        char type[16];
        if (sscanf(line, "%d\t%s\t%d\t%d", &inum, type, &major, &minor) == 4) {
            struct inode *ip = iget(ROOTDEV, inum);
            ip->iops->ilock(ip);
            ip->type  = T_DEV;
            ip->nlink = 1;
            ip->dev   = 0;
            ip->ref   = 1;
            ip->valid = 1;
            ip->major = major;
            ip->minor = minor;
            ip->iops->iunlock(ip);
            ip->iops->iput(ip);

            bool found = false;
            for (int i = 0; i < NDEV; i++) {
                if (devtab[i].inum == inum) {
                    devtab[i].inum  = ip->inum;
                    devtab[i].dev   = ip->dev;
                    devtab[i].type  = ip->type;
                    devtab[i].major = ip->major;
                    devtab[i].minor = ip->minor;
                    found           = true;
                    break;
                }
            }

            if (!found) {
                for (int i = 0; i < NDEV; i++) {
                    if (devtab[i].inum == 0) {
                        devtab[i].inum  = ip->inum;
                        devtab[i].dev   = ip->dev;
                        devtab[i].type  = ip->type;
                        devtab[i].major = ip->major;
                        devtab[i].minor = ip->minor;
                        break;
                    }
                }
            }
        }
    }
    fileclose(file);
}

int open_file(char *path, int omode)
{
    int fd;
    struct file *f;
    struct inode *ip;

    if (!devtab_parsed) {
        devtab_parsed = true;
        parse_devtab();
    }

    if (omode & O_CREATE) {
        ip = create(path, T_FILE, 0, 0);
        if (ip == nullptr) {
            return -1;
        }
    } else {
        if ((ip = namei(path)) == nullptr) {
            return -1;
        }
        ip->iops->ilock(ip);
        if (ip->type == T_DIR && omode != O_RDONLY) {
            ip->iops->iunlockput(ip);
            return -1;
        }
    }

    if ((f = filealloc()) == nullptr || (fd = fdalloc(f)) < 0) {
        if (f) {
            fileclose(f);
        }
        ip->iops->iunlockput(ip);
        return -1;
    }
    ip->iops->iunlock(ip);

    f->type     = FD_INODE;
    f->ip       = ip;
    f->off      = 0;
    f->readable = !(omode & O_WRONLY);
    f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
    return fd;
}

/** @brief Open or create a file. */
int sys_open(void)
{
    char *path;
    int omode;

    if (argstr(0, &path) < 0 || argint(1, &omode) < 0)
        return -1;

    return open_file(path, omode);
}


/** @brief Create a new directory. */
int sys_mkdir(void)
{
    char *path;
    struct inode *ip;

    if (argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == nullptr) {
        return -1;
    }
    ip->iops->iunlockput(ip);
    return 0;
}

/**
 * @brief Create a new device node.
 *
 * @return 0 on success, -1 on failure.
 */
int sys_mknod(void)
{
    struct inode *ip;
    char *path;
    int major, minor;

    if ((argstr(0, &path)) < 0 ||
        argint(1, &major) < 0 ||
        argint(2, &minor) < 0 ||
        (ip = create(path, T_DEV, major, minor)) == nullptr) {
        return -1;
    }
    ip->iops->iunlockput(ip);
    return 0;
}

/** @brief Change the current working directory. */
int sys_chdir(void)
{
    char *path;
    struct inode *ip;
    struct proc *curproc = current_process();
    char resolved[MAX_FILE_PATH];

    if (argstr(0, &path) < 0 || (ip = namei(path)) == nullptr) {
        return -1;
    }
    int rc = resolve_cwd_path(curproc, path, resolved);
    if (rc < 0) {
        ip->iops->iput(ip);
        return -1;
    }
    ip->iops->ilock(ip);
    if (ip->type != T_DIR) {
        ip->iops->iunlockput(ip);
        return -1;
    }
    ip->iops->iunlock(ip);
    ip->iops->iput(curproc->cwd);
    curproc->cwd = ip;
    safestrcpy(ip->path, resolved, MAX_FILE_PATH);
    safestrcpy(curproc->cwd_path, resolved, MAX_FILE_PATH);
    if (rc > 0 && resolved[0] == '\0') {
        ip->path[0]        = '\0';
        curproc->cwd_path[0] = '\0';
    }
    return 0;
}

/**
 * @brief Replace the current process image with a new program.
 *
 * @return Does not return on success; -1 if loading the program fails.
 */
int sys_exec(void)
{
    char *path, *argv[MAXARG];
    u32 uargv, uarg;

    if (argstr(0, &path) < 0 || argint(1, (int *)&uargv) < 0) {
        return -1;
    }
    memset(argv, 0, sizeof(argv));
    for (u32 i = 0;; i++) {
        if (i >= NELEM(argv))
            return -1;
        if (fetchint(uargv + 4 * i, (int *)&uarg) < 0)
            return -1;
        if (uarg == 0) {
            argv[i] = nullptr;
            break;
        }
        if (fetchstr(uarg, &argv[i]) < 0)
            return -1;
    }
    return exec(path, argv);
}

/**
 * @brief Create a unidirectional pipe and return descriptor pair.
 *
 * @return 0 on success, -1 on failure.
 */
int sys_pipe(void)
{
    int *fd;
    struct file *rf, *wf;
    int fd1;

    if (argptr(0, (void *)&fd, 2 * sizeof(fd[0])) < 0)
        return -1;
    if (pipealloc(&rf, &wf) < 0)
        return -1;
    int fd0 = -1;
    if ((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0) {
        if (fd0 >= 0)
            current_process()->ofile[fd0] = nullptr;
        fileclose(rf);
        fileclose(wf);
        return -1;
    }
    fd[0] = fd0;
    fd[1] = fd1;
    return 0;
}
