#include "auxvec.h"
#include <stdlib.h>
#include <stdint.h>
#include <pmos/vector.h>
#include <errno.h>
#include <string.h>

struct AuxVecBuilder *auxvec_new()
{
    return calloc(1, sizeof(struct AuxVecBuilder));
}

void auxvec_free(struct AuxVecBuilder *builder)
{
    if (!builder)
        return;

    char *str;
    VECTOR_FOREACH(builder->argv, str)
        free(str);
    VECTOR_FREE(builder->argv);

    VECTOR_FOREACH(builder->envp, str)
        free(str);
    VECTOR_FREE(builder->envp);

    VECTOR_FREE(builder->entries);

    free(builder);
}

static size_t auxvec_ptr_size(const struct AuxVecBuilder *builder)
{
    return builder->ptr_is_64bit ? sizeof(uint64_t) : sizeof(uint32_t);
}

size_t auxvec_size_serialized(const struct AuxVecBuilder *builder)
{
    if (!builder)
        return 0;

    size_t ptr_size = auxvec_ptr_size(builder);

    size_t size = 0;
    size_t ptr_align_mask = ptr_size - 1;

    char *str;
    VECTOR_FOREACH(builder->argv, str) {
        size += strlen(str) + 1;
    }

    VECTOR_FOREACH(builder->envp, str) {
        size += strlen(str) + 1;
    }

    // Align this stuff
    size = (size + ptr_align_mask) & ~ptr_align_mask;

    // Args...
    const struct AuxVecEntry *e;
    VECTOR_FOREACH_PTR(builder->entries, e)
        if (e->data_type == DATA_TYPE_EXTERNAL) {
            size_t size_s = e->external_data.size;
            size_s = (size_s + ptr_align_mask) & ~ptr_align_mask;
            size += size_s;
        }

    size += VECTOR_SIZE(builder->argv) * ptr_size;
    size += VECTOR_SIZE(builder->envp) * ptr_size;
    size += VECTOR_SIZE(builder->entries) * 2 * ptr_size;
    // argc + argv terminator + envp terminator + auxvec NULL entry
    size += 5 * ptr_size;

    // Align to 16
    size_t align_mask = 16 - 1;
    return (size + align_mask) & ~align_mask;
}

