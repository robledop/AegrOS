#include <assert.h>
#include <dirent.h>
#include <memory.h>
#include <printf.h>
#include <status.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <syscall.h>
#include <unistd.h>

extern int errno;

void clear_screen()
{
    printf("\033[2J\033[H");
}

int fstat(int fd, struct stat *stat)
{
    return syscall2(SYSCALL_STAT, fd, stat);
}

void putchar(char c)
{
    write(1, &c, 1);
}

int mkdir(const char *path)
{
    return syscall1(SYSCALL_MKDIR, path);
}

DIR *opendir(const char *path)
{
    const int fd = open(path, O_RDONLY | O_DIRECTORY);
    if (fd == -1) {
        return nullptr;
    }

    struct stat fs;
    const int res = fstat(fd, &fs);
    if (res < 0) {
        close(fd);
        return nullptr;
    }

    if (!S_ISDIR(fs.st_mode)) {
        close(fd);
        return nullptr;
    }

    DIR *dirp = malloc(sizeof(DIR));
    ASSERT(dirp);
    if (dirp == nullptr) {
        close(fd);
        return nullptr;
    }

    ASSERT(dirp != nullptr);

    dirp->fd     = fd;
    dirp->offset = 0;
    dirp->size   = fs.st_size;
    dirp->buffer = nullptr;

    return dirp;
}

#define BUFFER_SIZE 1024
struct dirent *readdir(DIR *dirp)
{
    ASSERT(dirp != nullptr);

    if ((uint32_t)dirp->offset >= dirp->size) {
        dirp->offset = 0;
        free(dirp->buffer);
        return nullptr;
    }

    static struct dirent entry;
    static uint16_t pos = 0;

    memset(&entry, 0, sizeof(struct dirent));
    if (dirp->buffer == nullptr) {
        dirp->buffer = calloc(BUFFER_SIZE, sizeof(char));
        if (dirp->buffer == nullptr) {
            return nullptr;
        }
    }

    ASSERT(dirp->buffer);

    if (pos >= dirp->nread) {
        // Refill the buffer
        dirp->nread = getdents(dirp->fd, dirp->buffer, BUFFER_SIZE);
        if (dirp->nread <= 0) {
            pos = 0;
            return nullptr;
        }
        pos = 0;
    }

    const struct dirent *d = (struct dirent *)((void *)dirp->buffer + pos);
    if (d->record_length == 0) {
        return nullptr;
    }

    entry.inode_number  = d->inode_number;
    entry.record_length = d->record_length;
    entry.offset        = d->offset;
    strncpy(entry.name, d->name, NAME_MAX);
    entry.name[NAME_MAX - 1] = '\0'; // Ensure null-termination

    pos += d->record_length; // Move to the next entry
    dirp->offset++;


    return &entry;
}

int closedir(DIR *dir)
{
    if (dir == nullptr) {
        return -1;
    }

    if (dir->buffer) {
        free(dir->buffer);
    }
    close(dir->fd);
    free(dir);

    return ALL_OK;
}

int getdents(unsigned int fd, struct dirent *buffer, unsigned int count)
{
    return syscall3(SYSCALL_GETDENTS, fd, buffer, count);
}

// Get the current directory for the current process
char *getcwd()
{
    return (char *)syscall0(SYSCALL_GETCWD);
}

// Set the current directory for the current process
int chdir(const char *path)
{
    return syscall1(SYSCALL_CHDIR, path);
}


int getkey()
{
    int c = 0;
    read(0, &c, 1); // Read from stdin
    return c;
}

int getkey_blocking()
{
    int key = 0;
    key     = getkey();
    while (key == 0) {
        key = getkey();
    }

    return key;
}

