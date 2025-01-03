#include <kernel.h>
#include <memory.h>
#include <printf.h>
#include <root_inode.h>
#include <status.h>
#include <thread.h>
#include <tty.h>
#include <vfs.h>

enum tty_mode {
    RAW,
    CANONICAL,
};

struct tty {
    enum tty_mode mode;
};

struct tty_input_buffer {
    char buffer[256];
    size_t head;
    size_t tail;
};

struct spinlock tty_lock;
static struct tty_input_buffer tty_input_buffer;
static struct tty tty;

extern struct inode_operations memfs_directory_inode_ops;

void tty_input_buffer_put(char c)
{
    tty_input_buffer.buffer[tty_input_buffer.head] = c;

    tty_input_buffer.head = (tty_input_buffer.head + 1) % sizeof(tty_input_buffer.buffer);

    wakeup(&tty);
}

static char tty_input_buffer_get(void)
{
    char c                = tty_input_buffer.buffer[tty_input_buffer.tail];
    tty_input_buffer.tail = (tty_input_buffer.tail + 1) % sizeof(tty_input_buffer.buffer);
    return c;
}


static bool tty_input_buffer_is_empty(void)
{
    return tty_input_buffer.head == tty_input_buffer.tail;
}

static void wait_for_input()
{
    if (tty_input_buffer_is_empty()) {
        sleep(&tty, &tty_lock);
    }
}

static void *tty_open(const struct path_root *path_root, FILE_MODE mode, enum INODE_TYPE *type_out, uint32_t *size_out)
{
    return nullptr;
}


static int tty_read(const void *descriptor, size_t size, off_t offset, char *out)
{
    acquire(&tty_lock);
    size_t bytes_read = 0;
    while (bytes_read < size) {
        while (tty_input_buffer_is_empty()) {
            wait_for_input();
        }

        const char c = tty_input_buffer_get();

        ((char *)out)[bytes_read++] = c;
        if (tty.mode == CANONICAL && c == '\n') {
            break; // End of line in canonical mode
        }
    }

    if (holding(&tty_lock)) {
        release(&tty_lock);
    }
    return (int)bytes_read;
}

static int tty_write(const void *descriptor, const char *buffer, const size_t size)
{
    for (size_t i = 0; i < size; i++) {
        putchar(buffer[i]);
    }

    return (int)size;
}

static int tty_stat(void *descriptor, struct stat *stat)
{
    stat->st_size = tty_input_buffer.head;
    stat->st_mode = S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
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

static int tty_seek(void *descriptor, uint32_t offset, enum FILE_SEEK_MODE seek_mode)
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

#define DEV_DIRECTORY "dev"
#define DEV_NAME "tty"

int tty_init(void)
{
    initlock(&tty_lock, "tty");
    memset(&tty_input_buffer, 0, sizeof(tty_input_buffer));

    tty.mode = CANONICAL;

    struct inode *dev_dir = nullptr;
    int res               = root_inode_lookup(DEV_DIRECTORY, &dev_dir);
    if (!dev_dir || res != 0) {
        root_inode_mkdir(DEV_DIRECTORY, &memfs_directory_inode_ops);
        res = root_inode_lookup(DEV_DIRECTORY, &dev_dir);
        if (ISERR(res) || !dev_dir) {
            return res;
        }
        dev_dir->fs_type = FS_TYPE_RAMFS;
        vfs_add_mount_point("/" DEV_DIRECTORY, -1, dev_dir);
    }

    struct inode *tty_device = {};
    if (dev_dir->ops->lookup(dev_dir, DEV_NAME, &tty_device) == ALL_OK) {
        return ALL_OK;
    }

    return dev_dir->ops->create_device(dev_dir, DEV_NAME, &tty_device_fops);
}
