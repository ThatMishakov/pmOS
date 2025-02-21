#include <pmos/pmbus_object.h>
#include <stdlib.h>
#include <string.h>

static const int TABLE_INITIAL_CAPACITY = 8;

pmos_bus_object_t *pmos_bus_object_create()
{
    pmos_bus_object_t *obj = malloc(sizeof(*obj));
    if (!obj)
        return NULL;

    
    pmos_property_t *table = malloc(sizeof(*table)*TABLE_INITIAL_CAPACITY);
    if (!table) {
        free(obj);
        return NULL;
    }

    obj->capacity = TABLE_INITIAL_CAPACITY;
    obj->name = NULL;
    obj->num_properties = 0;
    obj->properties = table;

    return obj;
}

static void release_property(pmos_property_t *p)
{
    if (!p)
        return;

    switch (p->type)
    {
    case PMOS_PROPERTY_STRING:
        free(p->value.s_val);
        break;
    case PMOS_PROPERTY_LIST:
        if (p->value.l_val == NULL)
            break;

        for(size_t i = 0; p->value.l_val[i] != NULL; ++i) {
            free(p->value.l_val[i]);
        }
        free(p->value.l_val);

        break;
    default:
        break;
    }

    free(p->name);
}

void pmos_bus_object_free(pmos_bus_object_t *obj)
{
    if (!obj)
        return;

    free(obj->name);
    for (size_t i = 0; i < obj->num_properties; ++i) {
        release_property(&obj->properties[i]);
    }
    free(obj->properties);
    return;
}

bool pmos_bus_object_set_name(pmos_bus_object_t *obj, const char *name)
{
    if (!obj || !name)
        return false;

    char *new_name = strdup(name);
    if (!new_name)
        return false;

    free(obj->name);
    obj->name = new_name;
    return true;
}

static size_t lower_bound(const char *key, pmos_property_t properties[], size_t count)
{
    size_t low = 0, high = count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (strcmp(properties[mid].name, key) < 0)
            low = mid + 1;
        else
            high = mid;
    }
    return low;
}

static bool set_property(pmos_bus_object_t *obj, pmos_property_t prop)
{
    size_t pos = lower_bound(prop.name, obj->properties, obj->num_properties);

    if (pos < obj->num_properties && strcmp(obj->properties[pos].name, prop.name) == 0) {
        release_property(obj->properties + pos);
        obj->properties[pos] = prop;
        return true;
    } else {
        if (obj->num_properties <= obj->capacity) {
            size_t new_capacity = obj->capacity*2;
            pmos_property_t *new_p = realloc(obj->properties, new_capacity);
            if (!new_p) {
                release_property(&prop);
                return false;
            }

            obj->properties = new_p;
            obj->capacity = new_capacity;
        }

        pmos_property_t *start = obj->properties + pos;
        memmove(start + 1, start, (obj->num_properties - pos) * sizeof(*start));
        *start = prop;
        obj->num_properties++;
        return true;
    }
}

bool pmos_bus_object_set_property_string(pmos_bus_object_t *obj, const char *prop_name, const char *value)
{
    char *new_name = strdup(prop_name);
    if (!new_name)
        return false;

    char *new_val = strdup(value);
    if (!new_val) {
        free(new_name);
        return false;
    }

    pmos_property_t prop = {
        .name = new_name,
        .type = PMOS_PROPERTY_STRING,
        .value.s_val = new_val,
    };

    return set_property(obj, prop);
}

bool pmos_bus_object_set_property_integer(pmos_bus_object_t *obj, const char *prop_name, uint64_t value)
{
    char *new_name = strdup(prop_name);
    if (!new_name)
        return false;

    pmos_property_t prop = {
        .name = new_name,
        .type = PMOS_PROPERTY_INTEGER,
        .value.i_val = value,
    }; 

    return set_property(obj, prop);
}

bool pmos_bus_object_set_property_list(pmos_bus_object_t *obj, const char *prop_name, const char **value)
{
    if (!obj || !prop_name || !value) {
        return false;
    }

    size_t count = 0;
    while (value[count])
        ++count;

    char *name_cloned = strdup(prop_name);
    if (!name_cloned)
        return false;

    char **l_val = malloc(sizeof(*l_val) * (count + 1));
    if (!l_val) {
        free(name_cloned);
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        l_val[i] = strdup(value[i]);
        if (!l_val[i]) {
            for (size_t j = 0; j < i; ++j)
                free(l_val[j]);
            free(name_cloned);
            free(l_val);
            return false;
        }
    }

    l_val[count] = NULL;

    pmos_property_t prop = {
        .name = name_cloned,
        .type = PMOS_PROPERTY_INTEGER,
        .value.l_val = l_val,
    }; 

    return set_property(obj, prop);
}