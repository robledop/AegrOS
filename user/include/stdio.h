#pragma once

#include <stddef.h>
#include <stdarg.h>

#define PRINTF_SUPPRESS_PUTCHAR_DECL 1
#include <printf.h>
#undef PRINTF_SUPPRESS_PUTCHAR_DECL

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct FILE {
    int fd;
    unsigned int flags;
    int error;
    int eof;
} FILE;

extern FILE __stdin_file;
extern FILE __stdout_file;
extern FILE __stderr_file;

#define stdin (&__stdin_file)
#define stdout (&__stdout_file)
#define stderr (&__stderr_file)

#define EOF (-1)

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
void rewind(FILE *stream);
int fflush(FILE *stream);
int fgetc(FILE *stream);
int fputc(int c, FILE *stream);
char *fgets(char *s, int size, FILE *stream);
int fputs(const char *s, FILE *stream);
int puts(const char *s);
int putchar(int c);
int fileno(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);

int fprintf(FILE *stream, const char *format, ...) ATTR_PRINTF(2, 3);
int vfprintf(FILE *stream, const char *format, va_list args) ATTR_VPRINTF(2);

int remove(const char *path);
int rename(const char *oldpath, const char *newpath);
