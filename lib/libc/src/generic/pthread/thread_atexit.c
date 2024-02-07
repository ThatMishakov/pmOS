#include <stddef.h>
#include <pmos/tls.h>
#include <errno.h>
#include <stdlib.h>

struct uthread *__get_tls();

void __call_thread_atexit()
{
    struct uthread * u = __get_tls();
    if (u == NULL)
        return;

    struct __atexit_list_entry *entry = u->atexit_list_head, *next;
    while (entry != NULL) {
        entry->destructor_func(entry->arg);
        next = entry->next;
        free(entry);
        entry = next;
    }
}

int __cxa_thread_atexit_impl(void (*func) (void *), void * arg, void * dso_handle)
{
    // TODO: This will need to be revised when we have dynamic linking
    struct uthread * u = __get_tls();
    if (u == NULL) {
        errno = EINVAL;
        return -1;
    }

    struct __atexit_list_entry *entry = malloc(sizeof(struct __atexit_list_entry));
    if (entry == NULL) {
        // malloc() sets errno
        // errno = ENOMEM;
        return -1;
    }

    *entry = (struct __atexit_list_entry) {
        .destructor_func = func,
        .arg = arg,
        .dso_handle = dso_handle,
        .next = u->atexit_list_head,
    };

    u->atexit_list_head = entry;
}