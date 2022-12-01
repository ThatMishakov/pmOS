#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

unsigned long int strtoul(const char * nptr, char ** endptr ,int base)
{
    const char* s = nptr;
    int neg = 0;
    int c;

    do {
        c = *s++;
    } while (isspace(c));

    if (c == '-') {
        neg = 1;
        c = *s++;
    } else if (c == '+') {
        c = *s++;
    }

    if ((base == 0 || base == 16) && c == '0' && (*s == 'X' || *s == 'x')) {
        c = s[1];
        s += 2;
        base = 16;
    }

    if (base == 0) {
        base = c == '0' ? 8 : 10; 
    }

    long int cutoff = ULONG_MAX/base;
    long int cutlim = ULONG_MAX%base;

    int k = 0;
    long int result = 0;
    while (c) {
        if (isdigit(c)) {
            c -= '0';
        } else if (isalpha(c)) {
            if (isupper(c))
                c = c - 'A' + 10;
            else
                c = c - 'a' + 10;
        } else
            break;

        if (c >= base)
            break;

        if (k < 0 || result > cutoff || (result == cutoff && c > cutlim))
            k = -1;
        else {
            k = 1;
            result *= base;
            result += c;
        }
        c = *s++;
    }

    if (k == -1) {
        result = ULONG_MAX;
        // errno = ERANGE;
    } else if (neg) {
        result = -result;
    }

    if (endptr != 0) {
        *endptr = (char*)(k ? s - 1 : nptr);
    }

    return result;
}