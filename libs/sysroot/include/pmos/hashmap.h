#ifndef _PMOS_HASHMAP_
#define _PMOS_HASHMAP_

#include <stddef.h>
#include <stdbool.h>

typedef struct pmos_hashtable_ll {
    struct pmos_hashtable_ll *next;
} pmos_hashtable_ll_t;

typedef struct _pmos_hashtable_t {
    pmos_hashtable_ll_t *hashtable;
    size_t array_size;
    size_t count;
} pmos_hashtable_t;
#define PMOS_HASHTABLE_INITIALIZER {NULL, 0, 0}

typedef size_t(*pmos_hashtable_get_hash_t)(pmos_hashtable_ll_t *element, size_t total_size);
typedef size_t(*pmos_hashtable_hash_for_value)(void *value, size_t total_size);
typedef bool(*pmos_hashtable_equals)(pmos_hashtable_ll_t *element, void *value);
typedef void(*pmos_hashmap_foreach_func)(pmos_hashtable_ll_t *element, void *ctx);

/// @brief Initializes hashtable. If it was already initialized, this will leak memory
/// @param hashtable Hashmap to initialize
/// @return 0 on success, -1 otherwise
int hasmap_initialize(pmos_hashtable_t *hashtable);

/// @brief Destoy (free memory associated with) a hash table
/// @param hashtable Hash table to be destroyed
void hashtable_destroy(pmos_hashtable_t *hashtable);

/// @brief Adds an element to the hashtable
/// @param hashtable Hash table to add the element to
/// @param hash_function Hash function
/// @param new_entry Entry to be added
/// @return 0 on success, -1 otherwise
int hashmap_add(pmos_hashtable_t *hashtable, pmos_hashtable_get_hash_t hash_function, pmos_hashtable_ll_t *new_entry);

/// @brief Deletes the value from the hashtable it is in
/// @param to_be_deleted Element to be deleted
void hashtable_delete(pmos_hashtable_t *hashtable, pmos_hashtable_get_hash_t hash_function, pmos_hashtable_ll_t *entry);

/// @brief Finds the value in the hashtable
/// @param hashtable Hashtable to be searched
/// @param value Value to be searched for (passed to hash_function and equals_function)
/// @param hash_function Function for hashing of value
/// @param equals_function Function which matches the value
/// @return The element, or NULL if it was not found, or if input is not valid
pmos_hashtable_ll_t *hashtable_find(const pmos_hashtable_t *hashtable, void *value, pmos_hashtable_hash_for_value hash_function, pmos_hashtable_equals equals_function);

/// @brief Calls func on each member of the hash table
/// @param hashtable Hash table
/// @param func Function to be called
/// @param ctx Context passed to function on each invocation
void hashtable_foreach(const pmos_hashtable_t *hashtable, pmos_hashmap_foreach_func func, void *ctx);

#define container_of(ptr, type, member) \
    (type *)((char *)(ptr) - offsetof(type, member))

#endif // _PMOS_HASHMAP