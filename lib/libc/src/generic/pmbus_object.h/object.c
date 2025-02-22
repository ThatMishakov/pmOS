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

static size_t object_header_fill(uint8_t *ptr, const char *name, pmos_port_t handle_port, uint64_t task_group, uint32_t length)
{
    size_t name_length = strlen(name);
    size_t properties_offset = ALIGN_TO(sizeof(struct IPC_Bus_Object) + name_length + 1, 8);

    struct IPC_Bus_Object o = {
        .size = length,
        .name_length = name_length,
        .properties_offset = properties_offset,
        .handle_port = handle_port,
        .task_group = task_group,
    };
    
    memcpy(ptr, &o, sizeof(struct IPC_Bus_Object));
    size_t offset = sizeof(struct IPC_Bus_Object);
    memcpy(ptr + offset, name, name_length);
    offset += name_length;
    memset(ptr + offset, 0, properties_offset - offset);
    return properties_offset;
}

static const size_t PUBLISH_OBJECT_HEADER_SIZE = 24;

bool pmos_bus_object_serialize_ipc(const pmos_bus_object_t *object, pmos_port_t reply_port, uint64_t user_arg, pmos_port_t handle_port, uint64_t task_group, uint8_t **data_out, size_t *size_out)
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
        .reply_port = reply_port,
        .user_arg = user_arg,
    };

    size_t offset = 0;
    memcpy(data, &po, PUBLISH_OBJECT_HEADER_SIZE);
    offset += PUBLISH_OBJECT_HEADER_SIZE;
    offset += object_header_fill(data + offset, object->name, handle_port, task_group, size - offset);
    for (size_t i = 0; i < object->num_properties; ++i) {
        offset += property_fill(data + offset, object->properties + i);
    }
    assert(offset == size);
    return true;
}