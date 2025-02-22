#ifndef PMOS_BUS_OBJECT_H
#define PMOS_BUS_OBJECT_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pmos/ports.h>

typedef enum {
    PMOS_PROPERTY_STRING = 1,
    PMOS_PROPERTY_INTEGER = 2,
    PMOS_PROPERTY_LIST = 3,
} pmos_property_type_t;

typedef struct {
    char *name; /* property name (dynamically allocated) */
    pmos_property_type_t type;
    union {
        char *s_val;    /* dynamically allocated string */
        uint64_t i_val;
        char **l_val; // Array of strings, with NULL terminator
    } value;
} pmos_property_t;

typedef struct {
    char *name; /* the object name (dynamically allocated) */
    pmos_property_t *properties; /* dynamic array of properties */
    size_t num_properties;
    size_t capacity;
} pmos_bus_object_t;

/// Create a new pmbus object. Returns NULL is there is not enough memory
pmos_bus_object_t *pmos_bus_object_create(void);

/// Free an object, with all of its contents
void pmos_bus_object_free(pmos_bus_object_t *obj);

/// @brief  Set the name of the object. Is the name was already set, replace it
/// @param obj Valid pmbus object
/// @param name Name (will be strdup()ed)
/// @return true if successful, false otherwise (obj not valid or not enough memory)
bool pmos_bus_object_set_name(pmos_bus_object_t *obj, const char *name);

/// @brief Gets the name of the object
/// @param obj Valid object
/// @return The name of the object. NULL is there is no name, or the object is not valid
const char *pmos_bus_object_get_name(pmos_bus_object_t *obj);

/// @brief Sets the property to the string. Replaces previous value if the property is not new
/// @param obj Valid pmbus object
/// @param prop_name Name of the property
/// @param value Value of the property
/// @return true if successful, false otherwise (obj not valid or not enough memory)
bool pmos_bus_object_set_property_string(pmos_bus_object_t *obj, const char *prop_name, const char *value);

/// @brief Sets the propery to the string. Replaces previous value if the property was not newly created 
/// @param obj Valid pmbus object
/// @param prop_name Name of the property
/// @param value Value of the property
/// @return true if successful, false otherwise (obj not valid or not enough memory)
bool pmos_bus_object_set_property_integer(pmos_bus_object_t *obj, const char *prop_name, uint64_t value);

/// @brief Sets the property to the list of strings. Replaces previous value if the property was not newly created
/// @param obj Valid pmbus object
/// @param prop_name Name of the property
/// @param value Array of strings, terminated by pointer to NULL
/// @return true if successful, false otherwise (not enough memory, obj or value are NULL)
bool pmos_bus_object_set_property_list(pmos_bus_object_t *obj, const char *prop_name, const char **value);

/// @brief Retrieves the propery
///
/// A reference to the property is retrieved from the object. It is valid as long as object is not modified
/// @param object Valid pmbus object
/// @param prop_name Name of the property
/// @return pointer to the property if it was found, NULL otherwise
const pmos_property_t *pmos_bus_object_get_property(const pmos_bus_object_t *object, const char *prop_name);

/// @brief Get first property in the object
/// @param object Valid pmbus object
/// @return Pointer to the first property. Can be NULL if the object has no properties or is invalid
const pmos_property_t *pmos_bus_object_first_property(const pmos_bus_object_t *object);

/// @brief Gets the next property in the object
/// @param object Valid
/// @param current Current property (property after this will be returned)
/// @return Pointer to the next property, or NULL if there is none
const pmos_property_t *pmos_bus_object_next_property(const pmos_bus_object_t *object);

bool pmos_bus_object_serialize_ipc(const pmos_bus_object_t *object, pmos_port_t reply_port, uint64_t user_arg, pmos_port_t handle_port, uint64_t task_group, uint8_t **data_out, size_t *size_out);

#endif /* PMOS_BUS_OBJECT_H */