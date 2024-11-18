#pragma once

#ifndef __KERNEL__
#error "This is a kernel header, and should not be included in userspace"
#endif

#include <inode.h>
#include <path_parser.h>
#include <stdint.h>

#define MAX_MOUNT_POINTS 10

typedef unsigned int FS_ITEM_TYPE;
#define FS_ITEM_TYPE_DIRECTORY 0
#define FS_ITEM_TYPE_FILE 1

typedef unsigned int FILE_SEEK_MODE;
enum { SEEK_SET, SEEK_CURRENT, SEEK_END };

typedef unsigned int FILE_MODE;
enum { FILE_MODE_READ, FILE_MODE_WRITE, FILE_MODE_APPEND, FILE_MODE_INVALID };

typedef unsigned int FILE_STAT_FLAGS;
enum { FILE_STAT_IS_READ_ONLY = 0b00000001 };

struct file_stat {
    FILE_STAT_FLAGS flags;
    uint32_t size;
};

struct disk;
typedef void *(*FS_OPEN_FUNCTION)(const struct path_root *path, FILE_MODE mode);
typedef int (*FS_READ_FUNCTION)(struct disk *disk, const void *private, uint32_t size, uint32_t nmemb, char *out);
typedef int (*FS_SEEK_FUNCTION)(void *private, uint32_t offset, FILE_SEEK_MODE seek_mode);
typedef int (*FS_CLOSE_FUNCTION)(void *private);
typedef int (*FS_STAT_FUNCTION)(struct disk *disk, void *private, struct file_stat *stat);
typedef int (*FS_IOCTL_FUNCTION)(int fd, uint64_t request, void *arg);
typedef int (*FS_RESOLVE_FUNCTION)(struct disk *disk);

struct directory_entry;
struct file_directory;

typedef int (*FS_GET_ROOT_DIRECTORY_FUNCTION)(const struct disk *disk, struct dir_entries *directory);
typedef int (*FS_GET_SUB_DIRECTORY_FUNCTION)(const struct disk *disk, const char *path, struct dir_entries *directory);

struct file_directory_entry {
    char *name;
    char *ext;
    uint8_t creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t access_date;
    uint16_t modification_time;
    uint16_t modification_date;
    uint32_t size;
    bool is_directory    : 1;
    bool is_read_only    : 1;
    bool is_hidden       : 1;
    bool is_system       : 1;
    bool is_volume_label : 1;
    bool is_long_name    : 1;
    bool is_archive      : 1;
    bool is_device       : 1;
};

struct file_directory {
    char *name;
    int entry_count;
    struct dir_entries *entries;
};

enum FS_TYPE {
    FS_TYPE_FAT16,
    FS_TYPE_RAMFS,
};

struct file_system {
    // file_system should return zero from resolve if the disk is using its file system
    FS_RESOLVE_FUNCTION resolve;
    FS_OPEN_FUNCTION open;
    FS_READ_FUNCTION read;
    FS_SEEK_FUNCTION seek;
    FS_STAT_FUNCTION stat;
    FS_CLOSE_FUNCTION close;
    FS_IOCTL_FUNCTION ioctl;

    FS_GET_ROOT_DIRECTORY_FUNCTION get_root_directory;
    FS_GET_SUB_DIRECTORY_FUNCTION get_subdirectory;

    char name[20];
    enum FS_TYPE type;
};

struct file_descriptor {
    int index;
    struct file_system *fs;
    // File descriptor private data
    void *fs_data;
    struct disk *disk;
};

struct mount_point {
    struct file_system *fs;
    char *prefix;
    uint32_t disk;
    struct inode *inode;
};

void fs_init(void);
void fs_add_mount_point(const char *prefix, uint32_t disk, struct file_system *fs, struct inode *inode);
int fopen(const char path[static 1], const char mode[static 1]);

__attribute__((nonnull)) int fread(void *ptr, uint32_t size, uint32_t nmemb, int fd);
int fseek(int fd, int offset, FILE_SEEK_MODE whence);
__attribute__((nonnull)) int fstat(int fd, struct file_stat *stat);
int fclose(int fd);
__attribute__((nonnull)) void fs_insert_file_system(struct file_system *filesystem);
__attribute__((nonnull)) struct file_system *fs_resolve(struct disk *disk);
int fs_open_dir(const char name[static 1], struct dir_entries **directory);
int fs_get_non_root_mount_point_count();
int fs_find_mount_point(const char *prefix);
struct mount_point *fs_get_mount_point(int index);
