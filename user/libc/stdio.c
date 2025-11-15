#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "status.h"
#include "stdio.h"
#include "user.h"

#define FILE_FLAG_READ   (1U << 0U)
#define FILE_FLAG_WRITE  (1U << 1U)
#define FILE_FLAG_APPEND (1U << 2U)
#define FILE_FLAG_STATIC (1U << 3U)

FILE __stdin_file  = {.fd = STDIN_FILENO, .flags = FILE_FLAG_READ | FILE_FLAG_STATIC, .error = 0, .eof = 0};
FILE __stdout_file = {.fd = STDOUT_FILENO, .flags = FILE_FLAG_WRITE | FILE_FLAG_STATIC, .error = 0, .eof = 0};
FILE __stderr_file = {.fd = STDERR_FILENO, .flags = FILE_FLAG_WRITE | FILE_FLAG_STATIC, .error = 0, .eof = 0};

static int parse_mode(const char *mode, unsigned int *flags_out, int *oflag_out)
{
    if (mode == nullptr || mode[0] == '\0') {
        errno = -EINVARG;
        return -1;
    }

    unsigned int flags = 0;
    int oflag          = 0;
    int c0             = mode[0];
    int plus           = 0;

    switch (c0) {
    case 'r':
        flags |= FILE_FLAG_READ;
        oflag = O_RDONLY;
        break;
    case 'w':
        flags |= FILE_FLAG_WRITE;
        oflag = O_WRONLY | O_CREATE;
        break;
    case 'a':
        flags |= FILE_FLAG_WRITE | FILE_FLAG_APPEND;
        oflag = O_WRONLY | O_CREATE;
        break;
    default:
        errno = -EINVARG;
        return -1;
    }

    for (const char *m = mode + 1; *m != '\0'; ++m) {
        if (*m == '+') {
            plus = 1;
        }
    }

    if (plus) {
        flags |= FILE_FLAG_READ | FILE_FLAG_WRITE;
        oflag &= ~(O_RDONLY | O_WRONLY);
        oflag |= O_RDWR;
        if (c0 == 'w' || c0 == 'a') {
            oflag |= O_CREATE;
        }
    }

    *flags_out = flags;
    *oflag_out = oflag;
    return 0;
}

static FILE *alloc_stream(int fd, unsigned int flags)
{
    FILE *stream = (FILE *)malloc(sizeof(FILE));
    if (stream == nullptr) {
        close(fd);
        errno = -ENOMEM;
        return nullptr;
    }
    stream->fd    = fd;
    stream->flags = flags;
    stream->error = 0;
    stream->eof   = 0;
    return stream;
}

static void update_error(FILE *stream, int err)
{
    stream->error = 1;
    errno         = err;
}

FILE *fopen(const char *path, const char *mode)
{
    unsigned int flags;
    int oflag;

    if (parse_mode(mode, &flags, &oflag) < 0) {
        return nullptr;
    }

    int fd = open(path, oflag);
    if (fd < 0) {
        return nullptr;
    }

    FILE *stream = alloc_stream(fd, flags);
    if (stream == nullptr) {
        return nullptr;
    }

    if ((flags & FILE_FLAG_APPEND) != 0U) {
        if (lseek(fd, 0, SEEK_END) < 0) {
            update_error(stream, -EBADF);
        }
    }

    return stream;
}

int fclose(FILE *stream)
{
    if (stream == nullptr) {
        errno = -EINVARG;
        return -1;
    }
    int result = close(stream->fd);
    if ((stream->flags & FILE_FLAG_STATIC) == 0U) {
        free(stream);
    } else {
        stream->fd    = -1;
        stream->error = 0;
        stream->eof   = 0;
    }
    return result;
}

static size_t limit_count(size_t size, size_t nmemb)
{
    if (size == 0 || nmemb == 0) {
        return 0;
    }
    if (nmemb > SIZE_MAX / size) {
        return SIZE_MAX;
    }
    return size * nmemb;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (stream == nullptr || ptr == nullptr) {
        errno = -EINVARG;
        return 0;
    }
    if ((stream->flags & FILE_FLAG_READ) == 0U) {
        update_error(stream, -EBADF);
        return 0;
    }

    size_t total = limit_count(size, nmemb);
    if (total == 0) {
        return 0;
    }

    int n = read(stream->fd, ptr, (int)total);
    if (n < 0) {
        update_error(stream, n);
        return 0;
    }
    if (n == 0) {
        stream->eof = 1;
        return 0;
    }

    if ((size_t)n < total) {
        stream->eof = 1;
    }
    return size == 0 ? 0 : (size_t)n / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (stream == nullptr || ptr == nullptr) {
        errno = -EINVARG;
        return 0;
    }
    if ((stream->flags & FILE_FLAG_WRITE) == 0U) {
        update_error(stream, -EBADF);
        return 0;
    }

    size_t total = limit_count(size, nmemb);
    if (total == 0) {
        return 0;
    }

    if ((stream->flags & FILE_FLAG_APPEND) != 0U) {
        if (lseek(stream->fd, 0, SEEK_END) < 0) {
            update_error(stream, -EBADF);
            return 0;
        }
    }

    int n = write(stream->fd, ptr, (int)total);
    if (n < 0) {
        update_error(stream, n);
        return 0;
    }
    return size == 0 ? 0 : (size_t)n / size;
}

