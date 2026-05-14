#include <pmos/pmbus_object.h>
#include <stdlib.h>
#include <string.h>
#include <pmos/ipc.h>
#include <assert.h>

static const int TABLE_INITIAL_CAPACITY = 8;

pmos_bus_object_t *pmos_bus_object_create()
{
    pmos_bus_object_t *obj = malloc(sizeof(*obj));
    if (!obj)
        return NULL;

    pmos_property_t *table = malloc(sizeof(*table) * TABLE_INITIAL_CAPACITY);
    if (!table) {
        free(obj);
        return NULL;
    }

    obj->capacity       = TABLE_INITIAL_CAPACITY;
    obj->name           = NULL;
    obj->num_properties = 0;
    obj->properties     = table;

    return obj;
}

static void release_property(pmos_property_t *p)
{
    if (!p)
        return;

    switch (p->type) {
    case PMOS_PROPERTY_STRING:
        free(p->value.s_val);
        break;
    case PMOS_PROPERTY_LIST:
        if (p->value.l_val == NULL)
            break;

        for (size_t i = 0; p->value.l_val[i] != NULL; ++i) {
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

const char *pmos_bus_object_get_name(pmos_bus_object_t *obj) { return obj ? obj->name : NULL; }

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
            size_t new_capacity    = obj->capacity * 2;
            pmos_property_t *new_p = realloc(obj->properties, new_capacity*sizeof(*obj->properties));
            if (!new_p) {
                release_property(&prop);
                return false;
            }

            obj->properties = new_p;
            obj->capacity   = new_capacity;
        }

        pmos_property_t *start = obj->properties + pos;
        memmove(start + 1, start, (obj->num_properties - pos) * sizeof(*start));
        *start = prop;
        obj->num_properties++;
        return true;
    }
}

const pmos_property_t *pmos_bus_object_get_property(const pmos_bus_object_t *object, const char *prop_name)
{
    if (!object || !prop_name)
        return NULL;

    size_t pos = lower_bound(prop_name, object->properties, object->num_properties);
    if (pos < object->num_properties && strcmp(object->properties[pos].name, prop_name) == 0) {
        return &object->properties[pos];
    } else {
        return NULL;
    }
}

bool pmos_bus_object_set_property_string(pmos_bus_object_t *obj, const char *prop_name,
                                         const char *value)
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
        .name        = new_name,
        .type        = PMOS_PROPERTY_STRING,
        .value.s_val = new_val,
    };

    return set_property(obj, prop);
}

bool pmos_bus_object_set_property_integer(pmos_bus_object_t *obj, const char *prop_name,
                                          uint64_t value)
{
    char *new_name = strdup(prop_name);
    if (!new_name)
        return false;

    pmos_property_t prop = {
        .name        = new_name,
        .type        = PMOS_PROPERTY_INTEGER,
        .value.i_val = value,
    };

    return set_property(obj, prop);
}

bool pmos_bus_object_set_property_list(pmos_bus_object_t *obj, const char *prop_name,
                                       const char **value)
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
        .name        = name_cloned,
        .type        = PMOS_PROPERTY_LIST,
        .value.l_val = l_val,
    };

    return set_property(obj, prop);
}

#define ALIGN_TO(p, alignment) \
    (((p) + ((alignment) - 1)) & ~(size_t)((alignment) - 1))

