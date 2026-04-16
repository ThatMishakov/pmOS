#pragma once
#include <pmos/vector.h>

VECTOR_TYPEDEF(char *, args_vector);
struct Args {
    args_vector vector;
};

// Replaces the variable at name with this callback. Gets called twice, once with NULL out_buffer to get the size
typedef size_t (*arg_replace_callback)(const char *name, size_t name_length, char *out_buffer, void *ctx);

// Initializes args
void args_init(struct Args *args);

// Frees the memory buffers inside of the Args...
void args_release(struct Args *args);

// True on success, false on ENOMEM
bool push_arg(struct Args *args, const char *arg);
bool push_arg_size(struct Args *args, const char *arg, size_t size);

// Parses args and pushes them back. Calls the callback to replace the args
bool parse_push_args(struct Args *args, const char *input, size_t input_size, arg_replace_callback callback, void *callback_ctx);

const char **args_get_argv(const struct Args *args);

