#include <pmos/pmbus_object.h>
#include <stdlib.h>
#include <string.h>
#include <pmos/vector.h>
#include <errno.h>

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

void *pmos_bus_filter_dup(const void *filter)
{
    if (!filter)
        return NULL;

    const struct _pmos_bus_filter_header *header = filter;
    switch (header->type) {
    case PMOS_BUS_FILTER_EQUALS_TYPE: {
        const pmos_bus_filter_equals *equals_filter = filter;
        return pmos_bus_filter_equals_create(equals_filter->key, equals_filter->value);
    }
    case PMOS_BUS_FILTER_DISJUNCTION_TYPE:
    case PMOS_BUS_FILTER_CONJUNCTION_TYPE: {
        // Since the layout is the same...
        const pmos_bus_filter_conjunction *conjunction_filter = filter;
        pmos_bus_filter_conjunction *new_filter = NULL;
        if (header->type == PMOS_BUS_FILTER_DISJUNCTION_TYPE)
            new_filter = pmos_bus_filter_disjunction_create();
        else
            new_filter = pmos_bus_filter_conjunction_create();

        if (!new_filter)
            return NULL;

        int result = 0;
        VECTOR_RESERVE(new_filter->values, conjunction_filter->values.size, result);
        if (result) {
            pmos_bus_filter_free(new_filter);
            return NULL;
        }

        void *v;
        VECTOR_FOREACH(conjunction_filter->values, v) {
            void *d = pmos_bus_filter_dup(v);
            if (!d) {
                pmos_bus_filter_free(new_filter);
                return NULL;
            }

            pmos_bus_filter_conjunction_add(new_filter, d);
        }
        return new_filter;
    }
    default:
        return NULL;
    }
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

void *pmos_bus_filter_deserialize_ipc(const uint8_t *data, size_t size)
{
    if (!data || size < sizeof(struct ConDisFilterBinary))
        return NULL;
    const struct ConDisFilterBinary *binary = (const struct ConDisFilterBinary *)data;
    if (binary->total_size > size)
        return NULL;
    
    switch (binary->type) {
    case PMOS_BUS_FILTER_EQUALS_TYPE: {
        if (binary->total_size < sizeof(struct EqualsFilterBinary))
            return NULL;

        const struct EqualsFilterBinary *equals_binary = (const struct EqualsFilterBinary *)data;
        uint32_t key_len = equals_binary->key_len;
        uint32_t value_len = equals_binary->value_len;

        if (sizeof(*equals_binary) + key_len + value_len > binary->total_size)
            return NULL;

        pmos_bus_filter_equals *filter = malloc(sizeof(*filter));
        if (!filter)
            return NULL;

        filter->header.type = PMOS_BUS_FILTER_EQUALS_TYPE;
        filter->key = strndup((const char *)(data + sizeof(*equals_binary)), key_len);
        if (!filter->key) {
            free(filter);
            return NULL;
        }

        filter->value = strndup((const char *)(data + sizeof(*equals_binary) + key_len), value_len);
        if (!filter->value) {
            free(filter->key);
            free(filter);
            return NULL;
        }

        return filter;
    }
    case PMOS_BUS_FILTER_CONJUNCTION_TYPE:
    case PMOS_BUS_FILTER_DISJUNCTION_TYPE: {
        size_t offset = sizeof(struct ConDisFilterBinary);

        pmos_bus_filter_disjunction *filter = NULL;
        if (binary->type == PMOS_BUS_FILTER_DISJUNCTION_TYPE)
            filter = pmos_bus_filter_disjunction_create();
        else
            filter = pmos_bus_filter_conjunction_create();

        if (!filter)
            return NULL;

        while (offset < binary->total_size) {
            if (offset + sizeof(struct ConDisFilterBinary) > binary->total_size) {
                pmos_bus_filter_free(filter);
                return NULL;
            }

            const struct ConDisFilterBinary *inner_binary = (const struct ConDisFilterBinary *)(data + offset);
            if (inner_binary->total_size > binary->total_size - offset) {
                pmos_bus_filter_free(filter);
                return NULL;
            }

            void *inner_filter = pmos_bus_filter_deserialize_ipc(data + offset, inner_binary->total_size);
            if (!inner_filter) {
                pmos_bus_filter_free(filter);
                return NULL;
            }

            int result = pmos_bus_filter_disjunction_add(filter, inner_filter);
            if (result) {
                pmos_bus_filter_free(inner_filter);
                pmos_bus_filter_free(filter);
                return NULL;
            }

            offset += inner_binary->total_size;
        }

        return filter;
    }
    default:
        return NULL;
    }
}


static bool matches_eq_filter(const pmos_bus_object_t *object, const pmos_bus_filter_equals *filter)
{
    if (!object || !filter)
        return false;

    const pmos_property_t *prop = pmos_bus_object_get_property(object, filter->key);
    if (!prop)
        return false;

    switch (prop->type) {
    case PMOS_PROPERTY_STRING:
        return strcmp(prop->value.s_val, filter->value) == 0;
    case PMOS_PROPERTY_INTEGER: {
        char *endptr = NULL;
        errno = 0;
        uint64_t val = strtoull(filter->value, &endptr, 0);
        if (*endptr)
            return false;
        if (errno)
            return false;

        return prop->value.i_val == val;
    }
    case PMOS_PROPERTY_LIST: {
        char **p = prop->value.l_val;
        while (*p) {
            if (strcmp(*p, filter->value) == 0)
                return true;
            ++p;
        }
        return false;
    }
    default:
        return false;
    }
}

bool pmos_bus_object_matches_filter(const pmos_bus_object_t *object, const void *filter)
{
    if (!object || !filter)
        return false;

    const struct _pmos_bus_filter_header *header = filter;
    switch (header->type) {
    case PMOS_BUS_FILTER_EQUALS_TYPE:
        return matches_eq_filter(object, filter);
    case PMOS_BUS_FILTER_CONJUNCTION_TYPE: {
        bool all_match = true;
        const pmos_bus_filter_conjunction *conjunction = filter;
        void *v;
        VECTOR_FOREACH(conjunction->values, v) {
            if (!pmos_bus_object_matches_filter(object, v)) {
                all_match = false;
                break;
            }
        }
        return all_match;
    }
    case PMOS_BUS_FILTER_DISJUNCTION_TYPE: {
        bool any_match = false;
        const pmos_bus_filter_disjunction *disjunction = filter;
        void *v;
        VECTOR_FOREACH(disjunction->values, v) {
            if (pmos_bus_object_matches_filter(object, v)) {
                any_match = true;
                break;
            }
        }
        return any_match;
    }
    default:
        return false;
    }
}