static size_t property_length(const pmos_property_t *prop)
{
    size_t size = 0;
    const size_t hdr_length = sizeof(struct IPC_Object_Property);
    switch (prop->type) {
        case PMOS_PROPERTY_STRING:
            size = ALIGN_TO(hdr_length + strlen(prop->name) + 1 + strlen(prop->value.s_val) + 1, 8);
            break;
        case PMOS_PROPERTY_INTEGER:
            size = ALIGN_TO(hdr_length + strlen(prop->name) + 1 + sizeof(uint64_t), 8);
            break;
        case PMOS_PROPERTY_LIST: {
            char **p = prop->value.l_val;
            while (*p) {
                size += strlen(*p) + 1;
                ++p;
            }

            size = ALIGN_TO(hdr_length + strlen(prop->name) + 1 + size, 8);
        }
        break;
    }
    return size;
}
static size_t property_fill(uint8_t *location, const pmos_property_t *prop)
{
    size_t size = 0;
    size_t name_length = strlen(prop->name);

    switch (prop->type) {
        case PMOS_PROPERTY_STRING: {
            const char *str = prop->value.s_val;

            size_t hdr_length = sizeof(struct IPC_Object_Property);
            size_t name_hdr_length = hdr_length + name_length + 1;
            size_t prop_length = strlen(str);
            size = ALIGN_TO(name_hdr_length + prop_length + 1, 8);

            struct IPC_Object_Property p = {
                .length = size,
                .type = PROPERTY_TYPE_STRING,
                .data_start = name_hdr_length,
            };
            memcpy(location, &p, hdr_length);
            memcpy(location + hdr_length, prop->name, name_length);
            location[name_hdr_length - 1] = '\0';
            memcpy(location + name_hdr_length, str, prop_length);
            memset(location + name_hdr_length + prop_length, 0, size - name_hdr_length - prop_length);
        } break;

        case PMOS_PROPERTY_INTEGER: {
            size_t hdr_length = sizeof(struct IPC_Object_Property);
            size_t name_hdr_length = hdr_length + name_length + 1;
            size_t int_offset = ALIGN_TO(name_hdr_length, 8);
            size = int_offset + sizeof(uint64_t);

            struct IPC_Object_Property p = {
                .length = size,
                .type = PROPERTY_TYPE_INTEGER,
                .data_start = int_offset,
            };
            memcpy(location, &p, hdr_length);
            memcpy(location + hdr_length, prop->name, name_length);
            memset(location + hdr_length + name_length, 0, int_offset - hdr_length - name_length);
            memcpy(location + int_offset, &prop->value.i_val, sizeof(uint64_t));
        } break;

        case PMOS_PROPERTY_LIST: {
            size_t array_entries = 0;
            size_t str_size = 0;

            size_t hdr_length = sizeof(struct IPC_Object_Property);
            size_t name_hdr_length = hdr_length + name_length + 1;
            memcpy(location + hdr_length, prop->name, name_length+1);

            char **array = prop->value.l_val;
            char *s;
            while ((s = array[array_entries])) {
                size_t size = strlen(s);
                memcpy(location + name_hdr_length + str_size, s, size+1);
                str_size += size+1;
                ++array_entries;
            }

            size = ALIGN_TO(name_hdr_length + str_size, 8);
            struct IPC_Object_Property p = {
                .length = size,
                .type = PROPERTY_TYPE_LIST,
                .data_start = name_hdr_length,
            };
            memcpy(location, &p, sizeof(p));
            memset(location + name_hdr_length + str_size, 0, size - name_hdr_length - str_size);
        } break;
    }

    return size;
}

static size_t object_header_length(const char *name)
{
    size_t l = sizeof(struct IPC_Bus_Object);
    l += strlen(name) + 1;
    return ALIGN_TO(l, 8);
}

static size_t object_header_fill(uint8_t *ptr, const char *name, uint32_t length)
{
    size_t name_length = strlen(name);
    size_t properties_offset = ALIGN_TO(sizeof(struct IPC_Bus_Object) + name_length + 1, 8);

    struct IPC_Bus_Object o = {
        .size = length,
        .name_length = name_length,
        .properties_offset = properties_offset,
    };
    
    memcpy(ptr, &o, sizeof(struct IPC_Bus_Object));
    size_t offset = sizeof(struct IPC_Bus_Object);
    memcpy(ptr + offset, name, name_length);
    offset += name_length;
    memset(ptr + offset, 0, properties_offset - offset);
    return properties_offset;
}

static const size_t PUBLISH_OBJECT_HEADER_SIZE = 8;

bool pmos_bus_object_serialize_ipc(const pmos_bus_object_t *object, uint8_t **data_out, size_t *size_out)
{
    if (!object)
        return false;

    if (!object->name)
        return false;

    size_t size = PUBLISH_OBJECT_HEADER_SIZE;
    size += object_header_length(object->name);
    for (size_t i = 0; i < object->num_properties; ++i) {
        size_t p_size = property_length(object->properties + i);
        assert(!(p_size % 8));
        size += p_size;
    }

    uint8_t *data = malloc(size);
    if (!data)
        return false;

    *data_out = data;
    *size_out = size;

    IPC_BUS_Publish_Object po = {
        .type = IPC_BUS_Publish_Object_NUM,
        .flags = 0,
    };

    size_t offset = 0;
    memcpy(data, &po, PUBLISH_OBJECT_HEADER_SIZE);
    offset += PUBLISH_OBJECT_HEADER_SIZE;
    offset += object_header_fill(data + offset, object->name, size - offset);
    for (size_t i = 0; i < object->num_properties; ++i) {
        offset += property_fill(data + offset, object->properties + i);
    }
    assert(offset == size);
    return true;
}

