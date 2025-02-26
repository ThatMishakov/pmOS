#include <pmos/hashmap.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

#define LOAD_FACTOR 3/5
#define SHRINK_FACTOR 1/4
#define MIN_SIZE 16

static void hashmap_link(pmos_hashtable_ll_t *array, pmos_hashtable_ll_t *entry, size_t size, pmos_hashtable_get_hash_t hash_function)
{
    size_t idx = hash_function(entry, size);
    assert(idx < size);
    entry->next = array[idx].next;
    array[idx].next = entry;
}

static bool hastable_grow_for_insert(pmos_hashtable_t *hashtable, pmos_hashtable_get_hash_t hash_function)
{
    if (hashtable->array_size * LOAD_FACTOR < hashtable->count) {
        size_t new_size = hashtable->array_size*2;
        pmos_hashtable_ll_t *new_array = calloc(new_size, sizeof(*new_array));
        if (!new_array)
            return false;

        for (size_t i = 0; i < hashtable->array_size; ++i) {
            while (hashtable->hashtable[i].next) {
                pmos_hashtable_ll_t *e = hashtable->hashtable[i].next;
                hashtable->hashtable[i].next = e->next;
                hashmap_link(new_array, e, new_size, hash_function);
            }
        }

        free(hashtable->hashtable);
        hashtable->hashtable = new_array;
        hashtable->array_size = new_size;
    }
    return true;
}

static void hashtable_maybe_shrink(pmos_hashtable_t *hashtable, pmos_hashtable_get_hash_t hash_function)
{
    if (hashtable->array_size > MIN_SIZE && hashtable->array_size * SHRINK_FACTOR > hashtable->count) {
        size_t new_size = hashtable->array_size/2;
        pmos_hashtable_ll_t *new_array = calloc(new_size, sizeof(*new_array));
        if (!new_array)
            return;

        for (size_t i = 0; i < hashtable->array_size; ++i) {
            while (hashtable->hashtable[i].next) {
                pmos_hashtable_ll_t *e = hashtable->hashtable[i].next;
                hashtable->hashtable[i].next = e->next;
                hashmap_link(new_array, e, new_size, hash_function);
            }
        }

        free(hashtable->hashtable);
        hashtable->hashtable = new_array;
        hashtable->array_size = new_size;
    }
}

int hasmap_initialize(pmos_hashtable_t *hashtable)
{
    pmos_hashtable_ll_t *ptr = calloc(MIN_SIZE, sizeof(*ptr));
    if (!ptr) {
        errno = ENOMEM;
        return -1;
    }

    hashtable->array_size = MIN_SIZE;
    hashtable->count = 0;
    hashtable->hashtable = ptr;
    return 0;
}

int hashmap_add(pmos_hashtable_t *hashtable, pmos_hashtable_get_hash_t hash_function, pmos_hashtable_ll_t *new_entry)
{
    if (!hashtable) {
        errno = EINVAL;
        return -1;
    }

    if (!hashtable->hashtable) {
        int result = hasmap_initialize(hashtable);
        if (result < 0)
            return result;
    }

    if (!hastable_grow_for_insert(hashtable, hash_function))
        return -1;

    hashmap_link(hashtable->hashtable, new_entry, hashtable->array_size, hash_function);
    ++hashtable->count;
    return 0;
}

void hashtable_foreach(const pmos_hashtable_t *hashtable, pmos_hashmap_foreach_func func, void *ctx)
{
    if (!hashtable || !func)
        return;

    for (size_t i = 0; i < hashtable->array_size; ++i) {
        pmos_hashtable_ll_t *l = hashtable->hashtable[i].next;
        while (l) {
            func(l, ctx);
            l = l->next;
        }
    }
}

void hashtable_delete(pmos_hashtable_t *hashtable, pmos_hashtable_get_hash_t hash_function, pmos_hashtable_ll_t *entry)
{
    if (!hashtable || !entry || !hashtable->array_size)
        return;

    assert(hash_function);

    size_t idx = hash_function(entry, hashtable->array_size);
    assert(idx < hashtable->array_size);

    pmos_hashtable_ll_t *e = hashtable->hashtable + idx;
    while (e && e->next != entry)
        e = e->next;

    if (e) {
        e->next = entry->next;
        hashtable->count--;

        hashtable_maybe_shrink(hashtable, hash_function);
    }
}

pmos_hashtable_ll_t *hashtable_find(const pmos_hashtable_t *hashtable, void *value, pmos_hashtable_hash_for_value hash_function, pmos_hashtable_equals equals_function)
{
    if (!hashtable || !hash_function || !equals_function || !hashtable->array_size)
        return NULL;

    size_t idx = hash_function(value, hashtable->array_size);
    assert(idx < hashtable->array_size);
    pmos_hashtable_ll_t *e = hashtable->hashtable[idx].next;
    while (e) {
        if (equals_function(e, value))
            return e;
        e = e->next;
    }
    return NULL;
}