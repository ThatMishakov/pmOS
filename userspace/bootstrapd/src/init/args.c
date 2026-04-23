#include "args.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "../io.h"

void args_release(struct Args *args)
{
    if (!args)
        return;

    char *str;
    VECTOR_FOREACH(args->vector, str) {
        free(str);
    }

    VECTOR_FREE(args->vector);
}

const char **args_get_argv(const struct Args *args)
{
    static char *empty_vec[] = {NULL};

    if (args->vector.size == 0)
        return (const char **) empty_vec;

    return (const char **) args->vector.data;
}

static bool push_owned_arg(struct Args *args, char *owned)
{
    if (args->vector.size == 0) {
        int result = 0;
        VECTOR_RESERVE(args->vector, 4, result);
        if (result)
            goto error;

        VECTOR_PUSH_BACK(args->vector, NULL);
    }

    int result;
    VECTOR_PUSH_BACK_CHECKED(args->vector, NULL, result);
    if (result)
        goto error;

    args->vector.data[args->vector.size-2] = owned;

    return true;
error:
    free(owned);
    return false;
}

bool args_push_arg(struct Args *args, const char *arg)
{
    if (!arg)
        return false;

    char *str = strdup(arg);
    if (!str)
        return false;

    return push_owned_arg(args, str);
}

bool push_arg_size(struct Args *args, const char *arg, size_t size)
{
    if (!args || (!arg && size != 0))
        return false;

    char *str = strndup(arg, size);
    if (!str)
        return false;

    return push_owned_arg(args, str);
}

void args_init(struct Args *args)
{
    if (!args)
        return;

    args->vector = (args_vector)VECTOR_INIT;
}

static bool push_arg_replace(struct Args *args, const char *str, size_t length, arg_replace_callback callback, void *callback_ctx)
{
    char *buff = NULL;
    if (*str == '$') {
        size_t size = callback(str + 1, length - 1, NULL, callback_ctx);
        if (size == 0)
            return false;

        buff = malloc(size + 1);
        if (!buff)
            return false;

        callback(str + 1, length - 1, buff, callback_ctx);
        buff[size] = '\0';
    } else {
        buff = malloc(length + 1);
        if (!buff)
            return false;
    
        memcpy(buff, str, length);
        buff[length] = '\0';
    }

    return push_owned_arg(args, buff);
}

bool parse_push_args(struct Args *args, const char *input, size_t input_size, arg_replace_callback callback, void *callback_ctx)
{
    // size_t initial_size = args->vector.size;
    // size_t current_size = initial_size;

    const char *ptr = input;
    const char *first_non_space = ptr;
    const char *end = input + input_size;
    
    // Another TODO: handle \' and \"...
    // And also escape characters...
    while (ptr < end && *ptr) {
        if (isspace(*ptr)) {
            if (first_non_space < ptr) {
                if (!push_arg_replace(args, first_non_space, ptr - first_non_space, callback, callback_ctx))
                    goto fail;
            }
            first_non_space = ptr + 1;
        }
        ++ptr;
    }

    if (first_non_space < ptr) {
        if (!push_arg_replace(args, first_non_space, ptr - first_non_space, callback, callback_ctx))
            goto fail;   
    }

    return true;
fail:
    // TODO: Clean up? (pop unused back)
    return false;
}