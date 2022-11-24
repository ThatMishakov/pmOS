#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

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

static int copy_string(char* to, const char* from, char flags, int width)
{
    // TODO: Padding & length
    int i = 0;
    while (from[i]) {
        to[i] = from[i];
        ++i;
    }
    return i;
}

int sprintf ( char * str, const char * format, ... )
{

    va_list arg;
    va_start(arg, format);

    int i = 0;
    int j = 0;
    while (format && format[i]) {
        if (format[i] != '%') {
            str[j++] = format[i++];
        } else {
            ++i;
            char flags = 0;
            int width = 0;
            int precision = 0;
            char rpt = 1;
            while (rpt)
                switch (format[i]) {
                case '%':
                    str[j++] = '%';
                    ++j;
                    rpt = 0;
                    break;
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
                case 'd':
                    j += int_to_string(va_arg(arg, int), 10, &str[j]);
                    rpt = 0;
                    ++i;
                    break;
                case 'u':
                    j += uint_to_string(va_arg(arg, unsigned int), 10, &str[j]);
                    rpt = 0;
                    ++i;
                    break;
                case 'x':
                    j += uint_to_string(va_arg(arg, unsigned long), 16, &str[j]);
                    rpt = 0;
                    ++i;
                    break;
                case 'c':
                    str[j++] = va_arg(arg, char);
                    rpt = 0;
                    ++i;
                    break;
                case 's':
                    j += copy_string(&str[j], va_arg(arg, const char*), flags, width);
                    rpt = 0;
                    ++i;
                    break;
                case 'n':
                    *va_arg(arg, int*) = j;
                    ++i;
                    rpt = 0;
                    break;
                }
        }
    }
    va_end(arg);
    str[j] = '\0';
    return j;
}