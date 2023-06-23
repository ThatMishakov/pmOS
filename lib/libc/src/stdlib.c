#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>

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

long int strtol(const char *str, char **endptr, int base) {
    // Skip leading whitespace
    while (isspace((unsigned char)*str)) {
        str++;
    }

    // Handle optional sign
    int sign = 1;
    if (*str == '-' || *str == '+') {
        sign = (*str++ == '-') ? -1 : 1;
    }

    // Check for valid base
    if (base < 2 || base > 36) {
        errno = EINVAL;
        if (endptr != NULL) {
            *endptr = (char *)str;
        }
        return 0;
    }

    // Handle base if not specified (0 or 16)
    if (base == 0) {
        if (*str == '0') {
            base = (*(str + 1) == 'x' || *(str + 1) == 'X') ? 16 : 8;
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (*str == '0' && (*(str + 1) == 'x' || *(str + 1) == 'X')) {
            str += 2;
        }
    }

    // Initialize variables
    long int result = 0;
    int overflow = 0;
    int digit;

    // Process digits
    while (isxdigit((unsigned char)*str)) {
        if (isdigit((unsigned char)*str)) {
            digit = *str - '0';
        } else {
            digit = tolower((unsigned char)*str) - 'a' + 10;
        }

        // Check for overflow
        if (result > (LONG_MAX - digit) / base) {
            overflow = 1;
            break;
        }

        result = result * base + digit;
        str++;
    }

    // Set endptr if provided
    if (endptr != NULL) {
        *endptr = (char *)str;
    }

    // Check for errors
    if (str == (const char *)endptr) {  // Cast endptr to const char*
        errno = EINVAL;
        return 0;
    } else if (overflow) {
        errno = ERANGE;
        return (sign == 1) ? LONG_MAX : LONG_MIN;
    }

    // Check if there are any trailing non-whitespace characters
    while (isspace((unsigned char)*str)) {
        str++;
    }
    if (*str != '\0' && endptr != NULL) {
        *endptr = (char *)str;
    }

    return sign * result;
}