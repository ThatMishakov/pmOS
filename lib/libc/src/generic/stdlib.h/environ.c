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

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char *environ_dummy = NULL;

// Sorted array of pointers to environment variables.
char **environ = &environ_dummy;

// Size of the environ array.
static size_t environ_size = 0;

// Number of environment variables.
static size_t environ_count = 0;

// Compare environment variable (up to '=' symbol) with a string
static int strenvcmp(const char *env, const char *right)
{
    while (*env != '=' && *right != '\0') {
        if (*env != *right) {
            return *env - *right;
        }
        env++;
        right++;
    }

    if (*env == '=') {
        return '\0' - *right;
    }

    return *env - '\0';
}

// Return lower bound of environ array for the given name.
static size_t env_lower_bound(const char *name)
{
    size_t lower = 0;
    size_t upper = environ_count;
    while (lower < upper) {
        size_t mid = lower + (upper - lower) / 2;
        if (strenvcmp(environ[mid], name) >= 0) {
            upper = mid;
        } else {
            lower = mid + 1;
        }
    }
    return lower;
}

int setenv(const char *name, const char *value, int overwrite)
{
    // Find the environment variable.
    size_t index = env_lower_bound(name);

    // Check that name is valid.
    if (name == NULL || strchr(name, '=') != NULL) {
        errno = EINVAL;
        return -1;
    }

    if (!overwrite && value == NULL) {
        return 0;
    }

    // If the environment variable exists, update it.
    if (index < environ_count && strenvcmp(environ[index], name) == 0) {
        char **env = &environ[index];

        if (overwrite && value == NULL) {
            // Remove the environment variable.
            free(env);

            // Move the environment variables after the removed one.
            memmove(&environ[index], &environ[index + 1],
                    (environ_count - index - 1) * sizeof(*environ));

            environ_count--;
            return 0;
        }

        size_t name_len = 0;
        if (overwrite && strcmp(*env + 1 + (name_len = strlen(name)), value) != 0) {
            size_t value_len = strlen(value);
            char *new_val    = malloc(name_len + 1 + value_len + 1);
            if (!new_val) {
                errno = ENOMEM;
                return -1;
            }
            free(*env);

            memcpy(new_val, name, name_len);
            new_val[name_len] = '=';
            memcpy(new_val + name_len + 1, value, value_len + 1);

            *env = new_val;
        }

        return 0;
    }

    // Insert the new environment variable.
    // Make sure there is enough space in the environ array.
    if (environ_count == environ_size) {
        // Double the size of the environ array.
        size_t new_environ_size = environ_size ? environ_size * 2 : 16;
        char **new_environ      = NULL;
        if (environ == &environ_dummy) {
            new_environ = malloc(new_environ_size * sizeof(*environ));
        } else {
            new_environ = realloc(environ, new_environ_size * sizeof(*environ));
        }
        if (!new_environ) {
            errno = ENOMEM;
            return -1;
        }
        environ      = new_environ;
        environ_size = new_environ_size;
    }

    const size_t name_len  = strlen(name);
    const size_t value_len = strlen(value);

    // Copy the new environment variable.
    char *new_val = malloc(name_len + 1 + value_len + 1);
    if (!new_val) {
        errno = ENOMEM;
        return -1;
    }
    memcpy(new_val, name, name_len);
    new_val[name_len] = '=';
    memcpy(new_val + name_len + 1, value, value_len + 1);

    // Move the environment variables after the insertion point.
    memmove(&environ[index + 1], &environ[index], (environ_count - index) * sizeof(*environ));

    // Insert the new environment variable.
    environ[index] = new_val;

    environ_count++;
    return 0;
}

char *getenv(const char *name)
{
    size_t index = env_lower_bound(name);
    if (index < environ_count && strenvcmp(environ[index], name) == 0) {
        return environ[index] + strlen(name) + 1;
    }
    return NULL;
}