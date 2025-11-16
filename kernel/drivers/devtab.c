#include <devtab.h>

#include "defs.h"
#include "fcntl.h"
#include "printf.h"
#include "proc.h"
#include "string.h"

static struct inode devtab[NDEV];
static bool devtab_loaded = false;

int devtab_lookup_major(struct inode *ip)
{
    if (ip == nullptr) {
        return -1;
    }
    for (int i = 0; i < NDEV; i++) {
        if (devtab[i].inum == ip->inum) {
            return devtab[i].major;
        }
    }
    return ip->major;
}

void devtab_save()
{
    const int fd      = open_file("/etc/devtab", O_RDWR | O_CREATE);
    struct file *file = current_process()->ofile[fd];

    for (int i = 0; i < NDEV; i++) {
        if (devtab[i].inum == 0) {
            continue;
        }
        char buf[64];
        const int n = snprintf(buf,
                               sizeof(buf),
                               "%d\tchar\t%d\t%d\t# %s\n",
                               devtab[i].inum,
                               devtab[i].major,
                               devtab[i].minor,
                               devtab[i].path);

        file_write(file, buf, n);
    }
    file_close(file);
    current_process()->ofile[fd] = nullptr;
}

void devtab_add_entry(struct inode *ip, const char *path)
{
    // TODO: this can be done better
    bool found = false;
    for (int i = 0; i < NDEV; i++) {
        if (devtab[i].inum == ip->inum) {
            devtab[i].inum  = ip->inum;
            devtab[i].dev   = ip->dev;
            devtab[i].type  = ip->type;
            devtab[i].major = ip->major;
            devtab[i].minor = ip->minor;
            memcpy(devtab[i].path, path, MAX_FILE_PATH);

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
                memcpy(devtab[i].path, path, MAX_FILE_PATH);
                break;
            }
        }
    }

    devtab_save();
}

void devtab_load()
{
    if (devtab_loaded) {
        return;
    }
    devtab_loaded = true;

    const int fd      = open_file("/etc/devtab", O_RDWR);
    struct file *file = current_process()->ofile[fd];
    char buf[512];
    struct stat st;
    file_stat(file, &st);
    const int n = file_read(file, buf, st.size);
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
                    memcpy(devtab[i].path, ip->path, MAX_FILE_PATH);
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
                        memcpy(devtab[i].path, ip->path, MAX_FILE_PATH);
                        break;
                    }
                }
            }
        }
    }
    file_close(file);
}