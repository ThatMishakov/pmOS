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

#include <errno.h>
#include <fcntl.h>
#include <kernel/errors.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdio_internal.h>
#include <stdlib.h>
#include <string.h>

const char default_terminal_port_name[] = "/pmos/terminald";

#define PRINTF_FLAG_LEFT_J 0x01
#define PRINTF_FLAG_P_PLUS 0x02
#define PRINTF_FLAG_SPACE  0x04
#define PRINTF_FLAG_HASH   0x08
#define PRINTF_FLAG_ZERO   0x10

typedef ssize_t (*write_str_f)(void *arg, const char *str, size_t size);

// Defined in filesystem.c
ssize_t __write_internal(long int fd, const void *buf, size_t count, size_t offset,
                         bool inc_offset);
ssize_t __read_internal(long int fd, void *buf, size_t count, bool should_seek, size_t offset);
off_t __lseek_internal(long int fd, off_t offset, int whence);
int __close_internal(long int fd);

int __check_fd_internal(long int fd, int posix_mode)
{
    // TODO
    return 0;
}

static int to_posix_mode(const char *mode)
{
    int posix_mode = 0;
    if (mode[0] == 'r') {
        posix_mode |= O_RDONLY;
    } else if (mode[0] == 'w') {
        posix_mode |= O_WRONLY;
        posix_mode |= O_CREAT;
        posix_mode |= O_TRUNC;
    } else if (mode[0] == 'a') {
        posix_mode |= O_WRONLY;
        posix_mode |= O_CREAT;
        posix_mode |= O_APPEND;
    } else {
        return -1;
    }

    if (mode[1] == '+') {
        posix_mode |= O_RDWR;
    }

    return posix_mode;
}

static FILE *open_files = NULL;
int file_lock           = 0;

void __close_files_on_exit()
{
    FILE *file = open_files;
    while (file) {
        FILE *next = file->next;
        fclose(file);
        file = next;
    }
}

FILE *stdin;
FILE *stdout;
FILE *stderr;

void __init_stdio()
{
    stdin  = fdopen(0, "r");
    stdout = fdopen(1, "w");
    stderr = fdopen(2, "w");

    if (!stdin || !stdout || !stderr) {
        exit(1);
    }

    setvbuf(stdout, NULL, _IOLBF, 0);
}

void __spin_pause();
#define LOCK(v)                                   \
    while (__sync_lock_test_and_set(v, 1) != 0) { \
        while (*v)                                \
            __spin_pause();                       \
    }
#define UNLOCK_FILE(v) __sync_lock_release(v);

FILE *fdopen(int fd, const char *mode)
{
    FILE *file = malloc(sizeof *file);
    if (!file)
        goto fail;

    memset(file, 0, sizeof(*file));
    file->fd = fd;

    file->buf_size  = BUFSIZ;
    file->buf_flags = _FILE_FLAG_FLUSHNEWLINE;
    // The buffer is gonna be malloc'ed on the first access

    int posix_mode = to_posix_mode(mode);
    file->mode     = posix_mode;

    int ret = __check_fd_internal(fd, posix_mode);
    if (ret < 0) {
        errno = -ret;
        goto fail;
    }

    LOCK(&file_lock);
    file->next = open_files;
    open_files = file;
    if (file->next)
        file->next->prev = file;
    UNLOCK_FILE(&file_lock);

    return file;
fail:
    free(file);
    return NULL;
}

FILE *fopen(const char *filename, const char *mode)
{
    int fd = -1;
    fd     = open(filename, to_posix_mode(mode), 0);
    if (fd < 0)
        goto fail;

    FILE *f = fdopen(fd, mode);
    if (!f)
        goto fail;

    return f;

fail:
    if (fd >= 0)
        close(fd);
    return NULL;
}

void flockfile(FILE *file) { LOCK(&file->lock); }

void funlockfile(FILE *file) { UNLOCK_FILE(&file->lock); }

