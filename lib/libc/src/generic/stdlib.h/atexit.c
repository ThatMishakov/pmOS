#include <stdlib.h>
#include <pthread.h>

// Pass the pointer for __cxa_atexit
// Normal atexit() functions should just ignore it
typedef void (*atexit_func_t)(void *);

/// Linked list of functions to call on exit
struct atexit_func_entry_t {
    struct atexit_func_entry_t * prev;
    struct atexit_func_entry_t * next;

    /// @brief Function to call on exit
    atexit_func_t destructor_func;

    /// @brief Argument to pass to the function. Only used for C++ objects.
    void *arg;

    /// @brief DSO handle where the function was registered. If it gets destroyed, the functions are called.
    void *dso_handle;
};

static struct atexit_func_entry_t * atexit_func_head = NULL;
static struct atexit_func_entry_t * atexit_func_tail = NULL;

// I don't anticipate atexit-related functions to be called a lot, so a spinlock should be fine
pthread_spinlock_t __atexit_funcs_lock = 0;

int __cxa_atexit(void (*func) (void *), void * arg, void * dso_handle)
{
    struct atexit_func_entry_t * new_func = malloc(sizeof(struct atexit_func_entry_t));
    if (!new_func) {
        // errno is set by malloc
        return -1;
    }

    new_func->destructor_func = func;
    new_func->arg = arg;
    new_func->dso_handle = dso_handle;
    new_func->next = NULL;

    pthread_spin_lock(&__atexit_funcs_lock);

    if (atexit_func_tail)
        atexit_func_tail->next = new_func;
    else
        atexit_func_head = new_func;
    
    new_func->prev = atexit_func_tail;
    atexit_func_tail = new_func;

    pthread_spin_unlock(&__atexit_funcs_lock);

    return 0;
}

int atexit(void (*func)(void))
{
    return __cxa_atexit((void (*)(void *))func, NULL, NULL);
}

void _atexit_pop_all()
{
    while (atexit_func_tail) {
        struct atexit_func_entry_t * func = atexit_func_tail;
        atexit_func_tail = func->prev;

        func->destructor_func(func->arg);
        free(func);
    }
    atexit_func_head = NULL;
}
