#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio_internal.h>
#include <pmos/system.h>
#include <kernel/errors.h>
#include <string.h>
#include <stdlib.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>

const char default_terminal_port_name[] = "/pmos/terminald";

#define PRINTF_FLAG_LEFT_J 0x01
#define PRINTF_FLAG_P_PLUS 0x02
#define PRINTF_FLAG_SPACE  0x04
#define PRINTF_FLAG_HASH   0x08
#define PRINTF_FLAG_ZERO   0x10

typedef ssize_t (*write_str_f) (void * arg, const char * str, size_t size);

// Defined in filesystem.c
ssize_t __write_internal(long int fd, const void * buf, size_t count, size_t offset, bool inc_offset);
ssize_t __read_internal(long int fd, void *buf, size_t count, bool should_seek, size_t offset);
off_t __lseek_internal(long int fd, off_t offset, int whence);
int __close_internal(long int fd);

int __check_fd_internal(long int fd, int posix_mode) {
    // TODO
    return 0;
}

static ssize_t write_file(void * arg, const char * str, size_t size)
{
    FILE * stream = (FILE*)arg;
    long int fd = (long int)stream;
    
    return __write_internal(fd, str, size, 0, true);
}

struct string_descriptor {
    char * buffer;
    size_t size;
    size_t pos;
    char flags;
};

static ssize_t write_string(void * arg, const char * str, size_t size)
{
    struct string_descriptor * stream = (struct string_descriptor*)arg;
    size_t max_size = stream->size - stream->pos;
    size_t size_s = size > max_size ? max_size : size;
    memcpy(&stream->buffer[stream->pos], str, size_s);
    stream->pos += size_s;
    return size_s;
}