int setvbuf(FILE *stream, char *buffer, int mode, size_t size)
{
    if (mode == _IONBF) {
        stream->buf      = NULL;
        stream->buf_size = 0;
    } else if (mode == _IOLBF || mode == _IOFBF) {
        stream->buf       = buffer;
        stream->buf_size  = size == 0 ? BUFSIZ : size;
        stream->buf_flags = mode == _IOLBF ? _FILE_FLAG_FLUSHNEWLINE : 0;
    } else {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

static ssize_t flush_buffer(FILE *stream)
{
    if (stream->buf_pos == 0)
        return 0;

    ssize_t ret = __write_internal(stream->fd, stream->buf, stream->buf_pos, 0, true);
    if (ret < 0)
        return ret;
    stream->buf_pos = 0;
    return 0;
}

static ssize_t write_file(void *arg, const char *str, size_t size)
{
    FILE *stream = (FILE *)arg;
    size_t t     = size;

    // Callee is expected to lock the file

    if (stream->buf_size != 0 && stream->buf == NULL) {
        stream->buf = malloc(stream->buf_size);
        if (!stream->buf) {
            errno = ENOMEM;
            return -1;
        }
        stream->buf_flags |= _FILE_FLAG_MYBUF;
    }

    if (stream->buf_size != 0 && (size > stream->buf_size - stream->buf_pos)) {
        ssize_t ret = flush_buffer(stream);
        if (ret < 0)
            return ret;
    }

    if (size > stream->buf_size) {
        ssize_t ret = __write_internal(stream->fd, str, size, 0, true);
        return ret;
    }

    if (size == 0)
        return 0;

    size_t i = size;
    for (; i > 0; i--) {
        if (str[i - 1] == '\n' && stream->buf_flags & _FILE_FLAG_FLUSHNEWLINE) {
            ssize_t ret = flush_buffer(stream);
            if (ret < 0)
                return ret;

            ret = __write_internal(stream->fd, str, i, 0, true);
            if (ret < 0)
                return ret;
            str += i;
            size -= i;
            break;
        }
    }

    memcpy(stream->buf + stream->buf_pos, str, size);
    stream->buf_pos += size;
    return t;
}

static int flush_file(FILE *stream)
{
    ssize_t ret = flush_buffer(stream);
    if (ret < 0)
        return ret;
    return 0;
}

struct string_descriptor {
    char *buffer;
    size_t size;
    size_t pos;
    char flags;
};

static ssize_t write_string(void *arg, const char *str, size_t size)
{
    struct string_descriptor *stream = (struct string_descriptor *)arg;
    size_t max_size                  = stream->size - stream->pos;
    size_t size_s                    = size > max_size ? max_size : size;
    memcpy(&stream->buffer[stream->pos], str, size_s);
    stream->pos += size_s;
    return size_s;
}

static int int_to_string(long int n, uint8_t base, char *str)
{
    char temp_str[65];
    int length    = 0;
    char negative = 0;
    if (n < 0) {
        negative = 1;
        str[0]   = '-';
        n *= -1;
    }
    if (n == 0) {
        temp_str[length] = '0';
        ++length;
    }
    while (n != 0) {
        int tmp = n % base;
        n /= base;
        if (tmp < 10)
            temp_str[length] = tmp + '0';
        else
            temp_str[length] = tmp + 'A' - 10;

        ++length;
    }
    for (int i = 0; i < length; ++i) {
        str[i + negative] = temp_str[length - i - 1];
    }
    if (negative)
        ++length;
    str[length] = 0;
    return length;
}

static int uint_to_string(unsigned long int n, uint8_t base, char *str, int hex_base)
{
    char temp_str[65];
    int length = 0;
    if (n == 0) {
        temp_str[length] = '0';
        ++length;
    }
    while (n != 0) {
        unsigned int tmp = n % base;
        n /= base;
        if (tmp > 9)
            temp_str[length] = tmp + hex_base - 10;
        else
            temp_str[length] = tmp + '0';

        ++length;
    }
    for (int i = 0; i < length; ++i) {
        str[i] = temp_str[length - i - 1];
    }
    str[length] = 0;
    return length;
}

static inline int max_(int a, int b) { return a > b ? a : b; }

static ssize_t __va_printf_closure(write_str_f puts, void *puts_arg, va_list arg,
                                   const char *format)
{
    size_t chars_transmitted = 0;
    ssize_t buffdiff         = 0;

    int i = 0;
    while (format && format[i] != '\0') {
        if (format[i] != '%') {
            ++buffdiff;
            ++i;
        } else {
            if (buffdiff > 0) {
                ssize_t p = puts(puts_arg, &format[i - buffdiff], buffdiff);
                if (p < 0)
                    return p;
                chars_transmitted += buffdiff;
                buffdiff = 0;
            }

            ++i;
            char flags              = 0;
            unsigned long int width = 0;
            int precision           = 0;
            char rpt                = 1;
            int is_long             = 0;
            int hex_base            = 'a';
            while (rpt)
                switch (format[i]) {
                case '%': {
                    buffdiff = 1;

                    rpt = 0;
                    ++i;
                    break;
                }
                case '-':
                    flags |= PRINTF_FLAG_LEFT_J;
                    ++i;
                    break;
                case '+':
                    flags |= PRINTF_FLAG_P_PLUS;
                    ++i;
                    break;
                case ' ':
                    flags |= PRINTF_FLAG_SPACE;
                    ++i;
                    break;
                case '#':
                    flags |= PRINTF_FLAG_HASH;
                    ++i;
                    break;
                case '0':
                    flags |= PRINTF_FLAG_ZERO;
                    ++i;
                    break;

                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9': { // TODO: Broken!
                    char *endptr = NULL;
                    width        = strtoul(&(format[i]), &endptr, 10);
                    i += &(format[i]) - endptr;
                    break;
                }
                case '*':
                    width = va_arg(arg, int);
                    ++i;
                    break;
                case '.':
                    // Precision
                    // TODO
                    break;
                case 'i':
                case 'd': {
                    long t;
                    if (is_long)
                        t = va_arg(arg, long);
                    else
                        t = va_arg(arg, int);

                    char *s_buff = malloc(max_(24, width + 1));
                    int j        = int_to_string(t, 10, s_buff);
                    j            = puts(puts_arg, s_buff, j);
                    if (j < 0) {
                        chars_transmitted = j;
                        goto end;
                    }
                    chars_transmitted += j;
                    rpt = 0;
                    ++i;
                    break;
                }
                case 'u': {
                    unsigned long t;
                    if (is_long)
                        t = va_arg(arg, unsigned long);
                    else
                        t = va_arg(arg, unsigned int);

                    char *s_buff = malloc(max_(24, width + 1));
                    int j        = uint_to_string(t, 10, s_buff, hex_base);
                    j            = puts(puts_arg, s_buff, j);
                    if (j < 0) {
                        chars_transmitted = j;
                        goto end;
                    }
                    chars_transmitted += j;
                    rpt = 0;
                    ++i;
                    break;
                }
                case 'o': {
                    unsigned long t;
                    if (is_long)
                        t = va_arg(arg, unsigned long);
                    else
                        t = va_arg(arg, unsigned int);

                    char *s_buff = malloc(max_(24, width + 1));
                    int j        = uint_to_string(t, 8, s_buff, hex_base);
                    j            = puts(puts_arg, s_buff, j);
                    if (j < 0) {
                        chars_transmitted = j;
                        goto end;
                    }
                    chars_transmitted += j;
                    rpt = 0;
                    ++i;
                    break;
                }
                case 'X':
                    hex_base = 'A';
                    __attribute__((fallthrough));
                case 'x': {
                    unsigned long t;
                    if (is_long)
                        t = va_arg(arg, unsigned long);
                    else
                        t = va_arg(arg, unsigned int);

                    char *s_buff = malloc(max_(24, width + 1));
                    int j        = uint_to_string(t, 16, s_buff, hex_base);
                    j            = puts(puts_arg, s_buff, j);
                    if (j < 0) {
                        chars_transmitted = j;
                        goto end;
                    }
                    chars_transmitted += j;
                    rpt = 0;
                    ++i;
                    break;
                }
                case 'c': {
                    int c = va_arg(arg, int);
                    int k = puts(puts_arg, (char *)&c, 1);
                    if (k < 0)
                        return k;
                    chars_transmitted += 1;

                    rpt = 0;
                    ++i;
                    break;
                }
                case 's': {
                    const char *str = va_arg(arg, const char *);
                    str             = str ? str : "(null)";
                    size_t size     = width;
                    if (size == 0)
                        size = strlen(str);
                    ssize_t k = puts(puts_arg, str, size);
                    if (k < 1) {
                        chars_transmitted = k;
                        goto end;
                    }
                    chars_transmitted += k;
                    rpt = 0;
                    ++i;
                    break;
                }
                // case 'n':  // TODO!
                //     *va_arg(arg, int*) = j;
                //     ++i;
                //     rpt = 0;
                //     break;
                case 'h':
                    is_long = 0;
                    ++i;
                    break;
                case 'l':
                    is_long = 1;
                    ++i;
                    break;
                default:
                    rpt = 0;
                    ++i;
                    break;
                }
        }
    }

end:
    if (/* chars_transmitted >= 0 && */ buffdiff > 0) {
        int p = puts(puts_arg, &format[i - buffdiff], buffdiff);
        if (p < 0)
            return p;
        chars_transmitted += buffdiff;
    }
    return chars_transmitted;
}

int fputc(int character, FILE *stream)
{
    LOCK(&stream->lock);
    int t = write_file(stream, (char *)&character, 1);
    UNLOCK_FILE(&stream->lock);
    return t;
}

int putchar(int c) { return fputc(c, stdout); }

int putc(int c, FILE *stream) { return fputc(c, stream); }

int fseek(FILE *stream, long offset, int origin)
{
    return __lseek_internal((long int)stream, offset, origin);
}

int fclose(FILE *stream)
{
    LOCK(&file_lock);
    if (stream->prev)
        stream->prev->next = stream->next;
    else
        open_files = stream->next;
    if (stream->next)
        stream->next->prev = stream->prev;
    UNLOCK_FILE(&file_lock);
    fflush(stream);
    int t = __close_internal(stream->fd);

    if (stream->buf_flags & _FILE_FLAG_MYBUF)
        free(stream->buf);
    free(stream);

    return t;
}

long ftell(FILE *stream)
{
    errno = ENOSYS;
    return -1;

    return __lseek_internal((long int)stream, 0, SEEK_CUR);
}

const char endline[] = "\n";

int fputs(const char *string, FILE *stream)
{
    LOCK(&stream->lock);
    size_t size = strlen(string);
    int t       = write_file(stream, string, size) + write_file(stream, endline, strlen(endline));
    UNLOCK_FILE(&stream->lock);
    return t;
}

int puts(const char *str) { return fputs(str, stdout); }

int fprintf(FILE *stream, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);

    LOCK(&stream->lock);
    int t = __va_printf_closure(write_file, stream, arg, format);
    UNLOCK_FILE(&stream->lock);

    va_end(arg);

    return t;
}

