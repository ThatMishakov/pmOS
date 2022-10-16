#include "utils.hh"
#include <stdarg.h>
#include "types.hh"
#include "asm.hh"
#include "start.hh"

void int_to_string(long int n, uint8_t base, char* str, int& length)
{
    char temp_str[65];
    length = 0;
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
}

void uint_to_string(unsigned long int n, uint8_t base, char* str, int& length)
{
    char temp_str[65];
    length = 0;
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
}
 
void t_print(const char *str, ...)
{
    va_list arg;
    va_start(arg, str);

    char at = str[0];
    unsigned int i = 0;
    while (at != '\0') {
        while (true) {
            if (at == '\0') {
                va_end(arg);
                return;
            }
            if (at == '%') break;
            term_write(str+i, 1);
            at = str[++i];
        }
       
        at = str[++i]; // char next to %
        char int_str_buffer[32];
        int len = 0;
        switch (at) {
            case 'i': { // signed integer
                i64 casted_arg = va_arg(arg, i64);
                int_to_string(casted_arg, 10, int_str_buffer, len);
                break;
            }
            case 'u': { // unsigned integer
                u64 casted_arg = va_arg(arg, u64);
                uint_to_string(casted_arg, 10, int_str_buffer, len);
                break;
            }
            case 'h': { // hexa number
                u64 casted_arg = va_arg(arg, u64);
                uint_to_string(casted_arg, 16, int_str_buffer, len);
                break;
            }
        }

        term_write(int_str_buffer, len);
        at = str[++i];
    }

    va_end(arg);
}

void term_write(const char * str, uint64_t length)
{
    for (uint64_t i = 0; i < length; ++i)
        printc(str[i]);
}
