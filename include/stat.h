#pragma once
#include "types.h"

#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Device

struct stat
{
    short type; // Type of file
    int dev; // File system's disk device
    u32 ino; // Inode number
    short nlink; // Number of links to file
    u32 size; // Size of file in bytes

    int ref;               // Reference count
    u16 i_uid;         /* Low 16 bits of Owner Uid */
    u32 i_atime;       /* Access time */
    u32 i_ctime;       /* Creation time */
    u32 i_mtime;       /* Modification time */
    u32 i_dtime;       /* Deletion Time */
    u16 i_gid;         /* Low 16 bits of Group Id */
    u32 i_flags;       /* File flags */
};