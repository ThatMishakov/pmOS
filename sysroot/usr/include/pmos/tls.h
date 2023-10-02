#ifndef _TLS_H
#define _TLS_H 1
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Internal TLS structure
 * 
 * An internal structure, used to hold Thread Local Storage data (created by the compiler to support variables with __thread keyword). 
 */
typedef struct TLS_Data {
    uint64_t memsz; ///< Memory size in bytes holding the TLS data
    uint64_t align; ///< Memory alignment

    uint64_t filesz; ///< The size of the data. If filesz < memsz, the rest is initialized to 0.
    unsigned char data[0]; ///< TLS data, to which the __thread variables are intiialized upon the program
                           ///< first start and thread creation.
} TLS_Data;

/**
 * @brief Node of the atexit list
 * 
 * This is an internal structure, used to hold the atexit list, for __cxa_thread_atexit_impl() function.
 * It should not be used directly.
 */
struct __atexit_list_entry {
    struct __atexit_list_entry * next;
    void (*destructor_func) (void *);
    void *arg;
    void *dso_handle;
};

/**
 * @brief Structure holging the thread local data
 * 
 * This structure holds the internal data stored needed for correct working of the treads.
 * tls_data variable holds the end of the region which is used by the __thread variabled.
 * self must point to itself. In x86, self must be loaded into %gs or %fs registors depending on the architecture.
 */
typedef struct uthread {
    unsigned char tls_data[0];
    struct uthread *self;
    void * stack_top;
    size_t stack_size;
    void * return_value; // A.k.a. uint64_t
    struct __atexit_list_entry * atexit_list_head;
};

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif // _TLS_H