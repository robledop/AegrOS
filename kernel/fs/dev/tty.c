#include <inode.h>
#include <kernel.h>
#include <root_inode.h>
#include <tty.h>
#include <vfs.h>
#include <vga_buffer.h>

extern struct inode_operations memfs_directory_inode_ops;

static void *tty_open(const struct path_root *path_root, FILE_MODE mode)
{
    return nullptr;
}

static int tty_read(const void *descriptor, size_t size, off_t offset, char *out)
{
    return 0;
}

static int tty_write(void *descriptor, const char *buffer, size_t size, off_t offset)
{
    for (size_t i = 0; i < size; i++) {
        putchar(buffer[i]);
    }

    return size;
}

static int tty_stat(void *descriptor, struct file_stat *stat)
{
    return 0;
}

static int tty_close(void *descriptor)
{
    return 0;
}

static int tty_ioctl(void *descriptor, int request, void *arg)
{
    return 0;
}

static int tty_seek(void *descriptor, uint32_t offset, FILE_SEEK_MODE seek_mode)
{
    return 0;
}

struct inode_operations tty_device_fops = {
    .open  = tty_open,
    .read  = tty_read,
    .write = tty_write,
    .stat  = tty_stat,
    .seek  = tty_seek,
    .close = tty_close,
    .ioctl = tty_ioctl,
};

int tty_init(void)
{
    struct inode *dev_dir = nullptr;
    int res               = root_inode_lookup("dev", &dev_dir);
    if (!dev_dir || res != 0) {
        root_inode_mkdir("dev", &memfs_directory_inode_ops);
        res = root_inode_lookup("dev", &dev_dir);
        if (ISERR(res) || !dev_dir) {
            return res;
        }
        dev_dir->fs_type = FS_TYPE_RAMFS;
    }

    vfs_add_mount_point("/dev", -1, dev_dir);

    return dev_dir->ops->create_device(dev_dir, "tty", &tty_device_fops);
}