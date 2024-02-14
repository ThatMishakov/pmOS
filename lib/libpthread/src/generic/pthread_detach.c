#include <pmos/tls.h>
#include <errno.h>
#include <stdbool.h>
#include <assert.h>
#include <pthread.h>

void __release_tls(struct uthread * u);

int pthread_detach(pthread_t thread)
{
    struct uthread * u = thread;
    if (u == NULL) {
        errno = EINVAL;
        return -1;
    }   

    if (u->thread_status == __THREAD_STATUS_DETACHED || u->thread_status == __THREAD_STATUS_JOINING) {
        errno = EINVAL;
        return -1;
    }

    // I don't think it makes a big difference but this potentially saves compare and swap, which in theory is expensive
    if (u->thread_status == __THREAD_STATUS_FINISHED) {
        __release_tls(u);
        return 0;
    }

    bool is_running = __sync_bool_compare_and_swap(&u->thread_status, __THREAD_STATUS_RUNNING, __THREAD_STATUS_DETACHED);
    if (!is_running) {
        // The thread is already finished (or is joining, which POSIX does not allow)
        assert(u->thread_status == __THREAD_STATUS_FINISHED);
        __release_tls(u);
    }

    return 0;
}