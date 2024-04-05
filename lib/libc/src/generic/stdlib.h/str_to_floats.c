/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <errno.h>
#include <math.h>

#include "../math/internal_pow.h"

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
            result *= __internal_pow(10.0, exponent);
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

float strtof(const char *str, char **endptr) {
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
    float result = 0.0f;
    while (isdigit((unsigned char)*str)) {
        result = result * 10.0f + (*str - '0');
        str++;
    }

    // Process digits after the decimal point
    if (*str == '.') {
        str++;
        float decimal_multiplier = 0.1f;
        while (isdigit((unsigned char)*str)) {
            result += (*str - '0') * decimal_multiplier;
            decimal_multiplier *= 0.1f;
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
            result *= __internal_powf(10.0f, exponent);
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
        return (negative) ? -HUGE_VALF : HUGE_VALF;
    } else if (errno == EINVAL) {
        // Invalid input
        return 0.0f;
    }

    // Return the final result
    return result;
}

long double strtold(const char *str, char **endptr) {
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
    long double result = 0.0L;
    while (isdigit((unsigned char)*str)) {
        result = result * 10.0L + (*str - '0');
        str++;
    }

    // Process digits after the decimal point
    if (*str == '.') {
        str++;
        long double decimal_multiplier = 0.1L;
        while (isdigit((unsigned char)*str)) {
            result += (*str - '0') * decimal_multiplier;
            decimal_multiplier *= 0.1L;
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
            result *= __internal_powl(10.0L, exponent);
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
        return (negative) ? -HUGE_VALL : HUGE_VALL;
    } else if (errno == EINVAL) {
        // Invalid input
        return 0.0L;
    }

    // Return the final result
    return result;
}