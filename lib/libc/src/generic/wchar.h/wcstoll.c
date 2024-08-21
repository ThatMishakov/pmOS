#include <wchar.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

long long wcstoll(const wchar_t *str, wchar_t **endptr, int base)
{
    // Skip leading whitespace
    while (iswspace(*str)) {
        str++;
    }

    // Handle optional sign
    int sign = 1;
    if (*str == L'-' || *str == L'+') {
        sign = (*str++ == L'-') ? -1 : 1;
    }

    // Check for valid base
    if (base < 2 || base > 36) {
        errno = EINVAL;
        if (endptr != NULL) {
            *endptr = (wchar_t *)str;
        }
        return 0;
    }

    // Handle base if not specified (0 or 16)
    if (base == 0) {
        if (*str == L'0') {
            base = (*(str + 1) == L'x' || *(str + 1) == L'X') ? 16 : 8;
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (*str == L'0' && (*(str + 1) == L'x' || *(str + 1) == L'X')) {
            str += 2;
        }
    }

    // Initialize variables
    long long int result = 0;
    int overflow         = 0;
    int digit;

    // Process digits
    while (isxdigit((unsigned char)*str)) {
        if (isdigit((unsigned char)*str)) {
            digit = *str - L'0';
        } else {
            digit = towlower((unsigned char)*str) - L'a' + 10;
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
        *endptr = (wchar_t *)str;
    }

    // Check for errors
    if (str == (const wchar_t *)endptr) { // Cast endptr to const wchar_t*
        errno = EINVAL;
        return 0;
    } else if (overflow) {
        errno = ERANGE;
        return (sign == 1) ? LLONG_MAX : LLONG_MIN;
    }

    // Check if there are any trailing non-whitespace characters
    while (isspace(*str)) {
        str++;
    }
    if (*str != '\0' && endptr != NULL) {
        *endptr = (wchar_t *)str;
    }

    return sign * result;
}