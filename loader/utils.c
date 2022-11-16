#include <utils.h>
#include <stdarg.h>
#include <stdint.h>
#include <io.h>

char strcmp(char* str1, char* str2)
{
    while (*str1 != '\0') {
        if (*str1 != *str2) return 0;
        ++str1; ++str2;
    }

    if (str2 != 0) return 0;

    return 1;
}

char str_starts_with(char* str1, char* str2)
{
    while (*str2 != '\0') {
        if (*str1 == '\0' || *str1 != *str2) return 0;
        ++str1; ++str2;
    }
    return 1;
}

int strlen(char* c)
{
    int size = 0;
    while (*c) {
        ++c;
        ++size;
    }
    return size;
}

void term_write(const char* buff, int length)
{
    print_str_n(buff, length);
}

void uint_to_string(unsigned long int n, uint8_t base, char* str, int* length)
{
    char temp_str[65];
    *length = 0;
    if (n == 0) {
        temp_str[*length] = '0';
        ++*length;
    }
    while (n != 0) {
        unsigned int tmp = n%base;
        n /= base;
        if (tmp > 9)
            temp_str[*length] = tmp + 'A' - 10;
        else
            temp_str[*length] = tmp + '0';
        
        ++*length;
    }
    for (int i = 0; i < *length; ++i) {
        str[i] = temp_str[*length - i - 1];
    }
    str[*length] = 0;
}

void int_to_string(long int n, uint8_t base, char* str, int* length)
{
    char temp_str[65];
    *length = 0;
    char negative = 0;
    if (n < 0) {
        negative = 1;
        str[0] = '-';
        n *= -1;
    }
    if (n == 0) {
        temp_str[*length] = '0';
        ++*length;
    }
    while (n != 0) {
        int tmp = n%base;
        n /= base;
        if (tmp < 10)
            temp_str[*length] = tmp + '0';
        else
            temp_str[*length] = tmp + 'A' - 10;
        
        ++*length;
    }
    for (int i = 0; i < *length; ++i) {
        str[i + negative] = temp_str[*length - i - 1];
    }
    if (negative) ++*length;
    str[*length] = 0;
}

void lprintf(const char *str, ...)
{
    va_list arg;
    va_start(arg, str);

    char at = str[0];
    unsigned int i = 0;
    while (at != '\0') {
        uint64_t s = i;
        while (1) {
            if (at == '\0') {
                va_end(arg);
                if (i - s > 0) {
                    term_write(str + s, i - s);
                }
                return;
            }
            if (at == '%') break;
            //term_write(str+i, 1);
            at = str[++i];
        }
        if (i - s > 0) {
            term_write(str + s, i - s);
        }
       
        at = str[++i]; // char next to %
        char int_str_buffer[32];
        int len = 0;
        switch (at) {
            case 'i': { // signed integer
                int64_t casted_arg = va_arg(arg, int64_t);
                int_to_string(casted_arg, 10, int_str_buffer, &len);
                break;
            }
            case 'u': { // unsigned integer
                uint64_t casted_arg = va_arg(arg, uint64_t);
                uint_to_string(casted_arg, 10, int_str_buffer, &len);
                break;
            }
            case 'h': { // hexa number
                uint64_t casted_arg = va_arg(arg, uint64_t);
                uint_to_string(casted_arg, 16, int_str_buffer, &len);
                break;
            }
        }

        term_write(int_str_buffer, len);
        at = str[++i];
    }

    va_end(arg);
}