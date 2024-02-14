#include <pthread.h>
#include <errno.h>
#include <stdbool.h>

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void)) {
    if (once_control == NULL || init_routine == NULL) {
        errno = EINVAL;
        return -1;
    }

    static const int __PTHREAD_ONCE_INIT = 0;

    if (*once_control != __PTHREAD_ONCE_INIT) {
        return 0;
    }

    bool should_initialize = __sync_bool_compare_and_swap(once_control, __PTHREAD_ONCE_INIT, 1);
    if (should_initialize) {
        init_routine();
    }

    return 0;
}