/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _STDIO_H
#define _STDIO_H 1

#define __DECLARE_SIZE_T
#define __DECLARE_SSIZE_T
#define __DECLARE_OFF_T
#include "__posix_types.h"

// This shouldn't be included here, but this header is broken without it.
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef uint64_t pmos_port_t;

// FILE * is actually POSIX file descriptor (int)
typedef struct _FILE {
    // Linked list of open files
    struct _FILE *next, *prev;
    int lock;

    int fd;
    int flags;
#define _FILE_FLAG_EOF          1
#define _FILE_FLAG_ERROR        2

    int mode;

    char *buf;
    size_t buf_size;
    size_t buf_pos;
#define _FILE_FLAG_FLUSHNEWLINE 1
#define _FILE_FLAG_MYBUF        2
    int buf_flags;

#define __UNGET_SIZE 8
    char unget[__UNGET_SIZE];
    int unget_pos;
#undef __UNGET_SIZE
} FILE;

typedef size_t fpos_t;

typedef int errno_t;
typedef size_t rsize_t;

#ifdef __STDC_HOSTED__

int remove(const char *filename);
int rename(const char *oldname, const char *newname);
FILE *tmpfile(void);
char *tmpnam(char *str);

int fclose(FILE *stream);
int fflush(FILE *stream);

/**
 * @brief Open a file.
 *
 * The `fopen` function opens the file specified by `filename` and associates it with the stream
 * `mode`.
 *
 * @param filename The name of the file to be opened.
 * @param mode     The access mode for the file (e.g., "r" for read, "w" for write, "a" for append).
 *
 * @return If successful, a pointer to the `FILE` structure associated with the opened file is
 * returned. If an error occurs, NULL is returned, and `errno` is set to indicate the error.
 */
FILE *fopen(const char *filename, const char *mode);

FILE *freopen(const char *filename, const char *mode, FILE *stream);
void setbuf(FILE *stream, char *buffer);
int setvbuf(FILE *stream, char *buffer, int mode, size_t size);

int fprintf(FILE *stream, const char *format, ...);
int fscanf(FILE *stream, const char *format, ...);
int printf(const char *format, ...);
int scanf(const char *format, ...);
int snprintf(char *s, size_t n, const char *format, ...);
int sprintf(char *str, const char *format, ...);
int sscanf(const char *s, const char *format, ...);
int vfprintf(FILE *stream, const char *format, va_list arg);
int vfscanf(FILE *stream, const char *format, va_list arg);
int vprintf(const char *format, va_list arg);
int vscanf(const char *format, va_list arg);
int asprintf(char **strp, const char *fmt, ...);
int vasprintf(char **strp, const char *fmt, va_list ap);

/**
 * @brief Format a string with a maximum length, using a va_list.
 *
 * This function writes formatted data to a character string `str` with a
 * specified maximum `size`, using the format and arguments provided in
 * `format` and `ap` (a `va_list`).
 *
 * @param str   A pointer to the destination character string.
 * @param size  The maximum number of characters to write, including the null
 *              terminator. If the formatted output exceeds this size, it will
 *              be truncated.
 * @param format A format string that specifies how the following arguments are
 *               formatted.
 * @param ap    A `va_list` containing the arguments to be formatted and
 *              inserted into the resulting string.
 *
 * @return The number of characters written (excluding the null terminator) if
 * successful. If the output is truncated, it returns the number of characters
 * that would have been written if the size were sufficiently large (not
 * counting the null terminator). If an error occurs, it returns a negative
 * value.
 */
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

int vsprintf(char *s, const char *format, va_list arg);
int vsscanf(const char *s, const char *format, va_list arg);

/**
 * @brief Get a character from a file stream.
 *
 * The `fgetc` function reads a character from the file stream specified by the file pointer
 * `stream`.
 *
 * @param stream The file stream to read a character from.
 *
 * @return On success, the character read is returned as an `int`. If an error occurs or the
 * end-of-file is reached, `EOF` is returned.
 */
int fgetc(FILE *stream);

char *fgets(char *str, int num, FILE *stream);

/**
 * @brief Write a character to the given file stream.
 *
 * The fputc() function writes the character c (converted to an unsigned char)
 * to the given file stream stream. If the write is successful, the character
 * is written, and its value is returned. If an error occurs, it returns
 * EOF (indicating failure).
 *
 * @param c The character to be written.
 * @param stream The file stream to write to.
 *
 * @return If successful, returns the character written; otherwise, returns EOF.
 */
int fputc(int c, FILE *stream);

int fputs(const char *str, FILE *stream);

