#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>

struct env {
    char *name;
    char *value;
};

// Sorted array of pointers to environment variables.
static struct env *environ = NULL;

// Size of the environ array.
static size_t environ_size = 0;

// Number of environment variables.
static size_t environ_count = 0;


// Return lower bound of environ array for the given name.
static size_t env_lower_bound(const char *name) {
    size_t lower = 0;
    size_t upper = environ_count;
    while (lower < upper) {
        size_t mid = lower + (upper - lower) / 2;
        if (strcmp(name, environ[mid].name) <= 0) {
            upper = mid;
        } else {
            lower = mid + 1;
        }
    }
    return lower;
}


int setenv(const char *name, const char *value, int overwrite) {
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
    if (index < environ_count && strcmp(environ[index].name, name) == 0) {
        struct env *env = &environ[index];

        if (overwrite && value == NULL) {
            // Remove the environment variable.
            free(env->name);
            free(env->value);

            // Move the environment variables after the removed one.
            memmove(&environ[index], &environ[index + 1], (environ_count - index - 1) * sizeof(struct env));

            environ_count--;
            return 0;
        }

        if (overwrite && strcmp(env->value, value) != 0) {
            free(env->value);
            env->value = strdup(value);
        }

        return 0;
    }

    // Insert the new environment variable.
    // Make sure there is enough space in the environ array.
    if (environ_count == environ_size) {
        // Double the size of the environ array.
        size_t new_environ_size = environ_size ? environ_size * 2 : 16;
        struct env *new_environ = realloc(environ, new_environ_size * sizeof(struct env));
        if (!new_environ) {
            errno = ENOMEM;
            return -1;
        }
        environ = new_environ;
        environ_size = new_environ_size;
    }

    // Move the environment variables after the insertion point.
    memmove(&environ[index + 1], &environ[index], (environ_count - index) * sizeof(struct env));

    char *name_copy = strdup(name);
    if (!name_copy) {
        errno = ENOMEM;
        return -1;
    }

    char *value_copy = strdup(value);
    if (!value_copy) {
        free(name_copy);
        errno = ENOMEM;
        return -1;
    }

    // Insert the new environment variable.
    environ[index].name = name_copy;
    environ[index].value = value_copy;

    environ_count++;
    return 0;
}