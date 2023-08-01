#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <errno.h>
#include <math.h>

double strtod(const char *str, char **endptr) {
    // Skip leading whitespace
    while (isspace((unsigned char)*str)) {
        str++;
    }

    // Handle optional sign
    bool negative = false;
    if (*str == '-') {
        negative = true;
        str++;
    } else if (*str == '+') {
        str++;
    }

    // Process digits before the decimal point
    double result = 0.0;
    while (isdigit((unsigned char)*str)) {
        result = result * 10.0 + (*str - '0');
        str++;
    }

    // Process digits after the decimal point
    if (*str == '.') {
        str++;
        double decimal_multiplier = 0.1;
        while (isdigit((unsigned char)*str)) {
            result += (*str - '0') * decimal_multiplier;
            decimal_multiplier *= 0.1;
            str++;
        }
    }

    // Handle exponent (if present)
    if (tolower((unsigned char)*str) == 'e') {
        str++;
        bool exp_negative = false;
        if (*str == '-') {
            exp_negative = true;
            str++;
        } else if (*str == '+') {
            str++;
        }

        int exponent = 0;
        while (isdigit((unsigned char)*str)) {
            exponent = exponent * 10 + (*str - '0');
            str++;
        }

        if (exp_negative) {
            exponent = -exponent;
        }

        // Adjust the result based on the exponent
        if (exponent != 0) {
            result *= pow(10.0, exponent);
        }
    }

    // Set endptr if provided
    if (endptr != NULL) {
        *endptr = (char *)str;
    }

    // Adjust the result for negative numbers
    if (negative) {
        result = -result;
    }

    // Check for errors
    if (errno == ERANGE || isinf(result)) {
        errno = ERANGE;
        return (negative) ? -HUGE_VAL : HUGE_VAL;
    } else if (errno == EINVAL) {
        // Invalid input
        return 0.0;
    }

    // Return the final result
    return result;
}