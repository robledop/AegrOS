#pragma once
#include "param.h"
#include "types.h"
#include "fs.h"
#include "sleeplock.h"
#include "stat.h"

struct file
{
    enum { FD_NONE, FD_PIPE, FD_INODE } type;

    int ref; // reference count
    char readable;
    char writable;
    struct pipe *pipe;
    struct inode *ip;
    u32 off;
};


struct inode_operations
{
    int (*dirlink)(struct inode *, char *, u32);
    struct inode * (*dirlookup)(struct inode *, char *, u32 *);
    struct inode * (*ialloc)(u32, short);
    void (*iinit)(int dev);
    void (*ilock)(struct inode *);
    void (*iput)(struct inode *);
    void (*iunlock)(struct inode *);
    void (*iunlockput)(struct inode *);
    void (*iupdate)(struct inode *);
    int (*readi)(struct inode *, char *, u32, u32);
    void (*stati)(struct inode *, struct stat *);
    int (*writei)(struct inode *, char *, u32, u32);
};


// in-memory copy of an inode
struct inode
{
    u32 dev;               // Device number
    u32 inum;              // Inode number
    int ref;               // Reference count
    u16 i_uid;         /* Low 16 bits of Owner Uid */
    u32 i_size;        /* Size in bytes */
    u32 i_atime;       /* Access time */
    u32 i_ctime;       /* Creation time */
    u32 i_mtime;       /* Modification time */
    u32 i_dtime;       /* Deletion Time */
    u16 i_gid;         /* Low 16 bits of Group Id */
    u32 i_flags;       /* File flags */

    struct sleeplock lock; // protects everything below here
    int valid;             // inode has been read from disk?
    struct inode_operations *iops;

    char path[MAX_FILE_PATH];
    short type; // copy of disk inode
    u16 major;
    u16 minor;
    u16 nlink;
    u32 size;
    void *addrs;
};

// table mapping major device number to
// device functions
struct devsw
{
    int (*read)(struct inode *, char *, int);
    int (*write)(struct inode *, char *, int);
};

extern struct devsw devsw[];

#define CONSOLE 1