int parse_mode(const char *mode_str, int *flags)
{
    if (strncmp(mode_str, "r", 1) == 0) {
        *flags = O_RDONLY;
    } else if (strncmp(mode_str, "w", 1) == 0) {
        *flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (strncmp(mode_str, "a", 1) == 0) {
        *flags = O_WRONLY | O_CREAT | O_APPEND;
    } else if (strncmp(mode_str, "r+", 2) == 0) {
        *flags = O_RDWR;
    } else if (strncmp(mode_str, "w+", 2) == 0) {
        *flags = O_RDWR | O_CREAT | O_TRUNC;
    } else if (strncmp(mode_str, "a+", 2) == 0) {
        *flags = O_RDWR | O_CREAT | O_APPEND;
    } else {
        return -1; // Invalid mode
    }
    return 0;
}

FILE *fopen(const char *pathname, const char *mode)
{
    int flags;
    if (parse_mode(mode, &flags) != 0) {
        // Set errno to EINVAL for invalid argument
        errno = EINVARG;
        return nullptr;
    }

    int fd = open(pathname, flags);
    if (fd == -1) {
        return nullptr;
    }

    FILE *stream = malloc(sizeof(FILE));
    if (!stream) {
        errno = ENOMEM;
        close(fd);
        return nullptr;
    }

    stream->fd          = fd;
    stream->buffer_size = BUFFER_SIZE;
    stream->buffer      = malloc(stream->buffer_size);
    if (!stream->buffer) {
        errno = ENOMEM;
        close(fd);
        free(stream);
        return nullptr;
    }
    stream->pos             = 0;
    stream->bytes_available = 0;
    stream->eof             = 0;
    stream->error           = 0;
    stream->mode            = flags;

    return stream;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t total_bytes = size * nmemb;
    size_t bytes_read  = 0;
    char *dest         = ptr;

    while (bytes_read < total_bytes) {
        if (stream->bytes_available == 0) {
            // Buffer is empty; read from file
            ssize_t n = read(stream->fd, stream->buffer, stream->buffer_size);
            if (n == -1) {
                stream->error = 1;
                return bytes_read / size;
            } else if (n == 0) {
                stream->eof = 1;
                break; // EOF reached
            }
            stream->bytes_available = n;
            stream->pos             = 0;
        }

        size_t bytes_to_copy = stream->bytes_available;
        if (bytes_to_copy > total_bytes - bytes_read) {
            bytes_to_copy = total_bytes - bytes_read;
        }

        memcpy(dest + bytes_read, stream->buffer + stream->pos, bytes_to_copy);
        stream->pos += bytes_to_copy;
        stream->bytes_available -= bytes_to_copy;
        bytes_read += bytes_to_copy;
    }

    return bytes_read / size;
}

int fclose(FILE *stream)
{
    if (!stream) {
        errno = EINVARG;
        return EOF;
    }

    // Flush write buffer if needed
    if (stream->mode & O_WRONLY || stream->mode & O_RDWR) {
        fflush(stream);
    }

    close(stream->fd);

    free(stream->buffer);
    free(stream);

    return 0;
}

int fflush(FILE *stream)
{
    if (!stream) {
        errno = EINVARG;
        return EOF;
    }

    if (stream->mode & O_WRONLY || stream->mode & O_RDWR) {
        // Flush write buffer
        if (stream->pos > 0) {
            ssize_t n = write(stream->fd, stream->buffer, stream->pos);
            if (n == -1) {
                stream->error = 1;
                return EOF;
            }
            stream->pos = 0;
        }
    }

    return 0;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t total_bytes   = size * nmemb;
    size_t bytes_written = 0;
    const char *src      = ptr;

    // Error checking
    if (!stream || !ptr || size == 0 || nmemb == 0) {
        return 0;
    }

    // Ensure the stream is writable
    if (!(stream->mode & O_WRONLY || stream->mode & O_RDWR)) {
        stream->error = 1;
        errno         = EBADF;
        return 0;
    }

    while (bytes_written < total_bytes) {
        size_t space_in_buffer = stream->buffer_size - stream->pos;
        size_t bytes_to_copy   = total_bytes - bytes_written;

        if (bytes_to_copy > space_in_buffer) {
            bytes_to_copy = space_in_buffer;
        }

        // Copy data to the buffer
        memcpy(stream->buffer + stream->pos, src + bytes_written, bytes_to_copy);
        stream->pos += bytes_to_copy;
        bytes_written += bytes_to_copy;

        // If buffer is full, flush it
        if (stream->pos == stream->buffer_size) {
            if (fflush(stream) == EOF) {
                // Error occurred during flush
                return bytes_written / size;
            }
        }
    }

    // Return the number of complete elements written
    return bytes_written / size;
}

int fseek(FILE *stream, long offset, int whence)
{
    // Flush write buffer if needed
    if (stream->mode & O_WRONLY || stream->mode & O_RDWR) {
        fflush(stream);
    }

    // Adjust file position
    off_t result = lseek(stream->fd, offset, whence);
    if (result == (off_t)-1) {
        stream->error = 1;
        return -1;
    }

    // Reset buffer
    stream->pos             = 0;
    stream->bytes_available = 0;
    stream->eof             = 0;

    return ALL_OK;
}

int ftell(FILE *stream)
{
    return lseek(stream->fd, 0, SEEK_CURRENT);
}

int vfprintf(FILE *stream, const char *format, va_list args)
{
    char buffer[1024];
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    if (len < 0) {
        return len;
    }

    return fwrite(buffer, len, 1, stream);
}

int fprintf(FILE *stream, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vfprintf(stream, format, args);
    va_end(args);
    return ret;
}

void rewind(FILE *stream)
{
    fseek(stream, 0, SEEK_SET);
}

int getc()
{
    int c = 0;
    read(STDIN_FILENO, &c, 1); // Read from stdin
    return c;
}

int ungetc(int c, FILE *stream)
{
    if (stream->pos == 0) {
        return EOF;
    }

    stream->buffer[stream->pos - 1] = c;
    stream->pos--;

    return c;
}

int fputc(int c, FILE *stream)
{
    char ch = c;
    write(stream->fd, &ch, 1);
    return c;
}

int vfscanf(FILE *stream, const char *format, va_list args)
{
    int matched = 0;
    int ch;
    const char *fmt = format;

    while (*fmt) {
        if (isspace(*fmt)) {
            // Skip any whitespace in the format string
            while (isspace(*fmt))
                fmt++;
            // Skip any whitespace in the input stream
            while (isspace(ch = fgetc(stream)) && ch != EOF)
                ;
            if (ch != EOF) {
                ungetc(ch, stream);
            }
        } else if (*fmt == '%') {
            fmt++;
            if (*fmt == 'd') {
                // Read an integer
                int *int_ptr = va_arg(args, int *);
                int num      = 0;
                int sign     = 1;
                int started  = 0;

                // Skip whitespace in input
                while (isspace(ch = fgetc(stream)) && ch != EOF)
                    ;
                if (ch == '-') {
                    sign = -1;
                    ch   = fgetc(stream);
                }
                while (isdigit(ch) && ch != EOF) {
                    started = 1;
                    num     = num * 10 + (ch - '0');
                    ch      = fgetc(stream);
                }
                if (started) {
                    *int_ptr = sign * num;
                    matched++;
                    if (ch != EOF) {
                        ungetc(ch, stream);
                    }
                } else {
                    if (ch != EOF) {
                        ungetc(ch, stream);
                    }
                    break; // No match found
                }
            } else if (*fmt == 's') {
                // Read a string
                char *str_ptr = va_arg(args, char *);
                int idx       = 0;

                // Skip whitespace in input
                while (isspace(ch = fgetc(stream)) && ch != EOF)
                    ;

                if (ch == EOF) {
                    break;
                }

                do {
                    str_ptr[idx++] = ch;
                    ch             = fgetc(stream);
                } while (!isspace(ch) && ch != EOF);

                str_ptr[idx] = '\0';
                matched++;
                if (ch != EOF) {
                    ungetc(ch, stream);
                }
            } else {
                // Unsupported format specifier
                return matched;
            }
            fmt++;
        } else {
            // Literal character match
            ch = fgetc(stream);
            if (ch != *fmt) {
                if (ch != EOF) {
                    ungetc(ch, stream);
                }
                break;
            }
            fmt++;
        }
    }

    return matched;
}

int scanf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vfscanf(stdin, format, args);
    va_end(args);
    return ret;
}
int sscanf(const char *str, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    FILE stream = {
        .fd              = -1,
        .buffer_size     = 0,
        .buffer          = (char *)str,
        .pos             = 0,
        .bytes_available = strlen(str),
        .eof             = 0,
        .error           = 0,
        .mode            = O_RDONLY,
    };
    int ret = vfscanf(&stream, format, args);
    va_end(args);
    return ret;
}