int sprintf(char *str, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);

    struct string_descriptor desc = {str, -1, 0, 0};

    int ret = __va_printf_closure(write_string, &desc, arg, format);

    va_end(arg);

    return ret;
}

int snprintf(char *str, size_t size, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);

    struct string_descriptor desc = {str, size, 0, 0};

    int ret = __va_printf_closure(write_string, &desc, arg, format);

    va_end(arg);

    return ret;
}

size_t fread(void *ptr, size_t size, size_t count, FILE *stream)
{
    // Not implemented
    errno = ENOSYS;
    return -1;

    return __read_internal((long int)stream, ptr, size * count, true, 0);
}

int getc_unlocked(FILE *stream)
{
    // Not implemented
    errno = ENOSYS;
    return -1;

    char c  = 0;
    int ret = fread(&c, 1, 1, stream);
    if (ret < 0)
        return ret;
    return c;
}

int getchar_unlocked(void) { return getc_unlocked(stdin); }

int getc(FILE *stream)
{
    char c  = 0;
    int ret = fread(&c, 1, 1, stream);
    if (ret < 0)
        return ret;
    return c;
}

int fgetc(FILE *stream)
{
    char c  = 0;
    int ret = fread(&c, 1, 1, stream);
    if (ret < 0)
        return ret;
    return c;
}

int printf(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);

    LOCK(&stdout->lock);
    int t = __va_printf_closure(write_file, stdout, arg, format);
    UNLOCK_FILE(&stdout->lock);

    va_end(arg);
    return t;
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    struct string_descriptor desc = {str, size, 0, 0};

    int ret = __va_printf_closure(write_string, &desc, ap, format);

    return ret;
}

size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream)
{
    LOCK(&stream->lock);
    size_t i = write_file(stream, ptr, size * count);
    UNLOCK_FILE(&stream->lock);
    return i;
}

void perror(const char *message)
{
    if (message != NULL && message[0] != '\0') {
        fprintf(stderr, "%s: ", message);
    }

    fprintf(stderr, "%s\n", strerror(errno));
}

int fflush(FILE *stream)
{
    LOCK(&stream->lock);
    int i = flush_file(stream);
    UNLOCK_FILE(&stream->lock);
    return i;
}