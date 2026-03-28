#include <pmos/pmbus_object.h>
#include <stdlib.h>
#include <string.h>
#include <pmos/vector.h>

#define PMOS_BUS_FILTER_EQUALS_TYPE 1
#define PMOS_BUS_FILTER_CONJUNCTION_TYPE 2
#define PMOS_BUS_FILTER_DISJUNCTION_TYPE 3

pmos_bus_filter_conjunction *pmos_bus_filter_conjunction_create()
{
    pmos_bus_filter_conjunction *filter = malloc(sizeof(*filter));
    if (!filter)
        return NULL;

    filter->header.type = PMOS_BUS_FILTER_CONJUNCTION_TYPE;
    filter->values = (pmos_filter_void_ptr_vector)VECTOR_INIT;

    return filter;
}

void pmos_bus_filter_conjunction_free(pmos_bus_filter_conjunction *filter)
{
    if (!filter)
        return;

    void *v;
    VECTOR_FOREACH(filter->values, v) {
        pmos_bus_filter_free(v);
    }
    VECTOR_FREE(filter->values);
    free(filter);
}

pmos_bus_filter_disjunction *pmos_bus_filter_disjunction_create()
{
    pmos_bus_filter_disjunction *filter = malloc(sizeof(*filter));
    if (!filter)
        return NULL;

    filter->header.type = PMOS_BUS_FILTER_DISJUNCTION_TYPE;
    filter->values = (pmos_filter_void_ptr_vector)VECTOR_INIT;

    return filter;
}

void pmos_bus_filter_disjunction_free(pmos_bus_filter_disjunction *filter)
{
    if (!filter)
        return;

    void *v;
    VECTOR_FOREACH(filter->values, v) {
        pmos_bus_filter_free(v);
    }
    VECTOR_FREE(filter->values);
    free(filter);
}

void pmos_bus_filter_equals_free(pmos_bus_filter_equals *filter)
{
    if (!filter)
        return;

    free(filter->key);
    free(filter->value);
    free(filter);
}

void pmos_bus_filter_free(void *filter)
{
    if (!filter)
        return;

    struct _pmos_bus_filter_header *header = filter;
    switch (header->type) {
    case PMOS_BUS_FILTER_EQUALS_TYPE:
        pmos_bus_filter_equals_free(filter);
        break;
    case PMOS_BUS_FILTER_CONJUNCTION_TYPE:
        pmos_bus_filter_conjunction_free(filter);
        break;
    case PMOS_BUS_FILTER_DISJUNCTION_TYPE:
        pmos_bus_filter_disjunction_free(filter);
        break;
    default:
        break;
    }
}

pmos_bus_filter_equals *pmos_bus_filter_equals_create(const char *key, const char *value)
{
    if (!key || !value)
        return NULL;

    pmos_bus_filter_equals *filter = malloc(sizeof(*filter));
    if (!filter)
        return NULL;

    filter->header.type = PMOS_BUS_FILTER_EQUALS_TYPE;
    filter->key = strdup(key);
    filter->value = strdup(value);
    if (!filter->key || !filter->value) {
        free(filter->key);
        free(filter->value);
        free(filter);
        return NULL;
    }

    return filter;
}

int pmos_bus_filter_disjunction_add(pmos_bus_filter_disjunction *filter, void *value)
{
    if (!filter || !value)
        return -1;

    struct _pmos_bus_filter_header *header = value;
    if (header->type != PMOS_BUS_FILTER_EQUALS_TYPE && header->type != PMOS_BUS_FILTER_CONJUNCTION_TYPE &&
        header->type != PMOS_BUS_FILTER_DISJUNCTION_TYPE) {
        return -1;
    }

    int result;
    VECTOR_PUSH_BACK_CHECKED(filter->values, value, result);
    return result;
}
int pmos_bus_filter_conjunction_add(pmos_bus_filter_conjunction *filter, void *value)
{
    return pmos_bus_filter_disjunction_add(filter, value);
}

struct EqualsFilterBinary {
    uint32_t type;
    uint32_t total_size; // Size of the key and value strings, aligned to 8 bytes
    uint32_t key_len;
    uint32_t value_len;
    // Followed by key and value strings, aligned to 8 bytes
};

struct ConDisFilterBinary {
    uint32_t type;
    uint32_t total_size; // Size of the inner filters, aligned to 8 bytes
    // Followed by inner filters
};

#define FILTER_SIZE_MAX UINT32_MAX


size_t pmos_bus_filter_serialize_ipc(const void *filter, uint8_t *data_out)
{
    if (!filter)
        return 0;

    const struct _pmos_bus_filter_header *header = filter;
    switch (header->type) {
    case PMOS_BUS_FILTER_EQUALS_TYPE: {
        const pmos_bus_filter_equals *equals_filter = filter;
        struct EqualsFilterBinary binary;
        binary.type = PMOS_BUS_FILTER_EQUALS_TYPE;
        binary.key_len = strlen(equals_filter->key);
        binary.value_len = strlen(equals_filter->value);

        size_t total_size = sizeof(binary) + binary.key_len + 1 + binary.value_len + 1;
        // Align to 8 bytes
        total_size = (total_size + 7) & ~((size_t)7);

        binary.total_size = total_size;

        if (total_size > FILTER_SIZE_MAX)
            return 0;

        if (data_out) {
            memcpy(data_out, &binary, sizeof(binary));
            memcpy(data_out + sizeof(binary), equals_filter->key, binary.key_len + 1);
            memcpy(data_out + sizeof(binary) + binary.key_len + 1, equals_filter->value, binary.value_len + 1);
            memset(data_out + sizeof(binary) + binary.key_len + 1 + binary.value_len + 1, 0,
                   total_size - (sizeof(binary) + binary.key_len + 1 + binary.value_len + 1));
        }

        return total_size;
    }
    case PMOS_BUS_FILTER_CONJUNCTION_TYPE:
    case PMOS_BUS_FILTER_DISJUNCTION_TYPE: {
        size_t total_size = sizeof(struct ConDisFilterBinary);
        const pmos_bus_filter_disjunction *compound_filter = filter; // Use disjunction struct for both, they have the same layout
        void *v;
        VECTOR_FOREACH(compound_filter->values, v) {
            size_t s = pmos_bus_filter_serialize_ipc(v, data_out ? data_out + total_size : NULL);
            if (s == 0)
                return 0;

            if (s > FILTER_SIZE_MAX - total_size)
                return 0;

            total_size += s;
        }

        if (data_out) {
            struct ConDisFilterBinary binary = {};
            binary.type = header->type;
            binary.total_size = total_size;

            memcpy(data_out, &binary, sizeof(binary));
        }
        
        return total_size;
        }
    }
        
    return 0;
}