int auxvec_serialize(const struct AuxVecBuilder *builder, uint64_t stack_top, uint8_t **data_out, size_t *size_out)
{
    if (!builder || !data_out || !size_out)
        return -EINVAL;

    size_t pointer_size = auxvec_ptr_size(builder);
    size_t pointer_mask = pointer_size - 1;

    int result = 0;
    size_t pos = 0;

    size_t args_size = VECTOR_SIZE(builder->argv);
    size_t args_offsets[args_size];
    size_t envp_size = VECTOR_SIZE(builder->envp);
    size_t envp_offsets[envp_size];
    size_t vals_size = VECTOR_SIZE(builder->entries);
    // Not all entries are likely to have external data, but nontheless allocate large enough array
    size_t entries_offsets[vals_size];

    size_t total_size = auxvec_size_serialized(builder);
    uint8_t *data_ptr = malloc(total_size);
    if (!data_ptr) {
        result = -ENOMEM;
        goto error;
    }

    for (size_t i = 0; i < args_size; ++i) {
        args_offsets[i] = pos;
        const char *str = builder->argv.data[i];
        size_t s = strlen(str) + 1;
        memcpy(data_ptr + pos, str, s);
        pos += s;
    }

    for (size_t i = 0; i < envp_size; ++i) {
        envp_offsets[i] = pos;
        const char *str = builder->envp.data[i];
        size_t s = strlen(str) + 1;
        memcpy(data_ptr + pos, str, s);
        pos += s;
    }

    size_t padding = (-pos) & pointer_mask;
    memset(data_ptr + pos, 0, padding);
    pos += padding;

    for (size_t i = 0; i < vals_size; ++i) {
        struct AuxVecEntry *e = builder->entries.data + i;

        if (e->data_type != DATA_TYPE_EXTERNAL)
            continue;

        entries_offsets[i] = pos;
        size_t s = e->external_data.size;
        memcpy(data_ptr + pos, e->external_data.data, s);
        size_t padding = (-s) & pointer_mask;
        memset(data_ptr + pos + s, 0, padding);
        pos += s + padding;
    }

    uint64_t start_of_args = stack_top - pos;
    memmove(data_ptr + total_size - pos, data_ptr, pos);
    size_t info_size = pos;

    pos = 0;
    if (builder->ptr_is_64bit) {
        // Append array size
        uint64_t array_count = args_size;
        memcpy(data_ptr + pos, &array_count, sizeof(array_count));
        pos += sizeof(array_count);

        for (size_t i = 0; i < args_size; ++i) {
            uint64_t ptr = args_offsets[i] + start_of_args;
            memcpy(data_ptr + pos, &ptr, sizeof(ptr));
            pos += sizeof(ptr);
        }

        memset(data_ptr + pos, 0, sizeof(uint64_t));
        pos += sizeof(uint64_t);

        for (size_t i = 0; i < envp_size; ++i) {
            uint64_t ptr = envp_offsets[i] + start_of_args;
            memcpy(data_ptr + pos, &ptr, sizeof(ptr));
            pos += sizeof(ptr);
        }

        memset(data_ptr + pos, 0, sizeof(uint64_t));
        pos += sizeof(uint64_t);

        for (size_t i = 0; i < vals_size; ++i) {
            struct AuxVecEntry *e = builder->entries.data + i;

            uint64_t type = e->entry_type;
            memcpy(data_ptr + pos, &type, sizeof(type));
            pos += sizeof(type);

            switch (e->data_type) {
            case DATA_TYPE_LONG: {
                int64_t l = e->long_data;
                memcpy(data_ptr + pos, &l, sizeof(l));
                pos += sizeof(l);
            }
                break;
            case DATA_TYPE_PTR: {
                uint64_t u = e->long_data;
                memcpy(data_ptr + pos, &u, sizeof(u));
                pos += sizeof(u);
            }
                break;
            case DATA_TYPE_EXTERNAL: {
                uint64_t ptr = entries_offsets[i] + start_of_args;
                memcpy(data_ptr + pos, &ptr, sizeof(ptr));
                pos += sizeof(ptr);
            }
                break;
            }
        }

        // NULL entry
        // The spec says it can be 1 eightword, with undefined a_val, but push 0 just in case
        memset(data_ptr + pos, 0, 2 * sizeof(uint64_t));
        pos += 2 * sizeof(uint64_t);
    } else {
        // Append array size
        uint32_t array_count = args_size;
        memcpy(data_ptr + pos, &array_count, sizeof(array_count));
        pos += sizeof(array_count);

        for (size_t i = 0; i < args_size; ++i) {
            uint32_t ptr = args_offsets[i] + start_of_args;
            memcpy(data_ptr + pos, &ptr, sizeof(ptr));
            pos += sizeof(ptr);
        }

        memset(data_ptr + pos, 0, sizeof(uint32_t));
        pos += sizeof(uint32_t);

        for (size_t i = 0; i < envp_size; ++i) {
            uint32_t ptr = envp_offsets[i] + start_of_args;
            memcpy(data_ptr + pos, &ptr, sizeof(ptr));
            pos += sizeof(ptr);
        }

        memset(data_ptr + pos, 0, sizeof(uint32_t));
        pos += sizeof(uint32_t);

        for (size_t i = 0; i < vals_size; ++i) {
            struct AuxVecEntry *e = builder->entries.data + i;

            uint32_t type = e->entry_type;
            memcpy(data_ptr + pos, &type, sizeof(type));
            pos += sizeof(type);

            switch (e->data_type) {
            case DATA_TYPE_LONG: {
                int32_t l = e->long_data;
                memcpy(data_ptr + pos, &l, sizeof(l));
                pos += sizeof(l);
            }
                break;
            case DATA_TYPE_PTR: {
                uint32_t u = e->long_data;
                memcpy(data_ptr + pos, &u, sizeof(u));
                pos += sizeof(u);
            }
                break;
            case DATA_TYPE_EXTERNAL: {
                uint32_t ptr = entries_offsets[i] + start_of_args;
                memcpy(data_ptr + pos, &ptr, sizeof(ptr));
                pos += sizeof(ptr);
            }
                break;
            }
        }

        // NULL entry
        // (same as above)
        memset(data_ptr + pos, 0, 2 * sizeof(uint32_t));
        pos += 2 * sizeof(uint32_t);
    }

    // Fill/align
    memset(data_ptr + pos, 0, total_size - pos - info_size);

    *data_out = data_ptr;
    data_ptr = 0;
    *size_out = total_size;

error:
    free(data_ptr);
    return result;
}

int auxvec_push_argv(struct AuxVecBuilder *builder, const char *argv[])
{
    if (!builder)
        return -EINVAL;

    if (!argv)
        return 0;

    auto ptr = argv;
    while (*ptr) {
        char *str = strdup(*ptr);
        if (!str)
            return -ENOMEM;

        int result;
        VECTOR_PUSH_BACK_CHECKED(builder->argv, str, result);
        if (result) {
            free(str);
            return -ENOMEM;
        }
        ++ptr;
    }

    return 0;
}

int auxvec_push_envp(struct AuxVecBuilder *builder, const char *envp[])
{
    if (!builder)
        return -EINVAL;

    if (!envp)
        return 0;

    auto ptr = envp;
    while (*ptr) {
        char *str = strdup(*ptr);
        if (!str)
            return -ENOMEM;

        int result;
        VECTOR_PUSH_BACK_CHECKED(builder->envp, str, result);
        if (result) {
            free(str);
            return -ENOMEM;
        }
        ++ptr;
    }

    return 0;
}