int fscanf(FILE *stream, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vfscanf(stream, format, args);
    va_end(args);
    return ret;
}

int fgetc(FILE *stream)
{
    char c;
    if (fread(&c, 1, 1, stream) == 0) {
        return EOF;
    }
    return c;
}

int feof(FILE *stream)
{
    return stream->eof;
}

int ferror(FILE *stream)
{
    return stream->error;
}

void clearerr(FILE *stream)
{
    stream->eof   = 0;
    stream->error = 0;
}

bool isascii(int c)
{
    return c >= 0 && c <= 127;
}

bool isprint(int c)
{
    return c >= 32 && c <= 126;
}

ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
    if (!lineptr || !n) {
        errno = EINVARG;
        return -1;
    }

    size_t size = 0;
    char *line  = *lineptr;

    if (!line) {
        line = malloc(128);
        if (!line) {
            errno = ENOMEM;
            return -1;
        }
        size = 128;
    }

    size_t i = 0;
    int c;
    while ((c = fgetc(stream)) != EOF) {
        if (i >= size - 1) {
            size *= 2;
            char *new_line = realloc(line, size);
            if (!new_line) {
                free(line);
                errno = ENOMEM;
                return -1;
            }
            line = new_line;
        }

        line[i++] = c;
        if (c == '\n') {
            break;
        }
    }

    if (c == EOF && i == 0) {
        free(line);
        return -1;
    }

    line[i]  = '\0';
    *lineptr = line;
    *n       = size;

    return i;
}

void perror(const char *s)
{
    printf("%s %s\n", s, strerror(errno));
}