/**
 * @brief Get a character from a file stream.
 *
 * The `getc` function reads a character from the file stream specified by the file pointer
 * `stream`.
 *
 * @param stream The file stream to read a character from.
 *
 * @return On success, the character read is returned as an `int`. If an error occurs or the
 * end-of-file is reached, `EOF` is returned.
 */
int getc(FILE *stream);

int getchar(void);
char *gets(char *str);

/**
 * @brief Write a character to the given file stream.
 *
 * The putc() function writes the character c (converted to an unsigned char)
 * to the given file stream stream. If the write is successful, the character
 * is written, and its value is returned. If an error occurs, it returns
 * EOF (indicating failure).
 *
 * @param c The character to be written.
 * @param stream The file stream to write to.
 *
 * @return If successful, returns the character written; otherwise, returns EOF.
 */
int putc(int c, FILE *stream);

int putchar(int character);
int puts(const char *str);
int ungetc(int character, FILE *stream);

/**
 * @brief Read data from a file stream.
 *
 * The fread() function reads up to `size` elements of data, each `count` bytes in size,
 * from the file stream specified by the file pointer `stream` and stores them in the
 * buffer pointed to by `ptr`.
 *
 * @param ptr   Pointer to the buffer where data will be stored.
 * @param size  The size in bytes of each element to be read.
 * @param count The number of elements to read.
 * @param stream The file stream to read data from.
 *
 * @return The total number of elements successfully read is returned.
 *         If an error occurs, or the end-of-file is reached, the return value may be less than
 * `count`. Use `feof` or `ferror` to determine the reason for the return value.
 */
size_t fread(void *ptr, size_t size, size_t count, FILE *stream);

size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream);

int fgetpos(FILE *stream, fpos_t *pos);

/**
 * @brief Set the file position indicator for a stream.
 *
 * The `fseek` function sets the file position indicator for the given stream `stream`
 * to the specified offset `offset` from the reference point `whence`. The reference point
 * can be one of the following constants:
 *
 * - `SEEK_SET`: The offset is relative to the beginning of the file.
 * - `SEEK_CUR`: The offset is relative to the current file position.
 * - `SEEK_END`: The offset is relative to the end of the file.
 *
 * After a successful call to `fseek`, the next operation on the stream, such as a read or write,
 * will occur at the specified position. If `fseek` encounters an error, it will return a nonzero
 * value, and the file position indicator may be unchanged.
 *
 * @param stream Pointer to the FILE object representing the stream.
 * @param offset The offset value to set the file position indicator to.
 * @param whence The reference point from which to calculate the offset.
 * @return 0 on success, or a nonzero value if an error occurred.
 */
int fseek(FILE *stream, long int offset, int whence);

int fsetpos(FILE *stream, const fpos_t *pos);

/**
 * @brief Get the current file position indicator for a stream.
 *
 * The `ftell` function obtains the current file position indicator for the given stream.
 *
 * @param stream A pointer to the FILE stream for which the position is to be determined.
 *
 * @return The current file position indicator, if successful.
 *         If an error occurs, `ftell` returns -1 and sets `errno` to indicate the error.
 */
long ftell(FILE *stream);

void rewind(FILE *stream);

void clearerr(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
void perror(const char *str);

/**
 * @brief Associate a file descriptor with a file stream.
 *
 * The `fdopen` function associates the existing file descriptor `fd` with the file stream `mode`.
 *
 * @param fd   The existing file descriptor to be associated with the stream.
 * @param mode The access mode for the stream (e.g., "r" for read, "w" for write, "a" for append).
 *
 * @return If successful, a pointer to the `FILE` structure associated with the stream is returned.
 *         If an error occurs, NULL is returned, and `errno` is set to indicate the error.
 */
FILE *fdopen(int fd, const char *mode);

int fileno(FILE *);

char *ctermid(char *);
FILE *fdopen(int, const char *);
int fileno(FILE *);
void flockfile(FILE *);
int fseeko(FILE *, off_t, int);
off_t ftello(FILE *);
int pclose(FILE *);
FILE *popen(const char *, const char *);
char *tempnam(const char *, const char *);

    #define BUFSIZ       4096
    #define EOF          -1
    #define FILENAME_MAX 1023
    #define FOPEN_MAX    65535
    #define L_tmpnam     20
    #define TMP_MAX      238328

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

    #define SEEK_SET 0
    #define SEEK_CUR 1
    #define SEEK_END 2

    #define _IONBF 0
    #define _IOLBF 1
    #define _IOFBF 2

    #define FP_NAN       0
    #define FP_INFINITE  1
    #define FP_NORMAL    2
    #define FP_SUBNORMAL 3
    #define FP_ZERO      4

#endif

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif // _STDIO_H