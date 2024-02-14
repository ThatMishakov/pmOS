#include <pthread.h>

#include "spin_pause.h"
#include "internal_rwlock.h"

struct pthread_key_data {
    void (*destructor)(void *);
    uint64_t generation;
    bool used;
};

// I don't like the notion of bounded arrays, so use a growable one instead
static struct pthread_key_data * pthread_key_data = NULL;
static size_t pthread_key_count = 0;
static size_t pthread_key_capacity = 0;

// I've tried to come up with something that uses less locks with dynamic arrays, but it seems too
// complicated and error prone, so just use an rwlock for now
static struct internal_rwlock pthread_key_rwlock = __INTERNAL_RWLOCK_INITIALIZER;

static size_t KEY_DATA_CAPACITY_INCREMENT = 16;



int pthread_key_create(pthread_key_t * key, void (*destructor)(void *)) {
    if (key == NULL) {
        errno = EINVAL;
        return -1;
    }

    __internal_rwlock_write_lock(&pthread_key_rwlock);

    // Grow the array if needed
    if (pthread_key_count == pthread_key_capacity) {
        size_t new_capacity = pthread_key_capacity + KEY_DATA_CAPACITY_INCREMENT;
        struct pthread_key_data * new_pthread_key_data = realloc(pthread_key_data, new_capacity * sizeof(struct pthread_key_data));
        if (new_pthread_key_data == NULL) {
            __internal_rwlock_write_unlock(&pthread_key_rwlock);
            // realloc() sets errno
            return -1;
        }

        // NULL out the new elements
        for (size_t i = pthread_key_capacity; i < new_capacity; i++) {
            new_pthread_key_data[i].destructor = NULL;
            new_pthread_key_data[i].generation = 0;
            new_pthread_key_data[i].used = false;
        }

        pthread_key_data = new_pthread_key_data;
        pthread_key_capacity = new_capacity;
    }

    // Find a free key
    size_t key_index;
    for (key_index = 0; key_index < pthread_key_capacity; key_index++) {
        if (!pthread_key_data[key_index].used) {
            break;
        }
    }

    // Set internal key
    struct pthread_key_data * arr_key = &pthread_key_data[key_index];

    arr_key->destructor = destructor;
    // Generation is needed to reuse keys, since someone could call pthread_key_delete() and then pthread_key_create() again
    // which might then call destructor (on some thread exit) with a stale pointer in a thread that used the key before
    arr_key->generation++;
    arr_key->used = true;

    pthread_key_count++;

    size_t generation = arr_key->generation;

    __internal_rwlock_write_unlock(&pthread_key_rwlock);

    // Do this outside of the lock to minimize the damage to other threads in case someone provides a bogus key
    key->index = key_index;
    key->generation = generation;

    return 0;
}

int pthread_key_delete(pthread_key_t key) {
    __internal_rwlock_write_lock(&pthread_key_rwlock);

    // Check if key is valid
    if (key.index >= pthread_key_capacity || !pthread_key_data[key.index].used) {
        __internal_rwlock_write_unlock(&pthread_key_rwlock);
        errno = EINVAL;
        return -1;
    }

    // Mark the key as unused
    pthread_key_data[key.index].used = false;

    pthread_key_count--;

    __internal_rwlock_write_unlock(&pthread_key_rwlock);

    return 0;
}

struct key_specific_data {
    void * data;
    uint64_t generation;
};

static __thread struct key_specific_data * key_specific_data = NULL;
static __thread size_t key_specific_data_size = 0;

int pthread_setspecific(pthread_key_t key, const void * value) {
    __internal_rwlock_read_lock(&pthread_key_rwlock);

    // Check if key is valid
    if (key.index >= pthread_key_capacity || !pthread_key_data[key.index].used) {
        __internal_rwlock_read_unlock(&pthread_key_rwlock);
        errno = EINVAL;
        return -1;
    }

    // Get generation
    uint64_t generation = pthread_key_data[key.index].generation;

    __internal_rwlock_read_unlock(&pthread_key_rwlock);

    // Grow the array if needed
    if (key_specific_data_size <= key.index) {
        // If value is NULL, then if an array is out of bounds, getspecific() will return NULL, so we don't need to allocate anything
        // and when pthread_setspecific() is called again with a valid key, it will allocate the array and set the NULL for us
        if (value == NULL) {
            return 0;
        }

        // Align new size to KEY_DATA_CAPACITY_INCREMENT to be inline with pthread_key_create()
        size_t new_size = key.index + KEY_DATA_CAPACITY_INCREMENT - (key.index % KEY_DATA_CAPACITY_INCREMENT);
        size_t new_size_bytes = new_size * sizeof(struct key_specific_data);
        struct key_specific_data * new_key_specific_data = realloc(key_specific_data, new_size_bytes);
        if (new_key_specific_data == NULL) {
            // realloc() sets errno
            return -1;
        }

        // NULL out the new elements
        for (size_t i = key_specific_data_size; i < new_size; i++) {
            new_key_specific_data[i].data = NULL;
            new_key_specific_data[i].generation = 0;
        }

        key_specific_data = new_key_specific_data;
        key_specific_data_size = new_size;
    }

    // Set the value
    // Here, if NULL is passed, generation does actually need to be updated to flush out old value
    // Set data anyway since branching is more expensive
    key_specific_data[key.index].data = (void *)value;
    key_specific_data[key.index].generation = generation;

    return 0;
}

void * pthread_getspecific(pthread_key_t key) {
    // Check if key is valid and set
    if (key.index >= key_specific_data_size) {
        return NULL;
    }

    // Check generation. If it doesn't match, then the key was deleted and recreated, so return NULL
    if (key_specific_data[key.index].generation != key.generation) {
        return NULL;
    }

    return key_specific_data[key.index].data;
}

void __pthread_key_call_destructors() {
    // I'm not sure it's 100% necessary, but lock the rwlock to prevent pthread_key_create()/_delete() from unexpectedly
    // pulling the rug from under us
    __internal_rwlock_read_lock(&pthread_key_rwlock);

    // Iterate over all keys and call destructors for the ones that are set for the current thread
    // and have a destructor
    for (size_t i = 0; i < key_specific_data_size; ++i) {
        // Check if the value is set for the current thread
        if (key_specific_data[i].data == NULL) {
            continue;
        }

        // Check generation, and that the key was not deleted
        if (key_specific_data[i].generation != pthread_key_data[i].generation || !pthread_key_data[i].used) {
            continue;
        }

        // Call destructor if present
        if (pthread_key_data[i].destructor != NULL) {
            pthread_key_data[i].destructor(key_specific_data[i].data);
        }
    }

    __internal_rwlock_read_unlock(&pthread_key_rwlock);

    // Free the key specific data
    free(key_specific_data);
}