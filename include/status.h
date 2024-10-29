#pragma once

#define ALL_OK 0
// Input/output error
#define EIO 1
// Invalid argument
#define EINVARG 2
// Out of memory
#define ENOMEM 3
// Invalid path
#define EBADPATH 4
// File system not supported
#define EFSNOTUS 5
// Read only file system
#define ERDONLY 6
// Operation not implemented
#define EUNIMP 7
// Instance already taken
#define EINSTKN 8
// Invalid format
#define EINFORMAT 9
// No such file or directory
#define ENOENT 10

// Resource temporarily unavailable
#define EAGAIN 11

static inline char *get_error_message(const int error)
{
    switch (error) {
    case -EIO:
        return "Input/output error";
    case -EINVARG:
        return "Invalid argument";
    case -ENOMEM:
        return "Out of memory";
    case -EBADPATH:
        return "Invalid path";
    case -EFSNOTUS:
        return "File system not supported";
    case -ERDONLY:
        return "Read only file system";
    case -EUNIMP:
        return "Operation not implemented";
    case -EINSTKN:
        return "Instance already taken";
    case -EINFORMAT:
        return "Invalid format";
    case -ENOENT:
        return "No such file or directory";
    case -EAGAIN:
        return "Resource temporarily unavailable";
    default:
        return "Unknown error";
    }
}