VECTOR_TYPEDEF(const char *, string_vector);

pmos_bus_object_t *pmos_bus_object_deserialize_ipc(const uint8_t *data, size_t size)
{
    if (!data || size < sizeof(struct IPC_Bus_Object))
        return NULL;

    const struct IPC_Bus_Object *obj = (const struct IPC_Bus_Object *)data;

    pmos_bus_object_t *result = pmos_bus_object_create();
    if (!result) {
        return NULL;
    }

    if (obj->size > size) {
        pmos_bus_object_free(result);
        return NULL;
    }

    if (obj->name_length == 0 || obj->name_length > size - sizeof(struct IPC_Bus_Object)) {
        pmos_bus_object_free(result);
        return NULL;
    }

    if (obj->name_length + sizeof(struct IPC_Bus_Object) > obj->properties_offset) {
        pmos_bus_object_free(result);
        return NULL;
    }

    char *name = strndup((const char *)(data + sizeof(struct IPC_Bus_Object)), obj->name_length);
    if (!name) {
        pmos_bus_object_free(result);
        return NULL;
    }
    result->name = name;

    size_t offset = obj->properties_offset;
    while (offset < size) {
        if (offset + sizeof(struct IPC_Object_Property) > size) {
            pmos_bus_object_free(result);
            return NULL;
        }

        const struct IPC_Object_Property *prop = (const struct IPC_Object_Property *)(data + offset);
        if (offset + prop->length > size) {
            pmos_bus_object_free(result);
            return NULL;
        }

        if (prop->data_start > prop->length) {
            pmos_bus_object_free(result);
            return NULL;
        }

        if (prop->data_start < sizeof(struct IPC_Object_Property)) {
            pmos_bus_object_free(result);
            return NULL;
        }

        size_t name_length = prop->data_start - sizeof(struct IPC_Object_Property);

        char *prop_name = strndup(prop->name, name_length);
        if (!prop_name) {
            pmos_bus_object_free(result);
            return NULL;
        }

        switch (prop->type) {
            case PROPERTY_TYPE_STRING: {
                char *str = strndup((const char *)prop + prop->data_start, prop->length - prop->data_start);
                if (!str) {
                    free(prop_name);
                    pmos_bus_object_free(result);
                    return NULL;
                }
                
                if (!pmos_bus_object_set_property_string(result, prop_name, str)) {
                    free(prop_name);
                    free(str);
                    pmos_bus_object_free(result);
                    return NULL;
                }
                free(prop_name);
                free(str);
            } break;
            case PROPERTY_TYPE_INTEGER: {
                if (prop->data_start + sizeof(uint64_t) != prop->length) {
                    free(prop_name);
                    pmos_bus_object_free(result);
                    return NULL;
                }

                uint64_t val;
                memcpy(&val, (const char *)prop + prop->data_start, sizeof(uint64_t));
                if (!pmos_bus_object_set_property_integer(result, prop_name, val)) {
                    free(prop_name);
                    pmos_bus_object_free(result);
                    return NULL;
                }
                free(prop_name);
            } break;
            case PROPERTY_TYPE_LIST: {
                if (prop->data_start > prop->length) {
                    free(prop_name);
                    pmos_bus_object_free(result);
                    return NULL;
                }

                string_vector vec = VECTOR_INIT;

                size_t offset = prop->data_start;
                size_t str_len = 0;
                while ((str_len = strnlen((const char *)prop + offset, prop->length - offset))) {
                    int r = 0;
                    VECTOR_PUSH_BACK_CHECKED(vec, (const char *)prop + offset, r);
                    if (r) {
                        free(prop_name);
                        VECTOR_FREE(vec);
                        pmos_bus_object_free(result);
                        return NULL;
                    }

                    offset += str_len + 1;
                }

                int r = 0;
                VECTOR_PUSH_BACK_CHECKED(vec, NULL, r);
                if (r) {
                    free(prop_name);
                    VECTOR_FREE(vec);
                    pmos_bus_object_free(result);
                    return NULL;
                }

                if (!pmos_bus_object_set_property_list(result, prop_name, vec.data)) {
                    free(prop_name);
                    VECTOR_FREE(vec);
                    pmos_bus_object_free(result);
                    return NULL;
                }
                free(prop_name);
                VECTOR_FREE(vec);
            } break;
            default:
                free(prop_name);
                pmos_bus_object_free(result);
                return NULL;
        }

        offset += prop->length;
        if (prop->length == 0) {
            pmos_bus_object_free(result);
            return NULL;
        }
    }
    return result;
}