#ifndef PMOS_BUS_OBJECT_H
#define PMOS_BUS_OBJECT_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pmos/ports.h>
#include <pmos/vector.h>

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


/// Header for the list of filters
struct _pmos_bus_filter_header {
    int type; 
};

/// Filter that checks if the property with the given key is equal to the given value
typedef struct {
    struct _pmos_bus_filter_header header;
    char *key;
    char *value;
} pmos_bus_filter_equals;

/// @brief Create a new "equals" filter, which checks if the property with the given key is equal to the
/// given value. Returns NULL if there is not enough memory. Values are copied.
/// @param key Key of the property to check
/// @param value Value to compare the property to
/// @return New equals filter, or NULL if there is not enough memory
pmos_bus_filter_equals *pmos_bus_filter_equals_create(const char *key, const char *value);

/// @brief Free an equals filter
/// @param filter Filter to free. Does nothing if filter is NULL
void pmos_bus_filter_equals_free(pmos_bus_filter_equals *filter);

VECTOR_TYPEDEF(void *, pmos_filter_void_ptr_vector);

/// Conjunction or disjunction of filters.
typedef struct {
    struct _pmos_bus_filter_header header;
    pmos_filter_void_ptr_vector values;
} pmos_bus_filter_conjunction, pmos_bus_filter_disjunction;

/// Create a new conjunction filter. Returns NULL if there is not enough memory. The filter should be freed with pmos_bus_filter_conjunction_free
/// @return New conjunction filter, or NULL if there is not enough memory
pmos_bus_filter_conjunction *pmos_bus_filter_conjunction_create();
/// @brief Adds a filter to the conjunction. Returns false if there is not enough memory or the filter is invalid (NULL or of wrong type)
/// @param filter Valid conjunction filter
/// @param value Value to be added to the conjunction. Can be a conjunction, disjunction or equals filter. Consumed if successfully added.
/// @return 0 if successful, -1 otherwise (not enough memory, filter or value are NULL, filter is of wrong type)
int pmos_bus_filter_conjunction_add(pmos_bus_filter_conjunction *filter, void *value);
void pmos_bus_filter_conjunction_free(pmos_bus_filter_conjunction *filter);

/// Create a new disjunction filter. Returns NULL if there is not enough memory. The filter should be freed with pmos_bus_filter_disjunction_free
/// @return New disjunction filter, or NULL if there is not enough memory
pmos_bus_filter_disjunction *pmos_bus_filter_disjunction_create();
/// @brief Adds a filter to the disjunction. Returns false if there is not enough memory or the filter is invalid (NULL or of wrong type)
/// @param filter Valid disjunction filter
/// @param value Value to be added to the disjunction. Can be a conjunction, disjunction or equals filter. Consumed if successfully added.
/// @return 0 if successful, -1 otherwise (not enough memory, filter or value are NULL, filter is of wrong type)
int pmos_bus_filter_disjunction_add(pmos_bus_filter_disjunction *filter, void *value);
/// @brief Free a disjunction filter
/// @param filter Filter to free. Does nothing if filter is NULL
void pmos_bus_filter_disjunction_free(pmos_bus_filter_disjunction *filter);

/// @brief Free a filter of any type. The correct type is determined from the header. Does nothing if filter is NULL
void pmos_bus_filter_free(void *filter);

/// @brief Serialize a filter to be sent over IPC. Passing NULL data_out will return the size without writing anything.
///
/// @param filter Valid filter to serialize
/// @param data_out If not NULL, pointer to the buffer where serialized data will be written
/// @return Size of the serialized data. Alligned to 8 bytes. Returns 0 if the filter is NULL or invalid
size_t pmos_bus_filter_serialize_ipc(const void *filter, uint8_t *data_out);
/// @brief Deserialize a filter from IPC data
/// @param data Data to deserialize from
/// @param size Size of the data
/// @return Pointer to the new filter, or NULL if the data is invalid or there is not enough memory. The filter should be freed with pmos_bus_filter_free
void *pmos_bus_filter_deserialize_ipc(const uint8_t *data, size_t size);


#endif /* PMOS_BUS_OBJECT_H */