#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio_internal.h>
#include <system.h>
#include <kernel/errors.h>
#include <string.h>
#include <stdlib.h>

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

static int uint_to_string(unsigned long int n, uint8_t base, char* str)
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
            temp_str[length] = tmp + 'A' - 10;
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

static int write_string(FILE * stream, const char* from, char flags, int width)
{
    return fputs(from, stream);
}

static inline int max_(int a, int b)
{
    return a > b ? a : b;
}

static int va_fprintf (FILE * stream, va_list arg, const char * format)
{
    int chars_transmitted = 0;
    static const int buffsize = 256;

    char* buff = malloc(buffsize + 1);
    int buffpos = 0;

    int i = 0;
    while (format && format[i]) {
        if (format[i] != '%') {
            buff[buffpos++] = format[i++];
            if (buffpos == buffsize) {
                buff[buffpos] = '\0';
                int p = fputs(buff, stream);
                free(buff);

                if (p < 0) return p;

                chars_transmitted += p;
                buff = malloc(buffsize + 1);
                buffpos = 0;
            }
        } else {
            ++i;
            char flags = 0;
            int width = 0;
            int precision = 0;
            char rpt = 1;
            int is_long = 0;
            while (rpt)
                switch (format[i]) {
                case '%': {
                    buff[buffpos++] = '%';
                    if (buffpos == buffsize) {
                        buff[buffpos] = '\0';
                        int p = fputs(buff, stream);
                        free(buff);

                        if (p < 0) return p;

                        chars_transmitted += p;
                        buff = malloc(buffsize + 1);
                        buffpos = 0;
                    }
                    
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
                case '9':
                    // TODO
                    break;
                case '*':
                    width = va_arg(arg, int);
                    ++i;
                    break;
                case '.':
                    // Precision
                    // TODO
                    break;

                // TODO: Length
                case 'i':
                case 'd':{
                    if (buffpos > 0) {
                        buff[buffpos] = '\0';
                        int p = fputs(buff, stream);
                        buffpos = 0;
                        if (p < 0) {
                            chars_transmitted = p;
                            goto end;
                        } else {
                            chars_transmitted += p;
                            free(buff);
                            buff = malloc(buffsize + 1);
                        }
                    }
                    
                    long t;
                    if (is_long) t = va_arg(arg, long);
                    else t = va_arg(arg, int);

                    char* s_buff = malloc(max_(24, width+1));
                    int j = int_to_string(t, 10, s_buff);
                    j = fputs(s_buff, stream);
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
                    if (buffpos > 0) {
                        buff[buffpos] = '\0';
                        int p = fputs(buff, stream);
                        buffpos = 0;
                        if (p < 0) {
                            chars_transmitted = p;
                            goto end;
                        } else {
                            chars_transmitted += p;
                            free(buff);
                            buff = malloc(buffsize + 1);
                        }
                    }
                    
                    unsigned long t;
                    if (is_long) t = va_arg(arg, unsigned long);
                    else t = va_arg(arg, unsigned int);

                    char *s_buff = malloc(max_(24, width+1));
                    int j = uint_to_string(t, 10, s_buff);
                    j = fputs(s_buff, stream);
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
                    if (buffpos > 0) {
                        buff[buffpos] = '\0';
                        int p = fputs(buff, stream);
                        buffpos = 0;
                        if (p < 0) {
                            chars_transmitted = p;
                            goto end;
                        } else {
                            chars_transmitted += p;
                            free(buff);
                            buff = malloc(buffsize + 1);
                        }
                    }
                    
                    unsigned long t;
                    if (is_long) t = va_arg(arg, unsigned long);
                    else t = va_arg(arg, unsigned int);

                    char *s_buff = malloc(max_(24, width+1));
                    int j = uint_to_string(t, 8, s_buff);
                    j = fputs(s_buff, stream);
                    if (j < 0) {
                        chars_transmitted = j;
                        goto end;
                    }
                    chars_transmitted += j;
                    rpt = 0;
                    ++i;
                    break;
                }
                case 'X': // Todo
                case 'x': {
                    if (buffpos > 0) {
                        buff[buffpos] = '\0';
                        int p = fputs(buff, stream);
                        buffpos = 0;
                        if (p < 0) {
                            chars_transmitted = p;
                            goto end;
                        } else {
                            chars_transmitted += p;
                            free(buff);
                            buff = malloc(buffsize + 1);
                        }
                    }

                    unsigned long t;
                    if (is_long) t = va_arg(arg, unsigned long);
                    else t = va_arg(arg, unsigned int);

                    char* s_buff = malloc(max_(24, width+1));
                    int j = uint_to_string(t, 16, s_buff);
                    j = fputs(s_buff, stream);
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
                    buff[buffpos++] = c;
                    if (buffpos == buffsize) {
                        buff[buffpos] = '\0';
                        int p = fputs(buff, stream);
                        free(buff);

                        if (p < 0) return p;

                        chars_transmitted += p;
                        buff = malloc(buffsize + 1);
                        buffpos = 0;
                    }
                    
                    rpt = 0;
                    ++i;
                    break;
                }
                case 's': {
                    if (buffpos > 0) {
                        buff[buffpos] = '\0';
                        int p = fputs(buff, stream);
                        buffpos = 0;
                        if (p < 0) {
                            chars_transmitted = p;
                            goto end;
                        } else {
                            chars_transmitted += p;
                            free(buff);
                            buff = malloc(buffsize + 1);
                        }
                    }
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
    if (chars_transmitted >= 0 && buffpos > 0) {
        buff[buffpos] = '\0';
        int p = fputs(buff, stream);
        if (p < 0) chars_transmitted = p;
        else chars_transmitted += p;
    }
    free(buff);
    return chars_transmitted;
}

int fputc ( int character, FILE * stream )
{
    char p[2] = " ";
    p[0] = character;
    return fputs(p, stream);
}

int fputs (const char * str, FILE * stream)
{
    if (stream == NULL || str == NULL) return EOF;

    int type = stream->type;
    int result = EOF;

    switch (type)
    {
    case _STDIO_FILE_TYPE_PORT: {
        size_t size = strlen(str);
        int k = send_message_port(stream->port_ptr, size, str);
        result = k == SUCCESS ? size : EOF;
        break;
    }
    default:
        result = EOF;
        break;
    }
    return result;
}

int puts(const char* str)
{
    return fputs(str, stdout);
}

int frpintf(FILE * stream, const char * format, ...)
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