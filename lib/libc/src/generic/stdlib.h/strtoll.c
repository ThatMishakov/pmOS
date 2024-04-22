#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>

long long int strtoll(const char *str, char **endptr, int base) {
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
    long long int result = 0;
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
        if (result > (LLONG_MAX - digit) / base) {
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
        return (sign == 1) ? LLONG_MAX : LLONG_MIN;
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