int fseek(FILE *stream, long offset, int whence)
{
    if (stream == nullptr) {
        errno = -EINVARG;
        return -1;
    }
    int result = lseek(stream->fd, (int)offset, whence);
    if (result < 0) {
        update_error(stream, result);
        return -1;
    }
    stream->eof = 0;
    return 0;
}

long ftell(FILE *stream)
{
    if (stream == nullptr) {
        errno = -EINVARG;
        return -1;
    }
    int pos = lseek(stream->fd, 0, SEEK_CUR);
    if (pos < 0) {
        update_error(stream, pos);
        return -1;
    }
    return pos;
}

void rewind(FILE *stream)
{
    if (stream == nullptr) {
        return;
    }
    if (fseek(stream, 0, SEEK_SET) == 0) {
        clearerr(stream);
    }
}

int fflush(FILE *stream)
{
    (void)stream;
    return 0;
}

int fgetc(FILE *stream)
{
    unsigned char ch;
    if (stream == nullptr) {
        errno = -EINVARG;
        return EOF;
    }
    if ((stream->flags & FILE_FLAG_READ) == 0U) {
        update_error(stream, -EBADF);
        return EOF;
    }
    int n = read(stream->fd, &ch, 1);
    if (n == 0) {
        stream->eof = 1;
        return EOF;
    }
    if (n < 0) {
        update_error(stream, n);
        return EOF;
    }
    return ch;
}

int fputc(int c, FILE *stream)
{
    unsigned char ch = (unsigned char)c;
    if (stream == nullptr) {
        errno = -EINVARG;
        return EOF;
    }
    if (fwrite(&ch, 1, 1, stream) != 1) {
        return EOF;
    }
    return ch;
}

char *fgets(char *s, int size, FILE *stream)
{
    if (s == nullptr || size <= 0 || stream == nullptr) {
        errno = -EINVARG;
        return nullptr;
    }

    int i = 0;
    while (i < size - 1) {
        int ch = fgetc(stream);
        if (ch == EOF) {
            break;
        }
        s[i++] = (char)ch;
        if (ch == '\n') {
            break;
        }
    }
    if (i == 0) {
        return nullptr;
    }
    s[i] = '\0';
    return s;
}

int fputs(const char *s, FILE *stream)
{
    size_t len = strlen(s);
    return fwrite(s, 1, len, stream) == len ? (int)len : EOF;
}

int puts(const char *s)
{
    if (fputs(s, stdout) == EOF) {
        return EOF;
    }
    if (fputc('\n', stdout) == EOF) {
        return EOF;
    }
    return 0;
}

int fileno(FILE *stream)
{
    if (stream == nullptr) {
        errno = -EINVARG;
        return -1;
    }
    return stream->fd;
}

int feof(FILE *stream)
{
    return stream != nullptr ? stream->eof : 0;
}

int ferror(FILE *stream)
{
    return stream != nullptr ? stream->error : 1;
}

void clearerr(FILE *stream)
{
    if (stream == nullptr) {
        return;
    }
    stream->error = 0;
    stream->eof   = 0;
}

static void file_putchar(char c, void *arg)
{
    FILE *stream = (FILE *)arg;
    fputc(c, stream);
}

int vfprintf(FILE *stream, const char *format, va_list args)
{
    if (stream == nullptr) {
        errno = -EINVARG;
        return -1;
    }
    return vfctprintf(file_putchar, stream, format, args);
}

int fprintf(FILE *stream, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vfprintf(stream, format, args);
    va_end(args);
    return ret;
}

int remove(const char *path)
{
    return unlink(path);
}

int rename(const char *oldpath, const char *newpath)
{
    if (link(oldpath, newpath) < 0) {
        return -1;
    }
    if (unlink(oldpath) < 0) {
        unlink(newpath);
        return -1;
    }
    return 0;
}
