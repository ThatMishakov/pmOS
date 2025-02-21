#ifndef _PMOS_HASHMAP_
#define _PMOS_HASHMAP_

#include <stddef.h>

typedef struct pmos_hashtable_ll {
    struct pmos_hashtable_ll *next, *prev;
} pmos_hashtable_ll_t;

typedef struct _pmos_hashtable_t {
    pmos_hashtable_ll_t *hashtable;
    size_t array_size;
    size_t count;
} pmos_hashtable_t;
#define PMOS_HASHTABLE_INITIALIZER {NULL, 0, 0}

typedef size_t(*pmos_hashtable_get_hash_t)(pmos_hashtable_ll_t *element, size_t total_size);
typedef bool(*pmos_hashtable_equals)(pmos_hashtable_ll_t *element, void *value);

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
void hashmap_delete(pmos_hashtable_ll *to_be_deleted);

pmos_hashtable_ll_t *hashmap_find(pmos_hashtable_t *hashtable, void *value, pmos_hashtable_get_hash_t hash_function, pmos_hashtable_equals equals_function);

#define container_of(ptr, type, member) \
    (type *)((char *)(ptr) - offsetof(type, member))

#endif // _PMOS_HASHMAP