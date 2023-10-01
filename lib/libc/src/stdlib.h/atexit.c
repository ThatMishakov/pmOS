#include <stdlib.h>
#include <pthread.h>

// Pass the pointer for __cxa_atexit
// Normal atexit() functions should just ignore it
typedef void (*atexit_func_t)(void *);

struct atexit_func_entry_t {
    /// @brief Function to call on exit
    atexit_func_t destructor_func;

    /// @brief Argument to pass to the function. Only used for C++ objects.
    void *arg;

    /// @brief DSO handle where the function was registered. If it gets destroyed, the functions are called.
    void *dso_handle;
};

struct atexit_func_entry_t *__atexit_funcs_array = NULL;
size_t __atexit_funcs_count = 0;
size_t __atexit_funcs_capacity = 0;

static int atexit_array_try_grow()
{
    if (__atexit_funcs_count == __atexit_funcs_capacity) {
        // Increase the capacity by a fixed increment
        size_t new_atexit_funcs_capacity = __atexit_funcs_capacity + 100;

        // Reallocate the file_pointer_array with the new capacity
        struct atexit_func_entry_t *temp = realloc(__atexit_funcs_array, new_atexit_funcs_capacity * sizeof(struct atexit_func_entry_t));
        if (temp == NULL) {
            // realloc() sets errno
            return -1;
        }
        __atexit_funcs_array = temp;
        __atexit_funcs_capacity = new_atexit_funcs_capacity;
    }

    return 0;
}

// I don't anticipate atexit-related functions to be called a lot, so a spinlock should be fine
pthread_spinlock_t __atexit_funcs_lock = 0;

int __cxa_atexit(void (*func) (void *), void * arg, void * dso_handle)
{
    pthread_spin_lock(&__atexit_funcs_lock);
    int grow_result = atexit_array_try_grow();
    if (grow_result != 0) {
        // atexit_array_try_grow() sets errno
        pthread_spin_unlock(&__atexit_funcs_lock);
        return -1;
    }

    struct atexit_func_entry_t *entry = &__atexit_funcs_array[__atexit_funcs_count];
    *entry = (struct atexit_func_entry_t) {
        .destructor_func = func,
        .arg = arg,
        .dso_handle = dso_handle,
    };

    __atexit_funcs_count++;

    pthread_spin_unlock(&__atexit_funcs_lock);
    return 0;
}

int atexit(void (*func)(void))
{
    return __cxa_atexit((void (*)(void *))func, NULL, NULL);
}

void _atexit_pop_all()
{
    while (__atexit_funcs_count > 0) {
        struct atexit_func_entry_t *e = __atexit_funcs_array + __atexit_funcs_count - 1;
        e->destructor_func(e->arg);
        --__atexit_funcs_count;
    }
}