static int int_to_string(long int n, uint8_t base, char* str)
{
    char temp_str[65];
    int length = 0;
    char negative = 0;
    if (n < 0) {
        negative = 1;
        str[0] = '-';
        n *= -1;
    }
    if (n == 0) {
        temp_str[length] = '0';
        ++length;
    }
    while (n != 0) {
        int tmp = n%base;
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
    if (negative) ++length;
    str[length] = 0;
    return length;
}

static int uint_to_string(unsigned long int n, uint8_t base, char* str, int hex_base)
{
    char temp_str[65];
    int length = 0;
    if (n == 0) {
        temp_str[length] = '0';
        ++length;
    }
    while (n != 0) {
        unsigned int tmp = n%base;
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

static inline int max_(int a, int b)
{
    return a > b ? a : b;
}

static ssize_t __va_printf_closure(write_str_f puts, void * puts_arg, va_list arg, const char * format)
{
    size_t chars_transmitted = 0;
    ssize_t buffdiff = 0;

    int i = 0;
    while (format && format[i] != '\0') {
        if (format[i] != '%') {
            ++buffdiff;
            ++i;
        } else {
            if (buffdiff > 0) {
                ssize_t p = puts(puts_arg, &format[i - buffdiff], buffdiff);
                if (p < 0) return p;
                chars_transmitted += buffdiff;
                buffdiff = 0;
            }

            ++i;
            char flags = 0;
            unsigned long int width = 0;
            int precision = 0;
            char rpt = 1;
            int is_long = 0;
            int hex_base = 'a';
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
                    char* endptr = NULL;
                    width = strtoul(&(format[i]), &endptr, 10);
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
                case 'd':{                    
                    long t;
                    if (is_long) t = va_arg(arg, long);
                    else t = va_arg(arg, int);

                    char* s_buff = malloc(max_(24, width+1));
                    int j = int_to_string(t, 10, s_buff);
                    j = puts(puts_arg, s_buff, j);
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
                    if (is_long) t = va_arg(arg, unsigned long);
                    else t = va_arg(arg, unsigned int);

                    char *s_buff = malloc(max_(24, width+1));
                    int j = uint_to_string(t, 10, s_buff, hex_base);
                    j = puts(puts_arg, s_buff, j);
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
                    if (is_long) t = va_arg(arg, unsigned long);
                    else t = va_arg(arg, unsigned int);

                    char *s_buff = malloc(max_(24, width+1));
                    int j = uint_to_string(t, 8, s_buff, hex_base);
                    j = puts(puts_arg, s_buff, j);
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
                    if (is_long) t = va_arg(arg, unsigned long);
                    else t = va_arg(arg, unsigned int);

                    char* s_buff = malloc(max_(24, width+1));
                    int j = uint_to_string(t, 16, s_buff, hex_base);
                    j = puts(puts_arg, s_buff, j);
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
                    int k = puts(puts_arg, (char*)&c, 1);
                    if (k < 0) return k;
                    chars_transmitted += 1;
                    
                    rpt = 0;
                    ++i;
                    break;
                }
                case 's': {
                    const char * str = va_arg(arg, const char*);
                    str = str ? str : "(null)";
                    size_t size = width;
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
                //case 'n':  // TODO!
                //    *va_arg(arg, int*) = j;
                //    ++i;
                //    rpt = 0;
                //    break;
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
    if (chars_transmitted >= 0 && buffdiff > 0) {
            int p = puts(puts_arg, &format[i - buffdiff], buffdiff);
            if (p < 0) return p;
            chars_transmitted += buffdiff;
    }
    return chars_transmitted;
}

int fputc ( int character, FILE * stream )
{
    return write_file(stream, (char*)&character, 1);
}

int putchar(int c)
{
    return write_file(stdout, (char*)&c, 1);
}

int putc(int c, FILE * stream)
{
    return fputc(c, stream);
}

int fseek(FILE *stream, long offset, int origin) {
    return __lseek_internal((long int)stream, offset, origin);
}

int fclose(FILE *stream) {
    return __close_internal((long int)stream);
}

long ftell(FILE *stream) {
    return __lseek_internal((long int)stream, 0, SEEK_CUR);
}

const char endline[] = "\n";

int fputs(const char* string, FILE* stream)
{
    size_t size = strlen(string);
    return write_file(stream, string, size) + write_file(stream, endline, sizeof(endline));
}

int puts(const char* str)
{
    return fputs(str, stdout);
}

int fprintf(FILE * stream, const char * format, ...)
{
    va_list arg;
    va_start(arg,format);

    return __va_printf_closure(write_file, stream, arg, format);

    va_end(arg);
}

int sprintf(char * str, const char * format, ...)
{
    va_list arg;
    va_start(arg,format);

    struct string_descriptor desc = {str, -1, 0, 0};

    int ret = __va_printf_closure(write_string, &desc, arg, format);

    va_end(arg);

    return ret;
}

int snprintf(char * str, size_t size, const char * format, ...)
{
    va_list arg;
    va_start(arg,format);

    struct string_descriptor desc = {str, size, 0, 0};

    int ret = __va_printf_closure(write_string, &desc, arg, format);

    va_end(arg);

    return ret;
}

size_t fread ( void * ptr, size_t size, size_t count, FILE * stream )
{
    return __read_internal((long int)stream, ptr, size*count, true, 0);
}

int getc(FILE *stream)
{
    char c = 0;
    int ret = fread(&c, 1, 1, stream);
    if (ret < 0) return ret;
    return c;
}

int fgetc(FILE *stream)
{
    char c = 0;
    int ret = fread(&c, 1, 1, stream);
    if (ret < 0) return ret;
    return c;
}

static int to_posix_mode(const char * mode) {
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


FILE * fdopen(int fd, const char * mode)
{
    int posix_mode = to_posix_mode(mode);

    int ret = __check_fd_internal(fd, posix_mode);
    if (ret < 0) {
        errno = -ret;
        return NULL;
    }

    return (FILE*)(unsigned long)fd;
}



int printf(const char* format, ...)
{
    va_list arg;
    va_start(arg,format);

    return __va_printf_closure(write_file, stdout, arg, format);

    va_end(arg);
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    struct string_descriptor desc = {str, size, 0, 0};

    int ret = __va_printf_closure(write_string, &desc, ap, format);

    return ret;
}

size_t fwrite ( const void * ptr, size_t size, size_t count, FILE * stream )
{
    return write_file(stream, ptr, size*count);
}

void perror(const char *message) {
    if (message != NULL && message[0] != '\0') {
        fprintf(stderr, "%s: ", message);
    }

    fprintf(stderr, "%s\n", strerror(errno));
}