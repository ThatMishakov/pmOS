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