#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio_internal.h>
#include <pmos/system.h>
#include <kernel/errors.h>
#include <string.h>
#include <stdlib.h>
#include <pmos/ipc.h>

FILE * stdin = NULL;
FILE * stdout = NULL;
FILE * stderr = NULL;

#define PRINTF_FLAG_LEFT_J 0x01
#define PRINTF_FLAG_P_PLUS 0x02
#define PRINTF_FLAG_SPACE  0x04
#define PRINTF_FLAG_HASH   0x08
#define PRINTF_FLAG_ZERO   0x10

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

static int _size_fputs (int size, const char * str, FILE * stream)
{
    if (stream == NULL || str == NULL) return EOF;

    int type = stream->type;
    int result = EOF;

    switch (type)
    {
    case _STDIO_FILE_TYPE_PORT: {
        static const int buffer_size = 252;

        struct {
            uint32_t type;
            char buffer[buffer_size];
        } desc;

        desc.type = IPC_Write_Plain_NUM;
        int s = 0;

        result = SUCCESS;

        for (int i = 0; i < size; i += buffer_size) {
            int size_s = size - i > buffer_size ? buffer_size : size - i;
            memcpy(desc.buffer, &str[i], size_s);

            int k = send_message_port(stream->port_ptr, size_s + sizeof(uint32_t), (const char*)(&desc));

            if (k == SUCCESS) {
                s += size_s;
            } else {
                if (s == 0)
                    result = EOF;

                break;
            }

            result = result == SUCCESS ? s : EOF;
        }

        break;
    }
    default:
        result = EOF;
        break;
    }
    return result;
}

static int write_string(FILE * stream, const char* from, char flags, size_t width)
{
    if (width == 0)
        return fputs(from, stream);

    size_t string_size;

    for (string_size = 0; string_size < width && from[string_size]; ++string_size) ;

    return _size_fputs(string_size, from, stream);
}

static int va_fprintf (FILE * stream, va_list arg, const char * format)
{
    int chars_transmitted = 0;
    int buffdiff = 0;

    int i = 0;
    while (format && format[i] != '\0') {
        if (format[i] != '%') {
            ++buffdiff;
            ++i;
        } else {
            if (buffdiff > 0) {
                int p = _size_fputs(buffdiff, &format[i - buffdiff], stream);
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
                    j = _size_fputs(j, s_buff, stream);
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
                    j = _size_fputs(j, s_buff, stream);
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
                    j = _size_fputs(j, s_buff, stream);
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
                    j = _size_fputs(j, s_buff, stream);
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
                    int k = _size_fputs(1, (char*)&c, stream);
                    if (k < 0) return k;
                    chars_transmitted += 1;
                    
                    rpt = 0;
                    ++i;
                    break;
                }
                case 's': {
                    int k = write_string(stream, va_arg(arg, const char*), flags, width);
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
            int p = _size_fputs(buffdiff, &format[i - buffdiff], stream);
            if (p < 0) return p;
            chars_transmitted += buffdiff;
    }
    return chars_transmitted;
}

int fputc ( int character, FILE * stream )
{
    return _size_fputs(1, (char*)&character, stream);
}

int putchar(int c)
{
    return _size_fputs(1, (char*)&c, stdout);
}

const char endline[] = "\n";

int fputs(const char* string, FILE* stream)
{
    size_t size = strlen(string);
    return _size_fputs(size, string, stream) + _size_fputs(sizeof(endline), endline, stream);
}

int puts(const char* str)
{
    return fputs(str, stdout);
}

int fprintf(FILE * stream, const char * format, ...)
{
    va_list arg;
    va_start(arg,format);

    return va_fprintf(stream, arg, format);

    va_end(arg);
}

int printf(const char* format, ...)
{
    va_list arg;
    va_start(arg,format);

    return va_fprintf(stdout, arg, format);

    va_end(arg);
}

size_t fwrite ( const void * ptr, size_t size, size_t count, FILE * stream )
{
    return _size_fputs(size*count, ptr, stream);
}